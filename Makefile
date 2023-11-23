ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif


CFLAGS:=-s -Ofast -Wall
CLIBS:=-lpng


TOOLS:=dump-font make-tbl make-detective-charset-h text-hunter detective-text-hunter dump-text make-minky-data
TOOLNAMES:=$(addprefix tool/,$(TOOLS))
DEPFILES:=$(addsuffix .d,$(TOOLNAMES))
EXEFILES:=$(addsuffix .exe,$(TOOLNAMES))

%.d: %.c
	$(CC) -M -MF $@ -MG -MP -MT $*$(DOTEXE) $<

%$(DOTEXE): %.c
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)


.PHONY: default clean tools
default: MinkyMomoEn.nes
#	@echo 'no default rule'

clean:
	$(RM) $(DEPFILES)
	$(RM) $(TOOLNAMES)
	$(RM) $(EXEFILES)

tools: $(EXEFILES)

include $(DEPFILES)



gen/font.png: tool/dump-font$(DOTEXE)
	$^ $@

gen/text.tbl: tool/make-tbl$(DOTEXE)
	$^ $@

gen/detective-charset.h: tool/make-detective-charset-h$(DOTEXE) detective-charset-defs.txt
	$^ $@

gen/text-defs-auto.txt: tool/text-hunter$(DOTEXE)
	$^ $@

gen/detective-text-defs-auto.txt: tool/detective-text-hunter$(DOTEXE)
	$^ $@

gen/text-auto.txt: tool/dump-text$(DOTEXE) text-defs.txt
	$^ $@

gen/minky-data.asm: tool/make-minky-data$(DOTEXE) data/free-space-defs.txt data/text.txt data/nametables.txt
	$^ $@

MinkyMomoEn.nes: minky.asm gen/minky-data.asm data/gfx.chr
	64tass -a -B -f -Wno-wrap-pc -o $@ $<

