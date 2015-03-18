CFLAGS=cflags() bigcflags() specialcflags()
DESTDIR=libdestdir()
LIBAFPC=afpclib()
I=includedir()

LIBAFPCSRCS=afpc.c afpcc.c
LIBAFPCOBJS=afpc.o afpcc.o

$(LIBAFPC):	$(LIBAFPCOBJS)
	ifdef([uselordertsort],[ar cr $(LIBAFPC) `lorder $(LIBAFPCOBJS)|tsort`],
	[ar rv  $(LIBAFPC) $(LIBAFPCOBJS)])

clean:
	-rm -f ${LIBAFPCOBJS} ${LIBAFPC} core *~

spotless:
	-rm -f ${LIBAFPCOBJS} ${LIBAFPC} core *~ *.orig Makefile makefile

install:	$(LIBAFPC)
	ifdef([sysvinstall],[install -f $(DESTDIR) $(LIBAFPC)],
		[${INSTALLER} $(LIBAFPC) $(DESTDIR)])
ifdef([uselordertsort],[],[	ranlib $(DESTDIR)/$(LIBAFPC)])

dist:
	@cat todist

lint:	$(LIBAFPCSRCS)
	lint $(LIBAFPCSRCS)

afpc.o:         afpc.c		$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afpc.h 
afpcc.o:        afpcc.c		$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/afpc.h 
