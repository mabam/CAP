# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 13:59:59 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O   -DSHORT_NAMES -DMELBOURNE
DESTDIR=/usr/local/cap
ETCDIR=/etc
CAPLIB=-lcap
I=/usr/include
# for other libraries (like BSD on hpux)
SLIB=

PROGS=atis 

# aufs.c definitions: USEVPRINTF - use vprintf in logging
ATISDEFS=-DUSEVPRINTF

# Make sure to define needgetopt if your system doesnt have it or
# just set GETOPT=att_getopt.o (or to a getopt of your own liking)
GETOPT=

all:	${PROGS}

atis:	atis.o nisaux.o ${GETOPT}
	${CC} ${LFLAGS} -o atis atis.o nisaux.o ${GETOPT} ${CAPLIB} ${SLIB}

atis.o:	$I/netat/abnbp.h
	${CC} ${CFLAGS} ${ATISDEFS} -DETCDIR=\"${ETCDIR}\" -c atis.c

nisaux.o: $I/netat/abnbp.h

att_getopt.c:
	ln -s ../extras/att_getopt.c

install: ${PROGS}
	-strip ${PROGS}
	${INSTALLER} ${PROGS} ${DESTDIR}

clean:
	-rm -f atis *.o core att_getopt.c *~

dist:
	@cat todist

