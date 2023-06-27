CC = gcc

CFLAGS  = -g3 -Wall

default: clipl

clipl: main.o lex.o parse.o readfile.o error.o gen.o
	$(CC) $(CFLAGS) -o clipl main.o lex.o parse.o readfile.o error.o gen.o
	rm *.o

main.o: main.c readfile.h lex.h parse.h error.h gen.o
	$(CC) $(CFLAGS) -c main.c

lex.o: lex.c lex.h error.h
	$(CC) $(CFLAGS) -c lex.c

parse.o: parse.c parse.h
	$(CC) $(CFLAGS) -c parse.c

readfile.o: readfile.c readfile.h
	$(CC) $(CFLAGS) -c readfile.c

error.o: error.c error.h
	$(CC) $(CFLAGS) -c error.c

gen.o: gen.c gen.h
	$(CC) $(CFLAGS) -c gen.c
