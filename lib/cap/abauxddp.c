/*
 * $Author: djh $ $Date: 1993/11/29 06:48:59 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abauxddp.c,v 2.2 1993/11/29 06:48:59 djh Rel djh $
 * $Revision: 2.2 $
 *
 */

/*
 * abauxddp.c - Datagram Delivery Protocol for native appletalk under A/UX
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 19, 1986    Schilit    Created.
 *  July  9, 1986    CCKim      Clean up some of Bill's stuff and allow
 *                              Appletalk protocols on ethernet with CAP
 *  Feb. 1988 Charlie - don't like the way the encapsulation code runs
 *   into the DDP code.  Drop out all the encapsulation code into
 *   another module (interface dep) and drop out part of DDP into it.
 *
 *  1990   William Roberts	Add support for A/UX native appletalk
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>

/* Use the direct Apple support for ddp. We insist on a long form
 * DDP header, and we can open our own file descriptor instead of
 * calling abOpen to do it for us. iDDPOpenSocketIOV and DDPClose
 * are modified to set up the fdlistener stuff that abOpen/abClose
 * used to deal with.
 */
#include <at/appletalk.h>
#include <at/lap.h>
#include <at/ddp.h>
#include <sys/stropts.h>        /* for strioctls */
#include <netinet/in.h>         /* for ntohs */

/* Fortunately the CAP include and the Apple ones don't conflict */

#include <netat/appletalk.h>    /* for private, boolean etc */
#include "cap_conf.h"

#ifndef DONT_DOCHKSUM
# define DO_CHKSUM 1
#else
# define DO_CHKSUM 0
#endif
boolean dochecksum = DO_CHKSUM;	/* can be patched if necessary */

short lap_proto = LAP_KERNEL;   /* kernel appletalk support */

extern int errno;

/* room for up to ddp + 1 for data buffer */
#define DDPIOVLEN (IOV_DDP_SIZE+1)
private DDP ddph;
private byte ddpdatabuffer[ddpMaxData];
private struct iovec rddpiov[DDPIOVLEN] = {
  {NULL, 0},                    /* LAP header */
  {(caddr_t)&ddph, ddpSize},    /* DDP header (me) (redundant) */
  {(caddr_t)ddpdatabuffer, ddpMaxData} /* ddp data */
};

typedef struct {
  int (*lproc)();               /* socket listener routine */
  int flags;                    /* flags */
#define DDPL_OLDINTERFACE 1
  int fd;                       /* DDP file descriptor */
} LISENTRY;                     /* listener entry */

private LISENTRY ddpl[ddpMaxSkt+1]; /* table of listeners */


/* abInit(disp)
 *
 * Get things started and find out our Node and Net
 * Maybe we should wait until later in case a seed bridge starts up?
 */

export word this_net;
export byte this_node;
export byte this_intfno;
export word bridge_net;         /* do we care about this any more? */
export byte bridge_node;
export word nis_net;            /* do we care about this any more? */
export byte nis_node;
export char this_zone[34];
export char async_zone[34];
export char this_intf[50];

export DBUG dbug;

abInit(disp)
int disp;
{
    int fd, x;
    at_ddp_cfg_t cfg;
    at_socket socket;
    struct strioctl si;

    init_fdlistening();
    DDPInit();

    /* find out our address */

    socket = 0;         /* don't care */
    fd = ddp_open(&socket);
    if (fd < 0) {
	perror("ddp_open");
	exit(0);
    }

    si.ic_cmd = DDP_IOC_GET_CFG;
    si.ic_timout = 1;
    si.ic_len = sizeof(cfg);
    si.ic_dp = (char *)&cfg;

    x = ioctl(fd, I_STR, &si);
    if (x < 0) {
	perror("abInit: strioctl failed");
	exit(-1);
    }

    if (disp || dbug.db_lap) {
	printf("abInit: [ddp %3d.%02d, %d] starting\n",
	    ntohs(cfg.node_addr.net)>>8,
	    ntohs(cfg.node_addr.net)&0x0FF,
	    cfg.node_addr.node);
    }
    this_net  = cfg.node_addr.net;
    this_node = cfg.node_addr.node;
    bridge_net  = cfg.router_addr.net;
    bridge_node = cfg.router_addr.node;

    x = ddp_close(fd);
    if (x < 0) {
	perror("ddp_close");
	exit(0);
    }
}

