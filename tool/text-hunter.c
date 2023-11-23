// Hunts for any calls to text routines and their text addresses.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "rom.h"
#include "text.h"


int main(int argc, char * argv[]) {
	const char * text_defs_out_name = argc < 2 ? NULL : argv[1];
	
	///////////////////////////////////////////////////////
	
	uint8_t prg[PRG_SIZE];
	FILE * f = fopen(ROM_NAME,"rb");
	if (!f) {
		printf("can't open rom: %s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	for (int i = 0; i < NES_HEADER_SIZE; i++)
		fgetc(f);
	fread(prg,1,PRG_SIZE,f);
	fclose(f);
	
	////////////////////////////////////////////////////////
	
	f = text_defs_out_name ? fopen(text_defs_out_name,"wb") : NULL;
	for (size_t i = 0; i < PRG_SIZE-2; i++) {
		unsigned op = prg[i];
		if (op == JSR_OPCODE || op == JMP_OPCODE) {
			unsigned lo = prg[i+1];
			unsigned hi = prg[i+2];
			
			if (lo == 0x77 && hi == 0xd0) {
				// start of subroutine
				printf("Text subroutine found at %X:%04X ",
					get_prg_offset_bank(i), get_prg_offset_addr(i));
				
				if (prg[i-4] == LDX_OPCODE && prg[i-2] == LDY_OPCODE) {
					printf("(lo %04X hi %04X -> %02X%02X)",
						get_prg_offset_addr(i-3), get_prg_offset_addr(i-1),
						prg[i-1], prg[i-3]
					);
					if (f)
						fprintf(f, "bank %X lo %04X hi %04X\n",
							get_prg_offset_bank(i),
							get_prg_offset_addr(i-3), get_prg_offset_addr(i-1)
						);
				} else {
					printf("(unclear)");
					if (f)
						fprintf(f, "# Unclear at %X:%04X\n",
							get_prg_offset_bank(i), get_prg_offset_addr(i));
				}
				
				putchar('\n');
			} else if (lo == 0x7e && hi == 0xd0) {
				// start of text
				printf("Text start found at %X:%04X ",
					get_prg_offset_bank(i), get_prg_offset_addr(i));
				
				if (prg[i-4] == LDX_OPCODE && prg[i-2] == LDY_OPCODE) {
					printf("(lo %04X hi %04X -> %02X%02X)",
						get_prg_offset_addr(i-3), get_prg_offset_addr(i-1),
						prg[i-1], prg[i-3]
					);
					if (f)
						fprintf(f, "bank %X lo %04X hi %04X\n",
							get_prg_offset_bank(i),
							get_prg_offset_addr(i-3), get_prg_offset_addr(i-1)
						);
				} else if (prg[i-6] == LDX_OPCODE && prg[i-4] == LDY_OPCODE) {
					printf("(lo %04X hi %04X -> %02X%02X)",
						get_prg_offset_addr(i-5), get_prg_offset_addr(i-3),
						prg[i-3], prg[i-5]
					);
					if (f)
						fprintf(f, "bank %X lo %04X hi %04X\n",
							get_prg_offset_bank(i),
							get_prg_offset_addr(i-5), get_prg_offset_addr(i-3)
						);
				} else {
					printf("(unclear)");
					if (f)
						fprintf(f, "# Unclear at %X:%04X\n",
							get_prg_offset_bank(i), get_prg_offset_addr(i));
				}
				
				putchar('\n');
			}
		}
	}
	if (f) fclose(f);
}