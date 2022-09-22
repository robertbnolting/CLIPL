#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lex.h"
#include "error.h"

#define DEFAULT_INCLUDE_PATH	"/home/yog/fun/compilers/fico/include/"

static char *input;
static int pos;
static char current;

static int cur_line;

#define next_char()	(current = input[++pos])
#define ungetch()	(current = input[--pos])
#define peek()		(input[pos+1])
#define prev()		(input[pos-1])

static char *to_zstring();
static void skip_layout_and_comments();

static void handle_identifier();
static void handle_hex();
static void handle_oct();
static void handle_bin();
static void handle_number();
static void handle_string();
static void handle_operator();
static void handle_separator();

static void preprocess();
static char *read_import_directive();
static int read_word();
static int getNextNewlineOffset();

Token_type Token;

Token_type *Token_stream;
size_t Token_stream_size;

void lexer_init(char *file_contents)
{
	input = file_contents;
	
	pos = 0;
	current = input[pos];

	preprocess(input);

	pos = 0;
	current = input[pos];

	cur_line = 1;

	Token_stream = (Token_type *) malloc(1);
	Token_stream_size = 0;
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

static void handle_hex()
{
	while (is_digit(current) || is_hex_letter(current)) {
		next_char();
	}

	Token.class = INT;
}

static void handle_oct()
{
	while (current >= '0' && current <= '7') {
		next_char();
	}

	Token.class = INT;
}

static void handle_bin()
{
	while (current == '0' || current == '1') {
		next_char();
	}

	Token.class = INT;
}

static void handle_number()
{
	while (is_digit(current) || (current == '.' && Token.class != FLOAT) ||
	       (is_base_prefix(current) && prev() == '0' && 
		( (current == 'x' || current == 'X') ? (is_digit(peek()) || is_hex_letter(peek())) : ( (current == 'b' || current == 'B') ? (peek() == '1' || peek() == '0') : 
		(peek() >= '0' && peek() <= '7'))))) {
		if (current == '.') {
			Token.class = FLOAT;
		}
		if (current == 'x' || current == 'X') {
			next_char();
			handle_hex();
			break;
		} else if (current == 'o' || current == 'O') {
			next_char();
			handle_oct();
			break;
		} else if (current == 'b' || current == 'B') {
			next_char();
			handle_bin();
			break;
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
			c_error("Missing \"");
		}
	}
	Token.class = STRING;
}

static void handle_operator()
{
	switch (current)
	{
		case '=':
			next_char();
			switch (current)
			{
				case '=':
					Token.class = EQ;
					break;
				default:
					ungetch();
					Token.class = current;
			}
			break;
		case '!':
			next_char();
			switch (current)
			{
				case '=':
					Token.class = NE;
					break;
				default:
					ungetch();
					Token.class = current;
					break;
			}
			break;
		case '+':
			next_char();
			if (current == '=') {
				Token.class = ADD_ASSIGN;
			} else {
				ungetch();
				Token.class = current;
			}
			break;
		case '-':
			next_char();
			if (current == '>') {
				Token.class = ARROW_OP;
			} else if (current == '=') {
				Token.class = SUB_ASSIGN;
			} else {
				ungetch();
				Token.class = current;
			}
			break;
		case '*':
			next_char();
			if (current == '=') {
				Token.class = MUL_ASSIGN;
			} else {
				ungetch();
				Token.class = current;
			}
			break;
		case '/':
			next_char();
			if (current == '=') {
				Token.class = DIV_ASSIGN;
			} else {
				ungetch();
				Token.class = current;
			}
			break;
		case '>':
			next_char();
			switch(current)
			{
				case '=':
					Token.class = GE;
					break;
				default:
					ungetch();
					Token.class = current;
					break;

			}
			break;
		case '<':
			next_char();
			switch (current)
			{
				case '=':
					Token.class = LE;
					break;
				default:
					ungetch();
					Token.class = current;
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
		case '.':
			if (is_digit(peek())) {
				do {
					next_char();
				} while (is_digit(current));
				ungetch();
				Token.class = FLOAT;
			} else {
				Token.class = current;
			}
			break;
		default:
			Token.class = current;
			break;
	}
}

static void skip_layout_and_comments()
{
	while (is_layout(current)) { 
		if (current == '\n') { cur_line++; }
		next_char(); 
	}
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
	} else {
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
	}

	Token.repr = to_zstring(startpos, pos - startpos);

	Token_stream = realloc(Token_stream, sizeof(Token_type) * (Token_stream_size+1));
	Token_stream[Token_stream_size].class = Token.class;
	Token_stream[Token_stream_size].repr = (char *) malloc((pos - startpos) + 1);
	strcpy(Token_stream[Token_stream_size].repr, Token.repr);
	Token_stream[Token_stream_size].line = cur_line;
	Token_stream_size++;
}

static void preprocess(char *text)
{
	char *content = NULL;
	for (;;) {
		if (current == '!') {
			int saved_pos = pos;
			content = read_import_directive();
			if (content != NULL) {
				size_t input_len = strlen(input);
				input = realloc(input, input_len + strlen(content) + 1);
				memmove(input+saved_pos+strlen(content), input+pos, input_len-pos);
				strncpy(input+saved_pos, content, strlen(content));
				input[strlen(content)+(input_len-pos)] = '\0';
			}
		}
		if (is_end_of_file(current)) {
			break;
		}
		next_char();
	}
}

static char *read_import_directive()
{
	next_char();

	if (!read_word("import")) {
		return NULL;
	} else {
		skip_layout_and_comments();
		char *filename = NULL;
		size_t filename_len = 0;
		char *filepath = NULL;
		while (is_letter(current) || is_digit(current) || is_underscore(current)) {
			filename = realloc(filename, filename_len+1);
			filename[filename_len] = current;
			filename_len++;
			next_char();
		}
		filename = realloc(filename, filename_len+1);
		filename[filename_len] = '\0';
		if (filename != NULL) {
			filepath = malloc(filename_len + strlen(DEFAULT_INCLUDE_PATH) + 1);
			strcpy(filepath, DEFAULT_INCLUDE_PATH);
			strcat(filepath, filename);
			//memmove(filename+strlen(DEFAULT_INCLUDE_PATH), filename, strlen(filename));
			//strncpy(filename, DEFAULT_INCLUDE_PATH, strlen(DEFAULT_INCLUDE_PATH));

			FILE *fp = fopen(filepath, "r");
			if (fp == NULL) {
				free(filepath);
				char cwd[1024] = {0};
				getcwd(cwd, sizeof(cwd));
				filepath = malloc(filename_len + strlen(cwd) + 2);
				strcpy(filepath, cwd);
				strcat(filepath, "/");
				strcat(filepath, filename);
				//filename = realloc(filename, strlen(cwd));
				//memmove(filename+strlen(cwd), filename, strlen(filename));
				//strncpy(filename, cwd, strlen(cwd));

				fp = fopen(filepath, "r");
				if (fp == NULL) {
					free(filename);
					free(filepath);
					c_error("Import file not found.");
					return NULL;
				}
			}
			fseek(fp, 0, SEEK_END);
			size_t file_sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			char *file_content = malloc(file_sz + 1);
			fread(file_content, 1, file_sz, fp);
			file_content[file_sz] = '\0';

			free(filename);
			fclose(fp);

			return file_content;
		} else {
			return NULL;
		}
	}
}

static int read_word(char *word)
{
	for (int i = 0; i < strlen(word); i++) {
		if (current != word[i]) {
			return 0;
		}
		next_char();
	}

	return 1;
}
