#include <errno.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *logfile = NULL;

void open_log_file(void) {
  if ((logfile = fopen("log", "w")) == NULL) {
    const int err = errno;
    fprintf(stderr, "failed to open log file: %s\n", strerror(err));
  }
}

void log_msg(char* msg) {
  if (logfile) fprintf(logfile, "%s\n", msg);
}

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} line_t;

line_t *line_new(size_t cap) {
  line_t *line = malloc(sizeof(line_t));
  if (line == NULL) {
    return NULL;
  }
  if (cap == 0) {
    line->data = NULL;
  } else {
    line->data = calloc(cap, sizeof(char));
    if (line->data == NULL) {
      free(line);
      return NULL;
    }
  }
  line->len = 0;
  line->cap = cap;
  return line;
}

void line_free(line_t *line) {
  if (line == NULL) {
    return;
  }
  if (line->data != NULL) {
    free(line->data);
  }
  line->len = 0;
  free(line);
}

void line_insert_brute_force(line_t *line, size_t pos, char c) {
  if (pos < 0 || pos > line->len) {
    return;
  }
  if (line->len + 1 >= line->cap) {
    size_t newcap = 2 * line->cap;
    char *d = realloc(line->data, newcap * sizeof(char));
    if (d == NULL) {
      return;
    }
    line->data = d;
    line->cap = newcap;
  }
  line->len++;
  char swap = c;
  for (size_t i = pos; i < line->len; i++) {
    char tmp = line->data[i];
    line->data[i] = swap;
    swap = tmp;
  }
  line->data[line->len] = '\0';
}

void line_remove_char_at(line_t *line, size_t pos) {
  if (pos < 0 || pos > line->len) {
    return;
  }
  if (line->len + 1 <= line->cap / 2) {
    size_t newcap = line->cap / 2;
    char *d = realloc(line->data, newcap * sizeof(char));
    if (d == NULL) {
      return;
    }
    line->data = d;
    line->cap = newcap;
  }
  for (size_t i = pos + 1; i < line->len; i++) {
    line->data[i - 1] = line->data[i];
  }
  line->data[--line->len] = '\0';
}

line_t *line_split(line_t *line, size_t pos) {
  if (pos < 0 || line->len == 0) {
    pos = 0;
  }
  if (line->len && pos > line->len - 1) {
    pos = line->len - 1;
  }
  size_t n = line->len - pos;
  line_t *lnew = line_new(n + 1);
  if (lnew == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < n; i++) {
    lnew->data[i] = line->data[pos + i];
    line->data[pos + i] = '\0';
  }
  lnew->len = line->len - pos;
  line->len = pos;
  return lnew;
}

typedef struct {
  line_t **lines;
  size_t size;
  size_t cap;
} buffer_t;

buffer_t *buffer_new(size_t cap) {
  buffer_t *buf = malloc(sizeof(buffer_t));
  if (buf == NULL) {
    return NULL;
  }
  buf->lines = cap ? malloc(cap * sizeof(line_t *)) : NULL;
  if (cap && buf->lines == NULL) {
    free(buf);
    return NULL;
  }
  buf->cap = cap;
  buf->size = 0;
  return buf;
}

void buffer_free(buffer_t *buf) {
  if (buf == NULL) {
    return;
  }
  for (size_t i = 0; i < buf->size; i++) {
    if (buf->lines[i] != NULL) {
      line_free(buf->lines[i]);
    }
  }
  free(buf->lines);
  buf->size = 0;
  free(buf);
}

int buffer_append_line(buffer_t *buf, line_t *line) {
  if (buf->size >= buf->cap) {
    size_t cap = buf->cap ? 2 * buf->cap : 2;
    line_t **lines = realloc(buf->lines, cap * sizeof(line_t *));
    if (lines == NULL) {
      return -1;
    }
    buf->cap = cap;
    buf->lines = lines;
  }
  buf->lines[buf->size++] = line;
  return 0;
}

