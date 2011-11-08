INCLUDES = -I./fx2lib/include
LIBS = fx2.lib -L./fx2lib/lib
CC = sdcc -mmcs51 --code-size 0x3c00 --xram-size 0x0200 --xram-loc 0x3c00 \
     -Wl"-b DSCR_AREA = 0x3e00" \
     -Wl"-b INT2JT = 0x3f00"

BASENAME=bulkloop
SOURCES = bulkloop.c
A51_SOURCES = dscr.a51
RELS=$(SOURCES:.c=.rel) $(A51_SOURCES:.a51=.rel)

.PHONY : all clean 

all : $(BASENAME).ihx

%.rel : %.a51
	asx8051 -logs $<

%.rel : %.c
	$(CC) -c $(INCLUDES) $<

$(BASENAME).ihx : $(RELS)
	$(CC) -o $@ $(INCLUDES) $(RELS) $(LIBS)

%.bix : %.ihx
	objcopy -I ihex -O binary $< $@

clean :
	rm -f *.ihx *.lnk *.lst *.map *.mem *.rel *.rst *.sym *.adb *.cdb *.asm *.bix

program : bulkloop.ihx
	sudo cycfx2prog reset prg:bulkloop.ihx run

install : $(BASENAME).ihx
	mkdir -p /usr/share/timetag
	cp $(BASENAME).ihx /lib/firmware/timetag-fx2.ihx
	cp timetag.rules /etc/udev/rules.d/timetag.rules

