all: timelord

timelord:	timelord.c timelord.h
	cc -o timelord timelord.c -lcap

install:
	cp timelord /usr/local/cap

shar:
	/usr/local/bin/shar README Makefile *.h *.c *.1l \
		tardis.1.3.sit.hqx > timelord.1.3.shar

clean:
	rm -f timelord

spotless:
	rm -f timelord *.orig
