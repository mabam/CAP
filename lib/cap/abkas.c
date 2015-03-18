/*
 * $Author: djh $ $Date: 1996/09/06 12:06:32 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abkas.c,v 2.4 1996/09/06 12:06:32 djh Rel djh $
 * $Revision: 2.4 $
 */

/*
 * abkas.c - use UNIX Kernel AppleTalk Support via AF_APPLETALK sockets
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Copyright (c) 1992 The University of Melbourne.
 *
 * Edit History:
 *
 *  February 1992	djh	Created
 *  May, 1996		djh	Updated
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
 *  OSErr SetNetRange(u_short range_start, u_short range_end)
 *     Set Network Range (Phase 2)
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

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef	linux
#include <linux/route.h>
#include <linux/sockios.h>
#else	/* linux */
#include <sys/uio.h>
#include <sys/sockio.h>
#endif	/* linux */
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <net/if.h>
#include <netdb.h>
#include <netat/appletalk.h>
#include <netat/compat.h>	/* to cover difference between bsd systems */
#include <netat/abnbp.h>

#define ATADDR_ANYNET	(u_short)0x0000
#define ATADDR_ANYNODE	(u_char)0x00
#define ATADDR_ANYPORT	(u_char)0x00
#define ATADDR_BCAST	(u_char)0xff

#ifdef s_net
#undef s_net
#endif s_net

/* AppleTalk Address */

struct at_addr {
  u_short s_net;
  u_char s_node;
};

/* Socket Address for AppleTalk */

struct sockaddr_at {
  short sat_family;
  u_char sat_port;
  struct at_addr sat_addr;
  char sat_zero[8];
};

/* Range info for extended networks */

struct net_range {
  u_char phase;
  u_short net_lo;
  u_short net_hi;
};

/* imported network information */

extern word	this_net,	bridge_net,	nis_net,	async_net;
extern byte	this_node,	bridge_node,	nis_node;
extern char	this_zone[34],	async_zone[34],	interface[50];

extern struct in_addr bridge_addr;

#ifdef PHASE2
extern word net_range_start, net_range_end;
#endif PHASE2

short lap_proto = LAP_KERNEL;	/* kernel appletalk support */

/*
 * Configuration defines
 *
 * NORECVMSG - no recvmsg()
 * NOSENDMSG - no sendmsg()
 * MEEDMSGHDR - no msghdr in sockets.h - define our own
 *
 */

#ifdef NORECVMSG
# ifndef NEEDNETBUF
#  define NEEDNETBUF
# endif  NEEDNETBUF
#endif NORECVMSG
#ifdef NOSENDMSG
# ifndef NEEDNETBUF
#  define NEEDNETBUF
# endif  NEEDNETBUF
#endif NOSENDMSG

import int ddp_protocol();	/* DDP protocol handler */

private int ddp_get();		/* kernel packet listener */
private int kip_get();		/* loopback packet listener */

private struct sockaddr_at addr; /* local appletalk socket addr */
private struct sockaddr_at raddr;/* input appletalk socket addr */
private struct sockaddr_in from; /* loopback receive address */
private struct sockaddr_in sndl; /* send packet to loopback */

private int skt2fd[ddpMaxSkt+1]; /* translate socket to file descriptor */
private int skt2kip[ddpMaxSkt+1];/* loopback socket to file descriptor */
private int ddpskt2udpport[ddpMaxWKS+1]; /* ddp "wks" skt to udp port */

private LAP laph;

export DBUG dbug;

/*
 * initialize
 *
 */

export
abInit(disp)
int disp;
{
  int i;
  static int here_before = 0;

  for (i = 0; i < ddpMaxSkt+1; i++) {
    skt2fd[i] = -1;		/* mark all these as unused */
    skt2kip[i] = -1;		/* mark all these as unused */
  }

  for (i = 0; i < ddpMaxWKS+1; i++)
    ddpskt2udpport[i] = -1;	/* mark as unused */

  if (!here_before) {
    openetalkdb(NULL);		/* set up variables */
    here_before = 1;
  }

  init_fdlistening();

  if (disp) {
      printf("abInit: [ddp: %3d.%02d, %d]",
	     ntohs(this_net) >> 8, ntohs(this_net) & 0xff, this_node);
      if (this_net != nis_net || this_node != nis_node)
	printf(", [NBP (atis) Server: %3d.%02d, %d]",
	       ntohs(nis_net) >> 8, ntohs(nis_net) & 0xff, nis_node);
      if (bridge_node)
	printf(", [GW: %3d.%02d, %d]",
	       ntohs(this_net) >> 8, ntohs(this_net) & 0xff, bridge_node);
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
  int len, fd, err;

  if (iov == NULL || iovlen < IOV_LAP_SIZE+1 || iovlen > IOV_READ_MAX)
    return(-1);

  if ((fd = socket(AF_APPLETALK, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return(fd);
  }

  bzero(&addr, sizeof(struct sockaddr_at));
  addr.sat_family = AF_APPLETALK;
  addr.sat_addr.s_net = htons(ATADDR_ANYNET);
  addr.sat_addr.s_node = ATADDR_ANYNODE;
  addr.sat_port = (rskt == 0) ? ATADDR_ANYPORT : rskt;

  if ((err = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_at))) < 0) {
    perror("bind()");
    close(fd);
    return(err);
  }

  len = sizeof(struct sockaddr_at);
  if ((err = getsockname(fd, (struct sockaddr *)&addr, &len)) < 0) {
    perror("getsockname()");
    close(fd);
    return(err);
  }

  *skt = addr.sat_port;

  iov[IOV_LAP_LVL].iov_base = (caddr_t)&laph;	/* remember this */
  iov[IOV_LAP_LVL].iov_len = lapSize;		/* and this */

  fdlistener(fd, ddp_get, iov, iovlen);		/* remember for later */

  skt2fd[*skt] = fd;		/* remember file descriptor for socket */

  /* open KIP loopback socket */

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    fdunlisten(skt2fd[*skt]);
    close(skt2fd[*skt]);
    return(fd);
  }

  sndl.sin_family = AF_INET;
  sndl.sin_addr.s_addr = INADDR_ANY;
  if ((sndl.sin_port = htons(ddp2ipskt(*skt))) == 0) {
    fdunlisten(skt2fd[*skt]);
    close(skt2fd[*skt]);
    close(fd);
    return(ddpSktErr);
  }

  if ((err = bind(fd, (struct sockaddr *)&sndl, sizeof(struct sockaddr))) < 0) {
    perror("bind()");
    fdunlisten(skt2fd[*skt]);
    close(skt2fd[*skt]);
    close(fd);
    return(ddpSktErr);
  }

  fdlistener(fd, kip_get, iov, iovlen);

  skt2kip[*skt] = fd;

  return(noErr);
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

  /* close AppleTalk skt */
  if ((fd = skt2fd[skt]) >= 0) {
    fdunlisten(fd);
    close(fd);
  }

  /* close KIP loopback */
  if ((fd = skt2kip[skt]) >= 0) {
    fdunlisten(fd);
    close(fd);
  }
    
  skt2fd[skt] = -1;		/* mark as unused */
  skt2kip[skt] = -1;		/* mark as unused */

  return(0);
}

int
abnet_cacheit()
{
}

/*
 * get a packet from the network
 *
 * construct appropriate DDP header.
 *
 */

private int
ddp_get(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  struct msghdr msg;
  int len, err;
  LAP *lap;
  DDP *ddp;

  /* want DDP type in byte 0 */
  iov[IOV_DDP_LVL].iov_len = 1;

#ifdef linux
  bzero((char *)&msg, sizeof(msg));
#endif linux
  msg.msg_name = (caddr_t) &raddr;
  msg.msg_namelen = sizeof(struct sockaddr_at);
  msg.msg_iov = iov+1;
  msg.msg_iovlen = iovlen-1;
#ifndef linux
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
#endif /* linux */

  len = sizeof(struct sockaddr_at);
  if ((err = getsockname(fd, (struct sockaddr *)&addr, &len)) < 0) {
    perror("getsockname()");
    return(err);
  }

  /*
   * getsockname returns net byte order
   *
   */
  addr.sat_addr.s_net = ntohs(addr.sat_addr.s_net);

  if ((len = recvmsg(fd, &msg, 0)) < 0) {
    perror("recvmsg()");
    return(len);
  }

  lap = (LAP *)iov[IOV_LAP_LVL].iov_base;
  lap->type = lapDDP;
  lap->dst = addr.sat_addr.s_node;
  lap->src = raddr.sat_addr.s_node;
  iov[IOV_LAP_LVL].iov_len = lapSize;

  ddp = (DDP *)iov[IOV_DDP_LVL].iov_base;
  ddp->type = *(char *)iov[IOV_DDP_LVL].iov_base;
  ddp->length = htons(len-1+ddpSize);
  ddp->checksum = 0;
  ddp->dstNet = addr.sat_addr.s_net;
  ddp->srcNet = raddr.sat_addr.s_net;
  ddp->dstNode = addr.sat_addr.s_node;
  ddp->srcNode = raddr.sat_addr.s_node;
  ddp->dstSkt = addr.sat_port;
  ddp->srcSkt = raddr.sat_port;
  iov[IOV_DDP_LVL].iov_len = ddpSize;

  return(ddp_protocol(iov+1, iovlen-1, len-1+ddpSize));
}

