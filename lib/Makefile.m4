

all:
	(cd cap; make)
	(cd afp; make)
	(cd afpc; make)
	ifelse(os,[xenix5],[(cd xenix; make)],[])

install: ifelse(os,[xenix5],[xenix5],[normal])

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
	ifelse(os,[xenix5],[-(cd xenix; make clean)],[])

spotless:
	-rm -f *.orig Makefile makefile
	-(cd cap; make spotless)
	-(cd afp; make spotless)
	-(cd afpc; make spotless)
	ifelse(os,[xenix5],[-(cd xenix; make clean)],[])

dist:
	@cat todist
	@(cd cap; make dist)
	@(cd afp; make dist)
	@(cd afpc; make dist)

