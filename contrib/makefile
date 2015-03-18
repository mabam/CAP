# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 13:59:43 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O  -DSHORT_NAMES -DMELBOURNE
I=/usr/include
O=

# Make sure to define needgetopt if your system doesnt have it
GETOPT=

# for other libraries (like BSD on hpux)
SLIB=

CAPLIB=-lcap
DESTDIR=/usr/local/cap
RENAMEFLAG=-DLWRENAMEFILE=\"/usr/local/lib/cap/lwrename.list\"

PROGS=snitch cvt2apple cvt2cap lwrename printqueue

all:	${PROGS}

snitch: snitch.o ${O} ${GETOPT}
	${CC} ${LFLAGS} -o snitch snitch.o ${GETOPT} ${O} ${CAPLIB} ${SLIB}

cvt2apple: cvt2apple.o ${O}
	${CC} ${LFLAGS} -o cvt2apple cvt2apple.o ${O} ${SLIB}

cvt2cap: cvt2cap.o ${O}
	${CC} ${LFLAGS} -o cvt2cap cvt2cap.o ${O} ${SLIB}

lwrename:	lwrename.o
	${CC} ${LFLAGS} -o lwrename lwrename.o ${O} ${CAPLIB} ${SLIB}

lwrename.o:	lwrename.c
	${CC} ${CFLAGS} ${RENAMEFLAG} -c lwrename.c

printqueue:	printqueue.o
	${CC} ${LFLAGS} -o printqueue printqueue.o ${O} ${CAPLIB} ${SLIB}

att_getopt.c:
	ln -s ../extras/att_getopt.c

install: ${PROGS}
	-strip ${PROGS}
	${INSTALLER} ${PROGS} ${DESTDIR}

clean:
	-rm -f ${PROGS} *.o core make.log err att_getopt.c *~
	-(cd AsyncATalk; make clean)
	-(cd AufsTools; make clean)
	-(cd MacPS; make clean)
	-(cd Messages; make clean)
	-(cd Timelord; make clean)

cleanexe:
	-rm -f ${PROGS}

dist:
	@cat todist
