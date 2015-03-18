I=includedir()
HFILES = aberrors.h abqueue.h appletalk.h afp.h afpcmd.h afpc.h \
	 compat.h sysvcompat.h macfile.h abnbp.h fcntldomv.h afppass.h

install: $(HFILES)
	-mkdir $I/netat
	cp $(HFILES) $I/netat

dist:
	@cat todist

clean:

spotless:
	-rm -f *.orig Makefile makefile
