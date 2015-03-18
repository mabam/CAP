CFLAGS=cflags() specialcflags()
LWFLAGS=lwflags()
I=includedir()
CAPLIB=libcap()
DESTDIR=capsrvrdestdir()

# for other libraries (like BSD on hpux)
SLIB=libspecial()

# See README file for notes about defines
# Valid: SFLOWQ=[1,2,3,4,5,6,7,8]
# Valid: IDLESTUFF, NO_STRUCT, NOACCT, CAPPRINTERS=location
PAPFLAGS=papflags() ifdef([capprinters],[-DCAPPRINTERS=]capprinters())
PAPBANNER=papbanner()

# USEVPRINTF - use vprintf in logging
ifdef([usevprintf],[],[#])VPRINTF=-DUSEVPRINTF
# If you have Transcript from Adobe for your laserWriter and want to
#  print text files, uncomment the next line and set the location properly
ifdef([pstextloc],[WPSTEXT="-DPSTEXT=pstextloc()"],
	[# WPSTEXT=-DPSTEXT=\"/usr/local/lib/ps/pstext\"])

# This is if you have transcript and and want page reversal if possible
ifdef([psrevloc],[WPSREVERSE=-DPSREVERSE=psrevloc()],
	[# WPSREVERSE=-DPSREVERSE=\"/usr/local/lib/ps/psrv\"])

ifdef([columbia],[all:	pstest ps8 ps7 papof],[all:	papif papof])

papif:	papif.o $(O)
	${CC} ${LFLAGS} -o papif papif.o $(O) $(CAPLIB) ${SLIB}

papif.o: papif.c
	${CC} ${CFLAGS} ${VPRINTF} ${PAPBANNER} ${PAPFLAGS} ${LWFLAGS} \
		${WPSTEXT} ${WPSREVERSE} -c papif.c

ifdef([columbia],[
pstest:	papif.c ${LIBCAP}
	${CC} -O ${VPRINTF} -DIDLESTUFF -DSFLOWQ=1 papif.c -o pstest -lcap

ps7:	papif.c ${LIBCAP}
	${CC} -O ${VPRINTF} -DIDLESTUFF -DNO_STRUCT -DSFLOWQ=1 -DPSTEXT=\"/usr/local/lib/ps/pstext\" -DPSREVERSE=\"/usr/local/lib/ps/psrev\" papif.c -o ps7 -lcap

ps8:	papif ${LIBCAP}
	cp papif ps8
	strip ps8
])

ifelse(os,[hpux],[
papof:
	echo "papof doesn't compile under hpux, but you don't really"
	echo "need it anyway"

],[
papof.o: papof.c
	${CC} -c ${CFLAGS} ${PAPBANNER} papof.c

papof:	papof.o
	${CC} ${LFLAGS} -o papof papof.o ${SLIB}

])
clean:
	-rm -f papif papof *.o

spotless:
	-rm -f papif papof *.o *.orig Makefile makefile

ifelse(os,[hpux],[
install: papif papof
	-strip papif papof
	ifdef([sysvinstall],[install -f $(DESTDIR) papif],
		[${INSTALLER} papif $(DESTDIR)])
],[
install: papif papof
	-strip papif papof
	ifdef([sysvinstall],[install -f $(DESTDIR) papif papof],
		[${INSTALLER} papif papof $(DESTDIR)])
])

dist:
	@cat todist
