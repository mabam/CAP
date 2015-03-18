#
# Macintosh Messages for UNIX
#
# djh@munnari.OZ.AU, 08/05/90
#

LDFLAGS = -lcap

all: macwho messages macuser

macwho: macwho.o notify.o
	cc -o macwho macwho.o notify.o ${LDFLAGS}

messages: messages.o notify.o
	cc -o messages messages.o notify.o ${LDFLAGS}
	/bin/rm -f macto macwall macmail macwrite
	ln messages macto
	ln messages macwall
	ln messages macmail
	ln messages macwrite
	/bin/rm -f messages

macuser: macuser.c notify.h
	cc -o macuser macuser.c ${LDFLAGS}

macwho.o: macwho.c notify.h
	cc -c macwho.c

messages.o: messages.c notify.h
	cc -c messages.c

notify.o: notify.c notify.h
	cc -c notify.c

install:
	cp macwho macuser macto /usr/local/cap
	(cd /usr/local/cap;ln macto macwall;ln macto macmail;ln macto macwrite)

shar:
	/usr/local/bin/shar README Makefile *.h *.c *.1l dot.forward \
		messages.hqx > messages.1.4.shar

clean:
	/bin/rm -f *.o core macto macwrite macwall macmail macuser macwho

spotless:
	/bin/rm -f *.o *.orig core macto macwrite macwall macmail macuser macwho
