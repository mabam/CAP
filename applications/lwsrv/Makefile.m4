CFLAGS=cflags() specialcflags()
DESTDIR=capsrvrdestdir()
LWFLAGS=lwflags()
LIBCAP=libcap()
I=includedir()
YACC=yacc
LEX=lex

# Valid are: -DADOBE_DSC2_CONFORMANT
SIMPLEFLAGS=simpleflags()

LWSRVOBJS=fontlist.o lwsrv.o papstream.o procset.o simple.o spmisc.o
LWSRV8OBJS=fontlist8.o lwsrv8.o papstream8.o procset8.o simple8.o \
	spmisc.o query.o list.o parse.o y.tab.o
LWSRVCONFIGOBJS=lwsrvconfig.o list.o packed.o parse.o y.tab.o

# for other libraries (like BSD on hpux)
SLIB=libspecial()
# lex library
LLIB= -ll

# make sure that you define point getopt to att_getopt.o if your system
# doesn't have it builtin
ATT_GETOPT=ifdef([needgetopt],[needgetopt])

all:	lwsrv lwsrv8 lwsrvconfig

lwsrv:	${LWSRVOBJS} ${ATT_GETOPT}
	${CC} -o lwsrv ${LFLAGS} ${LWSRVOBJS} ${ATT_GETOPT} ${LIBCAP} ${SLIB}

lwsrv8:	${LWSRV8OBJS} ${ATT_GETOPT}
	${CC} -o lwsrv8 ${LFLAGS} ${LWSRV8OBJS} ${ATT_GETOPT} ${LIBCAP} \
		${SLIB} ${LLIB}

lwsrvconfig:	${LWSRVCONFIGOBJS}
	${CC} -o lwsrvconfig ${LFLAGS} ${LWSRVCONFIGOBJS} ${SLIB} ${LLIB}

clean:
	-rm -f *.o lwsrv lwsrv8 lwsrvconfig att_getopt.c lex.yy.c y.tab.c

spotless:
	-rm -f *.o *.orig lwsrv lwsrv8 lwsrvconfig att_getopt.c \
		lex.yy.c y.tab.c Makefile makefile

install: lwsrv lwsrv8 lwsrvconfig
	-strip lwsrv lwsrv8 lwsrvconfig
	ifdef([sysvinstall],[install -f $(DESTDIR) lwsrv],
		[${INSTALLER} lwsrv ${DESTDIR}])
	ifdef([sysvinstall],[install -f $(DESTDIR) lwsrv8],
		[${INSTALLER} lwsrv8 ${DESTDIR}])
	ifdef([sysvinstall],[install -f $(DESTDIR) lwsrvconfig],
		[${INSTALLER} lwsrvconfig ${DESTDIR}])

dist:
	@cat todist

att_getopt.c:
	ln -s ../../extras/att_getopt.c

simple.o:       simple.c
	${CC} ${CFLAGS} ${SIMPLEFLAGS} -c simple.c

lwsrv.o:	lwsrv.c
	${CC} ${CFLAGS} ${LWFLAGS} -c lwsrv.c

simple8.o:       simple.c
	${CC} -DLWSRV8 ${CFLAGS} ${SIMPLEFLAGS} -o simple8.o -c simple.c

lwsrv8.o:	lwsrv.c
	${CC} -DLWSRV8 ${CFLAGS} ${LWFLAGS} -o lwsrv8.o -c lwsrv.c

fontlist8.o:	fontlist.c
	${CC} -DLWSRV8 ${CFLAGS} ${LWFLAGS} -o fontlist8.o -c fontlist.c

papstream8.o:	papstream.c
	${CC} -DLWSRV8 ${CFLAGS} ${LWFLAGS} -o papstream8.o -c papstream.c

procset8.o:	procset.c
	${CC} -DLWSRV8 ${CFLAGS} ${LWFLAGS} -o procset8.o -c procset.c

lwsrvconfig.o:	lwsrvconfig.c
	${CC} ${CFLAGS} ${LWFLAGS} -c lwsrvconfig.c

y.tab.o:	y.tab.c lex.yy.c
	${CC} ${CFLAGS} ${LWFLAGS} -c y.tab.c

y.tab.c:	parsey.y
	${YACC} parsey.y

lex.yy.c:	parsel.l
	${LEX} parsel.l

query.o:	query.c
	${CC} ${CFLAGS} ${LWFLAGS} -c query.c

# Dependencies
lwsrv.o:        lwsrv.c		$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/compat.h papstream.h
lwsrv8.o:       lwsrv.c		$I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/compat.h papstream.h fontlist.h \
				list.h query.h parse.h procset.h
simple.o:       simple.c        $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/sysvcompat.h $I/netat/compat.h \
				spmisc.h procset.h fontlist.h papstream.h
simple8.o:      simple.c        $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/sysvcompat.h $I/netat/compat.h \
				spmisc.h procset.h fontlist.h papstream.h \
				list.h query.h parse.h
fontlist.o:     fontlist.c      $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/sysvcompat.h fontlist.h \
				spmisc.h papstream.h
fontlist8.o:    fontlist.c      $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/sysvcompat.h fontlist.h \
				spmisc.h papstream.h list.h query.h parse.h
papstream.o:    papstream.c     $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/sysvcompat.h papstream.h 
papstream8.o:   papstream.c     $I/netat/appletalk.h \
				$I/netat/aberrors.h $I/netat/abqueue.h \
				$I/netat/sysvcompat.h papstream.h 
procset.o:      procset.c       $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/sysvcompat.h $I/netat/compat.h \
				procset.h spmisc.h
procset8.o:     procset.c       $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/sysvcompat.h $I/netat/compat.h \
				procset.h spmisc.h list.h query.h
spmisc.o:       spmisc.c        $I/netat/appletalk.h $I/netat/aberrors.h \
				$I/netat/abqueue.h $I/netat/sysvcompat.h \
				$I/netat/sysvcompat.h $I/netat/compat.h \
				spmisc.h 
query.o:	query.c		$I/netat/appletalk.h list.h query.h parse.h \
				papstream.h
y.tab.c:	parsey.y	list.h parse.h
lwsrvconfig.o:	lwsrvconfig.c	list.h parse.h packed.h
parse.o:	parse.c		list.h parse.h
packed.o:	packed.c	packed.h