OSErr
GetNodeAddress(mynode, mynet)
int *mynode, *mynet;
{
    *mynode = this_node;
    *mynet  = this_net;
    return (noErr);
}

/*
 * add these for compatibility with other changes ...
 *
 */

GetMyAddr(addr)
AddrBlock *addr;
{
    addr->net = this_net;
    addr->node = this_node;
}

SetMyAddr(addr)
AddrBlock *addr;
{
    this_net = addr->net;
    this_node = addr->node;
}

GetNisAddr(addr)
AddrBlock *addr;
{
    addr->net = nis_net;
    addr->node = nis_node;
}

SetNisAddr(addr)
AddrBlock *addr;
{
    nis_net = addr->net;
    nis_node = addr->node;
}


DDPInit()
{
  int i;

  /* initialize ddp listener array */
  for (i = 0; i < ddpMaxSkt+1; i++) {
    ddpl->lproc = NILPROC;
    ddpl->flags = 0;
  }

}


/*
 * OSErr DDPOpenSocket(int *skt, ProcPtr sktlis)
 * OSErr DDPOpenSocketIOV(int *skt,ProcPtr sktlis,struct iovec *iov,int iovlen)
 *
 * Open a DDP socket and optionally install a socket listener to the
 * listener table.  If skt is nonzero (it must be in the range of
 * 1 to 127) it specifies the socket's number.  If skt is 0 then
 * DDPOpenSocket dynamically assigns a socket number in the range
 * 128 to 254, and returns it in skt.  You can actually specify a socket
 * in the 128 to 254 range, but it's not a good idea :-).
 *
 * sktlis contains a pointer (ProcPtr) to the socket listener; if
 * it is NILPROC, the default listener will be used (NYI)
 *
 * If calling DDPOpenSocketIOV, then the iovector passed must be of
 * size (IOV_DDP_SIZE+1) (length for ddp+lap) plus one (data for caller).
 * In addition, it must be filled in from DDP_LVL+1 to the end
 *
 *
 * The listener is defined as:
 *  XXX_listener(int skt, PKT *pkt, int len, AddrBlock *addr)
 * if called from DDPOpenSocket and:
 *  XXX_listener(int skt, struct iovec *iov, int iovlen, AddrBlock *addr)
 * if called from DDPOpenSocketIOV
 *
 * The iov passed back to the listener will start after the ddp header
 * block
 *
*/
OSErr
DDPOpenSocketIOV(skt, sktlis, iov, iovlen)
int *skt;
ProcPtr sktlis;
struct iovec *iov;
int iovlen;
{
  int fd;
  at_socket s;
  int defDDPlis();      /* which always fails anyway - clearly a throwback */
  int ddp_upcall();

  /* allow 0 - means dynamic assignment */
  s = *skt;                     /* socket wanted */
  if (s >= ddpMaxSkt) {
    fprintf(stderr,"ddpOpenSocket: skt out of range\n");
    exit(0);
  }
  /* open the socket please */
  if ((fd = ddp_open(&s)) < 0) {
    if (dbug.db_ddp)
      fprintf(stderr,"ddp: open socket - socket open failed: %d\n", errno);
    return(fd);              /* return error if failed */
  }
  *skt = s;             /* socket number actually obtained */
  if (dbug.db_ddp) {
    fprintf(stderr, "ddp: open socket: opened socket %d\n", s);
  }

  iov[IOV_DDP_LVL].iov_base = (caddr_t)&ddph; /* install */
  iov[IOV_DDP_LVL].iov_len = ddpSize; /* install */

  /* add default or user's listener */
  ddpl[s].lproc = ((sktlis == NILPROC) ? defDDPlis : sktlis);
  ddpl[s].flags = 0;
  ddpl[s].fd = fd;

  /* add the file descriptor to the list of those listened for */
  fdlistener(fd, ddp_upcall, iov+IOV_DDP_LVL, iovlen-IOV_DDP_LVL);

  return(noErr);                        /* and return */
}


