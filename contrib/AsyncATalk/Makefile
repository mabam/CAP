#
# Makefile for Asynchronous AppleTalk, UNIX end
#
#	djh@munnari.oz.au, October 1988
#	Copyright 1988, 1991, The University of Melbourne
#

#
# if you define BROAD, you can use async (setuid root!) 
# for one user and don't need to have asyncad running
#
# CFLAGS=-DBROAD
CFLAGS=
LFLAGS=
CAPLIB= -lcap
# CAPLIB=-Wl,-L/usr/local/lib -lcap
O=
PROGS = async asyncad
DESTDIR = /usr/local/cap

all:	${PROGS}

async:	async.o atalkdbm.o $(O)
	cc ${LFLAGS} -o async async.o atalkdbm.o $(O) $(CAPLIB)

async.o:	async.c async.h macros.h
	cc ${CFLAGS} -c async.c

asyncad:	asyncad.o atalkdbm.o $(O)
	cc ${LFLAGS} -o asyncad asyncad.o atalkdbm.o $(O) $(CAPLIB)

asyncad.o:	asyncad.c async.h macros.h
	cc ${CFLAGS} -c asyncad.c

atalkdbm.o:	atalkdbm.c
	cc ${CFLAGS} -c atalkdbm.c

install: ${PROGS}
	cp ${PROGS} ${DESTDIR}
	(cd ${DESTDIR}; strip ${PROGS})
	rm ${PROGS}

tar:
	tar cvf async.1.4.tar async.1.4.hqx INSTALLATION README Makefile \
		async.h macros.h async.c atalkdbm.c asyncad.c

shar:
	/usr/local/bin/shar async.1.4.hqx INSTALLATION README Makefile \
		async.h macros.h async.c atalkdbm.c asyncad.c \
					> SHAR/async.atalk.1.4.shar

clean:
	rm -f ${PROGS} *.o core make.log err

spotless:
	rm -f ${PROGS} *.o *.orig core make.log err
