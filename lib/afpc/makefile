# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:08 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O   -DSHORT_NAMES -DMELBOURNE
DESTDIR=/usr/local/lib
LIBAFPC=libafpc.a
I=/usr/include

LIBAFPCSRCS=afpc.c afpcc.c
LIBAFPCOBJS=afpc.o afpcc.o

$(LIBAFPC):	$(LIBAFPCOBJS)
	ar rv  $(LIBAFPC) $(LIBAFPCOBJS)

clean:
	-rm -f ${LIBAFPCOBJS} ${LIBAFPC} core *~

install:	$(LIBAFPC)
	${INSTALLER} $(LIBAFPC) $(DESTDIR)
	ranlib $(DESTDIR)/$(LIBAFPC)

dist:
	@cat todist

lint:	$(LIBAFPCSRCS)
	lint $(LIBAFPCSRCS)

afpc.o:         afpc.c		$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afpc.h 
afpcc.o:        afpcc.c		$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afpc.h 
