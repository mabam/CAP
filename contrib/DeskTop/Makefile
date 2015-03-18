all: builddt dumpdt

CFLAGS=-I../..

builddt: builddt.o dtmisc.o
	${CC} -o builddt builddt.o dtmisc.o

dumpdt: dumpdt.o dtmisc.o
	${CC} -o dumpdt dumpdt.o dtmisc.o

builddt.o: builddt.c dt.h
	${CC} ${CFLAGS} -c builddt.c

dumpdt.o: dumpdt.c dt.h
	${CC} ${CFLAGS} -c dumpdt.c

dtmisc.o: dtmisc.c dt.h
	${CC} ${CFLAGS} -c dtmisc.c

shar:
	/usr/local/bin/shar README Makefile *.h *.c > capdt.1.0.shar

clean:
	rm -rf builddt dumpdt *.o a.out core
