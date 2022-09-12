#include <stdio.h>
#include <stdlib.h>

#include "readfile.h"

char *readFile(char *filename)
{
	FILE *fp = fopen(filename, "r");

	if (fp == NULL) {
		printf("Error: File not found.\n");
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *file_content = (char *) malloc(sz+1);

	fread((char*) file_content, 1, sz, fp);

	return file_content;
}
