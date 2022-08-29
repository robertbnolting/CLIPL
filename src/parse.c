#include <stdlib.h>

#include "parse.h"
#include "lex.h"

static int pos;

#define get()	(&token_stream[pos])
#define next()	(&token_stream[pos++])
#define peek()	(&token_stream[pos+1])
#define prev()	(&token_stream[pos-1])

void parser_init()
{
	pos = 0;

	parse();
}

static void expect(int tclass)
{
	Token *t = get();
	if (t->class != tclass) {
		printf("Error: Unexpected token.\n");
	}
}

static int next_token(int tclass)
{
	Token *t = get();
	if (t->class == tclass) {
		return 1;
	} else {
		return 0;
	}
}

static void parse()
{
	Token *start = get();

	switch (start.class)
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
	Token *t = next();

	if (t->class == BINARY_ARITH) {
		read_arithmetic_expr();
	} else if (t->class == SEMICOLON) {
	} else if (t->class == COMMA) {
	} else if (t->class == CLOSE_BRACE) {
	} else if (t->class == TWO_CHAR_COMPARE) {
	} else {
		printf("Error: Unexpected token.\n");
	}
}

static void read_arithmetic_expr()
{
	Token *t = get();
}
