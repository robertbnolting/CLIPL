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
		lexer_init(input);

		do {
			get_next_token();
#if ENABLE_LEX_OUTPUT
			switch (Token.class)
			{
				case IDENTIFIER: printf("Identifier: %s\n", Token.repr);
						 break;
				case INT: printf("Integer: %s\n", Token.repr);
					  break;
				case FLOAT: printf("Float: %s\n", Token.repr);
					    break;
				case STRING: printf("String: %s\n", Token.repr);
					     break;
				case EoF: printf("EoF: %s\n", Token.repr);
					  return 0;
				case ERRONEOUS: printf("Erroneous token: %s\n", Token.repr);
						break;
				case ONE_CHAR_ASSIGNMENT: printf("Single-char assignment operator: %s\n", Token.repr);
							  break;
				case BINARY_ARITH: printf("Binary arithmetic operator: %s\n", Token.repr);
						   break;
				case TWO_CHAR_ASSIGNMENT: printf("Two-char assignment operator: %s\n", Token.repr);
							  break;
				case TWO_CHAR_COMPARE: printf("Two-char compare operator: %s\n", Token.repr);
						       break;
				case DOT: printf("Dot separator: %s\n", Token.repr);
					  break;
				case COMMA: printf("Comma separator: %s\n", Token.repr);
					    break;
				case SEMICOLON: printf("Semicolon separator: %s\n", Token.repr);
						break;
				case OPEN_BRACE: printf("Open brace: %s\n", Token.repr);
					       break;
				case CLOSE_BRACE: printf("Closed brace: %s\n", Token.repr);
						break;
				default: printf("Single-char Operator or separator: %s\n", Token.repr);
					 break;
			}

#endif
			if (Token.class != EoF)
				free(Token.repr);
		} while (Token.class != EoF);

		/*
		for (int i = 0; i < Token_stream_size; i++) {
			printf("%s\n", Token_stream[i].repr);
		}
		*/

		parser_init();

		return 0;
	}

	printf("No input file specified.\n");
	return 1;
}
