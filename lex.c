#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "error.h"

static char *input;
static int pos;
static char current;

#define next_char()	(current = input[++pos])
#define ungetch()	(current = input[--pos])
#define peek()		(input[pos+1])

Token_type Token;

void start_lexer(char *file_contents)
{
	input = file_contents;
	pos = 0;
	current = input[pos];
}

static char *to_zstring(int start, int len)
{
	char *ret = (char *) malloc(len + 1);
	strncpy(ret, &input[start], len);
	ret[len] = '\0';

	return ret;
}

static void handle_identifier()
{
	while (is_letter(current) || is_digit(current) || is_underscore(current)) {
		next_char();
	}
	Token.class = IDENTIFIER;
}

static void handle_number()
{
	while (is_digit(current) || (current == '.' && Token.class != FLOAT)) {
		if (current == '.') {
			Token.class = FLOAT;
		}
		next_char();
	}
	if (Token.class != FLOAT) {
		Token.class = INT;
	}
}

static void handle_string()
{
	next_char();
	char last;
	while (!(current == '"' && last != '\\')) {
		last = current;
		next_char();
		if (is_end_of_file(current)) {
			Error("Missing \"");
		}
	}
	Token.class = STRING;
}

static void handle_operator()
{
	switch (current)
	{
		case '=':
		case '!':
			next_char();
			switch (current)
			{
				case '=':
					Token.class = TWO_CHAR_COMPARE;
					break;
				default:
					ungetch();
					if (current == '=') {
						Token.class = ONE_CHAR_ASSIGNMENT;
					}
					// handle single '!'
					break;
			}
			break;
		case '+':
		case '-':
		case '*':
		case '/':
			next_char();
			switch (current)
			{
				case '=':
					Token.class = TWO_CHAR_ASSIGNMENT;
					break;
				default:
					ungetch();
					Token.class = BINARY_ARITH;
					break;
			}
			break;
		default:
			Token.class = current;
			break;
	}
}

static void handle_separator()
{
	switch (current)
	{
		case '(':
		case '{':
		case '[':
			Token.class = OPEN_BRACE;
			break;
		case ')':
		case '}':
		case ']':
			Token.class = CLOSE_BRACE;
			break;
		case '.':
			if (is_digit(peek())) {
				do {
					next_char();
				} while (is_digit(current));
				ungetch();
				Token.class = FLOAT;
			} else {
				Token.class = DOT;
			}
			break;
		case ',':
			Token.class = COMMA;
			break;
		case ';':
			Token.class = SEMICOLON;
			break;
		default:
			Token.class = current;
			break;
	}
}

static void skip_layout_and_comments()
{
	while (is_layout(current)) { next_char(); }
	while (is_comment_starter(current)) {
		next_char();
		while (!is_comment_stopper(current)) {
			if (is_end_of_file(current)) return;
			next_char();
		}
		next_char();
		while (is_layout(current)) { next_char(); }
	}
}

void get_next_token()
{
	skip_layout_and_comments();

	int startpos = pos;
	if (is_end_of_file(current)) {
		Token.class = EoF;
		Token.repr  = "<EOF>";
		return;
	}
	if (is_letter(current)) {
		handle_identifier();
	} else {
		if (is_digit(current)) {
			handle_number();
		} else {
			if (current == '"') {
				handle_string();
				next_char();
			} else {
				if (is_separator(current)) {
					handle_separator();
					next_char();
				} else {
					if (is_operator(current)) {
						handle_operator();
						next_char();
					} else {
						Token.class = ERRONEOUS;
						next_char();
					}
				}
			}
		}
	}

	Token.repr = to_zstring(startpos, pos - startpos);
}
