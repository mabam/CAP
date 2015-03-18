# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:24 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp
CFLAGS=-DDEBUG  -O  -DSHORT_NAMES -DMELBOURNE
DESTDIR=/usr/local/cap
PROGS=
POBJS=
CAPLIB=-lcap
LFLAGS=

SRCS=aarp.c kip_mpx.c rtmp.c ethertalk.c ddprouter.c ddpsvcs.c ddpport.c \
	hash.c asyncatalk.c
OBJS=aarp.o kip_mpx.o rtmp.o ethertalk.o ddprouter.o ddpsvcs.o ddpport.o \
	hash.o asyncatalk.o

all:	${PROGS}

uab:	uab.o	${OBJS} ${POBJS}
	${CC} ${LFLAGS} -o uab uab.o ${OBJS} ${POBJS} ${CAPLIB}

install: ${PROGS}.install

.install:

uab.install: uab
	-strip uab
	${INSTALLER} uab ${DESTDIR}

kip_mpx.o: kip_mpx.c mpxddp.h gw.h node.h ddpport.h
	cc -c ${CFLAGS} kip_mpx.c

uab.o: uab.c mpxddp.h gw.h node.h ddpport.h if_desc.h
	cc -c ${CFLAGS} -DUAB_PIDFILE=\"/usr/local/lib/cap/uab.pid\" \
		-DBRIDGE_DESC=\"/usr/local/lib/cap/bridge_desc\" uab.c

lint:
	lint -h uab.c ${SRCS}

clean:
	rm -f *.o uab

# ddpport.h: mpxddp.h node.h
# gw.h: node.h ddport.h (mpxddp.h)
# if_desc.h: mpxddp.h

ddprouter.o: ddprouter.c gw.h node.h ddpport.h mpxddp.h
rtmp.o: rtmp.c gw.h node.h ddpport.h mpxddp.h

ethertalk.o: ethertalk.c proto_intf.h ethertalk.h node.h \
	ddpport.h if_desc.h mpxddp.h
aarp.o: aarp.c proto_intf.h ethertalk.h aarp_defs.h aarp.h

ddpport.o: ddpport.c ddpport.h node.h mpxddp.h

dlip.o: dlip.c proto_intf.h
snitp.o: snitp.c proto_intf.h

hash.o: hash.c hash.h

asyncatalk.o: asyncatalk.c
