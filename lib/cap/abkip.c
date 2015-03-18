/*
 * $Author: djh $ $Date: 1996/06/18 10:48:17 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abkip.c,v 2.8 1996/06/18 10:48:17 djh Rel djh $
 * $Revision: 2.8 $
 *
 */

/*
 * abkip.c - KIP (UDP encapsulated DDP packets) network module, this
 * file provides the interface from DDP to the outside world as it
 * sees it.  It includes the DDP module "routeddp".
 *
 * We have two delivery mechanisms:
 *   abkip provides the "standard KIP" DDP interface to a hardware bridge
 *   (via a range of UDP ports mapped from socket numbers).
 *
 *   abmkip provides the interface from DDP to the UAB bridge using a
 *   modified form of KIP delivery (via port 903, standard KIP is unusable).
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 * in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 14, 1986    Schilit	Created.
 *  June 18, 1986    CCKim      Chuck's handler runs protocol
 *
 */

/*
 * The following list of exported routines is provided so you'll know what
 * have to be done to do another interface type (ethertalk, etc)
 *
 * EXPORTED ROUTINES:
 *
 *  OSErr GetNodeAddress(int *mynode, int *mynet)
 *     Return node addresses
 *  OSErr GetBridgeAddress(AddrBlock *addr)
 *     Return bridge addresses
 *  OSErr SetBridgeAddress(AddrBlock *addr)
 *     Set bridge addresses
 *  int abInit(boolean dispay_message)
 *     Initialize AppleTalk
 *  int abOpen(int *returnsocket, int wantsocket, struct iovec iov[], iovlen)
 *     Open a DDP socket
 *  int abClose(int socket)
 *     Close a DDP socket
 *  void abnet_cacheit(word srcNet, byte srcNode)
 *     Call in DDP protocol layer to tell the lower layer that
 *      the last packet that came in was from srcNet, srcNode
 *  int routeddp(struct iovec *iov, int iovlen)
 *     This is the DDP incursion.  With a full AppleTalk implementation,
 *     this would be part of DDP (abddp2).  This routes the DDP packet:
 *     normally would decide where to send and then send via lap, with KIP
 *     decides where and sends via UDP.
 *
 */

/*
 * Define the following if you have problems with arriving data being lost.
 * By trial and error, 6k seems like a good place to start.
 */
#define SOCK_BUF_SIZE	6 * 1024
/*
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef linux
#include <sys/uio.h>
#endif  /* linux */
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netat/appletalk.h>
#ifndef UAB_MKIP
#include <netat/abnbp.h>	/* for nbpNIS */
#endif UAB_MKIP
#include <netat/compat.h>	/* to cover difference between bsd systems */

/* imported network information */

extern word	this_net,	bridge_net,	nis_net,	async_net;
extern byte	this_node,	bridge_node,	nis_node;
extern char	this_zone[34],	async_zone[34];
extern struct in_addr bridge_addr;

#ifndef UAB_MKIP
short lap_proto = LAP_KIP;		/* standard KIP		*/
#else UAB_MKIP
short lap_proto = LAP_MKIP;		/* modified KIP (UAB)	*/
#endif UAB_MKIP

/*
 * Configuration defines
 *
 * NORECVMSG - no recvmsg
 * NOSENDMSG - no sendmsg
 * NEEDMSGHDR - no msghdr in sockets.h - define our own
 *
*/
#ifdef NORECVMSG
# ifndef NEEDNETBUF
#  define NEEDNETBUF
# endif
#endif
#ifdef NOSENDMSG
# ifndef NEEDNETBUF
#  define NEEDNETBUF
# endif
#endif

#ifdef NEEDNETBUF
#ifdef NEEDMSGHDR
struct msghdr {
  caddr_t msg_name;		/* name to send to */
  int msg_namelen;		/* size of name */
  struct iovec *msg_iov;	/* io vec */
  int msg_iovlen;		/* length */
  int msg_accrights;		/* dummy */
  int msg_accrightslen;
};
#endif NEEDMSGHDR
#endif NEEDNETBUF

struct msghdr msgr;
struct msghdr msgw;

/* for forwarding using ip_resolve */
private struct in_addr ipaddr_src; /* ip address */
private word ddp_srcnet;	/* ddp network part */
private byte ddp_srcnode;	/* ddp node part */

