#include <stdlib.h>
#include <stdio.h>

#include "parse.h"
#include "lex.h"

static int pos;

#define get()	(&Token_stream[pos])
#define next()	(&Token_stream[pos++])
#define peek()	(&Token_stream[pos+1])
#define prev()	(&Token_stream[pos-1])

static void expect();
static int next_token();
static void parse();
static void read_int();
static void read_arithmetic_expr();

void parser_init()
{
	pos = 0;

	parse();
}

static void expect(int tclass)
{
	Token_type *t = get();
	if (t->class != tclass) {
		printf("Error: Unexpected token.\n");
	}
}

static int next_token(int tclass)
{
	Token_type *t = get();
	if (t->class == tclass) {
		return 1;
	} else {
		return 0;
	}
}

static void parse()
{
	Token_type *start = get();

	switch (start->class)
	{
		case EoF:
			return;
		case INT:
			read_int();
			break;
	}
}

static void read_int()
{
	Token_type *t = next();

	if (t->class == '+' || t->class == '-' ||
	    t->class == '*' || t->class == '/') 
	{
		read_arithmetic_expr();
	} else if (t->class == ';') {
	} else if (t->class == ',') {
	} else if (t->class == ')') {
	} else if (t->class == TWO_CHAR_COMPARE) {
	} else {
		printf("Error: Unexpected token.\n");
	}
}

static void read_arithmetic_expr()
{
	Token_type *t = get();
}
