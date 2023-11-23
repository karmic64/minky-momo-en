// ROM-related defines.

#pragma once


#define ROM_NAME "Mahou no Princess Minky Momo - Remember Dream (J) [!].nes"


#define NES_HEADER_SIZE 0x10

#define PRG_SIZE 0x20000
#define CHR_SIZE 0x20000

#define PRG_BANK_SIZE 0x4000
#define CHR_BANK_SIZE 0x1000

#define PRG_BANKS (PRG_SIZE / PRG_BANK_SIZE)
#define CHR_BANKS (CHR_SIZE / CHR_BANK_SIZE)

#define FIXED_PRG_BANK (PRG_BANKS - 1)

#define CHR_TILE_SIZE 0x10



#define LDA_OPCODE 0xa9
#define LDX_OPCODE 0xa2
#define LDY_OPCODE 0xa0

#define JSR_OPCODE 0x20
#define JMP_OPCODE 0x4c



unsigned get_prg_offset(unsigned bank, unsigned addr) {
	if (addr >= 0xc000) bank = FIXED_PRG_BANK;
	return (PRG_BANK_SIZE * bank) + (addr % PRG_BANK_SIZE);
}

unsigned get_chr_offset(unsigned bank, unsigned addr) {
	return (CHR_BANK_SIZE * bank) + (addr % CHR_BANK_SIZE);
}

unsigned get_chr_tile_offset(unsigned bank, unsigned tile) {
	return (CHR_BANK_SIZE * bank) + (tile * CHR_TILE_SIZE);
}


unsigned get_rom_prg_offset(unsigned bank, unsigned addr) {
	return NES_HEADER_SIZE + get_prg_offset(bank, addr);
}

unsigned get_rom_chr_offset(unsigned bank, unsigned addr) {
	return NES_HEADER_SIZE + PRG_SIZE + get_chr_offset(bank, addr);
}

unsigned get_rom_chr_tile_offset(unsigned bank, unsigned tile) {
	return NES_HEADER_SIZE + PRG_SIZE + get_chr_tile_offset(bank, tile);
}



unsigned get_prg_offset_bank(unsigned offs) {
	return offs / PRG_BANK_SIZE;
}

unsigned get_prg_offset_addr(unsigned offs) {
	unsigned addr = offs % PRG_BANK_SIZE;
	if (offs >= FIXED_PRG_BANK * PRG_BANK_SIZE)
		return addr | 0xc000;
	return addr | 0x8000;
}
