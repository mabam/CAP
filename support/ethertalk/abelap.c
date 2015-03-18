/*
 * $Author: djh $ $Date: 1996/04/25 00:37:02 $
 * $Header: /mac/src/cap60/support/ethertalk/RCS/abelap.c,v 2.12 1996/04/25 00:37:02 djh Rel djh $
 * $Revision: 2.12 $
*/

/*
 * abelap.c - Ethertalk network module (can fall back to KIP)
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 14, 1986    Schilit	Created.
 *  June 18, 1986    CCKim      Chuck's handler runs protocol
 *  April 28,1991    djh	Add Phase 2 support
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
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/types.h>
#if !defined(pyr) && !defined(__386BSD__)
#include <netinet/in.h>
#endif  /* pyr && __386BSD__ */
#include <netdb.h>
#include <netat/appletalk.h>
#include <netat/abnbp.h>	/* for nbpNIS */
#include <netat/compat.h>	/* to cover difference between bsd systems */
#include <rpc/rpc.h>
#include "../uab/ethertalk.h"	/* iso: level 1 */
#include "../uab/if_desc.h"	/* describes "if" */
#include "../uab/proto_intf.h"
#include "../uab/aarp_defs.h"
#include "aarpd.h"

#ifdef ultrix
#define ALT_RPC
#endif ultrix
#ifdef pyr
#define ALT_RPC
#endif pyr

/* RPC clients */

extern u_char *aarp_resolve_clnt();
extern u_char *rtmp_getbaddr_clnt();
extern u_char *rtmp_setbaddr_clnt();

/* imported network information */

extern word	this_net,	bridge_net,	nis_net,	async_net;
extern byte	this_node,	bridge_node,	nis_node;
extern char	this_zone[34],	async_zone[34],	interface[50];

#ifdef PHASE2
extern word net_range_start, net_range_end;
#endif PHASE2

extern struct in_addr bridge_addr;

#ifdef PHASE2			/* not a dynamic choice yet ...	*/
short lap_proto = LAP_ETALK;	/* default to EtherTalk Phase 2 */
#else  PHASE2
short lap_proto = LAP_ETALK;	/* default to EtherTalk Phase 1 */
#endif PHASE2

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

/* for forwarding using ip_resolve */
private struct in_addr ipaddr_src; /* ip address */
private word ddp_srcnet;	/* ddp network part */
private byte ddp_srcnode;	/* ddp node part */

private int lastnet;		/* net  last heard from */
private int lastnode;		/* node last heard from */
private int lastlnode;
private u_char *lasteaddr;
private u_char eaddrbuf[16];