private struct sockaddr_in from_sin; /* network struct of last packet rec. */
private struct sockaddr_in abfsin; /* apple bus foreign socketaddr/internet */

private word portrange;		/* old or new (200|768)		*/
word rebport;			/* used to hold swabbed rebPort	*/
#define rebPort		902	/* 0x386 for normal KIP		*/
#define mrebPort	903	/* 0x387 for MKIP with UAB	*/

#ifdef UAB_MKIP
private int abfd;		/* outgoing fd for UAB		*/
struct in_addr xdesthost;	/* localhost address		*/
#endif UAB_MKIP

int read_buf_len;		/* dummy for absched.c		*/

import int ddp_protocol();	/* DDP protocol handler		*/
private int kip_get();		/* our KIP listener		*/
export DBUG dbug;		/* debug flags			*/

/* BUG: bind doesn't work when lsin is on the stack! */
private struct sockaddr_in lsin; /* local socketaddr/internet */
private int skt2fd[ddpMaxSkt+1]; /* translate socket to file descriptor */

#ifndef UAB_MKIP
private int ddpskt2udpport[ddpMaxWKS+1]; /* ddp "wks" socket to udp port */
#endif UAB_MKIP

private LAP laph;

/*
 * OSErr GetNodeAddress(int *myNode,*myNet)
 *
 * GetNodeAddress returns the net and node numbers for the current
 * host.
 *
 * N.B. - the myNet address is in net (htons) format.
 *
*/

export OSErr
GetNodeAddress(myNode,myNet)
int *myNode,*myNet;
{
  *myNode = this_node;
  *myNet = this_net;
  return(noErr);			   /* is ok */
}

#ifndef UAB_MKIP
/*
 * ddp to udp wks translate table
 *
*/
struct wks {
  char *name;			/* name of wks (as in /etc/services) */
  int ddpport;			/* ddp port to map from */
  int udpport;			/* udp port to map to */
  int notinited;		/* tried /etc/services? */
};

/* udpport is initially set to the old (768) values for compatiblity */
#define WKS_entry(name, ddpsock) {(name), (ddpsock), ddpWKSUnix+(ddpsock), 1}

private struct wks wks[] = {
  WKS_entry("at-rtmp",rtmpSkt),
  WKS_entry("at-nbp",nbpNIS),
  WKS_entry("at-echo",echoSkt),
  WKS_entry("at-zis",zipZIS),
  WKS_entry(NULL, 0)
};

/*
 * Translate ddp socket to UDP port: returns 0 if no mapping
 *
*/
word
ddp2ipskt(ddpskt)
int ddpskt;
{
  struct wks *wksp;
  struct servent *serv;

  if (ddpskt < 0 || ddpskt > ddpMaxSkt)
    return(0);

  if (ddpskt & ddpWKS)		/* 128+x means non-wks */
    return(ddpskt + ddpNWKSUnix);

#ifdef STEAL_PORTS
  if (ddpskt >= ddpEWKS)	/* 64-128 experimental */
    return(ddpskt + ddpNWKSUnix);
#endif STEAL_PORTS

  if (ddpskt2udpport[ddpskt] < 0) {
    for (wksp = wks; wksp->name != NULL; wksp++)
      if (wksp->ddpport == ddpskt) {
	if ((serv = getservbyname(wksp->name, "udp")) != NULL)
	  wksp->udpport = ntohs(serv->s_port); /* replace with new */
	if (dbug.db_ini)
	  fprintf(stderr, "port for %s is %d\n",wksp->name,wksp->udpport);
	endservent();
	ddpskt2udpport[ddpskt] = wksp->udpport;
	return(wksp->udpport);
      }
    ddpskt2udpport[ddpskt] = 0;
  }
  return(ddpskt2udpport[ddpskt]);
}

#else UAB_MKIP

word
ddp2ipskt(skt)
int skt;
{
#ifdef STEAL_PORTS
	if (skt >= ddpEWKS)
	  return(skt+ddpNWKSUnix);
#endif STEAL_PORTS
	return((skt&0x80) ? (skt+ddpNWKSUnix) : (skt+portrange));
}
#endif UAB_MKIP


