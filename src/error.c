#include <stdio.h>

#include "error.h"

void c_error(const char *msg)
{
	printf("\x1b[31mCompile error:\x1b[0m %s\n", msg);
}
