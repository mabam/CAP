.\" formatted output is in install.doc
.TL
CAP installation procedures
.AB
This document gives a step by step approach to installation of the
Columbia AppleTalk Package for UNIX.
.sp
.AE
.SH
SEE ALSO
.LP
Notes and cookbooks in cap60/doc and UNIX manual entries in cap60/man.
.SH
Notes on CAP with EtherTalk
.LP
.nh
This file describes how to install CAP in its "traditional" default
configuration IPTalk - which is  based on Appletalk protocols encapsulated in
UDP/IP packets - and
configurations that support communications via EtherTalk Phase 1 or Phase
2 - Native EtherTalk, Kernel AppleTalk or the UNIX AppleTalk
Bridge, UAB (both Phase 1 only, obsolete) or via the UNIX AppleTalk Router,
UAR.
.sp
Native EtherTalk mode is available on a limited range
of hosts - those that support the Stanford ENET Packet Filter
model - currently SUN workstations, NetBSD, FreeBSD and BSDi, NeXT and
DEC OSF/1 or ULTRIX hosts only.
.sp
UAR will run on SGI IRIX, Sony NEWS, DEC OSF/1 or ULTRIX, SUN SunOS
and Solaris, IBM AIX and HP-UX workstations.  IPTalk mode is supported
on all of the above and more and is easily ported to other platforms.
.sp
.IP
Note: UAR is not bundled with CAP, see the CAP FAQ for more information,
the most recent copy of the FAQ
can always be obtained via FTP from munnari.OZ.AU in mac/CAP.faq.
.sp
.LP
The setup for Native EtherTalk, Kernel AppleTalk, UAB/UAR is performed
from within the
Configure script (but only if Configure determines that the host
is suitable) by modifying the CAP libraries as necessary.
Refer to the README files in the support/uab or
support/ethertalk directories for more information.
.sp
The choice of whether to use IPTalk,
Native EtherTalk/Kernel AppleTalk or UAB/UAR depends on your network
setup. If you have a majority of Macintoshes on LocalTalk connected to
an IPTalk gateway (see "Prerequisites" below) then use CAP in IPTalk mode.
If your Macintoshes are mostly connected via EtherNet and you
have other EtherTalk
gateways (Ciscos for example) then use CAP with Native EtherTalk or Kernel
AppleTalk. If you have no other gateways, or need to bridge two
EtherTalk networks, or want to run CAP with EtherTalk on hosts that don't
have Native EtherTalk support then use UAR.  On a SUN running SunOS you may
choose to use
the native NIT interface or install kernel modifications to run the 'ENET'
driver.
.SH
Prerequistes
.LP
To use CAP with IPTalk you must have a hardware bridge
that has the ability to gateway IPTalk packets to and from EtherTalk and/or
LocalTalk. Suitable candidates are the Webster MultiPort GateWay,
Cisco Router, Shiva FastPath and Cayman GatorBox. The code originally
written to handle
the IPTalk gateway function on the FastPath is often known as KIP (Kinetics
IP).
.sp
The file /etc/atalk.local (see atalk.local.5 in
the cap60/man directory) contains routing information for the static IPTalk
network and node numbers. It is not necessary to create this file
if you are using Native EtherTalk or UAR drivers and its presence may cause
problems if you are using Native EtherTalk without a gateway box.
.sp
Before you start here, you should have read the "README" and "NOTES"
files in the cap60 directory.  The README file gives a very basic overview
of CAP necessary to understand the following.
.sp
The installation NOTES file tries to point out places where you might
want to redefine things a little, explains configurations, and points
out machine dependencies.
.sp
A simple cookbook approach to CAP installation and printing is available in
cap60/doc/print.cookbook.
.SH
Review
.LP
or the the "before you do anything else" step.  If you had a previous
distribution of CAP (eg: 5.00), you should consider
whether it is important enough to dump it to tape and put it away for
safe keeping or not.  There are often considerable changes between
distributions.  You should make sure you keep around modifications,
patches, etc. even if you think they should be installed in the new
distribution just in case they are not.  You don't have to do this,
but if you are paranoid about problems occurring, you will (and
probably would without this prompting).
.SH
UDP Ports
.LP
Versions of IPTalk/UDP gateway code prior to April, 1988 had
"well known" AppleTalk DDP sockets mapped to priviledged 
UDP/IP ports starting at
768 (so the NBP socket 2 was UDP port 770, ECHO socket 4 was 772 etc).
.sp
In April, 1988, the NIC assigned a range
of UDP ports starting at 200 for the defined DDP services and assigned
the names "at-rtmp", "at-nbp", "at-echo", "at-zis" (not "at-zip" as
documented in some versions of KIP).  In addition,
ports were allocated where there were holes or unused sockets between
at-rtmp and at-zis.  CAP Release 6.0
and above dynamically decides which mappings
to use by doing "getservbyname()" calls based upon those names and using
the UDP port number returned.  getservbyname() normally searches the file
/etc/services but the information may also be cached by a NIS (aka Yellow
Pages) server.
If the requested service name does not exist, then the old
mappings (based on 768) are used.
.sp
Entries to be put in /etc/services to utilise the NIC range are
.sp
.nf
at-rtmp 	201/udp
at-nbp  	202/udp
at-echo 	204/udp
at-zis  	206/udp
.fi
.sp
[Important Note: In some circumstances, ports in the 768
range can be "stolen" by the portmapper. The symptom of this is that atis
complains that the NBP or ECHO ports are not available. There are two
solutions, start atis before portmap or change to the NIC assigned ports].
.sp
It should be clear from the above that you must make sure
that any mappings defined in /etc/services do not conflict
with the mappings used by the IPTalk gateway you are using.
Refer to the gateway documentation for details on how to alter the
port range.
.SH
Preparation
.LP
or the "make sure
things are okay first" step.
Some of the steps require
that you have access to directories that are often only accessible by
a systems person.  We'll try to identify them below.  In addition,
some of the servers can only be run from the "root" id.  We'll try to
identify these too.
.SH
[1] Get CAP Files.
.LP
CAP is distributed as a compressed tar file (see below for a list of
anonymous FTP sites). Since you are reading this information it could
be assumed that you have everything unpacked already. For completeness,
however, the commands to unpack the distribution are
.sp
.nf
		% compress \-d cap60.pl100.tar.Z
		% tar xf cap60.pl100.tar