/*
 * initialize
 *
*/
export
abInit(disp)
{
  int i;
  private word getPRange();

  for (i=0; i < ddpMaxSkt+1; i++) {
    skt2fd[i] = -1;		/* mark all these as unused */
  }
#ifndef UAB_MKIP
  for (i=0; i < ddpMaxWKS; i++)
    ddpskt2udpport[i] = -1;	/* mark unknown */
#endif UAB_MKIP

  bzero((char *)&msgr, sizeof(msgr));
  bzero((char *)&msgw, sizeof(msgw));

  rebport = htons(rebPort);	/* swap to netorder */
  portrange = getPRange();	/* which port range to use ? */
  init_fdlistening();

#ifdef UAB_MKIP
  /* no need to bind since we don't recv on this socket, just send... */
  if ((abfd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("abinit");
    return(abfd);
  }
# ifdef SOCK_BUF_SIZE
#  ifdef SO_RCVBUF
  { long len = SOCK_BUF_SIZE;
    if(setsockopt(abfd, SOL_SOCKET, SO_SNDBUF, (char *)&len, sizeof(long)) != 0)
      fprintf(stderr, "Couldn't set socket options\n");
  }
#  endif SO_RCVBUF
# endif SOCK_BUF_SIZE
#endif UAB_MKIP
  abfsin.sin_family = AF_INET;
  abfsin.sin_addr.s_addr = INADDR_ANY;
#ifndef UAB_MKIP
  openatalkdb(NULL); /* use default file, sets up variables this_* etc. */
#else UAB_MKIP
  openetalkdb(NULL); /* use default file, sets up variables this_* etc. */
  xdesthost.s_addr = inet_addr("127.0.0.1");
  if (xdesthost.s_addr == -1)
    return(-1);
#endif UAB_MKIP
  if (disp) {
      printf("abInit: [ddp: %3d.%02d, %d]",
	     ntohs(this_net)>>8, htons(this_net)&0xff, this_node);
      if (this_net != nis_net || this_node != nis_node)
	printf(", [NBP (atis) Server: %3d.%02d, %d]",
	       ntohs(nis_net)>>8, htons(nis_net)&0xff, nis_node);
      printf(" starting\n");
  }
  DDPInit();
  return(0);
}  

/*
 * int abOpen(int *skt,rskt, iov, iovlen)
 *
 * abOpen opens the ddp socket in "skt" or if "skt" is zero allocates
 * and opens a new socket.  Upon return "skt" contains the socket number
 * and the returned value is >=0 if no error, < 0 if error.
 *
 * iov should be an array of type "struct iov" of length at least
 * IOV_LAP_SIZE+1.  Levels after IOV_LAP_LVL are assume to filled.
 *
 */

int abOpen(skt,rskt, iov, iovlen)
int *skt;
int rskt;
struct iovec *iov;
int iovlen;
{
  int i,fd,err;
  int sktlimit = 128;
  word ipskt, ddp2ipskt();

  /* good enough for now */
  if (iov == NULL || iovlen < IOV_LAP_SIZE+1 || iovlen > IOV_READ_MAX)
    return(-1);

  if ((fd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("abopen");
    return(fd);
  }
#ifdef SOCK_BUF_SIZE
# ifdef SO_RCVBUF
  { long len = SOCK_BUF_SIZE;
    if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&len, sizeof(long)) != 0)
      fprintf(stderr, "Couldn't set socket options\n");
  }
# endif SO_RCVBUF
#endif SOCK_BUF_SIZE

  lsin.sin_family = AF_INET;
  lsin.sin_addr.s_addr = INADDR_ANY;

#ifdef STEAL_PORTS
  *skt = (rskt == 0 ? ddpEWKS : rskt); /* zero rskt is free choice */
  sktlimit += 64;
#else  STEAL_PORTS
  *skt = (rskt == 0 ? ddpWKS : rskt); /* zero rskt is free choice */
#endif STEAL_PORTS
  ipskt = ddp2ipskt(*skt);	/* translate into ip socket number */
  if (ipskt == 0)		/* bad socket? */
    return(ddpSktErr);
  for (i=0; i < sktlimit; i++,ipskt++,(*skt)++) {
    lsin.sin_port = htons(ipskt);
    if ((err = bind(fd, (struct sockaddr *)&lsin, sizeof(lsin))) == 0)
      break;
    if (rskt != 0)		/* bind failed and wanted exact? */
      return(err);		/* yes... */
  }
  if (err == 0 && i < sktlimit) {
    iov[IOV_LAP_LVL].iov_base = (caddr_t)&laph; /* remember this */
    iov[IOV_LAP_LVL].iov_len = lapSize; /* and this */
    fdlistener(fd, kip_get, iov, iovlen); /* remember for later */
    skt2fd[*skt] = fd;		/* remember file descriptor for socket */
    return(noErr);
  }
  perror("abopen bind");
  close(fd);
  return(err);
}

