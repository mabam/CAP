all: aufsmon aufslock

aufsmon: aufsmon.c
	cc -o aufsmon aufsmon.c

aufslock: aufslock.c
	cc -o aufslock aufslock.c

clean:
	rm -f *.o aufsmon aufslock

spotless:
	rm -f *.o *.orig aufsmon aufslock
