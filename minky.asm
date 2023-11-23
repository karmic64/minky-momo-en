
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; global

NES_HEADER_SIZE = $10

PRG_SIZE = $20000
CHR_SIZE = $20000

PRG_BANK_SIZE = $4000
CHR_BANK_SIZE = $1000

PRG_BANKS = PRG_SIZE / PRG_BANK_SIZE
CHR_BANKS = CHR_SIZE / CHR_BANK_SIZE

FIXED_PRG_BANK = PRG_BANKS - 1

CHR_TILE_SIZE = $10


prg_offs	.function bank, addr
	.endf NES_HEADER_SIZE + (bank * PRG_BANK_SIZE) + (addr % PRG_BANK_SIZE)

chr_offs	.function bank, addr
	.endf NES_HEADER_SIZE + PRG_SIZE + (bank * CHR_BANK_SIZE) + (addr % CHR_BANK_SIZE)

tile_offs	.function bank, tile
	.endf NES_HEADER_SIZE + PRG_SIZE + (bank * CHR_BANK_SIZE) + (tile * CHR_TILE_SIZE)

.binary "Mahou no Princess Minky Momo - Remember Dream (J) [!].nes"
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; zp vars
	
	.virtual $bd
	.dsection zp_vars
	.cerror * > $fc, "too long" ;factory/ghost minigames inexplicably use $fc
	.endv



	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; data include
	
	.include "gen/minky-data.asm"
	
	
	
	
	
	
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; main font chr
	
FONT_EN = binary("data/gfx.chr",0,EN_CHARSET_SIZE*8)
font_en_chr	.function char
	.endf FONT_EN[char*8:(char+1)*8]
	
	* = tile_offs($0f,$00)
	;.fill $100*CHR_TILE_SIZE, 0
	
	* = tile_offs($0f,0)
	.for i = 0, i < $100, i=i+1
		.text font_en_chr(i) x 2
	.next
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; detective font chr
	
	; table of tile indexes which are free for font chars
DETECTIVE_CHARSET_TBL = range($01,$40) .. range($47,$50) .. range($57,$60) .. range($87,$90) .. range($97,$a0) .. range($a7,$b0) .. range($b7,$c0) .. range($c7,$d0) .. range($d7,$e0) .. range($e7,$f0) .. range($f7,$100) .. range($c0,$100,$10)
	
	; overwrite chr with font tiles
	.for i = 0, i < len(DETECTIVE_CHARSET_TBL), i=i+1
		* = tile_offs($0a, DETECTIVE_CHARSET_TBL[i])
		.if i >= EN_CHARSET_SIZE
			;.fill CHR_TILE_SIZE,$ff
		.else
ch := font_en_chr(i)
			.for j = 0, j < 8, j=j+1
				.byte ch[j] ^ $ff
			.next
			.fill $08,$ff
		.endif
	.next
	
	; table for use by detective text routine
	.section section_7_8
detective_char_tbl	.text DETECTIVE_CHARSET_TBL[:EN_CHARSET_SIZE]
	.send
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; text routine / huffman decompression
	
	.cerror size(huffman_table) > $100, "huffman table is longer than a page"
	.cerror huffman_table & 1, "huffman table is not word-aligned"
	.section zp_vars
huffman_in_mask	.byte ?
huffman_in_mask_save	.byte ?
	.send
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; custom code
	
	.section section_7_8
	
	;;;;;;;;;;;;;;;;
	
get_text_raw_byte
	lda ($8e),y
step_text_pointer
	inc $8e
	inc $02ca
	bne +
	inc $8f
	inc $02cb
+	rts
	
get_text_byte
	ldy #0
	lda $8f
	bpl get_text_raw_byte
	ldx #0
	
get_huffman_bit
	lda huffman_in_mask
	bne +
	jsr step_text_pointer
	jsr init_huffman_mask
+	asl huffman_in_mask
	and ($8e),y
	cmp #1
	
	php
	lda huffman_table,x
	sta 0
	and #$3f
	rol
	sta 1
	txa
	and #$fe
	;clc
	adc 1
	tax
	plp
	
	lda 0
	bcc +
	asl
+	bpl get_huffman_bit
	
	lda huffman_table,x
	rts
	
	
	;;;;;;;;;;
	
init_text_hook
	sta $02db
	jmp init_huffman_mask
	
save_text_hook
	lda huffman_in_mask
	sta huffman_in_mask_save
init_huffman_mask
	lda #1
	sta huffman_in_mask
	rts
	
