# Magical Princess Minky Momo

This is the source code for my English translation patch of "Mahou no Princess Minky Momo - Remember Dream".

## Build requirements

* Original ROM in the root folder, named `Mahou no Princess Minky Momo - Remember Dream (J) [!].nes` (CRC32 `409CB096`)
* `make`
* C compiler, preferably `gcc`
* libpng (sort of)
* [64tass](https://tass64.sourceforge.net/)

Once you have this all set up, just go to the root folder and enter `make` to build.

## Project structure

This list details only useful files. Some tools/`make` recipes here are not actually used in the build process, and are just basic data extractors/hunters that were used during development. I chose to leave them here for the sake of completeness.

* `data/` - Any kind of (at least partially) manually-created data file.
	* `detective-charset-defs.txt` - Character set for the detective minigame. There's no need to change this.
	* `free-space-defs.txt` - List of free regions in the ROM. There's no need to change this.
	* `gfx.chr` - All the modified graphics are here. The first $800 bytes are for the 1BPP main font, and the rest of the graphics are 2BPP NES tile format and are haphazardly scattered around. You can edit it with [YY-CHR](https://w.atwiki.jp/yychr/) or similar tools. To add new characters to the font, you will also have to define them by editing the `en_charset` array in `tools/text.h`. To rearrange the rest, you'll have to mess with the main `minky.asm` file.
	* `nametables.txt` - Defines all the modified nametables. Lines starting with `@` define a new nametable, and there is no reason to change these. Other lines specify the row, column (`c` means "centered") and text strings (`\xx` indicates a constant tile definition).
	* `text.txt` - The real fun. All the text is here. Lines starting with `@` define a new text string, and there is no reason to change these. Other lines are pure text strings. `\x` is a control code and `\fxx` changes the displayed face icon. Refer to `notes.txt` for the functions of the control codes, and also mind the text line length limits.
	* `text-defs.txt` - Defines all the text string pointers. There's no need to change this.
* `gen/` - Any auto-generated files. You don't really need to concern yourself with them.
	* `text-auto.txt` - You can (and should) use this as a starting point for your `data/text.txt` file. All the strings are filled with placeholders.
* `tool/` - Tool source codes.
* `Mahou no Princess Minky Momo - Remember Dream (J) [!].nes` - The original ROM. Not included here.
* `Makefile` - The file `make` uses to build the project.
* `minky.asm` - Contains all the NES-side source code for the project.
* `MinkyMomoEn.nes` - When you build the project, this file will appear!
* `MinkyMomoEn.ips` - Keep in mind that this file is NOT automatically built and must be manually made when you're done.
* `notes.txt` - Haphazard notes on the game's internals. You might find the parts about the control codes/text line lengths useful.
