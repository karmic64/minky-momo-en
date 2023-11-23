// Generates the C header containing the detective subgame charset data.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <wchar.h>

#include "text.h"


int main(int argc, char * argv[]) {
	if (argc != 3) {
		puts("usage: make-detective-charset-h charsetdefs outname");
		return EXIT_FAILURE;
	}
	const char * defs_name = argv[1];
	const char * out_name = argv[2];
	
	///////////////////////////////////////////////////////////
	
	FILE * f = fopen(defs_name,"rb");
	if (!f) {
		printf("can't open %s: %s\n",defs_name,strerror(errno));
		return EXIT_FAILURE;
	}
	FILE * of = fopen(out_name,"wb");
	if (!of) {
		fclose(f);
		printf("can't open %s: %s\n",defs_name,strerror(errno));
		return EXIT_FAILURE;
	}
	
	fprintf(of,
		"#pragma once\n"
		"\n"
		"detective_char_t jp_detective_charset[] = {"
	);
	
	int kanji = 0;
	wchar_t line[0x100];
	while (1) {
		size_t linelen = 0;
		while (1) {
			wchar_t c = fgetwcfromutf8(f);
			if (c == L'\n' || c == WEOF)
				break;
			line[linelen++] = c;
		}
		line[linelen] = L'\0';
		
		if (!wcscmp(line, L"kanji")) {
			kanji = 1;
		} else if (kanji) {
			wchar_t * p = line;
			unsigned id = wcstoul(p, &p, 16);
			
			while (1) {
				wchar_t c = *(++p);
				if (!c)
					break;
				
				fprintf(of,
					"{0x%02X,0x%02X,L'",
					id+0x00, id+0x10);
				fputwc2utf8(c, of);
				fprintf(of,
					"'},");
				
				id++;
			}
		} else {
			wchar_t * p = line;
			unsigned top = wcstoul(p, &p, 16);
			unsigned bottom = wcstoul(p, &p, 16);
			
			while (1) {
				wchar_t c = *(++p);
				if (!c)
					break;
				
				fprintf(of,
					"{0x%02X,0x%02X,L'",
					top, bottom);
				fputwc2utf8(c, of);
				fprintf(of,
					"'},");
				
				bottom++;
			}
		}
		
		if (feof(f) || ferror(f)) break;
	}
	
	fseek(of, -1, SEEK_CUR);
	fprintf(of,
		"};"
	);
	
	fclose(f);
	fclose(of);
}

