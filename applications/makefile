# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:15 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp


all:
	(cd lwsrv; make)
	(cd papif; make)
	(cd aufs; make)

install:
	(cd lwsrv; make install)
	(cd papif; make install)
	(cd aufs; make install)

clean:
	-(cd lwsrv; make clean)
	-(cd papif; make clean)
	-(cd aufs; make clean)

dist:
	@cat todist
	@(cd papif; make dist)
	@(cd lwsrv; make dist)
	@(cd aufs; make dist)

