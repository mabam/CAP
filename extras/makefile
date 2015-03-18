# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 13:59:56 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O  -DSHORT_NAMES -DMELBOURNE
DESTDIR=/usr/local/cap
CAPLIB=-lcap
I=/usr/include
LFLAGS=
O=

PROGS=iwif

all:	${PROGS}

iwif:	iwif.o $(O)
	${CC} ${LFLAGS} -o iwif iwif.o

des.o:	des.c
	${CC} ${CFLAGS} -c des.c

install: ${PROGS}
	-strip ${PROGS}
	${INSTALLER} ${PROGS} ${DESTDIR}

clean:
	-rm -f ${PROGS} *.o core make.log err *~

cleanexe:
	-rm -f ${PROGS}

dist:
	@cat todist
