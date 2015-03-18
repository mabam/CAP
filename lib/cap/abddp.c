/*
 * $Author: djh $ $Date: 1996/06/18 10:48:17 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abddp.c,v 2.3 1996/06/18 10:48:17 djh Rel djh $
 * $Revision: 2.3 $
*/

/*
 * abddp.c - Datagram Delivery Protocol
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 19, 1986    Schilit	Created.
 *  July  9, 1986    CCKim	Clean up some of Bill's stuff and allow
 *				Appletalk protocols on ethernet with CAP
 *  Feb. 1988 Charlie - don't like the way the encapsulation code runs
 *   into the DDP code.  Drop out all the encapsulation code into
 *   another module (interface dep) and drop out part of DDP into it.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#ifndef	linux
#include <sys/uio.h>
#endif	/* linux */
#include <netat/appletalk.h>
#include <netinet/in.h>

#include "cap_conf.h"

import byte this_node, nis_node, bridge_node;
import word this_net, nis_net, bridge_net;

#ifndef DONT_DOCHKSUM
# define DO_CHKSUM 1
#else
# define DO_CHKSUM 0
#endif
boolean dochecksum = DO_CHKSUM;
private boolean checksumerror_drop = TRUE;

/* Following are the major allowed defines in this module */

/* -DINLINECHKSUM */
/* define if you want to the checksum inline.  Only been tested for a vax */
/* and definitely doesn't work correctly on some machines */

/* -DNOINLINECHKSUM */
/* Turns off inlinechecksumming if turned on for vax - this is to allow us */
/* to debug the code */

/* machine dependecies should be encoded here */

#ifdef vax
# ifndef NOINLINECHKSUM
#  define INLINECHKSUM
# endif
#endif

#ifdef INLINECHKSUM
/* Dan Tappan, BBN */
/*
 * This macro seems to produce close to optimal code on a VAX (after -O )
 */
#define ddp_chksum(xp, l, is, s) { \
  register unsigned char *ddp_chksum_p_var = (u_char *)xp; \
  register int ddp_chksum_cnt_var = l; \
  s = is; \
  while(--ddp_chksum_cnt_var >= 0) \
    if ((s = (s + *ddp_chksum_p_var++) << 1) & (1<<16)) ++s; \
  s &= 0xffff;			/* make off extra bits */ \
  }
#define ddpchksumtype register int
#else
#define ddp_chksum(xp, l, is, s) s = do_ddp_chksum(xp, l, is)
private word do_ddp_chksum();
#define ddpchksumtype word
#endif

/* room for up to ddp + 1 for data buffer */
#define DDPIOVLEN (IOV_DDP_SIZE+1)
private DDP ddph;
private byte ddpdatabuffer[ddpMaxData];
private struct iovec rddpiov[DDPIOVLEN] = {
  {NULL, 0},			/* LAP header */
  {(caddr_t)&ddph, ddpSize},	/* DDP header (me) (redundant) */
  {(caddr_t)ddpdatabuffer, ddpMaxData} /* ddp data */
};

typedef struct {
  int (*lproc)();		/* socket listener routine */
  int flags;			/* flags */
#define DDPL_OLDINTERFACE 1
} LISENTRY;			/* listener entry */

private LISENTRY ddpl[ddpMaxSkt+1]; /* table of listeners */

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
private OSErr
iDDPOpenSocketIOV(skt, iov, iovlen)
int *skt;
struct iovec *iov;
int iovlen;
{
  int refcd;
  int s;

  /* allow 0 - means dymanic assignment */
  s = *skt;			/* socket wanted */
  if (s >= ddpMaxSkt) {
    fprintf(stderr,"ddpOpenSocket: skt out of range\n");
    exit(0);
  }
  /* open the socket please */
  if ((refcd = abOpen(skt, s, iov, iovlen)) != 0) {
    if (dbug.db_ddp)
      fprintf(stderr,"ddp: open socket - socket open failed: %d\n", refcd);
    return(refcd);		/* return error if failed */
  }
  s = *skt;			/* real socket */
  if (dbug.db_ddp)
    fprintf(stderr, "ddp: open socket: opened socket %d\n", s);
  /* add default or user's listener */
  iov[IOV_DDP_LVL].iov_base = (caddr_t)&ddph; /* install */
  iov[IOV_DDP_LVL].iov_len = ddpSize; /* install */
  ddpl[s].lproc = NILPROC;
  ddpl[s].flags = 0;
  return(noErr);
}


ddpinstlistener(s, sktlis, flags)
int s;
ProcPtr sktlis;
int flags;
{
  int defDDPlis();

  ddpl[s].lproc = ((sktlis == NILPROC) ? defDDPlis : sktlis); 
  ddpl[s].flags = flags;
  return(noErr);			/* and return */
}


OSErr
DDPOpenSocket(skt,sktlis)
int *skt;
ProcPtr sktlis;
{
  OSErr err;

  /* call iov routine with default DDP iov */
  err = iDDPOpenSocketIOV(skt, rddpiov, DDPIOVLEN);
  if (err == noErr) {
    ddpinstlistener(*skt, sktlis, DDPL_OLDINTERFACE);
  }
  return(err);
}