/*
 * read a KIP packet via loopback
 *
 */

private int
kip_get(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  struct msghdr msg;
  int len;
  LAP *lap;

#ifdef linux
  bzero((char *)&msg, sizeof(msg));
#endif linux
  msg.msg_name = (caddr_t) &from;
  msg.msg_namelen = sizeof(from);
  msg.msg_iov = iov;
  msg.msg_iovlen = iovlen;
#ifndef linux
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
#endif /* linux */

  if ((len = recvmsg(fd, &msg, 0)) < 0) {
    perror("recvmsg()");
    return(len);
  }
  if (iov->iov_len != lapSize) /* check */
    return(-1);

  lap = (LAP *)iov->iov_base;

  if (lap->type == lapDDP)
    return(ddp_protocol(iov+1, iovlen-1, len-lapSize));

  return(-1);
}

export int
routeddp(iov, iovlen)
struct iovec *iov;
int iovlen;
{
  struct sockaddr_at taddr;
  struct sockaddr_in tokip;
  struct msghdr msg;
  DDP *ddp;
  LAP lap;
  int err;
  int fd;

  ddp = (DDP *)iov[IOV_DDP_LVL].iov_base; /* pick out ddp header */

  /* check ddp socket(s) for validity */
  if ( ddp->srcSkt == 0 || ddp->srcSkt == ddpMaxSkt ||
       ddp->dstSkt == 0 || ddp->dstSkt == ddpMaxSkt ||
       skt2fd[ddp->srcSkt] == -1 || skt2kip[ddp->srcSkt] == -1)
    return(ddpSktErr);

  /* check for loopback */
  if ((ddp->dstNet == this_net)
   && (ddp->dstNode == this_node || ddp->dstNode == 0xff)) {
    lap.type = lapDDP;
    lap.dst = ddp->dstNode;
    lap.src = this_node;
    iov[IOV_LAP_LVL].iov_base = (caddr_t)&lap;
    iov[IOV_LAP_LVL].iov_len = lapSize;

    tokip.sin_family = AF_INET;
    tokip.sin_addr.s_addr = bridge_addr.s_addr;
    if ((tokip.sin_port = htons(ddp2ipskt(ddp->dstSkt))) == 0)
      return(ddpSktErr);

#ifdef linux
  bzero((char *)&msg, sizeof(msg));
#endif linux
    msg.msg_name = (caddr_t)&tokip;
    msg.msg_namelen = sizeof(tokip);
    msg.msg_iov = iov;
    msg.msg_iovlen = iovlen;
#ifndef linux
    msg.msg_accrights = 0;
    msg.msg_accrightslen = 0;
#endif /* linux */
    fd = skt2kip[ddp->srcSkt];

    if ((err = sendmsg(fd, &msg, 0)) < 0)
      perror("sendmsg()");

    if (ddp->dstNode != 0xff)
      return(noErr);
  }

  /* send via kernel EtherTalk */

  bzero(&taddr, sizeof(struct sockaddr_at));
  taddr.sat_family = AF_APPLETALK;
  taddr.sat_addr.s_net = ddp->dstNet;
  taddr.sat_addr.s_node = ddp->dstNode;
  taddr.sat_port = ddp->dstSkt;

  /* collapse header, except for DDP Type */
  iov[IOV_DDP_LVL].iov_base += (ddpSize-1);
  iov[IOV_DDP_LVL].iov_len = 1; /* ddp type */

#ifdef linux
  bzero((char *)&msg, sizeof(msg));
#endif linux
  msg.msg_name = (caddr_t)&taddr;
  msg.msg_namelen = sizeof(struct sockaddr_at);
  msg.msg_iov = iov+1;
  msg.msg_iovlen = iovlen-1;
#ifndef linux
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
#endif /* linux */
  fd = skt2fd[ddp->srcSkt];

  if ((err = sendmsg(fd, &msg, 0)) < 0) {
    perror("sendmsg()");
    return(err);
  }

  return(noErr);
}