OSErr
DDPOpenSocket(skt,sktlis)
int *skt;
ProcPtr sktlis;
{
  OSErr err;

  err = DDPOpenSocketIOV(skt, sktlis, rddpiov, DDPIOVLEN);
  if (err == noErr) {
    ddpl[*skt].flags = DDPL_OLDINTERFACE;
  }
  return(err);
}


/*
 * OSErr DDPCloseSocket(int skt)
 *
 * DDPCloseSocket closes the skt, cancels all pending DDPRead calls
 * that have been made on that socket, and removes the socket listener
 * procedure.
 *
*/

OSErr
DDPCloseSocket(skt)
int skt;
{
  if (skt == 0 || skt >= ddpMaxSkt) {
    if (dbug.db_ddp)
      fprintf(stderr, "ddpCloseSocket: Socket %d out of range\n", skt);
    return;
  }
  ddpl[skt].lproc = NILPROC;    /* no procedure */

  fdunlisten(ddpl[skt].fd);     /* stop listening to the socket */
  return(ddp_close(ddpl[skt].fd));
}

OSErr
DDPWrite(abr)
abRecPtr abr;
{
  struct iovec iov[IOV_DDP_SIZE+1];

  iov[IOV_DDP_LVL+1].iov_base = (caddr_t) abr->proto.ddp.ddpDataPtr;
  iov[IOV_DDP_LVL+1].iov_len = abr->proto.ddp.ddpReqCount;

  return(DDPWriteIOV(abr,iov,IOV_DDP_SIZE+1));
}

/*
 * DDPWriteIOV
 *
 * DDPWriteIOV builds up DDP header and then passes off to routeddp
 *  who decides where to send it.  In the most cases, we'll probably
 *  have a version of routeddp per "network" type so we can "optimize"
 *
*/
/*ARGSUSED*/
OSErr
DDPWriteIOV(abr,iov,iovl)
abRecPtr abr;
struct iovec iov[];
int iovl;
{
  at_ddp_t ddp;
  ddpProto *dpr;
  at_socket skt;
  int err;

  dpr = &abr->proto.ddp;
  skt = dpr->ddpSocket; /* our socket number */

  if (skt == 0 || skt >= ddpMaxSkt || ddpl[skt].lproc == NILPROC) {
	return -1;
  }

  /* the DDP streams driver fills in everything else given
   * just checksum, dst_net, dst_node, dst_socket, type and data.
   * The length is inferred from the write or writev command.
   */
  ddp.dst_net = dpr->ddpAddress.net;
  ddp.dst_node = dpr->ddpAddress.node;
  ddp.dst_socket = dpr->ddpAddress.skt;
  ddp.type = dpr->ddpType;
  ddp.checksum = dochecksum;    /* driver computes the correct value */

  /* The DDP streams driver deals with LAP headers etc, so must
   * adjust the iov to point to the start of the DDP stuff
   */
  iov += IOV_DDP_LVL; iovl -= IOV_DDP_LVL;

  iov->iov_base = (caddr_t) &ddp; /* DDP header */
  iov->iov_len = ddpSize;

  err = writev(ddpl[skt].fd, iov, iovl);
  if (err <= 0) {
    if (dbug.db_ddp) {
	perror("ddp_write failed");
    }
    return -1;
  }
  return 0;
}

/*ARGSUSED*/
OSErr
DDPRead(abr,retCkSumErrs,async)
abRecPtr abr;
int retCkSumErrs,async;
{
  fprintf(stderr,"DDPRead NYI\n");
}