.fi
.sp
This creates a 'cap60' directory containing the
patch level 100 CAP distribution.
CAP updates and bug fixes are
normally distributed as 'patch' files. These are simply context diffs
of the original and the new files (see the UNIX command diff(1)).
To apply the patches
with a minimum of effort, it is recommended that you use the 'patch'
program written by Larry Wall. This can be obtained from sites that
archive postings from the newsgroup comp.sources.unix in the directory
volume7/patch2. It is important to ensure that any patches for
the 'patch' program sources are applied, some CAP 6.0 patches can fail if the
unmodified patch code is used.
.sp
CAP 6.0 patches are sent to the newsgroup comp.protocols.appletalk,
and, for sites with FTP access, patches for CAP 6.0 are
archived on munnari.OZ.AU in the directory mac/cap.patches, on rutgers.EDU
in src/cap60.patches and gatekeeper.DEC.COM in pub/net/cap/cap.patches.
The current patch level is recorded in the cap60/README file.
New patches are applied from the top level directory with the command
.sp
.nf
		% patch \-p < cap60.patches/cap60.patchNNN
.fi
.sp
where NNN is the patch number. The \-p option tells patch to obtain
information about which file to alter from the patch header. If you attempt
to apply a patch more than once, patch will enquire about "a reversed or
previously applied patch", answering yes to this will remove the patch,
leaving the original file, this is not good ...
.sp
In the following, it is assumed that you are in the top level
directory (e.g. so an ls(1) shows up samples, contrib, etc, lib,
applications, etc.)
.SH
[2] Configuration
.sp
.nf
		% ./Configure