/*
 * close off socket opened by abOpen()
 *
*/
export int
abClose(skt)
int skt;
{
  int fd;

  if (skt < 0 || skt > ddpMaxSkt) {
    fprintf(stderr,"abClose: skt out of range\n");
    exit(0);
  }
  fd = skt2fd[skt];
  if (fd < 0)
    return(0);
  if (close(fd) != 0)
    perror("abClose");		/* some error... */
  fdunlisten(fd);
  skt2fd[skt] = -1;		/* mark as unused */
  return(0);
}

#ifdef NEEDNETBUF

/* buffer larger than maximum ddp pkt by far */
private char net_buffer[ddpMaxData*2];

#ifdef NOSENDMSG
/*
 * limited sendmsg - limits to sizeof(net_buffer)
 *
*/
sendmsg(fd, msg, flags)
int fd;
struct msghdr *msg;
int flags;
{
  int err;
  int i, pos, len;
  struct iovec *iov;

  iov = msg->msg_iov;
  for (i=0, pos=0; i < msg->msg_iovlen; i++, iov++) {
    len = iov->iov_len;
    if (len+pos > sizeof(net_buffer)) /* if overflow */
      len = sizeof(net_buffer)-pos; /* then limit */
    bcopy(iov->iov_base, net_buffer+pos, len);
    pos+= len;
    if (len != iov->iov_len)	/* we don't have any more space */
      break;
  }
  len = pos;
#ifndef UAB_MKIP
  if ((err=sendto(fd,net_buffer,len,0,msg->msg_name,msg->msg_namelen)) < 0)
#else UAB_MKIP
  if ((err=sendto(abfd,net_buffer,len,0,msg->msg_name,msg->msg_namelen)) < 0)
#endif UAB_MKIP
    perror("abwrite");
  return(err);
}
#endif NOSENDMSG

#ifdef NORECVMSG
recvmsg(fd, msg, flags)
int fd;
struct msghdr *msg;
int flags;
{
  int err;
  int i, pos, len, blen;
  struct iovec *iov;

  err = recvfrom(fd, net_buffer, sizeof(net_buffer), 0,
		 msg->msg_name, &msg->msg_namelen);
  if (err < 0)
    perror("abread");
  for (blen=err,pos=0,i=0,iov=msg->msg_iov; i < msg->msg_iovlen; i++, iov++) {
    len = min(iov->iov_len, blen);
    if ((pos + len) > sizeof(net_buffer)) /* if asking for too much */
      len = sizeof(net_buffer) - pos; /* then limit */
    bcopy(net_buffer+pos, iov->iov_base, len);
    pos += len;
    blen -= len;
    /* either no more room or no more data */
    if (len != iov->iov_len)
      break;
  }
  return(err);  
}
#endif NORECVMSG
#endif NEEDNETBUF

#ifdef notdef
abwrite(addr, skt, iov,iovlen)
struct in_addr addr;
unsigned short skt;
struct iovec *iov;
{
  int err;

  abfsin.sin_addr = addr;
  abfsin.sin_port = skt;
  msgw.msg_name = (caddr_t) &abfsin;
  msgw.msg_namelen = sizeof(abfsin);
  msgw.msg_iov = iov;
  msgw.msg_iovlen = iovlen;
  if ((err = sendmsg(abfd,&msgw,0)) < 0)
    perror("abwrite");
  return(err);  
}

abread(fd, iov, iovlen)
struct iovec *iov;
{
  int err;

  msgr.msg_name = (caddr_t) &from_sin;
  msgr.msg_namelen = sizeof(from_sin);
  msgr.msg_iov = iov;
  msgr.msg_iovlen = iovlen;
  if ((err = recvmsg(fd,&msgr,0)) < 0)
    perror("abread");
  return(err);  
}
#endif notdef

/*
 * the KIP listener 
 */

