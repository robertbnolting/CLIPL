#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "readfile.h"
#include "lex.h"
#include "parse.h"
#include "error.h"

static void printHelp()
{
	printf(
	"Usage: clipl <file> [options]\n\n"
	"Options:\n"
	"-o		Specify the name of the output file\n"
	"-s		Output assembly\n"
	"-d[type]	Specify which debug outputs should be generated\n"
	"-dlex		Print lexer output\n"
	"-dast		Print abstract syntax tree output\n"
	"-dcfg		Print control-flow graph output\n"
	"-dsym		Print symbolic interpreter output\n"
	"-dlive		Print lva and graph-colorer output\n"
	"-dps		Print pseudo-assembly output(collides with above option)\n"
	"-D		Show all debug output (except -dps)\n"
	"-h		Print this help page\n"
	);
}

int ast_out = 0;
int cfg_out = 0;
int sym_out = 0;
int live_out = 0;
int ps_out = 0;

int main(int argc, char **argv)
{
	if (argc > 1) {
		char *filename = argv[1];
		int assembly_out = 0;
		int lex_out = 0;

		if (!strcmp(filename, "-h")) {
			printHelp();
			return 0;
		}

		if (strcmp(&filename[strlen(filename)-6], ".clipl")) {
			g_error("Unknown file format. Program  accepts files with .clipl extension.");
		}

		char *output_file = malloc(strlen(filename));
		strncpy(output_file, filename, strlen(filename) - 6);
		output_file[strlen(filename) - 6] = '\0';

		char *option;
		for (int i = 2; i < argc; i++) {
			option = argv[i];
			if (option[0] != '-') {
				printf("'-' required before option %s.\n", option);
			} else {
				switch (option[1])
				{
				case 'o':
					output_file = realloc(output_file, strlen(argv[i+1]) + 1);
					strcpy(output_file, argv[++i]);
					break;
				case 's':
					assembly_out = 1;
					break;
				case 'd':
					if (!strcmp(&option[2], "lex")) {
						lex_out = 1;
					} else if (!strcmp(&option[2], "ast")) {
						ast_out = 1;
					} else if (!strcmp(&option[2], "cfg")) {
						cfg_out = 1;
					} else if (!strcmp(&option[2], "sym")) {
						sym_out = 1;
					} else if (!strcmp(&option[2], "live")) {
						live_out = 1;
					} else if (!strcmp(&option[2], "ps")) {
						ps_out = 1;
					} else {
						printf("Unknown debug option %s.\n", &option[2]);
					}

					break;
				case 'D':
					lex_out = 1;
					ast_out = 1;
					cfg_out = 1;
					sym_out = 1;
					live_out = 1;
					break;
				case 'h':
					printHelp();
					break;
				default:
					printf("Unknown option: %s.\n", &option[1]);
					printHelp();
					break;
				}
			}
		}

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

		if (lex_out) {
			for (int i = 0; i < Token_stream_size; i++) {
				printf("%s\n", Token_stream[i].repr);
			}
		}

		parser_init(output_file);

		if (!assembly_out && !ps_out) {
			char *cmd = malloc(128);
			sprintf(cmd, "nasm -felf64 %s.s", output_file);
			system(cmd);

			sprintf(cmd, "rm %s.s", output_file);
			system(cmd);

			sprintf(cmd, "ld -o %s %s.o", output_file, output_file);
			system(cmd);

			sprintf(cmd, "rm %s.o", output_file);
			system(cmd);
		}

		return 0;
	}

	g_error("No input file specified.");
}
