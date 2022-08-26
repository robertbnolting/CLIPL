#include <stdio.h>

#include <error.h>

void Error(char *msg)
{
	printf("\x1b[31mERROR:\x1b[0m %s\n", msg);
}