.fi
.sp
.LP
You should run Configure to establish the baseline setup for your
system. The Configure script has inbuilt support for various hardware
platforms, making suitable changes for the vagaries of each.
Configure also
checks the byte ordering on the host machine and if necessary ensures
that the code is built correctly for "little-endian" machines.
Answering Configure questions with the default answers is
normally enough to build an IPTalk version
of CAP with a basic feature set, it is also possible to include
an extra set of features for the various CAP components, see the
files CAP60.README and m4.features for more details.
.sp
One Configure option allows the CAP files to be built and used within
a single directory hierarchy. This is useful for testing or evaluation
(see below).
.SH
[3] Make sure things will end up where you want them.
.LP
Figure out where you want everything.  The assumptions are: 
.sp
.nf
	cap libraries - /usr/local/lib - as a unix archive file
	cap programs - /usr/local/cap - as programs
	cap servers - /usr/local/cap - as programs
	cap config files - /usr/local/lib/cap or /etc
.fi
.sp
If you want things elsewhere, edit the file m4.setup and then run gen.makes.
.sp
By the way, /etc may be a bad place to put things.  At Columbia,
everything is generally put in /usr/local/lib/cap instead of /etc.
(Warning: if you want cap.printers in /etc, it is not enough to
redefine the "etcdir".  You must also uncomment the definition that
allows an alternate location for cap.printers. This also applies to
the file atalk.local. You have to change the definition in the makefile
in lib/cap).
.sp
This is also a good point to think about reconfiguring papif, lwsrv,
etc. to do what you want.  If you don't know yet, then don't worry,
you can recompile after you verify basic functionality.
.SH
[4] Re-Generate system makefiles
.sp
.nf
		% ./gen.makes
.fi
.sp
.LP
If you have edited the files m4.setup or m4.features, run
gen.makes to (re)generate your baseline makefiles.
There are "Makefile"s included,
but they are included for systems without the m4 preprocessor
and shouldn't be used unless absolutely necessary. Some machines
will generate a 'make' warning message about the presence of both
makefile and Makefile. You can ignore these warnings or, after the
first gen.makes,
run 'make spotless' which removes both makefile versions.  At this point it
is necessary to run gen.makes again to continue with the CAP build.
.SH
[5] Install header files.
.sp
.nf
		% make include
.fi
.sp
.LP
The simplest method is to type "make include".
This will create /usr/include/netat and copy the contents of cap60/netat
to /usr/include/netat.
.B Alternatively,
you could symbolically link
/usr/include/netat to cap60/netat with the command
.sp
.nf
		% ln \-s cap60/netat /usr/include/netat
.fi
.sp
.SH
[6] Testing
.LP
You can test things out without installing programs in system directories by
answering 'y' to the Configure question relating to installing CAP in a
single directory tree.
.sp
This will put:
.nf
	libraries in cap60/lib
	programs and servers in cap60/bin
	etc stuff in cap60/etc
.fi
.sp
In this case, it is not necessary to install the header files.
.LP
.SH
[7] Build Libraries.
.sp
.nf
		% make libsmade
.fi
.sp
.LP
Type: "make libsmade" from the top level cap directory.
This should build the cap libraries.
If this step doesn't work, then:
.sp
.nf
	(a) you didn't get the distribution correctly
	(b) you didn't install the header files correctly
	(c) you didn't Configure and generate the makefiles correctly
	(d) or worst of all the libraries don't work on your machine
.fi
.sp
If the problem is (d), you can refer to "PORTING" for some help.
.SH
[8] Installing libraries and building sample programs.
.sp
.nf
		% make programs