#ifdef PHASE2
u_char etherbroad[6] = {0x09, 0x00, 0x07, 0xff, 0xff, 0xff};
#else  PHASE2
u_char etherbroad[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#endif PHASE2

private struct sockaddr_in from_sin; /* network struct of last packet rec. */
private struct sockaddr_in abfsin; /* apple bus foreign socketaddr/internet */

word rebport;			/* used to hold swabbed rebPort */
#define rebPort 902		/* 0x386 */

private CLIENT *cl;		/* RPC client */

import int ddp_protocol();	/* DDP protocol handler */
private int etalk_listener();	/* EtherTalk listener */
private int kip_get();		/* KIP listener */
export DBUG dbug;		/* debug flags */

/* BUG: bind doesn't work when lsin is on the stack! */
private struct sockaddr_in lsin; /* local socketaddr/internet */
private int skt2fd[ddpMaxSkt+1]; /* translate socket to file descriptor */
private int skt2iovlen[ddpMaxSkt+1]; /* translate socket to file descriptor */
private struct iovec *skt2iov[ddpMaxSkt+1]; /* translate socket to file desc */
private int skt2pifd[ddpMaxSkt+1]; /* translate socket to file descriptor */
private int ddpskt2udpport[ddpMaxWKS+1]; /* ddp "wks" socket to udp port */

AI_HANDLE aih;
private LAP laph;
extern int errno;
extern aarptab_scan();
extern AARP_ENTRY *aarptab_find();

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

/* udpport is initially set to the old (768) values for compatibility */
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

/*
 * initialize
 *
*/
export
abInit(disp)
int disp;
{
  int i;
  extern int aarp_inited;
  static int here_before = 0;
#ifdef ALT_RPC
  struct sockaddr_in sin;
  int sock;
#endif ALT_RPC

  for (i=0; i < ddpMaxSkt+1; i++) {
    skt2fd[i] = -1;		/* mark all these as unused */
    skt2pifd[i] = -1;		/* mark all these as unused */
  }
  for (i=0; i < ddpMaxWKS; i++)
    ddpskt2udpport[i] = -1;	/* mark unknown */

  if (!here_before) {
    struct timeval tv;

    openetalkdb(NULL);		/* set up variables */
    
    if (*interface == '\0')	/* fall back to vanilla KIP mode */
      lap_proto = LAP_KIP;
    else {
      here_before = 1;
      if (!aarp_inited) {
        tv.tv_sec = AARP_SCAN_TIME;
        tv.tv_usec = 0;
        relTimeout(aarptab_scan, 0, &tv, TRUE);
      }
    }
  }

  rebport = htons(rebPort);	/* swap to netorder */
  init_fdlistening();

  abfsin.sin_family = AF_INET;
  abfsin.sin_addr.s_addr = INADDR_ANY;

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

  if (lap_proto == LAP_ETALK) {
#ifdef ALT_RPC
    struct timeval tv;
#endif ALT_RPC
    aih.ai_aarptab = (caddr_t)aarptab_init();
#ifdef ALT_RPC
    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    bzero(sin.sin_zero, sizeof(sin.sin_zero));
    sin.sin_addr.s_addr = htonl(0x7f000001);
    sock = RPC_ANYSOCK;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    cl = clntudp_create(&sin, AARPDPROG, AARPDVERS, tv, &sock);
#else  ALT_RPC
    cl = clnt_create("localhost", AARPDPROG, AARPDVERS, "udp");
#endif ALT_RPC
    if (cl == NULL) {
      clnt_pcreateerror("localhost");
      exit(1);
    }
    pi_setup();
  }
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
  word ipskt;
  int etph = -1;
  byte this_intfno;
  int sktlimit = 128;
  char *ep, *cp, this_intf[50];

  /* good enough for now */
  if (iov == NULL || iovlen < IOV_LAP_SIZE+1 || iovlen > IOV_READ_MAX)
    return(-1);

  if ((fd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("abopen");
    return(fd);
  }
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

    if (lap_proto == LAP_ETALK) {
      ep = NULL;
      strncpy(this_intf, interface, sizeof(this_intf));
      for (cp = this_intf; *cp != '\0'; cp++) {
        if (*cp >= '0' && *cp <= '9')
          ep = cp;
      }
      if (ep == NULL)
        return(ENODEV);
      this_intfno = *ep - '0';
      *ep = '\0';

      if ((etph = pi_open(ETHERTYPE_APPLETALK,*skt,this_intf,this_intfno))<=0) {
	close(fd);
	skt2fd[*skt] = -1;
	if (etph == 0)
	  return(EMFILE);
	else
	  return(errno);
      }
      pi_listener_2(etph, etalk_listener, iov, iovlen);
    }
    fdlistener(fd, kip_get, iov, iovlen); /* remember for later */
    skt2fd[*skt] = fd;		/* remember file descriptor for socket */
    skt2pifd[*skt] = etph;
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
  if (lap_proto == LAP_ETALK)
    pi_close(skt2pifd[skt]);
  if ((fd = skt2fd[skt]) < 0)
    return(0);
  if (close(fd) != 0)
    perror("abClose");		/* some error... */
  fdunlisten(fd);
  skt2fd[skt] = -1;		/* mark as unused */
  skt2pifd[skt] = -1;		/* mark as unused */
  return(0);
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
  if ((err=sendto(fd,net_buffer,len,0,msg->msg_name,msg->msg_namelen)) < 0)
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

private int
kip_get(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  struct msghdr msg;
  int len;
  LAP *lap;

  msg.msg_name = (caddr_t) &from_sin;
  msg.msg_namelen = sizeof(from_sin);
  msg.msg_iov = iov;
  msg.msg_iovlen = iovlen;
#if defined(__386BSD__) || defined(__FreeBSD__)
  msg.msg_control = 0;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
#else  /* __386BSD__ */
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
#endif /* __386BSD__ */
  if ((len = recvmsg(fd,&msg,0)) < 0) {
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
  return(-1);
}

#define elapSize (lapSize+14)

private int etalk_listener(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  /* room for packet and then some */
  static unsigned char rbuf[ddpMaxData+ddpSize+lapSize+100];
#define pack (rbuf+1)
#define packsize (sizeof(rbuf)-1)
  int cc;
  struct iovec *liov;
  int pos, i, len, lcc, liovlen;
  struct ethertalkaddr etaddr;
  DDP *ddp;
  ShortDDP *sddp;
#ifdef USING_FDDI_NET
  extern int source_offset;
#endif USING_FDDI_NET

  lastnet = -1;
  lastnode = -1;

  if ((cc = pi_reada(fd, pack, packsize, eaddrbuf)) <= 0)
    return(-1);
 
#ifdef USING_FDDI_NET
  lasteaddr = eaddrbuf + source_offset;
#else USING_FDDI_NET
  lasteaddr = eaddrbuf + 6;
#endif USING_FDDI_NET

  if (cc <= lapSize)  /* not much use if only lap */
    return(-1);

  pos = lapSize;
  cc -= pos;
  iov++;
  iovlen--;

  bcopy(pack, &laph, lapSize);

  if (laph.src == 0xff)		/* bad, bad, bad */
    return(-1);

  /*
   * lap dest isn't right
   *
   */
  if (laph.dst != 0xff && laph.dst != this_node)
    return(-1);

  liovlen=iovlen;
  liov=iov;
  lcc=cc;

  switch (laph.type) {
  case lapShortDDP:		/* handle shortDDP by hand */
    if (cc < ddpSSize || iovlen < 1)
      return(-1);

    sddp = (ShortDDP *)(pack + pos);
    ddp = (DDP *)iov->iov_base;
    pos += ddpSSize;
    lcc -= ddpSSize;
    liov++;
    liovlen--;
    cc += ddpSize - ddpSSize;

    ddp->length = sddp->length + ddpSize - ddpSSize;
    ddp->checksum = 0;
    ddp->dstNet = this_net;
    ddp->srcNet = this_net;
    ddp->dstNode = laph.dst;
    ddp->srcNode = laph.src;
    ddp->dstSkt = sddp->dstSkt;
    ddp->srcSkt = sddp->srcSkt;
    ddp->type = sddp->type;

    /* fall through */

  case lapDDP:

    for (; liovlen; liovlen--, liov++) {
      len = min(liov->iov_len, lcc);
      if ((pos + len) > packsize) /* if asking for too much */
	len = packsize - pos; /* then limit */
      bcopy(pack+pos, liov->iov_base, len);
      pos += len;
      lcc -= len;
      /* either no more room or no more data */
      if (len != liov->iov_len)
	break;
    }

    ddp = (DDP *)iov->iov_base;
    lastnode = ddp->srcNode;
    lastnet = ddp->srcNet;
    lastlnode = laph.src;

  /*
   * pick out source address for aarp table management if not self
   *
   */
  if (ddp->srcNode != this_node) {
#ifdef PHASE2
    etaddr.dummy[0] = 0;
    etaddr.dummy[1] = (ntohs(ddp->srcNet) >> 8) & 0xff;
    etaddr.dummy[2] = (ntohs(ddp->srcNet) & 0xff);
#else  PHASE2
    etaddr.dummy[0] = etaddr.dummy[1] = etaddr.dummy[2] = 0;
#endif PHASE2
    etaddr.node = ddp->srcNode;
#ifdef USING_FDDI_NET
    aarp_insert(&aih, eaddrbuf+source_offset, &etaddr, FALSE);
#else USING_FDDI_NET
    aarp_insert(&aih, eaddrbuf+6, &etaddr, FALSE);
#endif USING_FDDI_NET
  }

    return(ddp_protocol(iov, iovlen, cc));

  default:
    return(-1);
  }
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
  struct msghdr msg;
  word destskt;
  struct in_addr desthost;
  DDP *ddp;
  int err;
  int fd;
  unsigned char *eaddr;
  int destnode;
  int destnet;
  AARP_ENTRY *ae;
  struct ethertalkaddr etaddr;
  AddrBlock baddr;
  OSErr GetBridgeAddress();

  ddp = (DDP *)iov[IOV_DDP_LVL].iov_base; /* pick out ddp header */

  /* check ddp socket(s) for validity */
  if ( ddp->srcSkt == 0 || ddp->srcSkt == ddpMaxSkt ||
       ddp->dstSkt == 0 || ddp->dstSkt == ddpMaxSkt ||
      (fd = skt2fd[ddp->srcSkt]) == -1 )
    return(ddpSktErr);

  /* establish a dummy lap header */
  lap.type = lapDDP;
  lap.dst = ddp->dstNode;
  lap.src = this_node;
  iov[IOV_LAP_LVL].iov_base = (caddr_t) &lap; /* LAP header */
  iov[IOV_LAP_LVL].iov_len = lapSize; 	  /* size  */

  /* Use KIP if loopback, else ether */
  if ((lap_proto != LAP_ETALK)
  || ((ddp->dstNet == this_net
#ifdef PHASE2
    || (ddp->dstNet == 0x0000 && ddp->dstNode == 0xff)
#endif PHASE2
    ) && (ddp->dstNode == this_node || ddp->dstNode == 0xff))) {
    /* establish dest socket */
    destskt = (word)htons(ddp2ipskt(ddp->dstSkt));
    if (destskt == 0)		/* byte swapped zero is still zero */
      return(ddpSktErr);
    /* resolve mapping */
    ip_resolve(ddp->dstNet, ddp->dstNode, &desthost);

    /* send through */
    abfsin.sin_addr = desthost;
    abfsin.sin_port = destskt;

    msg.msg_name = (caddr_t) &abfsin;
    msg.msg_namelen = sizeof(abfsin);
    msg.msg_iov = iov;
    msg.msg_iovlen = iovlen;
#if defined(__386BSD__) || defined(__FreeBSD__)
    msg.msg_control = 0;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
#else  /* __386BSD__ */
    msg.msg_accrights = 0;
    msg.msg_accrightslen = 0;
#endif /* __386BSD__ */
    if ((err = sendmsg(fd,&msg,0)) < 0)
      perror("abwrite");
    if (ddp->dstNode != 0xff)
      return(err);
  }
#ifdef PHASE2
  /* delete the unwanted LAP header */
  iov[IOV_LAP_LVL].iov_len = 0;
#endif PHASE2
  if ((lap_proto == LAP_ETALK)
  && (ddp->dstNet != this_net || ddp->dstNode != this_node)) {
    eaddr = NULL;
#ifdef PHASE2
    if ((ddp->dstNet == 0 && ddp->dstNode == 0xff) /* local broadcast */
     || (ntohs(ddp->dstNet) >= ntohs(net_range_start)
         && ntohs(ddp->dstNet) <= ntohs(net_range_end))
     || (ntohs(ddp->dstNet) >= 0xff00 && ntohs(ddp->dstNet) <= 0xfffe)) {
      destnode = ddp->dstNode;
      destnet = ddp->dstNet;
    } else {
#else  PHASE2
    if (ddp->dstNet == this_net) {
      destnode = ddp->dstNode;
      destnet = ddp->dstNet;
    } else {
#endif PHASE2
      if (ddp->dstNet == lastnet && ddp->dstNode == lastnode) {
        destnode = lastlnode;
	destnet = lastnet;
        eaddr = lasteaddr;
      } else {
	if (GetBridgeAddress(&baddr) == noErr && baddr.node) {
          destnode = baddr.node;
	  destnet = baddr.net;
        } else
          return(-1);
      }
    }

    lap.dst = destnode;

    if (! eaddr) {
      if (destnode == 0xff)
	eaddr = etherbroad;
      else {
#ifdef PHASE2
	etaddr.dummy[0] = 0;
	etaddr.dummy[1] = (ntohs(destnet) >> 8) & 0xff;
	etaddr.dummy[2] = (ntohs(destnet) & 0xff);
#else  PHASE2
	etaddr.dummy[0] = etaddr.dummy[1] = etaddr.dummy[2] = 0;
#endif PHASE2
	etaddr.node = destnode;
	if ((ae = aarptab_find(&aih, &etaddr)) != NULL &&
	    ae->aae_flags & AARP_OKAY) {
	  if (ae->aae_flags & (AARP_PERM))
	    eaddr = ae->aae_eaddr;
	  else if (ae->aae_ttl > 0)
	    eaddr = ae->aae_eaddr;
	}
      }
    }

    if (! eaddr) {
      eaddr = aarp_resolve_clnt(&etaddr, cl);
      if (eaddr == NULL) {
	clnt_perror(cl, "localhost");
	return(-1);
      }
      if (eaddr[0] == 0 && eaddr[1] == 0 && eaddr[2] == 0 &&
	  eaddr[3] == 0 && eaddr[4] == 0 && eaddr[5] == 0) {
/*
 * no arp entry.  We have sent an arp request.  We want to pretend
 * that the write succeeded, since some applications abort on a
 * failed write.  Presumably they'll be expecting to read some sort
 * of response, and when that fails, will time out
 */
	int len, i;
	for (len = 0, i = 0 ; i < iovlen ; i++)
	  if (iov[i].iov_base && iov[i].iov_len >= 0)
	    len += iov[i].iov_len;
	return (len);
      }
      aarp_insert (&aih, eaddr, &etaddr, 1);
    }
      
    if ((err = pi_writev(skt2pifd[ddp->srcSkt], iov, iovlen, eaddr)) < 0)
      perror("abwrite");
    return(err);
  }
  return(noErr);
}

export void
dumpether(lvl,msg, ea)
int lvl;
char *msg;
byte *ea;
{
  logit(lvl, "%s: %x %x %x %x %x %x",msg,ea[0],ea[1],ea[2],ea[3],ea[4],ea[5]);
}

/*
 * Get Bridge Address (via RPC for Native EtherTalk)
 *
 */

OSErr
GetBridgeAddress(addr)
AddrBlock *addr;
{
      AddrBlock *baddr;

      if (lap_proto == LAP_ETALK) {
        baddr = (AddrBlock *) rtmp_getbaddr_clnt(addr, cl);
        if (baddr == NULL) {
	  clnt_perror(cl, "localhost");
	  exit(1);
	}
	bridge_net = baddr->net;
        bridge_node = baddr->node;
#ifndef PHASE2
        this_net = nis_net = bridge_net;
#endif  PHASE2
      }
      addr->net = bridge_net;
      addr->node = bridge_node;
      return(noErr);
}

/*
 * Set Bridge Address (via RPC for Native EtherTalk)
 *
 */

OSErr
SetBridgeAddress(addr)
AddrBlock *addr;
{
      AddrBlock *baddr;

      if (lap_proto == LAP_ETALK) {
        baddr = (AddrBlock *) rtmp_setbaddr_clnt(addr, cl);
        if (baddr == NULL) {
	  clnt_perror(cl, "localhost");
	  exit(1);
	}
#ifndef PHASE2
	this_net = nis_net = addr->net;
#endif  PHASE2
      }
      bridge_net = addr->net;
      bridge_node = addr->node;
      return(noErr);
}

#ifdef PHASE2
/*
 * Set Network Range (via RPC for Native EtherTalk)
 *
 */

OSErr
SetNetRange(range_start, range_end)
u_short range_start, range_end;
{
      AddrBlock *baddr;
      long range;

      if (lap_proto == LAP_ETALK) {
	range = range_start & 0xffff;
	range <<= 16;
	range |= range_end & 0xffff;
        baddr = (AddrBlock *) range_set_clnt(&range, cl);
        if (baddr == NULL) {
	  clnt_perror(cl, "localhost");
	  exit(1);
	}
      }
      this_net = nis_net = net_range_start = range_start;
      net_range_end = range_end;
      return(noErr);
}
#endif PHASE2
