#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void c_error(const char *msg, int line)
{
	if (line > 0) {
		printf("\x1b[91mCompile error\x1b[0m in line %d: %s\n", line, msg);
		exit(1);
	} else {
		printf("\x1b[91mCompile error\x1b[0m: %s\n", msg);
		exit(1);
	}
}

void c_warning(const char *msg, int line)
{
	if (line > 0) {
		printf("\x1b[93mCompile warning\x1b[0m in line %d: %s\n", line, msg);
	} else {
		printf("\x1b[93mCompile warning\x1b[0m: %s\n", msg);
	}
}

void token_error(const char *msg, int line)
{
	printf("\x1b[91mToken error\x1b[0m in line %d: %s\n", line, msg);
	exit(1);
}

void file_error(const char *msg, int line)
{
	printf("\x1b[91mFile inclusion error\x1b[0m in line %d: %s\n", line, msg);
	exit(1);
}
