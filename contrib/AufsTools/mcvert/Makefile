CSOURCES = mcvert.c hqxify.c unpack.c
SOURCES = mcvert.c hqxify.c unpack.c mactypes.h Makefile
OBJECTS = mcvert.o hqxify.o unpack.o
BIN = .
# for Solaris 2.X
# CFLAGS = -O
#
CFLAGS = -O -DBSD
DESTDIR = /usr/local/cap

mcvert: $(OBJECTS)
	cc $(CFLAGS) $(OBJECTS) -s -o $(BIN)/mcvert

$(OBJECTS): mactypes.h

print: 
	lpr -p -Pvmslp $(SOURCES)

shar:
	shar $(SOURCES) mcvert.1 > mcvert.shar

install: mcvert
	-strip mcvert
	cp mcvert $(DESTDIR)

clean:
	-rm -f mcvert $(OBJECTS)
