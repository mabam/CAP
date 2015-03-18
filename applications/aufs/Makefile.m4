CFLAGS=cflags() bigcflags() specialcflags()
I=includedir()
#
# SEE INSTALLATION for documentation
#

# valid are NONXLATE,FULL_NCS_SUPPORT,USECHOWN
#  USESTATFS or USEGETMNT
#  USEQUOTA or USESUNQUOTA
#  and GGTYPE="gid_t"
OSDEFS=aufsosdefs()
AFPLIB=libafp()
CAPLIB=libcap()

# for other libraries (like BSD on hpux)
SLIB=libspecial()

# for Rutgers
RULIB=libru()

# used mainly for debugging
CAPFILES=

# aufs.c definitions: USEVPRINTF - use vprintf in logging
AUFSDEFS=ifdef([usevprintf],[-DUSEVPRINTF ])

# to get "more" information about files with a speed penalty
# Also, is specific to 4.2 BSD.  May not work on some machines
ifdef([smartunixfinderinfo],[],[#])AFPUDB=-DSMART_UNIX_FINDERINFO

#For hpux (you have you may need to supply a routine that does rename)
# (Other limitations apply!!!!)
# RENAME=rename.o

# make sure that you define point getopt to att_getopt.o if your system
# doesn't have it builtin
GETOPT=ifdef([needgetopt],[needgetopt])

# This encodes the assumed location of certain directories
EXTRAS=../../extras
# Set the following approriately
DESTDIR=capsrvrdestdir()

#
# End of configurable options
#
SRCS=afpos.c afpvols.c afpfile.c afpdir.c afpfork.c \
	afpmisc.c afpserver.c aufsicon.c abmisc2.c \
	afpdt.c afpdid.c afposenum.c  afpavl.c \
	afposfi.c afpgc.c afppasswd.c afposlock.c aufsv.c \
	afpudb.c afposncs.c afpspd.c afpfid.c afpdsi.c aufscicon.c
OBJS=afpos.o afpvols.o afpfile.o \
	afpmisc.o afpserver.o aufsicon.o abmisc2.o \
	afpdt.o afpdir.o afpfork.o afpdid.o afposenum.o afpavl.o \
	afposfi.o afpgc.o afppasswd.o aufsv.o \
	afpudb.o afposncs.o afpspd.o afpfid.o afpdsi.o aufscicon.o
SYMLINKS=att_getopt.c

all:	aufs sizeserver afpidsrvr afpidlist afpidtool

aufs: aufs.o $(OBJS) $(CAPFILES) ${RENAME} $(GETOPT)
	${CC} $(LFLAGS) -o aufs aufs.o $(OBJS) $(CAPFILES) ${RENAME} \
		$(GETOPT) ${AFPLIB} ${CAPLIB} ${SLIB} ${RULIB}

sizeserver: sizeserver.o
	${CC} ${LFLAGS} -o sizeserver sizeserver.o ${SLIB} ${RULIB}

sizeserver.o:	sizeserver.c	sizeserver.h
	${CC} ${OSDEFS} ${CFLAGS} -c sizeserver.c

afpidsrvr: afpidsrvr.c ../../lib/afp/afpidaufs.h
	${CC} ${OSDEFS} ${CFLAGS} -o afpidsrvr afpidsrvr.c \
		${GETOPT} ${AFPLIB} ${SLIB}

afpidlist: afpidlist.c ../../lib/afp/afpidaufs.h
	${CC} ${OSDEFS} ${CFLAGS} -o afpidlist afpidlist.c \
		${GETOPT} ${AFPLIB} ${SLIB}

afpidtool: afpidtool.c ../../lib/afp/afpidaufs.h
	${CC} ${OSDEFS} ${CFLAGS} -o afpidtool afpidtool.c \
		${GETOPT} ${AFPLIB} ${SLIB}

newver: 
	/bin/sh aufs_vers.sh `cat aufs_vers` aufs_vers aufsv.c
	make all

aufsv.c: aufs_vers
	/bin/sh aufs_vers.sh `cat aufs_vers` useold aufsv.c

clean:
	-rm -f *.o aufs sizeserver afpidsrvr afpidlist afpidtool ${SYMLINKS}

spotless:
	-rm -f *.o *.orig aufs sizeserver afpidsrvr afpidlist afpidtool \
		${SYMLINKS} Makefile makefile

lint:	aufs.c $(SRCS)
	lint aufs.c $(SRCS)

install:	aufs sizeserver
	-strip aufs
	ifdef([sysvinstall],[install -f $(DESTDIR) aufs],
		[${INSTALLER} aufs $(DESTDIR)])
	-strip sizeserver
	ifdef([sysvinstall],[install -f $(DESTDIR) sizeserver],
		[${INSTALLER} sizeserver $(DESTDIR)])
	-strip afpidsrvr afpidlist afpidtool
	ifdef([sysvinstall],[install -f $(DESTDIR) afpidsrvr afpidlist afpidtool],
		[${INSTALLER} afpidsrvr afpidlist afpidtool $(DESTDIR)])

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

afpserver.o:  afpserver.c
	${CC} ${OSDEFS} ${CFLAGS} ${AUFSDEFS} -c afpserver.c

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
afpdsi.o:	afpdsi.c	$I/netat/appletalk.h ../../lib/cap/abasp.h \
				afpdsi.h
