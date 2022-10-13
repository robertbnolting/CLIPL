#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void c_error(const char *msg, int line)
{
	printf("\x1b[91mCompile error\x1b[0m in line %d: %s\n", line, msg);
	exit(1);
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
