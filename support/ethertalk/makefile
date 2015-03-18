# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:26 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS= -O   -DSHORT_NAMES -DMELBOURNE
DESTDIR=/usr/local/cap
PROGS=
POBJS=
ETCDIR=/etc
CAPLIB=-lcap

LIBABSRCS=abelap.c ethertalk.c ../uab/aarp.c ../uab/hash.c
LIBABOBJS=abelap.o ethertalk.o aarp.o hash.o

#
# abetalk.o provides EtherTalk support for CAP
#
all: ${PROGS}

abetalk.o: ${LIBABOBJS} ${POBJS} aarpd_clnt.o aarpd_xdr.o aarpd.h
	ld -r -o abetalk.o ${LIBABOBJS} ${POBJS} aarpd_clnt.o aarpd_xdr.o

aarpd:	aarpd.o aarpd_svc.o aarpd.h
	cc -o aarpd aarpd.o aarpd_svc.o ${CAPLIB}

aarptest: aarptest.o aarpd.h
	cc -o aarptest aarptest.o ${CAPLIB}

rtmptest: rtmptest.o aarpd.h
	cc -o rtmptest rtmptest.o ${CAPLIB}

aarpd.o: aarpd.c aarpd.h

abelap.o: abelap.c

ethertalk.o: ethertalk.c ../uab/ethertalk.h

snitp.o: snitp.c ../uab/proto_intf.h

senetp.o: senetp.c ../uab/proto_intf.h

aarpd_clnt.o: aarpd_clnt.c aarpd.h

aarpd_svc.o: aarpd_svc.c aarpd.h

aarpd_xdr.o: aarpd_xdr.c aarpd.h

aarp.o: ../uab/aarp.c ../uab/hash.h ../uab/proto_intf.h \
    ../uab/ethertalk.h ../uab/aarp_defs.h ../uab/aarp.h
	cc $(CFLAGS) -DAARPD -c ../uab/aarp.c

hash.o: ../uab/hash.c ../uab/hash.h
	cc $(CFLAGS) -c ../uab/hash.c

aarptest.o: aarptest.c aarpd.h

rtmptest.o: rtmptest.c aarpd.h

install: ${PROGS}.install

.install:

aarpd.install: aarpd
	-strip aarpd
	${INSTALLER} aarpd ${DESTDIR}

clean:
	rm -f *.o core aarpd aarptest rtmptest
