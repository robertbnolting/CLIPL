#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "error.h"

static char *input;
static int pos;
static char current;

#define next_char() (current = input[++pos])

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

static void handle_integer()
{
	while (is_digit(current)) { next_char(); }
	Token.class = INT;
}

static void handle_string()
{
	next_char();
	while (current != '"') {
		next_char();
		if (is_end_of_file(current)) {
			Error("Missing \"");
		}
	}
	Token.class = STRING;
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
			handle_integer();
		} else {
			if (current == '"') {
				handle_string();
				next_char();
			} else {
				if (is_operator(current) || is_separator(current)) {
					Token.class = current;
					next_char();
				} else {
					Token.class = ERRONEOUS;
					next_char();
				}
			}
		}
	}

	Token.repr = to_zstring(startpos, pos - startpos);
}