restore_text_hook
	sta $02ce
	lda huffman_in_mask_save
	sta huffman_in_mask
	rts
	
	.send
	
	
	* = prg_offs(FIXED_PRG_BANK,$d09d)
	jmp init_text_hook
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; standard text routine
	
	* = prg_offs(FIXED_PRG_BANK,$d105)
	.logical $d105
text_byte_hook
	jsr get_text_byte
	cmp $02be
	bne _not_face
	jsr get_text_byte
	sta $02db
	jmp $d0ed
	
_not_face
	cmp $02bd
	beq +
	jmp $d1ad
+	jsr get_text_byte
	jmp $d12a
	
	.cerror * > $d12a, "too long"
	.here
	
	
	* = prg_offs(FIXED_PRG_BANK,$d166)
	jmp save_text_hook
	
	* = prg_offs(FIXED_PRG_BANK,$d1ad)
	.logical $d1ad
	sta $10
	lda #0
	sta $0f
	jmp $d1b8
	.here
	
	* = prg_offs(FIXED_PRG_BANK,$d1f9)
	jmp restore_text_hook
	
	
	;; speed up prologue text
	* = prg_offs(0,$9987)
	lda #3
	
	;; speed up intro text
	* = prg_offs(0,$ad28)
	lda #3
	
	;; speed up main text 1
	* = prg_offs(FIXED_PRG_BANK,$e5c3)
	lda #3
	
	;; speed up main text 2
	;; this is required, otherwise the ending text will take too long
	* = prg_offs(FIXED_PRG_BANK,$e5fb)
	lda #3
	
	;; speed up after-game-over text
	* = prg_offs(FIXED_PRG_BANK,$e9d0)
	lda #3
	
	;; speed up post-bagu-fake-mama/papa scene
	* = prg_offs(4,$80bf)
	lda #3
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; detective text routine
	
	* = prg_offs(FIXED_PRG_BANK,$d3f5)
	.logical $d3f5
detective_text_byte_hook
	jsr get_text_byte
	cmp $02bd
	beq +
	tax
	lda detective_char_tbl,x
	jmp $d488
+	jsr get_text_byte
	;jmp $d407
	.fill $d407-*,$ea
	
	.cerror * > $d407, "too long"
	.here
	
	
	* = prg_offs(FIXED_PRG_BANK,$d443)
	jmp save_text_hook
	
	* = prg_offs(FIXED_PRG_BANK,$d488)
	sta $0f ;write only one char to vram, not 2
	inc $02c7
	ldx $02bb
	clc
	lda $02cc
	adc $d4b4,x
	sta $0c
	lda $02cd
	adc $d4b8,x
	sta $0d
	lda #$81
	sta $0e
	lda #$ff
	sta $10
	jmp $d4ad
	
	* = prg_offs(FIXED_PRG_BANK,$d4cf)
	jmp restore_text_hook
	
	; make detective text every row, not every 2
	* = prg_offs(FIXED_PRG_BANK,$d61d)
	jmp $d62e
	
	
	; speed up the detective subgame's text
	; it takes too long to display and many ingame seconds are lost
	* = prg_offs(1,$9177)
	lda #3
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; title screen
	
	* = tile_offs(6,$e0)
	;"magical" chr
	.binary "data/gfx.chr",$d00,CHR_TILE_SIZE*6
	.binary "data/gfx.chr",$e00,CHR_TILE_SIZE*6
	;"princess" chr
	.binary "data/gfx.chr",$f00,CHR_TILE_SIZE*8
	.binary "data/gfx.chr",$1000,CHR_TILE_SIZE*8
	
	;"minky" chr
	* = tile_offs(6,$07)
	.binary "data/gfx.chr",$1100,CHR_TILE_SIZE*9
	.binary "data/gfx.chr",$1200,CHR_TILE_SIZE*9
	.binary "data/gfx.chr",$1300,CHR_TILE_SIZE*9
	;"momo" chr
	.binary "data/gfx.chr",$1400,CHR_TILE_SIZE*9
	* = tile_offs(6,$2c)
	.binary "data/gfx.chr",$1500,CHR_TILE_SIZE*9
	* = tile_offs(6,$37)
	.binary "data/gfx.chr",$1600,CHR_TILE_SIZE*9
	
	;"remember dream" chr
	* = tile_offs(6,$67)
	.binary "data/gfx.chr",$1290,CHR_TILE_SIZE*7
	* = tile_offs(6,$78)
	.binary "data/gfx.chr",$1390,CHR_TILE_SIZE*7
	* = tile_offs(6,$ff)
	.binary "data/gfx.chr",$1490,CHR_TILE_SIZE
	
	
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; first menu
	
	; move the heart selector sprite left
	* = prg_offs(0, $adfd)
	ldx #$54
	; and up a pixel
	* = prg_offs(0, $adf4)
	ldy #$57
	* = prg_offs(0, $adfb)
	ldy #$6f
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; passwords
	
	; write the new hyphen character on password entry screen
	* = prg_offs(0,$af1a)
	lda #'-'
	
	; print hyphen between password when given, like the password entry screen
	; this is an improvement compared to the original game
	* = prg_offs(0,$b055)
	lda #$ff
	sta $0502
	lda #$00
	jsr password_print_hyphen_hook
	.section section_0_0
