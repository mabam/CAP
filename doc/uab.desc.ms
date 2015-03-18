.\" nroff -ms
.TL
Unix AppleTalk Bridge
.AB
This document describes a
Unix based AppleTalk Bridge 
.I (UAB)
designed to work on a variety of
host unix systems.  UAB also provides for mechanisms to deliver
packets internally.
.AE
.SH
INTRODUCTION
.LP
The Unix AppleTalk
Bridge (UAB) program allows certain unix systems to act as AppleTalk
Bridges.  UAB consists of a number of layers that can have multiple
implementations.  UAB can be functionally divided into two parts.  The
first is the actual AppleTalk Bridge implementation and the second are
the routines that define particular "Link Access Protocols" (aka
"hardware" delivery methods e.g. EtherTalk).  UAB also supports an
internal demultiplexing that allows
packets delivered to the UAB node to be delivered to other processes
within that system.  
.PP
Currently, UAB runs on Ultrix 1.2 (and beyond) and SunOS 4.0 and
supports EtherTalk.  Unfortunately, with the current definition of
KIP's UDP encapsulation and delivery rules, it is not feasible to
implement KIP.
The only internal packet
delivery mechanism defined is a modified version of KIP's UDP
encapsulation (modified-KIP) that uses a different UDP port range over
the internal
loopback; thus CAP programs must be relinked with a different low
level delivery mechanism to work with UAB.  Note that all packets for
these programs are sent and received through the UAB process.
Since UAB does not understand KIP, 
it is necessary to have an AppleTalk Bridge that
understands both KIP encapsulation and EtherTalk before KIP based
"systems" (e.g. programs compiled with CAP, bridges that only speak
KIP on the ethernet interface--revisions of KIP before 2/88, etc) can
work with UAB modified-KIP based programs.
.SH
Definitions
.LP
.IP
An 
.I interface
defines the
underlying delivery protocol.  The only delivery protocols supported at
the present time are EtherTalk (Phase 1) and Asynchronous AppleTalk.
.IP
A
.I port
abstracts an interface into a workable DDP entity.  DDP level
functions deal with ports rather than interfaces.  A port carries
information such as interface input/output mechanisms, ddp network
numbers, etc.
.IP
The
.I port manager
is a set of routines that handle ports.  Only the port manager
directly manipulates a port.  Both the lap layer and the ddp layer
call the port manager.
.IP
A
.I node
is a DDP/RTMP concept that defines nodes in a way that should contain
all the various LAP definitions.  In particular, a node is defined as the
tuple <# of bits, bits> where the number of bits can be between 1 and
255.  This is more general than the original LocalTalk LAP definition
which defines a node as 8 bits.
.SH
AppleTalk Bridge
.LP
UAB builds upon the concepts of 
.I interface,
.I port,
and
.I node
to separate itself from the underlying delivery mechanism.
As an AppleTalk bridge, it provides full RTMP and ZIP services as
defined in Inside AppleTalk.  In addition, it provides the NBP Bridge
Lookup services.
.PP
As all AppleTalk bridges, it is also a node on the various AppleTalk
networks to which it is directly connected.  Packets directed to its node
number (e.g. that aren't supposed to be forwarded) and which are not
directly related to bridge management (RTMP, ZIP, and NBP BrLk) are
handled in two ways.  The first provides a simple "port" wide
services: when the socket is "opened" it is opened for all known and
future ports.  The only one currently defined is DDP ECHO.  In the
future, it may be necessary or advisable to add other NBP services
such as outgoing lookup, internal name management, etc; however, that
has not yet been done.  For "unopened" sockets the packets are
sent to a "demultiplexer".  (NBP is considered "partially" opened for
our purposes-the handler only picks out the bridge lookup packets).
.PP
For historical reasons, we don't consider UAB to be directly connected to
the asynchronous appletalk network (thus no node number). This is because
we are really acting as a 'half-bridge' in conjunction with the async
client process. This will probably change in the future.
.PP
The demultiplexor/multiplexor is supposed to solve the problems of
sending to other
processes on the system (if the system is processes based like unix).
There are a number of requirements associated with the demultiplexor under
Unix.  First, the demultiplexor delivery mechanism does not have to be 
reliable since it is delivering ddp packets: since DDP is considered
unreliable, there must be higher level policies that ensure delivery.
Second, the demultiplexing end must be able to send DDP packets to the
correct processes
in a way that the processes can decode what the DDP socket number was.
For example, with UDP, it is simple enough to define a port range and
send the packet to a particular port: if a program is listening on it,
it will receive it and know exactly which socket (based upon a
mapping) it was meant for.  With UDP, the process knows that
a stronger condition holds
because the processes knows apriori what the DDP socket number is and
can do different reads based upon this (e.g. customized io vectors).
Third, the multiplexing end must be able to know the DDP socket that the
process is sending to.  With UDP, the best way is to have the
multiplexing end listen to a single socket: the recv call can return
the source port number (which then can be translated in to the DDP
socket).  Fourth, both sides must be relatively sure of the
"trustworthiness" of the packets: e.g. one must not be able to have
"untrustworthy" agents intercept or inject packets undetectably.
Fifth, it is necessary that the mechanism work within reasonable
implementation boundaries.  For instances, a mechanism that required
the full DDP range of 254 sockets to be opened (e.g have that many
file descriptors/open files) would not fit within
those requirements upon most if not all of today's unix systems.
.PP
The only mechanism defined so far that allows these requirements to be
fulfilled in a reasonable fashion is the modified KIP scheme, but even
there, the security requirement has been loosened.  The primary reason
that it works well is that one can define a single point of contact on
the demux/mux process that goes to many points (on many processes)
within the constraints mentioned above.  Basically,
it's real easy to use UDP because it allows one to use the
kernel to do the fan-in and fan-out
functionality.
.SH
Link Access Protocols/Interfaces
.LP
.I UAB
uses DDP ports and interfaces to abstract the bridge functionality
from the delivery mechanisms.  The level of separation is at the ddp
layer.  As defined before, an interface is a basic description of a
particular delivery protocol such as EtherTalk (ELAP, implemented) or
LocalTalk (ALAP, not implemented at present).
When initialized, an interface sends information to the ddp port
manager that defines its basic operating characteristics.
.PP
The only delivery protocols defined at present are the EtherTalk Link
Access Protocol (aka EtherTalk) and Asynchronous AppleTalk.  Other delivery
protocols may be
defined for other systems with particular hardware (e.g. Mac II
running A/UX with a localtalk card) in the future or by other parties.
The SunOS and Ultrix ELAPs are implemented on top of a specialized
facility available on both (in different forms) that allow "opening"
an Ethernet protocol: all packets addressed to that host with a
particular protocol type are delivered to the UAB process.  No or very
little processing is done by the kernel.  To complete these ELAP, AARP
is also implemented.  The only protocol interface library
implemented under SunOS is based upon the streams version of the
Network Interface Tap first made available in SunOS 4.0.  The protocol
interface library for Ultrix is based on the Data Link Inteface
facility (c.f. DECNET documentation).
.PP
The ELAP implementation is abstracted from the actual "ethernet
protocol" facilities by the use of a set of "protocol interface
routines" (poor choice of names, but was made a long time ago when the
routines were meant for a far different purpose).
