CFLAGS=-DDEBUG cflags() specialcflags()
DESTDIR=capsrvrdestdir()
PROGS=capdprogs()
POBJS=capdpobjs()
CAPLIB=libcap()
LFLAGS=

SRCS=capd.c
OBJS=capd.o

all:	${PROGS}

capd:	${OBJS} ${POBJS}
	${CC} ${LFLAGS} -o capd ${OBJS} ${POBJS} ${CAPLIB}

install: ${PROGS}.install

.install:

capd.install: capd
	-strip capd
	ifdef([sysvinstall],[install -f $(DESTDIR) capd],
		[${INSTALLER} capd ${DESTDIR}])

lint:
	lint -h capd.c ${SRCS}

clean:
	rm -f *.o capd

spotless:
	rm -f *.o *.orig capd

capd.o: capd.c
	${CC} -c capd.c ${CFLAGS}

capd.kas.o: capd.kas.c
	${CC} -c capd.kas.c ${CFLAGS}

