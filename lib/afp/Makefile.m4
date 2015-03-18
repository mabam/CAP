CFLAGS=cflags() bigcflags() specialcflags()
DESTDIR=libdestdir()
OSDEFS=afposdefs()
LIBAFP=afplib()
I=includedir()
DES=desloc()

LIBAFPSRCS=afperr.c afpcmd.c afppacks.c afposlock.c afppass.c \
	afpidaufs.c afpidclnt.c afpidgnrl.c
LIBAFPOBJS=afperr.o afpcmd.o afppacks.o afposlock.o des.o afppass.o \
	afpidaufs.o afpidclnt.o afpidgnrl.o

$(LIBAFP):	$(LIBAFPOBJS)
	ifdef([uselordertsort],[ar cr $(LIBAFP) `lorder $(LIBAFPOBJS)| tsort`],
	[ar rv  $(LIBAFP) $(LIBAFPOBJS)])

des.o: ${DES}/des.c
	(cd ${DES}; make des.o)
	cp ${DES}/des.o .

clean:
	-rm -f ${LIBAFPOBJS} ${LIBAFP} core *~

spotless:
	-rm -f ${LIBAFPOBJS} ${LIBAFP} core *~ *.orig Makefile makefile

install:	$(LIBAFP)
	ifdef([sysvinstall],[install -f $(DESTDIR) $(LIBAFP)],
		[${INSTALLER} $(LIBAFP) $(DESTDIR)])
ifdef([uselordertsort],[],[	ranlib $(DESTDIR)/$(LIBAFP)])

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

afpidaufs.o:	afpidaufs.c afpidaufs.h afpidnames.h
afpidclnt.o:	afpidclnt.c afpidaufs.h afpidnames.h
afpidgnrl.o:	afpidgnrl.c afpidaufs.h afpidnames.h
afppass.o:	afppass.c	$I/netat/afppass.h
