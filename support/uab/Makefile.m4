CFLAGS=-DDEBUG cflags() specialcflags()
DESTDIR=capsrvrdestdir()
PROGS=uabprogs()
POBJS=uabpobjs()
CAPLIB=libcap()
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
	ifdef([sysvinstall],[install -f $(DESTDIR) uab],
		[${INSTALLER} uab ${DESTDIR}])

kip_mpx.o: kip_mpx.c mpxddp.h gw.h node.h ddpport.h
	${CC} -c ${CFLAGS} -DTAB=etalklocal() -DMTAB=etalklocal() kip_mpx.c

uab.o: uab.c mpxddp.h gw.h node.h ddpport.h if_desc.h
	${CC} -c ${CFLAGS} -DUAB_PIDFILE=uabpidfile() \
			-DBRIDGE_DESC=uabbrdescr() uab.c

lint:
	lint -h uab.c ${SRCS}

clean:
	rm -f *.o uab

spotless:
	rm -f *.o *.orig uab Makefile makefile

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