/*
 * configure the network interface
 *
 * (expects network numbers in host byte order
 * and returns network numbers in net byte order)
 *
 */

int
ifconfig(node, net, net_lo, net_hi)
int *node, *net, *net_lo, *net_hi;
{
  struct sockaddr_at addr;
  struct sockaddr_at *sat;
  struct net_range *nr;
  struct ifreq ifreq;
  int len, err;
  int fd;

  if ((fd = socket(AF_APPLETALK, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return(fd);
  }
  
  strncpy(ifreq.ifr_name, interface, sizeof(ifreq.ifr_name));
  sat = (struct sockaddr_at *)&ifreq.ifr_addr;
  bzero((char *)sat, sizeof(struct sockaddr_at));
  sat->sat_family = AF_APPLETALK;
  sat->sat_addr.s_net = htons(*net);
  sat->sat_addr.s_node = *node;
  sat->sat_port = ATADDR_ANYPORT;

#ifdef	PHASE2
  nr = (struct net_range *)&sat->sat_zero;
  nr->net_lo = htons(*net_lo);
  nr->net_hi = htons(*net_hi);
  nr->phase = 2;
#endif	/* PHASE2 */

  if ((err = ioctl(fd, SIOCSIFADDR, &ifreq)) < 0) {
    perror("SIOCSIFADDR");
    return(err);
  }

  bzero((char *)&addr, sizeof(struct sockaddr_at));
  addr.sat_family = AF_APPLETALK;
  addr.sat_addr.s_net = htons(ATADDR_ANYNET);
  addr.sat_addr.s_node = ATADDR_ANYNODE;
  addr.sat_port = ATADDR_ANYPORT;

  if ((err = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_at))) < 0) {
    perror("bind()");
    return(err);
  }

  len = sizeof(struct sockaddr_at);
  if ((err = getsockname(fd, (struct sockaddr *)&addr, &len)) < 0) {
    perror("getsockname()");
    return(err);
  }

  /*
   * getsockname returns net byte order
   *
   */
  *net = addr.sat_addr.s_net;
  *node = addr.sat_addr.s_node;

  close(fd);
  return(0);
}

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

/*
 * udpport is initially set to the old
 * (768) values for compatibility
 *
 */

#define WKS_entry(name, ddpsock) {(name), (ddpsock), ddpWKSUnix+(ddpsock), 1}

private struct wks wks[] = {
  WKS_entry("at-rtmp", rtmpSkt),
  WKS_entry("at-nbp", nbpNIS),
  WKS_entry("at-echo", echoSkt),
  WKS_entry("at-zis", zipZIS),
  WKS_entry(NULL, 0)
};

/*
 * Translate ddp socket to UDP port: returns 0 if no mapping
 *
 */

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
	  wksp->udpport = ntohs(serv->s_port); /*  replace with new */
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

/*
 * buffer larger than maximum ddp pkt by far
 *
 */

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
  if ((err=sendto(fd,net_buffer,len,0,msg->msg_name,msg->msg_namelen)) < 0)
    perror("sendmsg()");
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
    perror("recvfrom()");
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

/*
 * OSErr GetNodeAddress(int *myNode,*myNet)
 *
 * GetNodeAddress returns the net and node numbers for the current host.
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

/*
 * Set Node Address
 *
 */

OSErr
SetNodeAddress(myNet, myNode)
int myNet, myNode;
{
      int net_lo, net_hi;

      net_lo = 0x0000;
      net_hi = 0xfffe;

      if (ifconfig(&myNode, &myNet, &net_lo, &net_hi) == 0) {
        this_net = myNet;
        this_node = myNode;
        etalkdbupdate(NULL);
	return(noErr);
      }
      return(-1);
}