.fi
.sp
.LP
After building the libraries, you use "make programs" to install the
libraries into a readily accessible place (usually /usr/local/lib) and
compile the samples, applications and contributed programs.
.SH
[9] Installation
.sp
.nf
		% make install
.fi
.sp
.LP
You can install the various programs into their final destinations by
typing "make install", but you might want to test them first.
.sp
Change directory to cap60/etc.  Look at start-cap-servers and figure out if this
is what you want - modify it if necessary.  If you don't know
what you want it to be, don't worry - you can do it later, but make
sure you don't remove the line with atis in it.  
.sp
You should then copy start-cap-servers to /usr/local/lib/cap (or other
desired location).
.sp
At this point, primary installation is done.
.SH
[10] Verification.
.LP
.sp
If you are using IPTalk,
you should have already tested the gateway software before
proceeding further and
you must have /etc/atalk.local
installed (see the file man/atalk.local.5 for details of
the file contents). Skip from here to step A. below.
.sp
If you are
using Native EtherTalk, then you should read the file
cap60/support/ethertalk/README and then run aarpd with
suitable interface and zone name arguments. Follow this by
atis which determines the AppleTalk network numbers from the network (see
step C below for atis verification procedure).
.sp
If you are using Kernel AppleTalk, ensure that the necessary
code is installed in the kernel (comes installed with Linux),
read cap60/support/capd/README,
run capd with suitable interface and zone name arguments. Follow
this with atis, as above.
.sp
If you are using UAB, read the file cap60/support/uab/README
and the files to which it refers. Ensure that you edit and
install the 'bridge_desc' file.
.sp
If you are using UAR, see the README file supplied with the distribution.
The CAP FAQ also contains information about where to get UAR and its use
with CAP.
.sp
At this point, the /etc/etalk.local file should look
similar to the following:
.sp
.nf
	#
	# EtherTalk dynamic configuration data
	#
	# Last update:	Sat Dec 29 14:46:45 1990
	#
	# Generated by Native EtherTalk (Phase 1)
	#
	thisNet     	99.116
	thisNode    	69
	thisZone    	"unimelb-CompSci"
	bridgeNet   	99.116
	bridgeNode  	69
	bridgeIP    	127.0.0.1
	nisNet      	99.116
	nisNode     	69
	asyncNet    	170.170
	asyncZone   	"unimelb-Async"
.fi
.sp
If things look OK to this point, proceed with the rest of
the verification steps.
.sp
.LP
A.  Change directory to cap60/samples and run 
.I getzones.
You should see something similar to the following:
.sp
.nf
	% getzones
	unimelb-CivEng
	unimelb-AgEng
	unimelb-Ed1888
	unimelb-History
	unimelb-FineArts
	unimelb-Research
	unimelb-Classic
	unimelb-Language
	unimelb-Chemistry
	unimelb-Registrar
	...