password_print_hyphen_hook
	sta $0503
	.for i = 3, i >= 0, i=i-1
		lda $04fd+i+0
		sta $04fd+i+1
	.next
	lda #'-'
	sta $04fd
	rts
	.send
	
	; new password character table (password entry screen)
	* = prg_offs(0,$af2d)
	.text "01234BDGHMNQRTW♡"
	.text "56789bdghmnqrtw?"
	
	; new password character table (given password on game quit)
	* = prg_offs(0,$b067)
	.text "01234BDGHMNQRTW♡"
	.text "56789bdghmnqrtw?"
	
	
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; overworld 1
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; chr replacement
	
	;; fruits
	* = tile_offs($15,$e5)
	.binary "data/gfx.chr",$850,CHR_TILE_SIZE*4
	* = tile_offs($15,$f1)
	.binary "data/gfx.chr",$890,CHR_TILE_SIZE
	.binary "data/gfx.chr",$990,CHR_TILE_SIZE
	.binary "data/gfx.chr",$910,CHR_TILE_SIZE
	.binary "data/gfx.chr",$a10,CHR_TILE_SIZE
	.binary "data/gfx.chr",$950,CHR_TILE_SIZE*4
	
	;; records
	* = tile_offs($15,$86)
	.binary "data/gfx.chr",$8c0,CHR_TILE_SIZE*3
	* = tile_offs($15,$96)
	.binary "data/gfx.chr",$9c0,CHR_TILE_SIZE*3
	* = tile_offs($15,$a8)
	.binary "data/gfx.chr",$8f0,CHR_TILE_SIZE
	* = tile_offs($15,$b8)
	.binary "data/gfx.chr",$9f0,CHR_TILE_SIZE
	
	;; arcade
	* = tile_offs($15,$8e)
	.binary "data/gfx.chr",$a50,CHR_TILE_SIZE*2
	* = tile_offs($15,$9e)
	.binary "data/gfx.chr",$b50,CHR_TILE_SIZE*2
	* = tile_offs($15,$ae)
	.binary "data/gfx.chr",$a70,CHR_TILE_SIZE*2
	* = tile_offs($15,$be)
	.binary "data/gfx.chr",$b70,CHR_TILE_SIZE*2
	* = tile_offs($15,$ce)
	.binary "data/gfx.chr",$a90,CHR_TILE_SIZE*1
	* = tile_offs($15,$de)
	.binary "data/gfx.chr",$b90,CHR_TILE_SIZE*1
	
	;; burger
	* = tile_offs($15,$eb)
	.binary "data/gfx.chr",$aa0,CHR_TILE_SIZE*5
	* = tile_offs($15,$fb)
	.binary "data/gfx.chr",$ba0,CHR_TILE_SIZE*5
	
	;; elec
	* = tile_offs($15,$4e)
	.binary "data/gfx.chr",$b00,CHR_TILE_SIZE*2
	* = tile_offs($15,$5e)
	.binary "data/gfx.chr",$c00,CHR_TILE_SIZE*2
	* = tile_offs($15,$6e)
	.binary "data/gfx.chr",$b20,CHR_TILE_SIZE
	* = tile_offs($15,$7e)
	.binary "data/gfx.chr",$c20,CHR_TILE_SIZE
	
	;; hardware
	* = tile_offs($15,$0a)
	.binary "data/gfx.chr",$10c0,CHR_TILE_SIZE*4
	* = tile_offs($15,$1a)
	.binary "data/gfx.chr",$11c0,CHR_TILE_SIZE*2
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;; map fixes
	
MAP_HEIGHT_1 = $0b
METATILE_SIZE_1 = 4
	
