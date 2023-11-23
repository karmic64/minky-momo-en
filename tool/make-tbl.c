// Creates a table file for viewing the text in a hex editor.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>

#include "text.h"



int main(int argc, char * argv[]) {
	if (argc != 2) {
		puts("usage: make-tbl outname");
		return EXIT_FAILURE;
	}
	const char * out_name = argv[1];
	
	if (!setlocale(LC_ALL,""))
	{
		puts("can't set locale");
		return EXIT_FAILURE;
	}
	
	FILE * f = fopen(out_name,"wb");
	if (!f) {
		printf("can't open %s: %s\n",out_name,strerror(errno));
		return EXIT_FAILURE;
	}
	unsigned i = 0;
	while (jp_charset[i]) {
		fprintf(f, "%02X=", i);
		wchar_t c = jp_charset[i++];
		fputwc2utf8(c,f);
		fputc('\n',f);
	}
	fclose(f);
	
}
