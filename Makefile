vio: main.c
	gcc -g -Wall -Werror -Wpedantic -o vio main.c -lncurses
clean:
	rm vio
