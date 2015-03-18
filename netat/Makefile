HFILES = aberrors.h abqueue.h appletalk.h afp.h afpcmd.h afpc.h \
	 compat.h sysvcompat.h macfile.h abnbp.h fcntldomv.h

install: $(HFILES)
	-mkdir /usr/include/netat
	cp $(HFILES) /usr/include/netat

dist:
	@cat todist

clean:
