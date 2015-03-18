# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:17 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O   -DSHORT_NAMES -DMELBOURNE
I=/usr/include
#
# SEE INSTALLATION for documentation
#

# valid are NONXLATE,FULL_NCS_SUPPORT,USECHOWN
#  USESTATFS or USEGETMNT
#  USEQUOTA or USESUNQUOTA
#  and GGTYPE="gid_t"
OSDEFS= -DUSESTATFS -DUSESUNQUOTA
AFPLIB=-lafp
CAPLIB=-lcap

# for other libraries (like BSD on hpux)
SLIB=

# used mainly for debugging
CAPFILES=

# aufs.c definitions: USEVPRINTF - use vprintf in logging
AUFSDEFS=-DUSEVPRINTF 

# to get "more" information about files with a speed penalty
# Also, is specific to 4.2 BSD.  May not work on some machines
#AFPUDB=-DSMART_UNIX_FINDERINFO

#For hpux (you have you may need to supply a routine that does rename)
# (Other limitations apply!!!!)
# RENAME=rename.o

# make sure that you define point getopt to att_getopt.o if your system
# doesn't have it builtin
GETOPT=

# This encodes the assumed location of certain directories
EXTRAS=../../extras
# Set the following approriately
DESTDIR=/usr/local/cap

#
# End of configurable options
#
SRCS=afpos.c afpvols.c afpfile.c afpdir.c afpfork.c \
	afpmisc.c afpserver.c aufsicon.c abmisc2.c \
	afpdt.c afpdid.c afposenum.c  afpavl.c \
	afposfi.c afpgc.c afppasswd.c afposlock.c aufsv.c \
	afpudb.c afposncs.c afpspd.c sizeserver.c
OBJS=afpos.o afpvols.o afpfile.o \
	afpmisc.o afpserver.o aufsicon.o abmisc2.o \
	afpdt.o afpdir.o afpfork.o afpdid.o afposenum.o afpavl.o \
	afposfi.o afpgc.o afppasswd.o aufsv.o \
	afpudb.o afposncs.o afpspd.o sizeserver.o
SYMLINKS=att_getopt.c

all:	aufs 

aufs: aufs.o $(OBJS) $(CAPFILES) ${RENAME} $(GETOPT)
	${CC} $(LFLAGS) -o aufs aufs.o $(OBJS) $(CAPFILES) ${RENAME} \
		$(GETOPT) ${AFPLIB} ${CAPLIB} ${SLIB}

newver: 
	/bin/sh aufs_vers.sh `cat aufs_vers` aufs_vers aufsv.c
	make all

aufsv.c: aufs_vers
	/bin/sh aufs_vers.sh `cat aufs_vers` useold aufsv.c

clean:
	-rm -f *.o aufs ${SYMLINKS}

lint:	aufs.c $(SRCS)
	lint aufs.c $(SRCS)

install:	aufs
	-strip aufs
	${INSTALLER} aufs $(DESTDIR)

dist:
	@cat todist

att_getopt.o:	att_getopt.c

att_getopt.c:
	ln -s ${EXTRAS}/att_getopt.c

afpos.o:	afpos.c
	${CC} ${OSDEFS} ${CFLAGS} -c afpos.c

afposncs.o:	afposncs.c
	${CC} ${OSDEFS} ${CFLAGS} -c afposncs.c

afpudb.o:	afpudb.c
	${CC} ${CFLAGS} ${AFPUDB} -c afpudb.c

aufs.o:	aufs.c
	${CC} ${OSDEFS} ${CFLAGS} ${AUFSDEFS} -c aufs.c

# Dependencies
afpos.o:        afpos.c         $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h \
				afpvols.h $I/netat/afpcmd.h 
afpudb.o:	afpudb.c	$I/netat/appletalk.h afpudb.h
afpfork.o:      afpfork.c       $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h \
				$I/netat/afpcmd.h afpntoh.h 
afpdir.o:       afpdir.c        $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h \
				$I/netat/afpcmd.h afpntoh.h 
afposfi.o:      afposfi.c	$I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h afpgc.h afpudb.h
afpvols.o:      afpvols.c       $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h \
				$I/netat/afpcmd.h afpvols.h \
				afpntoh.h 
afpfile.o:      afpfile.c       $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h \
				$I/netat/afpcmd.h afpntoh.h 
afpmisc.o:      afpmisc.c       $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h 
afpserver.o:    afpserver.c     $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h \
				$I/netat/afpcmd.h afpntoh.h 
aufsicon.o:     aufsicon.c      $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h 
afpcmd.o:       afpcmd.c        $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h $I/netat/afpcmd.h 
abmisc2.o:      abmisc2.c       $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h \
				$I/netat/afpcmd.h 
afpdt.o:        afpdt.c         $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h \
				$I/netat/afp.h $I/netat/afpcmd.h \
				afpvols.h afpdt.h afpavl.h \
				afpntoh.h afpudb.h
afpdid.o:       afpdid.c        $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h 
afposenum.o:    afposenum.c     $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afp.h \
				afpdt.h afpavl.h 
afppacks.o:     afppacks.c      $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h $I/netat/afpcmd.h 
afpavl.o:       afpavl.c        afpavl.h 
afperr.o:       afperr.c        $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/afp.h 
afpgc.o:        afpgc.c         afpgc.h 
afppasswd.o:    afppasswd.c	$I/netat/sysvcompat.h afppasswd.h
afposncs.o:	afposncs.c	$I/netat/appletalk.h $I/netat/afp.h \
				afposncs.h afps.h
sizeserver.o:	sizeserver.c	sizeserver.h
