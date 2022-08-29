CC = gcc

CFLAGS  = -g -Wall

default: compile

compile: main.o lex.o parse.o readfile.o error.o
	$(CC) $(CFLAGS) -o compile main.o lex.o parse.o readfile.o error.o
	rm *.o

main.o: main.c readfile.h lex.h parse.h error.h
	$(CC) $(CFLAGS) -c main.c

lex.o: lex.c lex.h error.h
	$(CC) $(CFLAGS) -c lex.c

parse.o: parse.c parse.h
	$(CC) $(CFLAGS) -c parse.c

readfile.o: readfile.c readfile.h
	$(CC) $(CFLAGS) -c readfile.c

error.o: error.c error.h
	$(CC) $(CFLAGS) -c error.c
