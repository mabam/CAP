CFLAGS=cflags() specialcflags()
DESTDIR=capdestdir()
CAPLIB=libcap()
I=includedir()
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
	ifdef([sysvinstall],[install -f $(DESTDIR) ${PROGS}],
		[${INSTALLER} ${PROGS} ${DESTDIR}])

clean:
	-rm -f ${PROGS} *.o core make.log err *~

spotless:
	-rm -f ${PROGS} *.o *.orig core make.log err *~ Makefile makefile

cleanexe:
	-rm -f ${PROGS}

dist:
	@cat todist