METATILES_BASE_1 = $9e9c
METATILES_ATTRIB_1 = $a0bc
METATILES_COLLISION_1 = $a14c
	
	
	;;;;;;;;;;;;; elec
	* = prg_offs(4,METATILES_BASE_1 + $7d*METATILE_SIZE_1)
	.byte $4e,$4f,$5e,$5f
	.byte $4e,$6e,$5e,$7e


	;;;;;;;;;;;;; arcade
	* = prg_offs(4,$a57d + 3*MAP_HEIGHT_1 + 6)
	.byte $80
	.fill MAP_HEIGHT_1 - 1
	.byte $81
	.fill MAP_HEIGHT_1 - 1
	.byte $82
	.fill MAP_HEIGHT_1 - 1
	.byte $2c  ;; this frees metatile $83
	
	* = prg_offs(4,METATILES_BASE_1 + $80*METATILE_SIZE_1)
	.byte $8e,$8f,$9e,$9f
	.byte $ae,$8e,$be,$9e
	.byte $af,$ce,$bf,$de
	
	
	;;;;;;;;;;;;; records
	* = prg_offs(4,METATILES_BASE_1 + $84*METATILE_SIZE_1)
	.byte $86,$87,$96,$97
	.byte $a2,$88,$b2,$98
	.byte $86,$a3,$96,$b3
	.byte $a8,$2b,$b8,$2b
	
	
	
	
	;;;;;;;;;;;;;; fruits
	* = prg_offs(4,$a36c + 2*MAP_HEIGHT_1 + 6)
	.byte $4e
	.fill MAP_HEIGHT_1 - 1
	.byte $4f
	.fill MAP_HEIGHT_1 - 1
	.byte $83  ;; previously freed metatile
	.fill MAP_HEIGHT_1 - 1
	
	* = prg_offs(4,METATILES_BASE_1 + $83*METATILE_SIZE_1)
	.byte $f1,$f3,$f2,$f4
	
	* = prg_offs(4,METATILES_ATTRIB_1 + $83)
	.byte 1
	
	
	;;;;;;;;;;;;;; hardware
	* = prg_offs(4,METATILES_BASE_1 + $41*METATILE_SIZE_1)
	.byte $0a,$0b,$2b,$1a
	.byte $0c,$0d,$0b,$0c
	* = prg_offs(4,METATILES_BASE_1 + $4a*METATILE_SIZE_1)
	.byte $2b,$2b,$1b,$2b
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; game center
	
	* = prg_offs(5,$ad54) ;print text on one line
	.logical $ad54
	cpy #$18
	bne $ad4e
	lda #$ea
	sta $0c
	lda #$22
	sta $0d
	lda #$18
	sta $0e
	lda #$ff
	sta $0c + 2 + 1 + $18
	jmp $ad7c
	
	.here
	
	
	; speed up exiting text
	* = prg_offs(FIXED_PRG_BANK,$ebd4)
	lda #3
	
	
	;translated names (24 chars each but only use 21)
	* = prg_offs(5,$ae1d)
	; ミンキー・チェイス
	.text "Minky Chase             "
	; バーガー・パニック
	.text "Burger Panic            "
	; 工場はつらいよ１９９２
	.text "Factory's Tough 1992    "
	; ドクター？？モモ
	.text "Doctor ?? Momo          "
	; シャーロック・モモ
	.text "Sherlock Momo           "
	; うらないのへや
	.text "Fortune-telling Room    "
	; ごぉすと・でばっがぁ
	; (this one is not actually viewable ingame)
	.text "Ghost Debugger          "
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; record store
	
	* = prg_offs(5,$87f3) ;print text on one line
	.logical $87f3
	cpy #$18
	bne $87ed
	lda #$ea
	sta $0c
	lda #$22
	sta $0d
	lda #$18
	sta $0e
	lda #$ff
	sta $0c + 2 + 1 + $18
	jmp $881b
	
	.here
	
	
	;translated names (24 chars each but only use 21)
	* = prg_offs(5,$8938)
	; まよなかのほうもんしゃ
	.text "Midnight Visitor        "
	; あんずばやしでこんにちは
	.text "Hello Apricot Grove     "
	; チェイス・ハリーアップ！
	.text "Chase Hurry Up!         "
	; メッセンジャー
	.text "Messenger               "
	; ゲーム・オーバー
	.text "Game Over               "
	; ファンファーレ　A
	.text "Fanfare A               "
	; ハードボイルド・ダディ
	.text "Hardboiled Daddy        "
	; こねこの　かくれんぼ
	.text "Kitty's Hide'n'seek     "
	; モーニング・ライト
	.text "Morning Light           "
	; ラン・ラン・ラン　！
	.text "Run Run Run!            "
	; ドリーミー・ナイト
	.text "Dreamy Night            "
	; チェンジ　チェンジ　！
	.text "Change Change!          "
	; ファンファーレ　B
	.text "Fanfare B               "
	; マジカル　タウン
	.text "Magical Town            "
	; シー・ユー・アゲイン♡
	.text "See You Again♡          "
	; ミンキー・パーティー
	.text "Minky Party             "
	
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; fortune-telling
	
	;; speed up text
	* = prg_offs(2,$abd6)
	lda #3
	
	;; change entry number tiles to the proper ones
	* = prg_offs(2,$afcb)
	adc #+'0' - $10
	* = prg_offs(2,$afdf)
	adc #+'0' - $10
	* = prg_offs(2,$aff3)
	adc #+'0' - $10
	
	;; change birthday entry text
	* = prg_offs(2,$b081)
	.byte $0e ;length
	.text "    "
	.text " / "
	.text "  "
	.text " / "
	.text "  "
	.byte $ff,$ff,$ff,$ff,$ff ;don't print the tenten mark
	
	;; change today's date entry text
	* = prg_offs(2,$b099)
	.byte $0e ;length
	.text "    "
	.text " / "
	.text "  "
	.text " / "
	.text "  "
	.byte $ff,$ff,$ff,$ff,$ff ;don't print the tenten mark
	
	;; don't do any onscreen number tile -> string char conversion
	* = prg_offs(2,$b03a)
	adc #0
	
	;; change date print string
	* = prg_offs(2,$b0af)
	.text $ff,$ff,$ff,$ff
	.text " / "
	.text $ff,$ff
	.text " / "
	.text $ff,$ff
	.text $ff,$00 ;string terminator
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; car chase game
	
	; ガン sprite chr
	; this one is used when you hit a car from behind
	* = tile_offs($12,$1e)
	.binary "data/gfx.chr",$0900,CHR_TILE_SIZE
	.binary "data/gfx.chr",$0a00,CHR_TILE_SIZE
	
	; ゴン sprite chr
	; this one is used when a car hits you from behind
	* = tile_offs($12,$64)
	.binary "data/gfx.chr",$0900,CHR_TILE_SIZE
	.binary "data/gfx.chr",$0a00,CHR_TILE_SIZE
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; burger shop game
	
	; スカ tile chr
	* = tile_offs($0b,$3d)
	.binary "data/gfx.chr",$0820,CHR_TILE_SIZE*3
	* = tile_offs($0b,$4d)
	.binary "data/gfx.chr",$0920,CHR_TILE_SIZE*3
	* = tile_offs($0b,$5d)
	.binary "data/gfx.chr",$0a20,CHR_TILE_SIZE*3
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; detective game
	
