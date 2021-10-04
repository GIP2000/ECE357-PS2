all: find
PHONY: clean

find: find.c
	gcc -o find find.c

clean: find
	rm -f find
