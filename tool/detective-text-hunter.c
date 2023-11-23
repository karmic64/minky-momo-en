// Hunts for detective subgame item names

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "rom.h"


uint8_t prg[PRG_SIZE];

int main(int argc, char * argv[]) {
	if (argc != 2) {
		puts("detective-text-hunter outname");
		return EXIT_FAILURE;
	}
	const char * out_name = argv[1];
	
	/////////////////////////////////////////
	
	FILE * f = fopen(ROM_NAME, "rb");
	if (!f) {
		printf("can't open %s: %s\n", ROM_NAME,strerror(errno));
		return EXIT_FAILURE;
	}
	for (int i = 0; i < NES_HEADER_SIZE; i++) {
		fgetc(f);
	}
	fread(prg, 1, PRG_SIZE, f);
	fclose(f);
	
	//////////////////////////////////////////
	
	f = fopen(out_name,"wb");
	if (!f) {
		printf("can't open %s: %s\n", out_name,strerror(errno));
		return EXIT_FAILURE;
	}
	for (unsigned room = 0; room < 0x10; room++) {
		unsigned room_ptr_offs = get_prg_offset(1, 0x9abc + (room*2));
		unsigned room_ptr = prg[room_ptr_offs+0] | (prg[room_ptr_offs+1] << 8);
		unsigned room_offs = get_prg_offset(1, room_ptr);
		unsigned objects_ptr = prg[room_offs+0] | (prg[room_offs+1] << 8);
		unsigned objects_offs = get_prg_offset(1, objects_ptr);
		
		unsigned object_list_size = prg[room_offs+2];
		int max_object = -1;
		for (unsigned i = 0; i < object_list_size*2; i++) {
			int oid = prg[room_offs + 3 + i];
			if (oid > max_object) {
				max_object = oid;
			}
		}
		
		for (int i = 0; i <= max_object; i++) {
			unsigned object_name_offs = objects_offs + (i * 0x0a) + 8;
			unsigned object_name_ptr = get_prg_offset_addr(object_name_offs);
			fprintf(f,
				"bank 1 lo %04X hi %04X detective\n",
				object_name_ptr+0, object_name_ptr+1);
		}
	}
	fclose(f);
}