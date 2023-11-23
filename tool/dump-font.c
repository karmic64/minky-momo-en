// Dumps the game's font to a PNG file.
// Used to figure out which characters correspond to which code.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "rom.h"
#include "image.h"


#define IMAGE_WIDTH (0x10 * 8)
#define IMAGE_STRIDE (IMAGE_WIDTH / 8)
#define IMAGE_HEIGHT (0x10 * 8 * 2)


int main(int argc, char * argv[]) {
	if (argc != 2) {
		puts("usage: dump-font outfile");
		return EXIT_FAILURE;
	}
	const char * out_name = argv[1];
	FILE * f = NULL;
	
	////////////////////////////////////
	
	f = fopen(ROM_NAME,"rb");
	if (!f) {
		printf("can't open rom: %s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	
	image_t image;
	image.width = IMAGE_WIDTH;
	image.height = IMAGE_HEIGHT;
	image.stride = IMAGE_STRIDE;
	image.bit_depth = 1;
	image.color_type = PNG_COLOR_TYPE_GRAY;
	image.palette_size = 0;
	init_image(&image);
	
	for (unsigned ch = 0; ch < 0x100; ch++) {
		fseek(f, get_rom_prg_offset(FIXED_PRG_BANK,0xcda0+ch), SEEK_SET);
		int tile_top = fgetc(f);
		fseek(f, get_rom_prg_offset(FIXED_PRG_BANK,0xce9b+ch), SEEK_SET);
		int tile_bottom = fgetc(f);
		
		unsigned ix = ch & 0x0f;
		unsigned iy = (ch / 0x10) * 0x10;
		fseek(f, get_rom_chr_tile_offset(0x0f,tile_top), SEEK_SET);
		for (unsigned y = 0; y < 0x10; y++) {
			if (y == 8) {
				fseek(f, get_rom_chr_tile_offset(0x0f,tile_bottom), SEEK_SET);
			}
			
			image.rows[iy + y][ix] = fgetc(f);
		}
	}
	
	fclose(f);
	
	/////////////////////////////////////
	
	write_image_to_png(&image, out_name);
	free_image(&image);
	
}

