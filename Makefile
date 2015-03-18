
all:
	@echo "You didn't read NOTES and doc/install.ms did you?"
	@echo
	@echo "First two steps are: Configure and gen.makes"

include: .
	(cd netat; make install)

libsmade:
	(cd lib/cap; make)
	(cd lib/afp; make)
	(cd lib/afpc; make)
	touch libsmade

libinstall: libsmade
	(cd lib/cap; make install)
	(cd lib/afp; make install)
	(cd lib/afpc; make install)
	touch libinstall

programs: libinstall
	-(cd etc; make)
	-(cd samples; make)
	-(cd contrib; make)
	-(cd applications; make)
	-(cd support/uab; make)
	-(cd support/capd; make)
	-(cd support/ethertalk; make)
	touch programs

install: libinstall programs
	-(cd etc; make install)
	-(cd samples; make install)
	-(cd contrib; make install)
	-(cd applications; make install)
	-(cd support/uab; make install)
	-(cd support/capd; make install)
	-(cd support/ethertalk; make install)

cap.shar: listtodist
	cap.dist shar

cap.tar: listtodist
	cap.dist tar

dist:
	@cat todist
	@(cd netat; make dist)
	@(cd lib; make dist)
	@(cd etc; make dist)
	@(cd samples; make dist)
	@(cd contrib; make dist)
	@(cd extras; make dist)
	@(cd doc; make dist)
	@(cd man; make dist)
	@(cd applications; make dist)

clean:
	-rm -f m4.tmp
	-rm -f libsmade
	-rm -f programs
	-rm -f libinstall
	-(cd netat; make clean)
	-(cd man; make clean)
	-(cd doc; make clean)
	-(cd lib; make clean)
	-(cd etc; make clean)
	-(cd samples; make clean)
	-(cd contrib; make clean)
	-(cd extras; make clean)
	-(cd applications; make clean)
	-(cd support/uab; make clean)
	-(cd support/capd; make clean)
	-(cd support/ethertalk; make clean)

spotless:
	-rm -f *.orig
	-rm -f m4.tmp
	-rm -f libsmade
	-rm -f programs
	-rm -f libinstall
	-(cd netat; make spotless)
	-(cd man; make spotless)
	-(cd doc; make spotless)
	-(cd lib; make spotless)
	-(cd etc; make spotless)
	-(cd samples; make spotless)
	-(cd contrib; make spotless)
	-(cd extras; make spotless)
	-(cd applications; make spotless)
	-(cd support/uab; make spotless)
	-(cd support/capd; make spotless)
	-(cd support/ethertalk; make spotless)
