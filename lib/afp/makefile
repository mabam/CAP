# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:06 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O   -DSHORT_NAMES -DMELBOURNE
DESTDIR=/usr/local/lib
OSDEFS=
LIBAFP=libafp.a
I=/usr/include
DES=../../extras

LIBAFPSRCS=afperr.c afpcmd.c afppacks.c afposlock.c
LIBAFPOBJS=afperr.o afpcmd.o afppacks.o afposlock.o des.o

$(LIBAFP):	$(LIBAFPOBJS)
	ar rv  $(LIBAFP) $(LIBAFPOBJS)

des.o: ${DES}/des.c
	(cd ${DES}; make des.o)
	cp ${DES}/des.o .

clean:
	-rm -f ${LIBAFPOBJS} ${LIBAFP} core *~

install:	$(LIBAFP)
	${INSTALLER} $(LIBAFP) $(DESTDIR)
	ranlib $(DESTDIR)/$(LIBAFP)

dist:
	@cat todist

lint:	$(LIBAFPSRCS)
	lint $(LIBAFPSRCS)

afposlock.o:	afposlock.c
	${CC} ${OSDEFS} ${CFLAGS} -c afposlock.c

# Dependencies
afpcmd.o:       afpcmd.c	$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/afp.h $I/netat/afpcmd.h 
afperr.o:       afperr.c        $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/sysvcompat.h $I/netat/afp.h 
afppacks.o:     afppacks.c      $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/sysvcompat.h $I/netat/afp.h \
				$I/netat/afpcmd.h 
afposlock.o:    afposlock.c	$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/afp.h $I/netat/afpcmd.h