.fi
.sp
If getzones prints a warning message or fails with a -1096 error then the
problem is usually that there is no AppleTalk router on the local network
and therefore no zones available. In this situation you should be using "*"
as the zone name argument for aarpd.
.sp
The next program to try is
.I atlook.
If everything is okay, you
should see some appletalk entities.  If you installed the IPTalk gateway code
(aka KIP) correctly before, then you should minimially see the IPGATEWAY entry.
[Should you not see the IPGATEWAY entry, then the assumption is that
the UDP code isn't functional or you are not using IPTalk].  For example:
.nf
	% atlook
	abInit: [ddp:  55.32, 130] starting
	01 - 128.59.35.40:IPGATEWAY@* [Net: 58.01 Node:220 Skt: 72]
	...
	$ 
.fi
Another really simple program to try is
.I "atlooklws"
which will look for
and query LaserWriters.
.LP
If 
.I atlook
doesn't work, then:
.IP
(a) you may not have installed the IPTalk gateway correctly

(b) you may not have installed atalk.local in the place
.I atlook
expects it, although it should complain if the /etc/atalk.local file is not
there or is incorrectly formatted

(c) if
.I atlook
coredumps, then something is really wrong.  you are
probably on a machine that CAP doesn't work on.
.LP
B.  If you have a LaserWriter and you see it in 
.I atlook,
then another
level of testing is to run the sample program 
.I tlw
(just type "tlw <laserwriter name>"). 
.LP
C.  To test the server functionality, as "root" run 
.I "atis".
To see if
atis is running properly, run "atistest" from the samples directory.
.sp
.nf
	% atistest
	CAP distribution 6.00 using UDP encapsulation, January 1991
	Copyright (c) 1986,1987,1988 by The Trustees of Columbia
	University in the City of New York

	abInit: [ddp:  93.38, 1] starting
	debugging NBP
	Registering "atis test:testing@*"
	NBP SndNBP: sending
	NBP nbp_timeout: 4 tick timeout on -134218532, 3 remain
	NBP SndNBP: sending
	NBP nbp_timeout: 4 tick timeout on -134218532, 2 remain
	NBP SndNBP: sending
	NBP nbp_timeout: 4 tick timeout on -134218532, 1 remain
	NBP SndNBP: sending
	NBP nbp_timeout: 4 tick timeout on -134218532, 0 remain
	NBP SndNBP: sending
	NBP status done: found -134218532
	Okay
.fi
.sp
If it signals proper operation with an "Okay" message, then you can
confirm things again (odds are everything is okay) by:
.IP
(a) running 
.I atlook,
there should be an entry like
.sp
.nf
1 - atis test:testing@*                      [Net: 93.38  Node:  1 Skt:143]
.fi
.sp
(b) typing "atis dump" (as root) and looking at /usr/tmp/nis.db
you should see (apart from some comments at the top of the file)
.sp
.nf
net=93.38, node=1, skt=143, enum=7, !9!7!1! ? atis test:testing@*
.fi
.sp
(the nis.db entry is the information that atis returns when an NBP name
lookup is performed, such as by atlook).
.LP
To get rid of the "atis test" entry, simply edit /usr/tmp/nis.db and
delete the line, then type "atis reload" (as root).  Alternatively,
simply kill the running atis process.  A useful maintenance tool is
to alias 'nised' to 'atis dump; vi /usr/tmp/nis.db; atis reload'.

The most common problem in getting atis to run is failing to setup IPTalk
on the gateway, the atalk.local file
(or, if used, atalkatab) properly. The UNIX/CAP host and the gateway may
also not agree on the network broadcast address, this will affect NBP lookups.

.LP
D.  After verification, you will want things to start up
automatically, edit (or get a superuser to edit) /etc/rc.local to run the
following lines (or an equivalent):
.sp
.nf
if [ \-f /usr/local/lib/cap/start-cap-servers ]; then
   /usr/local/lib/cap/start-cap-servers & echo \-n " CAP " > /dev/console
fi
.fi
.sp
.SH
[11] Cleaning Up.
.sp
.nf
		% make clean
.fi
.sp
.LP
After final installation, you can do a 'make clean'. This removes all
the compiled binaries and object files thus saving on disc space.
.SH
CHANGES FROM RUTGERS CAP
.LP
There are some important changes to note if you are already using Native
EtherTalk from the original Rutgers distribution. Shared memory is no longer
used, the file /etc/cap.ether.shared is thus not required. The modifications
needed to the file /etc/atalk.local to select a zone and an ethernet interface
are not needed as CAP now uses the file /etc/etalk.local for both Native
EtherTalk and UAB. See the man files atalk.local.5 and etalk.local.5 for more
details. For Native EtherTalk, you may use etalk.local to seed the values for
"interface" and "thisZone" but the preferred method is to supply this
information as arguments to aarpd, as in
.sp
.nf
		aarpd  ie0  myZone
.fi
