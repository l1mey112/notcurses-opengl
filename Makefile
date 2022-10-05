.PHONY: run
run: a.out
	./a.out

a.out: main.c Makefile
	gcc -lnotcurses -lnotcurses-core -lX11 -lGLX \
		-Wall -Wextra -pedantic -ggdb \
		main.c