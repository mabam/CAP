DESTDIR= /usr/local/cap

capit: capit.c
	cc -o capit -O capit.c

install: capit
	-strip capit
	cp capit $(DESTDIR)

clean:
	-rm -f capit *.o
