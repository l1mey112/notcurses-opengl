.PHONY: run
run: main.c
	gcc -ggdb -lnotcurses -lnotcurses-core main.c
	./a.out
