#
# CAP manual entries
#

all: links txt

links:
	ln papif.8 papof.8
	ln atprint.1 tlw.1
	ln atprint.1 lwpr.1
	ln atprint.1 isrv.8
	ln atprint.1 iwpr.1
	ln CAP.3 libcap.3
	ln CAP.3 libafpc.3
	ln CAP.3 libafp.3
	ln cvt2apple.1 cvt2cap.1
	ln atlook.1 atpinger.1
	ln atlook.1 atlooklws.1

#
# make only one text copy of the source files
#
txt:
	nroff -man CAP.3 > text/CAP.3
	nroff -man CAP.8 > text/CAP.8
	nroff -man UAB.8 > text/UAB.8
	nroff -man AUFS.1 > text/AUFS.1
	nroff -man AUFS.8 > text/AUFS.8
	nroff -man ash.1 > text/ash.1
	nroff -man atalk.local.5 > text/atalk.local.5
	nroff -man etalk.local.5 > text/etalk.local.5
	nroff -man atis.8 > text/atis.8
	nroff -man atlook.1 > text/atlook.1
	nroff -man atprint.1 > text/atprint.1
	nroff -man cvt2apple.1 > text/cvt2apple.1
	nroff -man getzones.1 > text/getzones.1
	nroff -man instappl.1 > text/instappl.1
	nroff -man lwsrv.8 > text/lwsrv.8
	nroff -man papif.8 > text/papif.8
	nroff -man snitch.1 > text/snitch.1
	nroff -man aarpd.8 > text/aarpd.8

clean:
	rm -f papof.8 tlw.1 lwpr.1 isrv.8 iwpr.1
	rm -f libcap.3 libafpc.3 libafp.3
	rm -f cvt2cap.1 atpinger.1 atlooklws.1 text/*

spotless: clean
	rm -f *.orig
