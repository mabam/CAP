DESTDIR = /usr/local/cap

all: sit stuffit

stuffit: sit
	-rm -f stuffit
	ln sit stuffit

sit:	sit.o updcrc.o
	cc -o sit sit.o updcrc.o

install: sit
	-strip sit
	cp sit $(DESTDIR)
	-rm -f $(DESTDIR)/stuffit
	(cd $(DESTDIR); ln sit stuffit)

clean:
	-rm -f stuffit sit *.o