OSErr
DDPRdCancel(abr)
abRecPtr abr;
{
  fprintf(stderr,"DDPRdCancel NYI\n");
}

defDDPlis(skt,ddp,len,addr)
DDP *ddp;
AddrBlock *addr;
{
  fprintf(stderr,"defDDPlis NYI\n");
/***** copy data into user buffer *****/
}

/*
 * ddp_protocol(ddp,len)
 *
 * ddp_protocol is the installed LAP protocol handler for protocol
 * type lapDDP (2).  This routine gets called by LAP when a packet
 * is received with lapDDP in the protocol field.  ddp_protocol
 * passes the packet to the socket listener installed by the
 * DDPOpenSocket call.
 *
 * With direct support for DDP, this routine is entered from the
 * very simple ddp_upcall routine installed as the fdlistener for
 * the appropriate file descriptor.
 *
 * Caller provides an iov pointing to DDP header
*/

int ddp_protocol(iov, iovlen, plen)
struct iovec *iov;
int iovlen;
int plen;
{
  at_socket skt;
  AddrBlock addr;
  int len, cnt, oldstyle;
  at_ddp_t *ddp;

  /* iovlen == 1 means just a ddp header */
  if (iovlen < 1 || iov->iov_len != ddpSize) {
    return -1;
  }

  ddp = (at_ddp_t *)iov->iov_base; /* it must be aligned ok already*/
  len = ntohs(ddp->length) & 0x3ff; /* get the "real" length */
  if (plen < len) {             /* not enought data? */
    if (dbug.db_ddp)
      fprintf(stderr, "BAD PACKET: ddp reports more data than came in\n");
    return;                     /* drop pkt */
  }

  skt = ddp->dst_socket;

  if (ddpl[skt].lproc == NILPROC) { /* listener proc for socket */
    if (dbug.db_ddp)
      fprintf(stderr,"ddp_protocol: no socket listener!\n");
    return;
  }

  addr.net = ddp->src_net;
  addr.node = ddp->src_node;
  addr.skt = ddp->src_socket;
  iov++;                        /* skip ddp header */
  iovlen--;
  plen -= ddpSize;              /* reduce to data size */
  if (iovlen < 1) {
    return 0;   /* nothing to send, so drop it */
  }
  if (ddpl[skt].lproc) {        /* be postitive */
    if (ddpl[skt].flags & DDPL_OLDINTERFACE)
      (*ddpl[skt].lproc)(skt,ddp->type,iov->iov_base,plen,&addr);
    else
      (*ddpl[skt].lproc)(skt, ddp->type, iov, iovlen, plen, &addr);
  }
  return 0;
}

private int ddp_upcall(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
    int nbytes;

    nbytes = readv(fd, iov, iovlen);
    return (ddp_protocol(iov, iovlen, nbytes));
}


/*
 * Call with TRUE or FALSE - FALSE means ignore, TRUE means don't ignore
 *
*/
checksum_error(which)
boolean which;
{
  /* who cares - we certainly don't */
}


/* Utility routine to sprintf an address in a standard format
 *
 *      net_hi.net_lo,node socket
 *
 * It returns its first argument, for convenient use in printf.
 */
char *appletalk_ntoa(buf, addr)
char buf[];
AddrBlock *addr;
{
    sprintf(buf, "%d.%02d,%d %d",
	((addr->net)>>8)& 0x0FF, (addr->net)& 0x0FF,
	addr->node, addr->skt);
    return buf;
}

/* int ddp_skt2fd(skt)
 *
 * Converts a socket number into a file descriptor, or returns -1.
 * Used by upper layers which have converted over to A/UX support,
 * since CAP is otherwise keen only to pass around socket numbers.
 */

int ddp_skt2fd(skt)
byte skt;
{
    if (skt > 0 && skt <= ddpMaxSkt && ddpl[skt].lproc != NILPROC) {
	return ddpl[skt].fd;
    }

    /* invlaid skt or no socket listener - presumed not open */
    return -1;
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
