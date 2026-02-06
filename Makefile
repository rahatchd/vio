vio: main.c
	gcc -g -Wall -Werror -Wpedantic -Wshadow -Wconversion -fsanitize=address,undefined -o vio main.c -lncurses
clean:
	rm vio
