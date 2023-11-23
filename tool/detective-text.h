// Detective-subgame-text-related defines.

#pragma once

#include <stdint.h>
#include <wchar.h>

typedef struct {
	uint8_t top;
	uint8_t bottom;
	wchar_t ch;
} detective_char_t;

#include "../gen/detective-charset.h"

wchar_t get_detective_char(unsigned top, unsigned bottom) {
	for (size_t i = 0; i < sizeof(jp_detective_charset)/sizeof(*jp_detective_charset); i++) {
		detective_char_t * c = &jp_detective_charset[i];
		if (c->top == top && c->bottom == bottom)
			return c->ch;
	}
	return L'\0';
}
