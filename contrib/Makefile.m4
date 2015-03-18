CFLAGS=cflags() specialcflags()
I=includedir()
O=

# Make sure to define needgetopt if your system doesnt have it
GETOPT=ifdef([needgetopt],[att_getopt.o])

# for other libraries (like BSD on hpux)
SLIB=libspecial()

CAPLIB=libcap()
AFPLIB=libafp()
UDESTDIR=capdestdir()
SDESTDIR=capsrvrdestdir()
RENAMEFLAG=-DLWRENAMEFILE=lwrenamefile()

ifdef([useatis],[],[# ])ATISPROGS=snitch

SRVR=lwrename printqueue ${ATISPROGS} aufsmkusr aufsmkkey
USER=cvt2apple cvt2cap
PROGS= ${USER} ${SRVR} 

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
	${CC} ${LFLAGS} -o printqueue printqueue.o ${O} ${CAPLIB} ${AFPLIB} ${SLIB}

aufsmkusr:	aufsmkusr.o
	${CC} ${LFLAGS} -o aufsmkusr aufsmkusr.o ${O} ${AFPLIB} ${SLIB}

aufsmkkey:	aufsmkkey.o
	${CC} ${LFLAGS} -o aufsmkkey aufsmkkey.o ${O} ${AFPLIB} ${SLIB}

att_getopt.c:
	ln -s ../extras/att_getopt.c

install: ${PROGS}
	-strip ${PROGS}
	ifdef([sysvinstall],[install -f ${UDESTDIR} ${USER}],
		[${INSTALLER} ${USER} ${UDESTDIR}])
	ifdef([sysvinstall],[install -f ${SDESTDIR} ${SRVR}],
		[${INSTALLER} ${SRVR} ${SDESTDIR}])

clean:
	-rm -f ${PROGS} *.o core make.log err att_getopt.c *~
	-(cd AppManager; make clean)
	-(cd AsyncATalk; make clean)
	-(cd AufsTools; make clean)
	-(cd MacPS; make clean)
	-(cd Messages; make clean)
	-(cd Timelord; make clean)

spotless:
	-rm -f ${PROGS} *.o *.orig core make.log err att_getopt.c *~
	-rm -f Makefile makefile
	-(cd AppManager; make spotless)
	-(cd AsyncATalk; make spotless)
	-(cd AufsTools; make spotless)
	-(cd MacPS; make spotless)
	-(cd Messages; make spotless)
	-(cd Timelord; make spotless)

cleanexe:
	-rm -f ${PROGS}

dist:
	@cat todist
