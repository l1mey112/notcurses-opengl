.PHONY: run
run: a.out
	./a.out

a.out: main.c Makefile
	gcc -lnotcurses -lnotcurses-core -lglfw -lOpenGL -lGL -lGLEW \
		-Wall -Wextra -pedantic -ggdb \
		main.c