.PHONY: run
run: main.c
	gcc -ggdb -lnotcurses -lnotcurses-core -lOpenCL -lm main.c
	./a.out