buffer_t *buffer_from_file(FILE *file) {
  buffer_t *buf = buffer_new(0);
  if (buf == NULL) {
    return NULL;
  }
  line_t *line = line_new(0);
  if (line == NULL) {
    buffer_free(buf);
    return NULL;
  }
  ssize_t n;
  while ((n = getline(&line->data, &line->cap, file)) != -1) {
    line->len = (size_t)n;
    if (line->len && line->data[line->len - 1] == '\n') {
      line->data[--line->len] = '\0';
    }
    if (buffer_append_line(buf, line) != 0) {
      line_free(line);
      buffer_free(buf);
      return NULL;
    }
    line = line_new(0);
    if (line == NULL) {
      buffer_free(buf);
      return NULL;
    }
  }
  line_free(line);
  return buf;
}

void buffer_insert_line(buffer_t *buf, line_t *line, size_t n) {
  if (n < 0 || n > buf->size || buf == NULL || line == NULL) {
    return;
  }
  if (buf->size + 1 >= buf->cap) {
    size_t cap = buf->cap ? 2 * buf->cap : 2;
    line_t **lines = realloc(buf->lines, cap * sizeof(line_t *));
    if (lines == NULL) {
      return;
    }
    buf->cap = cap;
    buf->lines = lines;
  }
  line_t *swap = line;
  for (size_t i = n + 1; i < buf->size + 1; i++) {
    line_t *tmp = buf->lines[i];
    buf->lines[i] = swap;
    swap = tmp;
  }
  buf->size++;
}

void close_file(FILE *file) {
  if (fclose(file) != 0) {
    const int err = errno;
    fprintf(stderr, "failed to close file: %s\n", strerror(err));
    exit(err);
  }
}

void log_buffer(buffer_t *buf, FILE *file) {
  printf("buffer:\n");
  printf("===\n");
  for (size_t i = 0; i < buf->size; i++) {
    printf("%s\n", buf->lines[i]->data);
  }
  printf("===\n");

  size_t total_cap = 0;
  for (size_t i = 0; i < buf->size; i++) {
    total_cap += buf->lines[i]->len;
  }
  printf("estimated resources used: %zu bytes\n", total_cap);
  fseek(file, 0L, SEEK_END);
  printf("original file size: %zu bytes; %zu lines\n", ftell(file), buf->size);
}

size_t clamp(size_t min, size_t max, size_t val) {
  if (val <= min) {
    return min;
  }
  if (val >= max) {
    return max;
  }
  return val;
}

enum mode_t {
  MODE_INSERT,
  MODE_NORMAL,
};

typedef struct {
  size_t cx;
  size_t cy;
  size_t maxx;
  size_t maxy;
  size_t topoff;
  size_t prefx;
  mode_t mode;
  buffer_t *buf;
} tui_state_t;

size_t num_digits(size_t n) {
  size_t d = 0;
  do {
    n /= 10;
    d++;
  } while (n);
  return d;
}

void render_status_bar(tui_state_t *ts) {
  move(ts->maxy - 1, 0);
  attron(COLOR_PAIR(2));
  switch (ts->mode) {
  case MODE_INSERT:
    printw("-- INSERT --");
    break;
  case MODE_NORMAL:
    printw("-- NORMAL --");
    break;
  }
  const size_t digits = num_digits(ts->cx) + num_digits(ts->cy);
  const int left_offset = 12;
  const int right_offset = 6;
  for (size_t i = left_offset; i < ts->maxx - digits - right_offset; i++) {
    printw(" ");
  }
  printw("%d, %d", ts->cy, ts->cx);
  for (size_t i = ts->maxx; i > ts->maxx - digits - right_offset; i--) {
    printw(" ");
  }
  attroff(COLOR_PAIR(2));
  move(ts->cy, ts->cx);
}

