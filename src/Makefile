CC = gcc

CFLAGS  = -g -Wall

default: compile

compile: main.o lex.o readfile.o error.o
	$(CC) $(CFLAGS) -o compile main.o lex.o readfile.o error.o
	rm *.o

main.o: main.c readfile.h lex.h error.h
	$(CC) $(CFLAGS) -c main.c

lex.o: lex.c lex.h error.h
	$(CC) $(CFLAGS) -c lex.c

readfile.o: readfile.c readfile.h
	$(CC) $(CFLAGS) -c readfile.c

error.o: error.c error.h
	$(CC) $(CFLAGS) -c error.c