private int
kip_get(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  int len;
  LAP *lap;

  msgr.msg_name = (caddr_t) &from_sin;
  msgr.msg_namelen = sizeof(from_sin);
  msgr.msg_iov = iov;
  msgr.msg_iovlen = iovlen;
  if ((len = recvmsg(fd,&msgr,0)) < 0) {
    perror("abread");
    return(len);
  }
  if (iov->iov_len != lapSize) /* check */
    return(-1);
  lap = (LAP *)iov->iov_base;
  switch (lap->type) {
  case lapDDP:
    return(ddp_protocol(iov+1, iovlen-1, len-lapSize));
    break;
  default:
    return(-1);
  }
  /* return(-1); */
}

/*
 * This is the DDP/UDP interface 
 *
*/

/* srcNet and node of last incoming packet sent to DDP */
/* and valid */
export void
abnet_cacheit(srcNet, srcNode)
word srcNet;
byte srcNode;
{
  ddp_srcnet = srcNet;		/* remember where last packet came from */
  ddp_srcnode = srcNode;
  ipaddr_src.s_addr = (from_sin.sin_port == rebport) ? 0 :
    from_sin.sin_addr.s_addr;
}

private int
ip_resolve(ddpnet, ddpnode, iphost)
word ddpnet;
byte ddpnode;
struct in_addr *iphost;
{
  if (ipaddr_src.s_addr != 0 && ddpnet == ddp_srcnet && ddpnode == ddp_srcnode)
    iphost->s_addr = ipaddr_src.s_addr;
  else
    iphost->s_addr = bridge_addr.s_addr;
}

private LAP lap;

export int
routeddp(iov, iovlen)
struct iovec *iov;
int iovlen;
{
  word destskt, ddp2ipskt();
  struct in_addr desthost;
  DDP *ddp;
  int err;
  int fd;

  ddp = (DDP *)iov[IOV_DDP_LVL].iov_base; /* pick out ddp header */

  /* check ddp socket(s) for validity */
  if ( ddp->srcSkt == 0 || ddp->srcSkt == ddpMaxSkt ||
       ddp->dstSkt == 0 || ddp->dstSkt == ddpMaxSkt ||
       (fd = skt2fd[ddp->srcSkt]) == -1 )
    return(ddpSktErr);

  /* KIP routing code */
  /* establish dest socket */
  destskt = (word)htons(ddp2ipskt(ddp->dstSkt));
  if (destskt == 0)		/* byte swapped zero is still zero */
    return(ddpSktErr);
  /* resolve mapping */
  ip_resolve(ddp->dstNet, ddp->dstNode, &desthost);

  /* establish a dummy lap header */
  lap.type = lapDDP;
  lap.dst = ddp->dstNode;
  lap.src = this_node;
  iov[IOV_LAP_LVL].iov_base = (caddr_t) &lap; /* LAP header */
  iov[IOV_LAP_LVL].iov_len = lapSize; 	  /* size  */

  /* send through */
#ifndef UAB_MKIP
  abfsin.sin_addr = desthost;
  abfsin.sin_port = destskt;
#else UAB_MKIP
  abfsin.sin_addr = xdesthost;
  abfsin.sin_port = htons(mrebPort);
#endif UAB_MKIP
  msgw.msg_name = (caddr_t) &abfsin;
  msgw.msg_namelen = sizeof(abfsin);
  msgw.msg_iov = iov;
  msgw.msg_iovlen = iovlen;
#ifndef UAB_MKIP
  if ((err = sendmsg(fd,&msgw,0)) < 0)
#else UAB_MKIP
  if ((err = sendmsg(abfd,&msgw,0)) < 0)
#endif UAB_MKIP
    perror("abwrite");
  return(err);  
}

private word
getPRange()
{
	struct servent *getservbyname();

	if(getservbyname("at-rtmp", "udp") == NULL)
		return(ddpWKSUnix);	/* 768 */
	else
		return(ddpOWKSUnix);	/* 200 */
}

/*
 * avoid using global variables
 *
 */

OSErr
GetBridgeAddress(addr)
AddrBlock *addr;
{
  addr->net = bridge_net;
  addr->node = bridge_node;
  return(noErr);
}

OSErr
SetBridgeAddress(addr)
AddrBlock *addr;
{
  bridge_net = addr->net;
  bridge_node = addr->node;
  return(noErr);
}

#ifdef linux
/*
 * dummy routines for loader
 *
 */
void
rtmp_timer()
{
}
void
rtmp_data()
{
}
void
rtmp_release()
{
}
#endif /* linux */
