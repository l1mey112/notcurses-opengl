.PHONY: run
run: a.out
	./a.out

a.out: main.c Makefile
	gcc -lnotcurses -lnotcurses-core -lOpenCL -lm \
		-Wall -Wextra -pedantic -ggdb \
		main.c