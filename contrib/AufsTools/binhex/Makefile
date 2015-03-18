# for Solaris 2.X
# CFLAGS= -O
#
CFLAGS=	-O -Dstrrchr=rindex
LFLAGS=
CFILES=	binhex.c gethead.c crc.c run.c 8to6.c
OFILES=	binhex.o gethead.o crc.o run.o 8to6.o
DESTDIR= /usr/local/cap

all: binhex unxbin

unxbin : binhex
	-rm -f unxbin
	ln binhex unxbin

binhex:	$(OFILES)
	cc -o binhex $(OFILES)

binhex.o:	Makefile
gethead.o:	Makefile

debug:
	cc -o binhexd -g $(CFILES)

lint:
	lint $(LFLAGS) $(CFILES)

install: binhex
	-strip binhex
	cp binhex $(DESTDIR)
	-rm -f $(DESTDIR)/unxbin
	(cd $(DESTDIR); ln binhex unxbin)

clean:
	-rm -f binhex unxbin $(OFILES)