/*
 * Get Bridge Address
 *
 */

OSErr
GetBridgeAddress(baddr)
AddrBlock *baddr;
{
      baddr->net = bridge_net;
      baddr->node = bridge_node;
      return(noErr);
}

/*
 * Set Bridge Address
 *
 */

OSErr
SetBridgeAddress(baddr)
AddrBlock *baddr;
{
      bridge_node = baddr->node;
      bridge_net = baddr->net;
      if (this_net == 0)
	SetNodeAddress(bridge_net, 0);
      etalkdbupdate(NULL);
      return(noErr);
}

#ifdef PHASE2
/*
 * Set Network Range
 *
 */

OSErr
SetNetRange(range_start, range_end)
u_short range_start, range_end;
{
      this_net = nis_net = net_range_start = range_start;
      net_range_end = range_end;
      etalkdbupdate(NULL);
      return(noErr);
}
#endif PHASE2

/*
 * Maintain a simple routing table for use with Kernel AppleTalk
 *
 * Copyright 1996, The University of Melbourne
 *
 */

struct rtmp {
	u_short net_lo;			/* network range start		*/
	u_short net_hi;			/* network range end		*/
	u_short dist;			/* distance to destination	*/
	u_short rnet;			/* router network address	*/
	u_char rnode;			/* router node address		*/
	u_char state;			/* entry state			*/
#define RTMP_GOOD	0
#define RTMP_SUSP	1
#define RTMP_BAD1	2
#define RTMP_BAD2	3
	u_char flags;			/* flag bits for this entry	*/
#define RTMP_EXTENDED	0x01
	u_char dummy;			/* padding to longword		*/
	struct rtmp *next;		/* next in linked list		*/
};

#define RTMPNULL	((struct rtmp *)NULL)

#define RTMPTABSIZ	256
#define RTMPTABMSK	RTMPTABSIZ-1

#define RTMP_MODIFIED	1
#define RTMP_DELETED	2

struct rtmp *rtmpTab[RTMPTABSIZ];

/*
 * pkt points to RTMP tuples
 * from router rnode.rnet
 *
 */

void
rtmp_data(rnode, rnet, pkt, len)
u_char rnode;
u_short rnet;
u_char *pkt;
int len;
{
	int i;
	u_char dist;
	u_char *data;
	int tuplelen;
	int modified;
	static int init = 0;
	u_short net_lo, net_hi;
	struct rtmp *rtmp_new();
	struct rtmp *rtmp_delete();
	struct rtmp *rtmp, *rtmp_find();
	void rtmp_kernel_update();
	void rtmp_insert();

	if (!init) {
	  for (i = 0; i < RTMPTABSIZ; i++)
	    rtmpTab[i] = RTMPNULL;
	  init = 1;
	}

	data = pkt;
	rnet = ntohs(rnet);

	while (data < (pkt+len)) {
	  net_lo = (data[0] << 8) | data[1];
	  dist = data[2] & 0x1f;
	  if (data[2] & 0x80) {
	  /* extended tuples */
	    net_hi = (data[3] << 8) | data[4];
	    tuplelen = 6;
	  } else {
	  /* non-extended */
	    net_hi = net_lo;
	    tuplelen = 3;
	  }

	  modified = 0;

	  if ((rtmp = rtmp_find(net_lo, net_hi)) != RTMPNULL) {
	  /* update the entry */
	    if (rtmp->state == RTMP_BAD1 && dist < 15) {
	    /* replace entry */
	      if (rtmp->rnet != rnet
	       || rtmp->rnode != rnode)
	        modified = 1;
	      rtmp->dist = dist+1;
	      rtmp->rnet = rnet;
	      rtmp->rnode = rnode;
	      rtmp->state = RTMP_GOOD;
	    } else {
	      if (rtmp->dist >= dist+1 && dist < 15) {
	      /* replace entry */
	        if (rtmp->rnet != rnet
	         || rtmp->rnode != rnode)
	          modified = 1;
	        rtmp->dist = dist+1;
	        rtmp->rnet = rnet;
	        rtmp->rnode = rnode;
	        rtmp->state = RTMP_GOOD;
	      } else {
	        if (rtmp->rnet == rnet
	         && rtmp->rnode == rnode) {
	        /* net further away now */
	          if (dist != 31) {
	            rtmp->dist = dist+1;
	            if (rtmp->dist < 16)
	              rtmp->state = RTMP_GOOD;
	            else {
	              rtmp_kernel_update(rtmp, RTMP_DELETED);
	              (void)rtmp_delete(rtmp);
	            }
	          } else
	            rtmp->state = RTMP_BAD1;
	        }
	      }
	    }
	  } else {
	  /* create a new entry */
	    if ((rtmp = rtmp_new()) != RTMPNULL) {
	      rtmp->net_lo = net_lo;
	      rtmp->net_hi = net_hi;
	      rtmp->dist = dist+1;
	      rtmp->rnet = rnet;
	      rtmp->rnode = rnode;
	      rtmp->state = (dist == 31) ? RTMP_BAD1 : RTMP_GOOD;
	      rtmp_kernel_update(rtmp, RTMP_MODIFIED);
	      rtmp_insert(rtmp);
	    }
	  }

	  if (data[2] & 0x80)
	    if (rtmp != RTMPNULL)
	      rtmp->flags |= RTMP_EXTENDED;

	  if (modified)
	    rtmp_kernel_update(rtmp, RTMP_MODIFIED);

	  data += tuplelen;
	}

	return;
}