detective_write_string_chr	.macro
		.for i = 0, i < len(\1), i=i+1
ch := font_en_chr((\1)[i])
			.rept 2
				.for j = 0, j < 8, j=j+1
					.byte ch[j] ^ $ff
				.next
			.next
		.next
	.endm
	
	; "before" chr
	* = tile_offs($0a,$63)
	.binary "data/gfx.chr",$0c30,CHR_TILE_SIZE*5
	; "after" chr
	* = tile_offs($0a,$73)
	#detective_write_string_chr "After"
	; "crime" chr
	* = tile_offs($0a,$fb)
	#detective_write_string_chr "Crime"
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; game over
	
	; text chr
	* = tile_offs($1d,$d8)
	.text font_en_chr('T') x 2
	.text font_en_chr('r') x 2
	.text font_en_chr('y') x 2
	.text font_en_chr('a') x 2
	.text font_en_chr('g') x 2
	.text font_en_chr('i') x 2
	.text font_en_chr('n') x 2
	.text font_en_chr('?') x 2
	
	
	
	
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;; ending
	
	;; just one more char per row (in bagu/dream pearls scene)
	* = prg_offs(FIXED_PRG_BANK,$f343)
	ldx #$18
	;; speed up text in bagu/dream pearls scene
	* = prg_offs(FIXED_PRG_BANK,$f353)
	lda #$03
	
	;; move credits left 1 tile
	* = prg_offs(FIXED_PRG_BANK,$f5a4)
	ldx #$05
	
	;; osimai charset
	* = tile_offs($1e,$e0)
	.binary "data/gfx.chr",$c80,8*CHR_TILE_SIZE
	.binary "data/gfx.chr",$e80,8*CHR_TILE_SIZE
	.binary "data/gfx.chr",$d80,8*CHR_TILE_SIZE
	.binary "data/gfx.chr",$f80,8*CHR_TILE_SIZE
	
	
	
	
	
	
	
	