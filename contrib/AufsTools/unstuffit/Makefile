GETOPT = 
#GETOPT = getopt.o
DESTDIR = /usr/local/cap

all: unsit unstuffit

unstuffit: unsit
	-rm -f unstuffit
	ln unsit unstuffit

unsit : unsit.o updcrc.o $(GETOPT)
	cc -o unsit unsit.o updcrc.o $(GETOPT)

unsit.o : unsit.c stuffit.h
getopt.o : getopt.c

install: unsit
	-strip unsit
	cp unsit $(DESTDIR)
	-rm -f $(DESTDIR)/unstuffit
	(cd $(DESTDIR); ln unsit unstuffit)

clean:
	-rm -f unstuffit unsit *.o
