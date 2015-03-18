#
# $Author: djh $ $Date: 91/02/15 22:50:33 $
# $Header: Makefile,v 2.1 91/02/15 22:50:33 djh Rel $
# $Revision: 2.1 $
#

#
# Makefile for BSD library for xenix.
#

SHELL   = /bin/sh
CC      = cc
CFLAGS  = -O
OBJS    = fsync.o ftruncate.o groups.o rename.o scandir.o
SRCS    = fsync.c ftruncate.c groups.c rename.c scandir.c
OTHERS  = Makefile
SHAR    = libbsd.sh
LIB     = Slibbsd.a

all:    $(LIB)

install: all
	copy -m $(LIB) /usr/lib/386/$(LIB)
	copy -m ../afp/libafp.a /usr/lib/386/Slibafp.a
	copy -m ../afpc/libafpc.a /usr/lib/386/Slibafpc.a
	copy -m ../cap/libcap.a /usr/lib/386/Slibcap.a

clean:
	rm -f *.o

clobber: clean
	rm -f $(LIB) $(SHAR)

shar:   $(SHAR)

$(SHAR): $(OTHERS) $(SRCS)
	shar $(OTHERS) $(SRCS) >$@

$(LIB): $(OBJS)
	ar rc $@ $(OBJS)
	ranlib $@
