# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 13:59:51 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O  -DSHORT_NAMES -DMELBOURNE
I=/usr/include
O=

# Valid: SFLOWQ=[1,2,3,4,5,6,7,8]
LWFLAGS=-DDOCNAME -DPAGECOUNT=512

# location of cap.printers file


# Make sure to define needgetopt if your system doesnt have it
GETOPT=

PROGS=lwpr tlw atlook atlooklws atpinger iwpr isrv atistest ash \
	instappl getzones

DESTDIR=/usr/local/cap
CAPLIB=-lcap
AFPLIB=-lafpc -lafp

# for other libraries (like BSD on hpux)
SLIB=

all:	${PROGS}

atistest:	atistest.o ${O}
	${CC} ${LFLAGS} -o atistest atistest.o ${O} ${CAPLIB} ${SLIB}

getzones:	getzones.o ${O}
	${CC} ${LFLAGS} -o getzones getzones.o ${O} ${CAPLIB} ${SLIB}

ash.o:	ash.c
	${CC} ${CFLAGS}  -c ash.c

ash:	ash.o ${CAPFILES}
	${CC} ${LFLAGS} -o ash ash.o ${CAPFILES} ${AFPLIB} ${CAPLIB} ${SLIB}

instappl: instappl.o $(GETOPT)
	${CC} ${LFLAGS} -o instappl instappl.o $(GETOPT) ${SLIB}

# iwpr and lwpr share sources...
iwpr:	iwpr.o $(O) ${GETOPT}
	${CC} ${LFLAGS} -o iwpr iwpr.o ${GETOPT} $(O) $(CAPLIB) ${SLIB}

lwpr:	lwpr.o $(O) ${GETOPT}
	${CC} ${LFLAGS} -o lwpr lwpr.o ${GETOPT} $(O) $(CAPLIB) ${SLIB}

lwpr.o: lwpr.c
	${CC} ${CFLAGS} ${LWFLAGS} ${CAPPRINTERS} -c lwpr.c

iwpr.o:	lwpr.c
	cp lwpr.c iwpr.c
	${CC} ${CFLAGS} ${LWFLAGS} -c -DIMAGEWRITER iwpr.c
	rm iwpr.c

isrv:	isrv.o $(O)
	${CC} $(LFLAGS) -o isrv isrv.o $(O) $(CAPLIB) ${SLIB}

isrv.o:	isrv.c
	${CC} ${CFLAGS} -c isrv.c

tlw:	tlw.o $(O)
	${CC} ${LFLAGS} -o tlw tlw.o $(O) $(CAPLIB) ${SLIB}

tlw.o:	tlw.c
	${CC} ${CFLAGS} ${LWFLAGS} -c tlw.c

#
# atlook, atlooklw, and atpinger all have a common source
#
atlook:	atlook.o ${GETOPT} $(O)
	${CC} ${LFLAGS} -o atlook atlook.o $(O) ${GETOPT} $(CAPLIB) ${SLIB}

# copy because some machines won't do it right o.w.
atlooklws.o: atlook.c
	cp atlook.c atlooklws.c
	${CC} ${CFLAGS} -c -DATLOOKLWS atlooklws.c
	rm atlooklws.c
	
atlooklws:	atlooklws.o ${GETOPT} ${O}
	${CC} ${LFLAGS} -o atlooklws atlooklws.o ${GETOPT} $(O) $(CAPLIB) ${SLIB}

atpinger.o: atlook.c
	cp atlook.c atpinger.c
	${CC} ${CFLAGS} -c -DATPINGER atpinger.c
	rm atpinger.c

atpinger:	atpinger.o $(O) ${GETOPT}
	${CC} ${LFLAGS} -o atpinger atpinger.o ${GETOPT} $(O) $(CAPLIB) ${SLIB}

att_getopt.c:
	ln -s ../extras/att_getopt.c

install: ${PROGS}
	-strip ${PROGS}
	${INSTALLER} ${PROGS} ${DESTDIR}

clean:
	-rm -f ${PROGS} *.o core make.log err *~ att_getopt.c

cleanexe:
	-rm -f ${PROGS}

dist:
	@cat todist