OSErr
DDPOpenSocketIOV(skt, sktlis, iov, iovlen)
int *skt;
ProcPtr sktlis;
struct iovec *iov;
int iovlen;
{
  OSErr err;

  err = iDDPOpenSocketIOV(skt, iov, iovlen);
  if (err == noErr) {
    ddpinstlistener(*skt, sktlis, 0);
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
      fprintf(stderr, "ddpRemLis: Socket out of range\n");
    return(ddpSktErr);
  }
  ddpl[skt].lproc = NILPROC;	/* no procedure */
  /* close out the socket and return any errors */
  return(abClose(skt));
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
  DDP ddp;
  ddpProto *dpr;
  int i;
  ddpchksumtype chksum;

  dpr = &abr->proto.ddp;
  ddp.length = htons(ddpSize+dpr->ddpReqCount);
  ddp.dstNet = dpr->ddpAddress.net;
  ddp.dstNode = dpr->ddpAddress.node;
  ddp.dstSkt = dpr->ddpAddress.skt;
  ddp.srcNet = this_net;
  ddp.srcNode = this_node;
  ddp.srcSkt = dpr->ddpSocket;
  ddp.type = dpr->ddpType;
  if (dochecksum) {
    ddp_chksum(&ddp.dstNet, ddpSize-4, 0, chksum);
    for (i=IOV_DDP_LVL+1; i < iovl; i++)
      ddp_chksum(iov[i].iov_base, iov[i].iov_len, chksum, chksum);
    if (chksum == 0) chksum = 0xffff;
    ddp.checksum = htons(chksum);
  } else {
    ddp.checksum = 0;
  }
  iov[IOV_DDP_LVL].iov_base = (caddr_t) &ddp; /* DDP header */
  iov[IOV_DDP_LVL].iov_len = ddpSize;	       
  return(routeddp(iov, iovl));
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
 * In the case of UDP encapsulated DDP packets, there is no LAP layer
 * and the upcall comes from the "network gateway" level (abkip)
 *
 * Can in with iov pointing to DDP header
*/

ddp_protocol(iov, iovlen, plen)
struct iovec *iov;
int iovlen;
int plen;
{
  byte skt;
  byte *p;
  AddrBlock addr;
  int i;
  int len;
  int cnt;
  int oldstyle;			/* hack */
  DDP *ddp;
  ddpchksumtype chksum;

  if (iovlen < 1 || iov->iov_len != ddpSize) /* iovlen==1 means just ddph */
    return;

  ddp = (DDP *)iov->iov_base; /* know aligned okay */
  len = ntohs(ddp->length) & 0x3ff; /* get the "real" length */
  if (plen < len || len < ddpSize) {		/* not enought data? */
    if (dbug.db_ddp)
      fprintf(stderr, "BAD PACKET: ddp reports more data than came in\n");
    return;			/* drop pkt */
  } else plen = len;  /* truncate if len < plen */

  if (dochecksum) {
    if (ddp->checksum != 0) {
      ddp_chksum(&ddp->dstNet, ddpSize-4, 0, chksum);
      len -= ddpSize;		/* drop ddp size off */
      for (i = 1 ; i < iovlen; i++) {
	cnt = min(len, iov[i].iov_len);
	ddp_chksum(iov[i].iov_base, cnt, chksum, chksum);
	if (cnt != iov[i].iov_len) /* out of data */
	  break;
	len -= cnt;
      }
      if (chksum == 0)
	chksum = 0xffff;
      if (ntohs(ddp->checksum) != chksum) {
	if (checksumerror_drop) {
	  fprintf(stderr,
		  "Checksum error: Incoming: %x, calculated %x [%d.%d]\n",
		  ntohs(ddp->checksum), chksum, ntohs(ddp->srcNet),
		  ddp->srcNode);
	  fprintf(stderr, "Dropping packet\n");
	  return;			/* drop packet */
	}
      }
    }
  }

  /* pass down the srcNet and srcNode of the incoming packet, so we can */
  /* cache information below based on the transport */
  abnet_cacheit(ddp->srcNet,ddp->srcNode);
  skt = ddp->dstSkt;

  if (ddpl[skt].lproc == NILPROC) { /* listener proc for socket */
    if (dbug.db_ddp)
      fprintf(stderr,"ddp_protocol: no socket listener for %d!\n", skt);
    return;
  }

  addr.net = ddp->srcNet;
  addr.node = ddp->srcNode;
  addr.skt = ddp->srcSkt;
  iov++;			/* skip ddp header */
  iovlen--;
  plen -= ddpSize;		/* reduce to data size */
  if (iovlen < 1)		/* nothing to send */
    return;			/* drop it */
  if (ddpl[skt].lproc) {	/* be postitive */
    if (ddpl[skt].flags & DDPL_OLDINTERFACE)
      (*ddpl[skt].lproc)(skt,ddp->type,iov->iov_base,plen,&addr);
    else
      (*ddpl[skt].lproc)(skt, ddp->type, iov, iovlen, plen, &addr);
  }
}

#ifndef INLINECHKSUM
/*
 * Compute a 16 bit checksum via the following algorithm:
 *  for each byte: sum = byte + sum (unsigned), rotate sum left
 *
 * Note: to complete the algorithm, the caller must use a value of 0xffff
 * if the checksum is zero
 *
 * note: the algorithm below works efficently on a vax, may not work
 *   particularly well on other machines
 *
*/
private word
do_ddp_chksum(p, cnt, sum)
register byte *p;
register int cnt;
word sum;
{
  register dword xsum = sum;

  while (cnt-- > 0) {
    /* add in new byte, clip off extraneous info, shift as half of rotate */
    xsum = ((xsum + *p++) & 0xffff) << 1;
    /* add in the 16th bit (in 17th position) */
    xsum |= (xsum >> 16);
  }
  return((word)xsum);
}
#endif

/*
 * Call with TRUE or FALSE - FALSE means ignore, TRUE means don't ignore
 *
 */

checksum_error(which)
boolean which;
{
  checksumerror_drop = which;
}

