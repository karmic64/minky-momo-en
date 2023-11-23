// Creates an editable text file from the text address list.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "rom.h"
#include "text.h"
#include "detective-text.h"



uint8_t prg[PRG_SIZE];


#define expand_array(ptr, cnt, max, initial_max) \
	if ((cnt) >= (max)) { \
		if ((max) == 0) \
			(max) = (initial_max); \
		else \
			(max) *= 2; \
		(ptr) = realloc((ptr), (max) * sizeof(*(ptr))); \
	}



#define MAX_STRING_POINTERS 0x10

typedef struct {
	uint8_t bank;
	uint16_t addr;
	unsigned ptr_cnt;
	uint16_t lo[MAX_STRING_POINTERS];
	uint16_t hi[MAX_STRING_POINTERS];
	uint8_t detective;
} text_string_t;

text_string_t * text_string_tbl = NULL;
size_t text_string_cnt = 0;
size_t text_string_max = 0;

size_t add_text_string(unsigned bank, unsigned addr, unsigned lo, unsigned hi, unsigned detective) {
	// check duplicate
	for (size_t i = 0; i < text_string_cnt; i++) {
		text_string_t * cmp = &text_string_tbl[i];
		if (cmp->bank == bank && cmp->addr == addr) {
			if (cmp->ptr_cnt == MAX_STRING_POINTERS) {
				printf("WARNING: ignoring bank %u addr %04X lo %04X hi %04X\n",
					bank, addr, lo, hi);
			} else {
				cmp->lo[cmp->ptr_cnt] = lo;
				cmp->hi[cmp->ptr_cnt] = hi;
				cmp->ptr_cnt++;
			}
			return cmp - text_string_tbl;
		}
	}
	
	// must add new
	expand_array(text_string_tbl, text_string_cnt + 1, text_string_max, 0x200);
	text_string_t * this = &text_string_tbl[text_string_cnt++];
	this->bank = bank;
	this->addr = addr;
	this->ptr_cnt = 1;
	this->lo[0] = lo;
	this->hi[0] = hi;
	this->detective = detective;
	return this - text_string_tbl;
}







int main(int argc, char * argv[]) {
	if (argc != 3) {
		puts("usage: dump-text textdefs outfile");
		return EXIT_FAILURE;
	}
	const char * text_defs_name = argv[1];
	const char * out_name = argv[2];
	
	///////////////////////////////////////////////////////
	
	
	FILE * f = fopen(ROM_NAME,"rb");
	if (!f) {
		printf("can't open rom: %s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	for (int i = 0; i < NES_HEADER_SIZE; i++)
		fgetc(f);
	fread(prg,1,PRG_SIZE,f);
	fclose(f);
	
	/////////////////////////////////////////////////////// read text defs
	
	f = fopen(text_defs_name,"r");
	if (!f) {
		printf("can't open %s: %s\n",text_defs_name,strerror(errno));
		return EXIT_FAILURE;
	}
	
	char line[0x100];
	int lineno = 0;
	while (1) {
		// get line
		lineno++;
		if (!fgets(line, 0x100, f))
			break;
		if (feof(f) || ferror(f))
			break;
		
		// kill comment
		char * sharp = strchr(line, '#');
		if (sharp)
			*sharp = '\0';
		
		// parse loop
		const char * whitespace = " \n\r\t\v\f";
		int cmdcount = 0;
		int bank = -1;
		int lo = -1;
		int hi = -1;
		int amt = -1;
		int detective = 0;
		while (1) {
			char * cmd = strtok(cmdcount++ ? NULL : line, whitespace);
			if (!cmd)
				break;
			
			if (!strcasecmp(cmd, "detective")) {
				detective = 1;
				continue;
			}
			
			char * args = strtok(NULL, whitespace);
			if (!args) {
				printf("%d: %s has no arg\n",lineno,cmd);
				break;
			}
			int arg = 0;
			char * argp = args;
			while (*argp) {
				arg *= 0x10;
				char c = *(argp++);
				if (c >= 'A' && c <= 'F') {
					arg += (c - 'A') + 0x0a;
				} else if (c >= 'a' && c <= 'f') {
					arg += (c - 'a') + 0x0a;
				} else if (c >= '0' && c <= '9') {
					arg += c - '0';
				} else {
					arg = -1;
					break;
				}
			}
			if (arg < 0) {
				printf("%d: can't parse %s as hex integer\n",lineno,args);
				break;
			}
			
			if (!strcasecmp(cmd, "bank")) {
				bank = arg;
			} else if (!strcasecmp(cmd, "lo")) {
				lo = arg;
			} else if (!strcasecmp(cmd, "hi")) {
				hi = arg;
			} else if (!strcasecmp(cmd, "amt")) {
				amt = arg;
			} else {
				printf("%d: don't know what %s is\n",lineno,cmd);
				break;
			}
		}
		
		// add strings
		if (bank >= 0 && lo >= 0 && hi >= 0) {
			if (amt < 1) {
				amt = 1;
			}
			
			unsigned loi = get_prg_offset(bank, lo);
			unsigned hii = get_prg_offset(bank, hi);
			while (amt--) {
				unsigned addr = prg[loi++] | (prg[hii++] << 8);
				add_text_string(bank, addr, lo, hi, detective);
				lo++;
				hi++;
			}
		}
	}
	
	fclose(f);
	
	//////////////////////////////////////////////////////// write text
	
	FILE * of = fopen(out_name,"wb");
	if (!of) {
		printf("can't open %s: %s\n",out_name,strerror(errno));
		return EXIT_FAILURE;
	}
	
	for (size_t i = 0; i < text_string_cnt; i++) {
		const text_string_t * ts = &text_string_tbl[i];
		unsigned offset = get_prg_offset(ts->bank, ts->addr);
		
		// header
		fprintf(of,
			"@%u ",
			ts->bank);
		for (unsigned i = 0; i < ts->ptr_cnt; i++) {
			fprintf(of,
				"%04X/%04X ",
				ts->lo[i], ts->hi[i]);
		}
		if (ts->detective) {
			fprintf(of, "detective ");
		}
		fputc('\n', of);
		
		// original text in a comment
		fputc('#', of);
		fputc(' ', of);
		unsigned i = offset;
		while (1) {
			unsigned c = prg[i++];
			if (c == 0xff) {
				// command
				unsigned cc = prg[i++];
				fputc('\\', of);
				fputc(cc + '0', of);
				if (!cc) {
					// end of string
					break;
				}
				if (cc == 1 || cc == 2) {
					// new line
					fputc('\n', of);
					fputc('#', of);
					fputc(' ', of);
				}
			} else if (!ts->detective && c == 0xfe) {
				// face change
				unsigned cc = prg[i++];
				fprintf(of, "\\f%02X", cc);
			} else if (ts->detective) {
				// detective char
				unsigned cc = prg[i++];
				fputwc2utf8(get_detective_char(c, cc), of);
			} else {
				// regular char
				fputwc2utf8(jp_charset[c], of);
			}
		}
		
		// placeholder text
		fprintf(of,
			"\n\\2No TL %u %04X\\2\\0\n\n\n", ts->bank, ts->addr);
	}
	
	fclose(of);
}