/*
 * RTMP validity timer, check & age each entry.
 *
 */

void
rtmp_timer()
{
	int i, nrtmp;
	struct rtmp *rtmp, *rtmp_delete();
	void Timeout(), rtmp_timer();
	void rtmp_kernel_update();

	for (i = 0, nrtmp = 0; i < RTMPTABSIZ; i++) {
	  rtmp = rtmpTab[i];
	  while (rtmp != RTMPNULL) {
	    switch (rtmp->state) {
	      case RTMP_GOOD:
	        if (rtmp->dist != 0)
	  	  rtmp->state = RTMP_SUSP;
	        break;
	      case RTMP_SUSP:
	        rtmp->state = RTMP_BAD1;
	        break;
	      case RTMP_BAD1:
	        rtmp->state = RTMP_BAD2;
	        break;
	      case RTMP_BAD2:
	        rtmp_kernel_update(rtmp, RTMP_DELETED);
	        rtmp = rtmp_delete(rtmp);
	        continue;
	        break;
	    }
	    rtmp = rtmp->next;
	    nrtmp++;
	  }
	}

#ifdef DEBUG
	for (i = 0; i < RTMPTABSIZ; i++) {
	  rtmp = rtmpTab[i];
	  while (rtmp != RTMPNULL) {
	    logit(1, "%4x %c %4x -> net %4x node %3d dist %d state %d",
	      rtmp->net_lo, (rtmp->flags & RTMP_EXTENDED) ? '-' : ' ',
	      rtmp->net_hi, rtmp->rnet, rtmp->rnode, rtmp->dist, rtmp->state);
	    rtmp = rtmp->next;
	  }
	}
#endif /* DEBUG */

	logit(1, "Processed %d entries", nrtmp);

	/*
	 * schedule another call
	 *
	 */
	Timeout(rtmp_timer, 0, 40);

	return;
}

/*
 * exiting, delete all routes from kernel tables
 * and reset the network number information.
 *
 */

void
rtmp_release()
{
	int i;
	struct rtmp *rtmp;
	int node, net, net_lo, net_hi;
	void rtmp_kernel_update();

	for (i = 0; i < RTMPTABSIZ; i++) {
	  rtmp = rtmpTab[i];
	  while (rtmp != RTMPNULL) {
	    rtmp_kernel_update(rtmp, RTMP_DELETED);
	    rtmp = rtmp->next;
	  }
	}

	node = 0x00;
	net = 0xff00;
	net_lo = 0x0000;
	net_hi = 0xfffe;

	ifconfig(&node, &net, &net_lo, &net_hi);

	return;
}

/*
 * create a new RTMP element
 *
 */

struct rtmp *
rtmp_new()
{
	struct rtmp *rtmp;

	rtmp = (struct rtmp *)malloc(sizeof(struct rtmp));

	if (rtmp == RTMPNULL)
	  return(RTMPNULL);

	rtmp->flags = 0x00;

	return(rtmp);
}

