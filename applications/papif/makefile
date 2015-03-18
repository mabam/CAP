# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:20 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O  -DSHORT_NAMES -DMELBOURNE
LWFLAGS=-DDOCNAME -DPAGECOUNT=512
I=/usr/include
CAPLIB=-lcap
DESTDIR=/usr/local/cap

# for other libraries (like BSD on hpux)
SLIB=

# See README file for notes about defines
# Valid: SFLOWQ=[1,2,3,4,5,6,7,8]
# Valid: IDLESTUFF, NO_STRUCT, NOACCT, CAPPRINTERS=location
PAPFLAGS=-DMACUSER -DIDLESTUFF 
PAPBANNER=-DCHARGEBANNER

# USEVPRINTF - use vprintf in logging
VPRINTF=-DUSEVPRINTF
# If you have Transcript from Adobe for your laserWriter and want to
#  print text files, uncomment the next line and set the location properly
# WPSTEXT="-DPSTEXT=\"/usr/local/lib/ps/pstext\"

# This is if you have transcript and and want page reversal if possible
# WPSREVERSE=-DPSREVERSE=\"/usr/local/lib/ps/psrev\"

all:	papif papof

papif:	papif.o $(O)
	${CC} ${LFLAGS} -o papif papif.o $(O) $(CAPLIB) ${SLIB}

papif.o: papif.c
	${CC} ${CFLAGS} ${VPRINTF} ${PAPBANNER} ${PAPFLAGS} ${LWFLAGS} \
		${WPSTEXT} ${WPSREVERSE} -c papif.c




papof.o: papof.c
	${CC} -c ${CFLAGS} ${PAPBANNER} papof.c

papof:	papof.o
	${CC} ${LFLAGS} -o papof papof.o ${SLIB}


clean:
	-rm -f papif papof *.o


install: papif papof
	-strip papif papof
	${INSTALLER} papif papof $(DESTDIR)


dist:
	@cat todist
