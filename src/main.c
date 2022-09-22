#include <stdio.h>
#include <stdlib.h>

#include "readfile.h"
#include "lex.h"
#include "parse.h"
#include "error.h"

#define ENABLE_LEX_OUTPUT 0

int main(int argc, char **argv)
{
	if (argc > 1) {
		char *filename = argv[1];

		char *input = readFile(filename);
		if (input) {
			lexer_init(input);
		} else {
			return 1;
		}

		do {
			get_next_token();
			if (Token.class != EoF)
				free(Token.repr);
		} while (Token.class != EoF);

#if ENABLE_LEX_OUTPUT
		for (int i = 0; i < Token_stream_size; i++) {
			printf("%s\n", Token_stream[i].repr);
			//printf("%d\n", Token_stream[i].class);
		}
#endif

		parser_init();

		return 0;
	}

	printf("No input file specified.\n");
	return 1;
}
