// Text-related defines.

#pragma once

#include <stdio.h>
#include <wchar.h>

#if L'鬱' != 0x9b31
#	error "wchars must be unicode codepoints"
#endif

const wchar_t jp_charset[] = 
	L"十字努高得点車気力人子供入家上下"
	L"左右見口交番無登一屋近退治作木今"
	L"何時行宝石店員工場長電器洋館中大"
	L"外本夢思現実金病物分土出あいうえ"
	L"おかきくけこさしすせそたちつてと"
	L"なにぬねのはひふへほまみむめもや"
	L"　ゆ　よらりるれろわをんっぁぃぅ" // the canonical space character is 0x60
	L"ぇぉゃゅょ『』！アイウエオカキク"
	L"ケコサシスセソタチツテトナニヌネ"
	L"ノハヒフヘホマミムメモヤ：ユ♡ヨ"
	L"ラリルレロワーンッァィゥェォャュ"
	L"ョ、。？０１２３４５６７８９┏┓"
	L"┗┛┃━・▾ＡＢがぎぐげござじず"
	L"ぜぞだぢづでどばびぶべぼぱぴぷぺ"
	L"ぽガギグゲゴザジズゼゾダヂヅデド"
	L"バビブベボパピプペポヴ"
	;

const wchar_t en_charset[] = 
	L" 0123456789"
	L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	L"abcdefghijklmnopqrstuvwxyz"
	L".,…!?'“”-:/¥~♡"
	L"┏┓┗┛┃━"
	;


unsigned get_en_char(wchar_t c) {
	size_t i = 0;
	while (en_charset[i]) {
		if (c == en_charset[i])
			return i;
		i++;
	}
	printf("can't encode char '%lc' (U+%04X)\n",c,c);
	return 0;
}



unsigned get_utf8_size(unsigned b) {
	if ((b & 0xf8) == 0xf0) {
		return 4;
	} else if ((b & 0xf0) == 0xe0) {
		return 3;
	} else if ((b & 0xe0) == 0xc0) {
		return 2;
	} else if (b < 0x80) {
		return 1;
	} else {
		return 0;
	}
}


wchar_t fgetwcfromutf8(FILE * f) {
	int b = fgetc(f);
	if (b == EOF)
		return WEOF;
	
	wchar_t v;
	int c;
	if ((b & 0xf8) == 0xf0) {
		v = b & 7;
		c = 3;
	} else if ((b & 0xf0) == 0xe0) {
		v = b & 0x0f;
		c = 2;
	} else if ((b & 0xe0) == 0xc0) {
		v = b & 0x1f;
		c = 1;
	} else if (b < 0x80) {
		return b;
	} else {
		return WEOF;
	}
	
	while (c--) {
		int nb = fgetc(f);
		if (nb == EOF || ((nb & 0xc0) != 0x80))
			return WEOF;
		v <<= 6;
		v |= nb & 0x3f;
	}
	
	return v;
}


int fputwc2utf8(wchar_t c, FILE * f) {
	if (c >= 0x800) {
		fputc((c>>12)|0xe0,f);
		fputc(((c>>6)&0x3f)|0x80,f);
		return fputc((c&0x3f)|0x80,f);
	} else if (c >= 0x80) {
		fputc((c>>6)|0xc0,f);
		return fputc((c&0x3f)|0x80,f);
	} else {
		return fputc(c,f);
	}
}