void normal(tui_state_t *ts, int key) {
  switch (key) {
  case 'k':
  case KEY_UP:
    ts->cy--;
    break;
  case 'j':
  case KEY_DOWN:
    ts->cy++;
    break;
  case 'h':
  case KEY_LEFT:
    if (ts->prefx > 0) {
      ts->prefx--;
    }
    break;
  case 'l':
  case KEY_RIGHT:
    ts->prefx++;
    if (ts->prefx >= ts->buf->lines[ts->cy]->len) {
      ts->prefx = ts->buf->lines[ts->cy]->len - 1;
    }
    break;
  case 'i':
    ts->mode = MODE_INSERT;
    break;
  default:
    break;
  }
  ts->cy = clamp(0, ts->buf->size ? ts->buf->size - 1 : 0, ts->cy);
  ts->cx = clamp(
      0, ts->buf->lines[ts->cy]->len ? ts->buf->lines[ts->cy]->len - 1 : 0,
      ts->prefx);
  move(ts->cy, ts->cx);
}

void render_buffer(tui_state_t *ts) {
  size_t max_rows = ts->buf->size > ts->maxy - 2 ? ts->maxy - 2 : ts->buf->size;
  move(0, 0);
  for (size_t i = 0; i < max_rows; i++) {
    printw("%s\n", ts->buf->lines[i]->data);
  }
  move(ts->cy, ts->cx);
  refresh();
}

void render_line(tui_state_t *ts) {
  move(ts->cy, 0);
  clrtoeol();
  printw("%s", ts->buf->lines[ts->cy]->data);
  move(ts->cy, ts->cx);
  refresh();
}

void insert(tui_state_t *ts, int key) {
  switch (key) {
  case 27: // ESC
    nodelay(stdscr, TRUE);
    noecho();
    key = getch();
    if (key == ERR) {
      ts->mode = MODE_NORMAL;
    } else {
      ungetch(key);
    }
    nodelay(stdscr, FALSE);
    break;
  case KEY_BACKSPACE:
  case KEY_DC:
  case 127:
    line_remove_char_at(ts->buf->lines[ts->cy], ts->cx - 1);
    if (ts->cx > 0) {
      ts->cx--;
      ts->prefx = ts->cx;
    }
    render_buffer(ts);
    break;
  case '\n':
  case KEY_ENTER: {
    line_t *lnew = line_split(ts->buf->lines[ts->cy], ts->cx);
    buffer_insert_line(ts->buf, lnew, ts->cy);
    render_buffer(ts);
    ts->cy++;
    ts->cx = 0;
    render_status_bar(ts);
    move(ts->cy, ts->cx);
    break;
  }
  default:
    if (0 < key && key < 255) { // ascii
      line_insert_brute_force(ts->buf->lines[ts->cy], ts->cx, (char)key);
      ts->cx++;
      ts->prefx = ts->cx;
      render_line(ts);
    }
    break;
  }
}

void tui(buffer_t *buf) {
  initscr();
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLACK);
  init_pair(2, COLOR_BLACK, COLOR_WHITE);
  keypad(stdscr, TRUE);
  set_escdelay(0);
  cbreak();
  noecho();
  clear();
  refresh();
  tui_state_t ts = {0};
  ts.mode = MODE_NORMAL;
  ts.buf = buf;
  getmaxyx(stdscr, ts.maxy, ts.maxx);
  render_buffer(&ts);
  render_status_bar(&ts);
  int key;
  do {
    noecho();
    key = getch();
    switch (ts.mode) {
    case MODE_NORMAL:
      normal(&ts, key);
      break;
    case MODE_INSERT:
      insert(&ts, key);
      break;
    }
    render_status_bar(&ts);
    refresh();
  } while (!(ts.mode == MODE_NORMAL && key == 'q'));
  endwin();
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename>\n", argv[0]);
    exit(1);
  }
  const char *filename = argv[1];
  FILE *file = NULL;
  if ((file = fopen(filename, "r")) == NULL) {
    const int err = errno;
    fprintf(stderr, "failed to open file: %s\n", strerror(err));
    exit(err);
  }
  open_log_file();
  buffer_t *buf = buffer_from_file(file);
  if (buf == NULL) {
    fprintf(stderr, "failed to load file into buffer");
    close_file(file);
    exit(1);
  }

  printf("before tui\n");
  log_buffer(buf, file);

  tui(buf);

  printf("after tui\n");
  log_buffer(buf, file);

  buffer_free(buf);
  close_file(file);
  if (logfile) close_file(logfile);
  exit(0);
}
