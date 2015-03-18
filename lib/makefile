# Makefile autoconfigured for ...
# SunOS system on Fri Feb 15 14:00:01 EST 1991



MFLAGS=
LFLAGS=
CC=cc
LD=ld
SHELL=/bin/sh
INSTALLER=cp


all:
	(cd cap; make)
	(cd afp; make)
	(cd afpc; make)
	

install: normal

normal:
	(cd cap; make install)
	(cd afp; make install)
	(cd afpc; make install)

xenix5:
	(cd xenix; make install)

clean:
	-(cd cap; make clean)
	-(cd afp; make clean)
	-(cd afpc; make clean)
	

dist:
	@cat todist
	@(cd cap; make dist)
	@(cd afp; make dist)
	@(cd afpc; make dist)

