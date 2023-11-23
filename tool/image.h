// Image-related defines.

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>

#include <png.h>


typedef struct {
	png_uint_32 width;
	png_uint_32 height;
	png_uint_32 stride;
	png_uint_32 bit_depth;
	png_uint_32 color_type;
	
	png_uint_32 palette_size;
	png_color * palette;
	
	png_byte * data;
	png_byte ** rows;
} image_t;

void init_image(image_t * i) {
	if (i->palette_size) {
		i->palette = malloc(i->palette_size * sizeof(*i->palette));
	}
	
	i->data = malloc(i->height * i->stride);
	
	i->rows = malloc(i->height * sizeof(*i->rows));
	for (unsigned y = 0; y < i->height; y++) {
		i->rows[y] = i->data + (y * i->stride);
	}
}

void free_image(image_t * i) {
	if (i->palette_size) {
		free(i->palette);
	}
	free(i->data);
	free(i->rows);
}

int write_image_to_png(image_t * i, const char * png_name) {
	FILE * f = fopen(png_name, "wb");
	if (!f) {
		printf("can't open %s: %s\n", png_name,strerror(errno));
		return 1;
	}
	png_structp png_ptr = png_create_write_struct(
		PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(f);
		puts("can't create png struct");
		return 1;
	}
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(f);
		puts("can't create png info struct");
		return 1;
	}
	
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(f);
		return 1;
	}
	png_init_io(png_ptr,f);
	png_set_IHDR(png_ptr,info_ptr,i->width,i->height,
		i->bit_depth,i->color_type,PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
	if (i->palette_size) {
		png_set_PLTE(png_ptr,info_ptr,i->palette,i->palette_size);
	}
	png_set_rows(png_ptr,info_ptr,i->rows);
	png_write_png(png_ptr,info_ptr,PNG_TRANSFORM_IDENTITY,NULL);
	
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(f);
	return 0;
}
