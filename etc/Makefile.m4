CFLAGS=cflags() nbpflags() specialcflags()
SDESTDIR=capsrvrdestdir()
UDESTDIR=capdestdir()
ETCDIR=etcdest()
CAPLIB=libcap()
I=includedir()
# for other libraries (like BSD on hpux)
SLIB=libspecial()

ifdef([useatis],[],[# ])ATISPROGS=atis
PROGS=${ATISPROGS} 

# aufs.c definitions: USEVPRINTF - use vprintf in logging
ifdef([usevprintf],[],[#])ATISDEFS=-DUSEVPRINTF

# Make sure to define needgetopt if your system doesnt have it or
# just set GETOPT=att_getopt.o (or to a getopt of your own liking)
GETOPT=ifdef([needgetopt],[att_getopt.o])

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
	-mkdir ${SDESTDIR} ${UDESTDIR}
	ifdef([sysvinstall],[install -f ${SDESTDIR} ${PROGS}],
		[${INSTALLER} ${PROGS} ${SDESTDIR}])

clean:
	-rm -f atis *.o core att_getopt.c *~

spotless:
	-rm -f atis *.o *.orig core att_getopt.c *~ Makefile makefile

dist:
	@cat todist

