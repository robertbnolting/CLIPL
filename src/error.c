#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void c_error(const char *msg, int line)
{
	printf("\x1b[31mCompile error\x1b[0m in line %d: %s\n", line, msg);
	exit(1);
}