/*
 * free memory occupied by RTMP element
 *
 */

void
rtmp_free(rtmp)
struct rtmp *rtmp;
{
	free((char *)rtmp);

	return;
}

/*
 * insert RTMP tuple in linked list
 *
 */

void
rtmp_insert(rtmp)
struct rtmp *rtmp;
{
	int idx, rtmp_hash();
	struct rtmp *p, *q;

	idx = rtmp_hash(rtmp->net_lo, rtmp->net_hi);

	p = rtmpTab[idx];
	q = RTMPNULL;

	while (p != RTMPNULL) {
	  if (p->net_lo >= rtmp->net_lo)
	    break;
	  q = p;
	  p = p->next;
	}

	rtmp->next = p;

	if (q == RTMPNULL)
	  rtmpTab[idx] = rtmp;
	else
	  q->next = rtmp;

	return;
}

/*
 * delete RTMP tuple from linked list
 *
 */

struct rtmp *
rtmp_delete(rtmp)
struct rtmp *rtmp;
{
	int idx, rtmp_hash();
	struct rtmp *next, *p, *q;
	void rtmp_free();

	idx = rtmp_hash(rtmp->net_lo, rtmp->net_hi);

	p = rtmpTab[idx];
	q = RTMPNULL;

	while (p != RTMPNULL) {
	  if (p == rtmp)
	    break;
	  q = p;
	  p = p->next;
	}

	if (p == RTMPNULL)
	  return(p);

	next = rtmp->next;

	if (q == RTMPNULL)
	  rtmpTab[idx] = next;
	else
	  q->next = next;

	rtmp_free(rtmp);

	return(next);
}

/*
 * find an entry in the RTMP table.
 *
 */

struct rtmp *
rtmp_find(net_lo, net_hi)
u_short net_lo, net_hi;
{
	struct rtmp *rtmp;
	int rtmp_hash();

	rtmp = rtmpTab[rtmp_hash(net_lo, net_hi)];

	while (rtmp != RTMPNULL) {
	  if (rtmp->net_lo == net_lo
	   && rtmp->net_hi == net_hi)
	    return(rtmp);
	  rtmp = rtmp->next;
	}

	return(RTMPNULL);
}

/*
 * simple hash of high and low net numbers
 *
 */

int
rtmp_hash(net_lo, net_hi)
register u_short net_lo, net_hi;
{
	register int i, idx;

	for (i = 0; i < 17; i++)
	  if ((idx = ((net_lo>>i) & RTMPTABMSK)) == ((net_hi>>i) & RTMPTABMSK))
	    return(idx);
	
	return(0);
}

/*
 * update (add) or delete entry in kernel routing table
 *
 */

void
rtmp_kernel_update(rtmp, what)
struct rtmp *rtmp;
int what;
{
#ifdef linux
	u_short net;
	struct rtentry rt;
	struct sockaddr_at *ad;
	struct sockaddr_at *gw;

	if (rtmp == RTMPNULL)
	  return;

	ad = (struct sockaddr_at *)&rt.rt_dst;
	gw = (struct sockaddr_at *)&rt.rt_gateway;

	ad->sat_family = AF_APPLETALK;
	gw->sat_family = AF_APPLETALK;
	ad->sat_port = 0;
	gw->sat_port = 0;
	ad->sat_addr.s_node = 0;
	gw->sat_addr.s_node = rtmp->rnode;
	gw->sat_addr.s_net = htons(rtmp->rnet);

	if (skt2fd[1] == -1)
	  return;

	/*
	 * the kernel only keeps routes to single networks,
	 * we have to add an entry for each possible net
	 * in each network range :-(
	 *
	 */
	for (net = rtmp->net_lo; net <= rtmp->net_hi; net++) {
	  ad->sat_addr.s_net = htons(net);
	  switch (what) {
	    case RTMP_MODIFIED:
	      rt.rt_flags = (RTF_GATEWAY|RTF_UP);
	      (void)ioctl(skt2fd[1], SIOCADDRT, &rt);
	      break;
	    case RTMP_DELETED:
	      rt.rt_flags = RTF_GATEWAY;
	      (void)ioctl(skt2fd[1], SIOCDELRT, &rt);
	      break;
	  }
	}
#endif /* linux */

	return;
}
