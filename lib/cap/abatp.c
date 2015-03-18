/*
 * $Author: djh $ $Date: 1996/06/18 10:48:17 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abatp.c,v 2.9 1996/06/18 10:48:17 djh Rel djh $
 * $Revision: 2.9 $
 *
 */

/*
 * abatp.c - Appletalk Transaction Protocol
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Edit History:
 *
 *  June 15, 1986    CCKim	Created
 *  June 30, 1986    CCKim      Clean up, finish XO
 *  July 15, 1986    CCKim	Some more cleanup, fix bug with rsptimeout,
 *				allow incoming socket in atpsndreq
 *  July 28, 1986    CCKim      Make sure auto req socket gets deleted in child
 *  July 28, 1986    CCKim      Wasn't handling case when rspcb didn't have
 *	valid response very well, fix it.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#ifndef	linux
#include <sys/uio.h>
#endif	/* linux */
#include <sys/time.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include "abatp.h"


/*
 * both suns and pyramids are such that byte alignment issues force
 * use to copy around the atp header.  Reason it is ifdef'ed is that
 * we really don't want to do this if it isn't necessary
 *
 * Actually, what would be real nice is if the read was done with
 *  a readv and we had a chain of buffers, etc, but things weren't done
 *  that way.  Problem with this is that you really have to know too
 *  much at the lower levels to do it...
 *
 * CCK: actually not, we can assume we only get ddp packets
 * at the lower levels (when reading off a particular fd) or else
 * the base implementation is brain damaged.
 *
*/

OSErr ATPSndRequest();
OSErr cbATPSndRequest();
OSErr ATPOpenSocket();
OSErr ATPCloseSocket();
OSErr ATPGetRequest();
OSErr cbATPGetRequest();
OSErr ATPRspCancel();
OSErr ATPSndRsp();
OSErr cbATPSndRsp();
void ATPSetResponseTimeout();
/* OSErr ATPAddRsp(); */
private void atp_listener();
private void tcb_timeout();
private void rsptimeout();
private boolean handle_request();
private boolean handle_release();
private boolean handle_response();
private OSErr atpreqsend();
private OSErr atprelsend();
private OSErr atpxmitres();

private int atpreqskt = -1;
private int atpreqsktpid = 1;	/* pid 1 is init - hope it never runs this */
/* baseline value */
private dword atpresptimeout = RESPONSE_CACHE_TIMEOUT;
/* room for atp + user data + extra to make count vs. index */
#define ATPIOVLEN (IOV_ATP_SIZE+1)
private ATP atph;		/* atp header */
private byte atpdata[atpMaxData]; /* room for ATP data bytes */
private struct iovec ratpiov[ATPIOVLEN] = {
  {NULL, 0},			/* lap */
  {NULL, 0},			/* ddp */
  {(caddr_t)&atph, atpSize},	/* atp */
  {(caddr_t)atpdata, atpMaxData} /* atp user data */
};
private int delete_tcb_skt();
private int ATPWrite();

/*
 * ATPSndRequest
 *
 * Send a request to the remote ATP.  As documented by Apple, except
 * a non-zero atp request socket means to use that socket instead of
 * some socket we generate
 *
*/

OSErr
ATPSndRequest(abr, async)
ABusRecord *abr;
int async;				/* boolean - true means runs async */
{
  int to;

  if ((to = cbATPSndRequest(abr, NULL, 0L)) != noErr)
    return(to);

  if (async)
    return(noErr);		/* all done if asynchronous */

  to = abr->proto.atp.atpTimeOut;
  while (abr->abResult == 1)
    abSleep(to,TRUE);		/* wakeup on events */    

  return(abr->abResult);
}

OSErr
cbATPSndRequest(abr, callback, cbarg)
ABusRecord *abr;
int (*callback)();
caddr_t cbarg;
{
  atpProto *atpproto;
  TCB *tcb;

  atpproto = &abr->proto.atp;
  if ((atpproto->atpReqCount > atpMaxData) ||
      (atpproto->atpRspBDSPtr == NULL))
    return(atpLenErr);

  if (atpproto->atpSocket == 0) {
    if (atpreqskt < 0 || atpreqsktpid != getpid()) {
      if (atpreqskt >= 0) {
	delete_tcb_skt(atpreqskt); /* get rid of outstanding requests */
	DDPCloseSocket(atpreqskt);		/* make sure child cleans up */
      }
      atpreqskt = 0;
      if (DDPOpenSocketIOV(&atpreqskt,atp_listener,ratpiov,ATPIOVLEN)!=noErr) {
	atpreqskt = -1;		/* make sure */
	return(tooManySkts);
      }
      atpreqsktpid = getpid();	/* mark pid */
    }
    atpproto->atpSocket = atpreqskt;
  }

  /* should check buffer list */
  /* get a free tcb */
  if ((tcb = create_tcb(atpproto->atpSocket, abr, callback, cbarg)) == NULL)
    return(noDataArea);

  if (atpreqsend(tcb) < 0) {
    tcb->callback = NULL;	/* no callback! */
    delete_tcb(tcb);
    return(badATPSkt);
  }
  atpproto->atpNumRsp = 0;	/* make sure zero */
  abr->abResult = 1;
  Timeout(tcb_timeout,tcb,atpproto->atpTimeOut); /* q a timeout */
  return(noErr);
}

/*
 * ATPKillGetReq
 *
 * cancel an outstanding ATPGetRequest
 *
 */

OSErr
ATPKillGetReq(abr, async)
ABusRecord *abr;
boolean async;
{
  RqCB *rqcb;

  rqcb = find_rqcb_abr(abr);
  if (rqcb == NULL)
    return(cbNotFound);
  rqcb->abr->abResult = sktClosed;
  delete_rqcb(rqcb);
  return(noErr);
}

/*
 * ATPReqCancel
 *
 * cancel an outstanding ATPSndRequest 
 *
 */

OSErr
ATPReqCancel(abr, async)
ABusRecord *abr;
boolean async;
{
  TCB *tcb;

  tcb = find_tcb_abr(abr);
  if (tcb == NULL)
    return(cbNotFound);
  tcb->abr->abResult = sktClosed;
  /* should we actually close the socket? */
  remTimeout(tcb_timeout, tcb);
  delete_tcb(tcb);
  return(noErr);
}

/* 
 *
 * Responder code
 *
*/

/* 
 * ATPOpenSocket(AddrBlock *addr,int *skt)
 *
 * ATPOpenSocket opens a socket for the purpose of receiving requests.
 * "skt" contains the socket number of the socket to open, or zero if
 * dynamic allocation is desired.  "addr" contains a filter from which
 * requests will be accepts.  A 0 in the network number, node ID, or 
 * socket number field of the "addr" record acts as a "wild card."
 *
 * Note: if you are only going to send requests and receive response
 * from these requests, you do not need to open an ATP socket with
 * ATPOpenSocket.
 * 
*/

OSErr
ATPOpenSocket(addr,skt)
AddrBlock *addr;
int *skt;
{
  if (DDPOpenSocketIOV(skt,atp_listener,ratpiov, ATPIOVLEN) != noErr)
    return(tooManySkts);
  if (create_atpskt(*skt, addr, NULL) == NULL)
    return(noDataArea);
  return(noErr);
}

/*
 * ATPCloseSocket(int skt)
 *
 * ATPCloseSocket closes the responding socket whose number is
 * specified by "skt."  It releases the data structure associated
 * with all pending asynchronous calls involving that socket; these
 * calls are completed immediately and return the result code
 * sktClosed.
 * 
*/
OSErr
ATPCloseSocket(skt)
int skt;
{
  int v;
  RspCB *rspcb;
  RqCB *rqcb;

  while ((rspcb = find_rspcb_skt(skt)) != NULL) {
    if (rspcb->abr != NULL)
      rspcb->abr->abResult = sktClosed; /* completed */
    /* completion code set before rspcb so completion code set */
    killrspcb(rspcb);
  }
  while ((rqcb = find_rqcb(skt)) != NULL) {
    rqcb->abr->abResult = sktClosed;
    delete_rqcb(rqcb);
  }
  v = delete_atpskt(skt);	/* v is nominal amount of work to do */
  if (dbug.db_atp && v)
      fprintf(stderr,"atp: ****atpclose with %d on the queue...\n",v);
  (void)DDPCloseSocket(skt);	/* close the socket (drop codes) */
  return(noErr);		/* ignore any ddp error */
}

#ifdef ATPREQCACHE
private int Have_CPkt = 0;	/* 1 for pkt, -1 for re-using, 0 for none */
private u_char CPkt_skt;
private ATP CPkt_atp;
private u_char CPkt_buf[atpMaxData];
private int CPkt_buflen;
private AddrBlock CPkt_addr;

/*
 * The timeout handling is used for replaying
 * the cache packet or purging it.
 *
 */

void
CPkt_timeout(arg)
caddr_t arg;
{
  if (dbug.db_atp)
    fprintf(stderr, "atp: cache req pkt timeout p=%d c=%d\n",arg,Have_CPkt);

  /*
   * if a replay timeout and have a cache packet
   * then replay it
   *
   */
  if (arg == (caddr_t)0 && Have_CPkt) {
    if (dbug.db_atp)
      fprintf(stderr, "atp: re-handling cache pkt p=%d c=%d\n",arg,Have_CPkt);
    Have_CPkt = -1;
    handle_request(CPkt_skt, &CPkt_atp, CPkt_buf, CPkt_buflen, &CPkt_addr);
    remTimeout(CPkt_timeout, (caddr_t)1);
  }

  /*
   * no more cache packet
   *
   */
  Have_CPkt = 0;

  return;
}
#endif ATPREQCACHE

/*
 * ATPGetRequest(int skt, ABusRecord *abr, int async)
 *
 * ATPGetRequest sets up the mechanism to receive a request sent by
 * a remote node issuing ATPSndRequest or ATPRequest.  "skt" contains
 * the socket number of the socket that should listen for a request;
 * this socket must have been opened by calling ATPOpenSocket.
*/
OSErr
ATPGetRequest(abr,async)
ABusRecord *abr;
int async;			/* boolean - true means runs async */
{
  int to;

  if ((to = cbATPGetRequest(abr, NULL, 0L)) != noErr)
    return(to);
  if (async)
    return(noErr);
  while (abr->abResult == 1) 	/* wait for completion */
    abSleep(400,TRUE);		/* wakeup on events */
  return(abr->abResult);	/* and return result */
}

OSErr
cbATPGetRequest(abr, callback, cbarg)
ABusRecord *abr;
int (*callback)();
caddr_t cbarg;
{
  /* 
   * Only one listen request per socket is allowed - more than one
   * really does lead to an ambiguity problem - maybe queue them
   * up in the future?  (What to do if one is blocking and other is
   * not?)
   */

  abr->abResult = 1;		/* not completed */
  if (create_rqcb(abr->proto.atp.atpSocket,abr,callback,cbarg) == NULL)
    return(noDataArea);
#ifdef ATPREQCACHE
  /*
   * if cache entry exists and matches,
   * fire up immediate timeout
   *
   */
  if (Have_CPkt && (CPkt_skt == abr->proto.atp.atpSocket)) {
    remTimeout(CPkt_timeout, (caddr_t)1);
    Timeout(CPkt_timeout, (caddr_t)0, 0);
  }
#endif ATPREQCACHE
  /* no Timeout on this operation... */
  return(noErr);
}


/*
 * Cancel a previous SndRsp
 *
 * (Possibly kill off socket?)
 *
*/
OSErr
ATPRspCancel(abr, async)
ABusRecord *abr;
int async;
{
  RspCB *rspcb;

  if ((rspcb = find_rspcb_abr(abr)) == NULL)
    return(cbNotFound);
  abr->abResult = noErr;	/* must be done before killrspcb because */
				/* of callback */
  return(killrspcb(rspcb));	/* ignore error for now */
}

/*
 * kill off a RSPCB transation
 *
*/
killrspcb(rspcb)
RspCB *rspcb;
{
  remTimeout(rsptimeout, rspcb);    
  delete_rspcb(rspcb);		/* remove it from the list */
  return(noErr);		/* no errror */
}


/* 
 * ATP Send Response
 *
*/
OSErr
ATPSndRsp(abr, async)
ABusRecord *abr;
int async;			/* boolean - true means runs async */
{
  int to ;
  if ((to = cbATPSndRsp(abr, NULL, 0L)) < 0)
    return(to);
  if (!async) {
    while (abr->abResult == 1)
      abSleep(400, TRUE);
    return(abr->abResult);
  }
  return(noErr);
}

OSErr
cbATPSndRsp(abr, callback, cbarg)
ABusRecord *abr;
int (*callback)();
caddr_t cbarg;
{
  atpProto *atpproto;
  RspCB *rspcb;
  
  /* check socket and data lengths */

  /* should check atpNumBufs and atpBDSSize conform */
  /* note ddp errors are supposed to be ignored...*/
  /* try sending first to ensure socket is open, ow we will have */
  /* it running back there without being able to function properly */
  atpproto = &abr->proto.atp;

  if (atpxmitres(abr, 0xff >> (8-atpproto->atpNumBufs)) < 0)
    return(badATPSkt);
  /* Find active rspcb and put responses in cache for rexmit, start timeout */
  rspcb = find_rspcb(atpproto->atpSocket, atpproto->atpTransID, 
		     &atpproto->atpAddress);
  if (rspcb != NULL) {
    rspcb->abr = abr;		/* save this away */
    rspcb->callback = callback;
    rspcb->cbarg = cbarg;
    abr->abResult = 1;
    remTimeout(rsptimeout, rspcb);
    if (atpresptimeout)
      Timeout(rsptimeout, rspcb, atpresptimeout);
  } else {
    rspcb = create_rspcb(0, 0, NULL); /* create a dummy rspcb */
    rspcb->abr = abr;		/* save this away */
    rspcb->callback = callback;
    rspcb->cbarg = cbarg;
    abr->abResult = noErr;	/* if we are at least once.... */
    /* timeout immediately */
    Timeout(rsptimeout, rspcb, 0);
  }

  return(noErr);
}


/*
 * Set the atp response cache timeout value
 *
*/
void
ATPSetResponseTimeout(value)
dword value;
{
  atpresptimeout = value;
}


#ifdef NOTDEFINEDATALL
/*
 * DO NOT USE THIS ROUTINE - SOME RETHINKING IS NEEDED TO ALLOW
 * IT TO BE INTEGRATED!!!!
*/
OSErr
ATPAddRsp(fd, abr, async)
int fd;
ABusRecord *abr;
int async;			/* boolean - true means runs async */
{
  ATP atp;
  atpProto *atpproto;
  RspCB *rspcb;

  /* check socket and data lengths */

  atp.control = atpRspCode;	/* response */
  atpproto = &abr->proto.atp;
  atp.control |= (atpproto->fatpEOM) ? atpEOM : 0;
  atp.transID = atpproto->atpTransID;
  atp.bitmap = atpproto->atpNumRsp;
  atp.userData = atpproto->atpUserData;
  return(ATPWrite(atpproto,&atp,
		  atpproto->atpDataPtr,atpproto->atpReqCount));
}

#endif /* NOT YET IMPLEMENTED */


/*
 * atp_Listener -
 *   here we watch for incoming ATP packets and demux them to the
 *   appropriate handler (request, response, release)
 * 
 * Since we opened with DDPOpenSocketIOV, our input will be in an
 * iovec with the first pointing to the atp header, second to the
 * atp data.
 *
*/
private void
atp_listener(skt,type,iov,iovlen,packet_length,addr)
u_char skt;
u_char type;
struct iovec *iov;
int iovlen;
int packet_length;
AddrBlock *addr;
{
  ATP *atp;
  char *pkt_data;			/* pointer to user data */

  /* Check the packet type - see if it TReq or TRel (others are */
  /* considered illegal?) */
  
  if (type != ddpATP || iovlen < 1 || packet_length < atpSize)
    return;			/* drop it */

  atp = (ATP *)iov->iov_base; /* get atp header */
  iov++;			/* move past atp header */
  iovlen--;			/* move past atp header */
  packet_length -= atpSize;	/* reduce to data Size */
  if (iovlen < 1) {		/* can't be from us! */
    if (dbug.db_atp) {
      fprintf(stderr,"atp: [ATP_LISTENER: net=%d.%d, node=%d, skt=%d]\n",
	      nkipnetnumber(addr->net),nkipsubnetnumber(addr->net),
	      addr->node,addr->skt);
      fprintf(stderr,"atp: internal error: iovlen < 1 before handle\n");
    }
    return;
  }
  pkt_data = iov->iov_base; /* get user  data */
  if (dbug.db_atp)
    fprintf(stderr,"atp: [ATP_LISTENER: net=%d.%d, node=%d, skt=%d]\n",
	    nkipnetnumber(addr->net),nkipsubnetnumber(addr->net),
	    addr->node,addr->skt);

  switch (atp->control & atpCodeMask) {
  default:
    return;			/* drop packet */
  case atpRspCode:
    handle_response(skt, atp, pkt_data, packet_length, addr);
    break;
  case atpReqCode:		/* TReq case: */
    handle_request(skt, atp, pkt_data, packet_length, addr);
    break;
  case atpRelCode:
    handle_release(skt, atp->transID, addr);
    break;
  }
}

/*
 * tcb_timeout(int tcbno)
 *
 * tcb_timeout is called via the Timeout() mechanism when
 * an ATP response has been pending for too long.
 *
*/ 
private void
tcb_timeout(tcb)
TCB *tcb;
{
  u_char *retries;

  if (dbug.db_atp)
    fprintf(stderr,"atp: tcb_timeout: here with TCB %x\n",tcb);

  retries = &tcb->abr->proto.atp.atpRetries; /* get retries pointer */
  if (*retries != 0) {		/* exceeded retries? */
    *retries -= (*retries == 255) ? 0 : 1;
    atpreqsend(tcb);		/* no, queue up another */
    Timeout(tcb_timeout,tcb,tcb->abr->proto.atp.atpTimeOut);
  } else {
    tcb->abr->abResult = reqFailed;
    delete_tcb(tcb);
  }
}

/*
 * rsptimeout
 *
 * Handle the timeout of a rspcb.  Basically, just note that
 * a release wasn't sent by the remote if we ever responded
 * to the incoming request.  Not so clear what we should
 * do if the response was never issued, so we just drop in
 * that case....
*/
private void
rsptimeout(rspcb)
RspCB *rspcb;
{
  /* if skt is zero, then dummy rspcb */
  if (rspcb->atpsocket != 0) {
    if (dbug.db_atp)
      fprintf(stderr,"atp: removing rspcb %x for timeout\n", rspcb);
    /* assuming we tried to respond! */
    if (rspcb->abr != NULL)
      rspcb->abr->abResult = noRelErr; /* completed */
  } else {
    if (dbug.db_atp)
      fprintf(stderr,"atp: removing dummy rspcb %x\n", rspcb);
  }
  delete_rspcb(rspcb);		/* okay! */
}


/*
 * handle_request
 *
 * handle an incoming ATP request packet
 *
*/
private boolean
handle_request(skt, atp, databuf, dblen, addr)
int skt;
ATP *atp;
char *databuf;
int dblen;
AddrBlock *addr;
{
  RspCB *rspcb;
  RqCB *rqcb;
  atpProto *ap;

  if (find_atpskt(skt, addr) == NULL) {
    if (dbug.db_atp)
      fprintf(stderr,"atp: handle_request: Socket was never opened or \
address mismatch\n");
    return(FALSE);
  }

  if (dbug.db_atp)
    fprintf(stderr, "atp: Incoming request on socket %d with TID %d\n",
	    skt, ntohs(atp->transID));

  /*
   * If the packets has its XO bit set and a matching RspCB exists 
   * then:
   *  - retransmit all response pkts REQUESTED
   *  - restart release timer
   *  - exit
   */
  if (atp->control & atpXO) {
    rspcb = find_rspcb(skt, atp->transID, addr);
    if (rspcb != NULL) {
      if (dbug.db_atp)
	fprintf(stderr,"atp: exactly once: rspcb %x\n", rspcb);
      /* we should really record the average number of requests that */
      /* have "lost" packets and average number lost per response size */
      if (dbug.db_atp)
	fprintf(stderr,"atp: *incoming bitmap: %x*\n",atp->bitmap);
      if (rspcb->abr != NULL)	/* response to rexmit? */
	atpxmitres(rspcb->abr, atp->bitmap); /* do it */
      remTimeout(rsptimeout, rspcb);
      if (atpresptimeout)
	Timeout(rsptimeout, rspcb, atpresptimeout);
      /* if we allowed "parts" of response at a time, then we would */
      /* have to return the bitmap if sts was set previously */
      /* (e.g. need atpAddRsp for this to be meaningful) */
      return(FALSE);
    } else {
      if (dbug.db_atp)
	fprintf(stderr,"atp: exactly once transaction: no rspcb\n");
    }
  }
  /* If RqCB doesn't exist for the local socket or if the packet's */
  /* source address doesn't match the admissible requestor address in */
  /* the RqCB then ignore the packet and exit */
  /*XXX should do address filtering here */
#ifdef ATPREQCACHE
  if ((rqcb = find_rqcb(skt)) == NULL) {
    /*
     * a RqCB does not exist, maybe it will exist
     * shortly so cache this packet for reply if
     * it is an XO packet
     *
     */
    if ((atp->control & atpXO) && Have_CPkt >= 0) {
      if (dbug.db_atp)
	fprintf(stderr, "atp: cacheing XO req packet\n");
      /*
       * clear purge timer if we have
       * an old cache packet
       *
       */
      if (Have_CPkt)
	remTimeout(CPkt_timeout, (caddr_t)1);
      CPkt_skt = skt;
      CPkt_atp = *atp;
      bcopy(databuf, (char *)CPkt_buf, dblen);
      CPkt_buflen = dblen;
      CPkt_addr = *addr;
      Have_CPkt = 1;
      /*
       * set purge timer, retransmit
       * rate is 2 seconds
       *
       */
      Timeout(CPkt_timeout, (caddr_t)1, sectotick(2)-1);
    }
    return(FALSE);		/* drop pkt */
  }
#else  ATPREQCACHE
  if ((rqcb = find_rqcb(skt)) == NULL)
    return(FALSE);		/* drop pkt */
#endif ATPREQCACHE

  /*
    If packet's XO bit is set then create a RspCB and start its
    release timer
    */
  if (atp->control & atpXO) {
    rspcb = create_rspcb(skt, atp->transID, addr);
    if (dbug.db_atp)
      fprintf(stderr,"atp: XO: created rspcb %x on socket %d, TID %d\n",
	      rspcb, skt, ntohs(atp->transID));
    if (atpresptimeout)
      Timeout(rsptimeout, rspcb, atpresptimeout);
  }

  /* Notify the client about the arrival of the request and destroy */
  /* the corresponding RqCB */

  ap = &rqcb->abr->proto.atp;	/* handle on the protocol */

  ap->atpAddress = *addr;	/* copy address for user */
  ap->atpBitMap = atp->bitmap;
  ap->atpTransID = atp->transID;
  ap->atpUserData = atp->userData;
  ap->fatpXO = (atp->control & atpXO) ? 1 : 0;
  ap->atpActCount = min(ap->atpReqCount, dblen);
  /* NULL dataPtr just means he doesn't care about the data - just header */
  if (ap->atpDataPtr && databuf)
    bcopy(databuf,ap->atpDataPtr,ap->atpActCount);
  else
    ap->atpActCount = 0;
  rqcb->abr->abResult = noErr;
  delete_rqcb(rqcb);
  return(TRUE);
}

/*
 * handle_release
 *
 * handle in incoming ATP release packet
 *
*/
private boolean
handle_release(skt, tid, addr)
int skt;
int tid;
AddrBlock *addr;
{
  RspCB *rspcb;

  if (dbug.db_atp)
    fprintf(stderr,"atp: removing rspcb for trel on skt %d, TID %d\n",
	    skt, ntohs(tid));

  rspcb = find_rspcb(skt, tid, addr);
  if (rspcb == NULL) {	/* nothing to do */
    if (dbug.db_atp)
      fprintf(stderr,
	      "atp: rqcb_listener: atp rel on skt %d, TID %d, with no rspcb\n",
	      skt, ntohs(tid));
    return(FALSE);
  }
  if (rspcb->abr != NULL)	/* never tried to respond... */
    rspcb->abr->abResult = noErr; /* completed */
  remTimeout(rsptimeout, rspcb);    
  delete_rspcb(rspcb);		/* remove it from the list */
  return(TRUE);
}

/*
 * handle_response
 * 
 * handle an incoming ATP response packet
 *
*/
private boolean
handle_response(skt, atp, databuf, dblen, addr)
int skt;
ATP *atp;
char *databuf;
int dblen;
AddrBlock *addr;
{
  TCB *tcb;
  int seqno;
  BDSPtr bds;
  atpProto *ap;

  /* a) find matching TCB */
  /* b) no matching tcb means drop pkt */
  if ((tcb = find_tcb(skt, atp->transID)) == NULL) {
    if (dbug.db_atp)
      fprintf(stderr, "atp: no matching tid for response tid %d\n");
    return(FALSE);		/* drop packet */
  }

  ap = &tcb->abr->proto.atp;

  if (bcmp(addr, &ap->atpAddress, sizeof(AddrBlock)) != 0) {
    if (dbug.db_atp) {
      fprintf(stderr, "atp: security: response not from requested address\n");
      fprintf(stderr, "atp: expected: [net %d.%d, node %d, skt %d]\n",
	      nkipnetnumber(ap->atpAddress.net),
	      nkipsubnetnumber(ap->atpAddress.net),
	      ap->atpAddress.node, ap->atpAddress.skt);
    }
    return(FALSE);		/* drop packet */
  }

  /* Check pkt is expected by checking pkt sequence no. against bitmap */
  if (((1 << atp->bitmap) &tcb->atp.bitmap) == 0) {
    if (dbug.db_atp)
      fprintf(stderr,
      "atp: response sequence %d not expected or already received\n",
	      atp->bitmap);
    return(FALSE);		/* drop packet */
  }

  /* Clear corresponding bit in bitmap to note we got the packet */
  tcb->atp.bitmap &= ~(1<<atp->bitmap);	/* okay, got our packet */
  ap->atpNumRsp++;		/* increment count */

  /* EOM - don't expect any pkts with higher sequence number */
  if (atp->control & atpEOM)
    tcb->atp.bitmap &= ~((0xff >> atp->bitmap) << atp->bitmap);

  seqno = atp->bitmap;		/* get sequence number */
  remTimeout(tcb_timeout,tcb); /* remove timeout... */

  /* move data into correct response buffer */
  /* error checking is minimal, but keeps code from core dumping */
  /* don't check seqno against numbufs - all okay if bitmap matches */
  /* numbufs okay (should check at call time, but tuff) */
  bds = &(tcb->abr->proto.atp.atpRspBDSPtr[seqno]);
  bds->userData = atp->userData;
  bds->dataSize = dblen;	/* set size to what came in */
  if (bds->buffPtr && databuf)	/* keep us from doing bad things */
    /* but only copy what fits */
    bcopy(databuf,bds->buffPtr,min(bds->buffSize, dblen));
  else
    bds->dataSize = 0;
  /* This will probably cause problems, but if we have a incoming pkt */
  /* that is larger than the bds size, then we simply truncate the incoming */
  /* pkt to bds->buffSize, but record the actual length */
  if (dbug.db_atp)
    fprintf(stderr,"atp: resp: tid %d, seqno %d, len %d\n",
	    ntohs(tcb->atp.transID), seqno, bds->dataSize);

  if (atp->control & atpSTS) {	/* handle status control */
    if (dbug.db_atp)
      fprintf(stderr, "atp: handle_response: sts set, resending request\n");
    atpreqsend(tcb);		/* by resending request */
  }

  /* bitmap = 0 means that we are all done */
  if (tcb->atp.bitmap == 0) {
    ap->fatpEOM = atp->control & atpEOM ? 1 : 0;
    tcb->abr->abResult = noErr;
    /* handle XO with trel's  */
    if (ap->fatpXO)
      atprelsend(tcb);
    delete_tcb(tcb);
    return(TRUE);
  }

  Timeout(tcb_timeout,tcb,ap->atpTimeOut); /* else new timer */
  return(FALSE);
}

/*
 * atpreqsend
 *
 * Send the request packet specified by the current TCB.
 *
*/
private OSErr
atpreqsend(tcb)
TCB *tcb;
{
  atpProto *ap;

  if (dbug.db_atp)
    fprintf(stderr,"atp: Sending request: tid %d\n",ntohs(tcb->atp.transID));

  ap = &tcb->abr->proto.atp;
  tcb->atp.control = atpReqCode | (ap->fatpXO ? atpXO : 0);
  return(ATPWrite(ap,&tcb->atp,ap->atpDataPtr,ap->atpReqCount));
}

/*
 * atprelsend
 *
 * Send a release on current request specified by the TCB
 *
 * Assumes that we need not send data if any was associated with packet.
*/
private OSErr
atprelsend(tcb)
TCB *tcb;
{
  atpProto *ap;

  if (dbug.db_atp)
    fprintf(stderr,"atp: Sending rel: tid %d\n",ntohs(tcb->atp.transID));

  ap = &tcb->abr->proto.atp;
  tcb->atp.control = atpRelCode; /* release on request */
  return(ATPWrite(ap, &tcb->atp, (char *)0, 0));
}


/*
 *
 * atpxmitres - transmit response
 *
 *  Send back a response to a request.  Send only responses specified
 *  by the bitmap
 *
*/
private OSErr
atpxmitres(abr, bitmap)
ABusRecord *abr;
BitMapType bitmap;
{
  int i, err;
  BDS *bds;
  ATP atp;
  atpProto *atpproto;
  
  atpproto = &abr->proto.atp;
  atp.control = atpRspCode;	/* mark as response */
  atp.transID = atpproto->atpTransID; /* give TID */
  for (i=0; i < (int)atpproto->atpNumBufs; i++) {
    if ( ( (bitmap >> i) & 0x1) ==  0)
      continue;
    if (i==(atpproto->atpNumBufs-1))
      atp.control |= (atpproto->fatpEOM) ? atpEOM : 0;
    atp.bitmap = i;		/* sequence */
    bds = &atpproto->atpRspBDSPtr[i];
    atp.userData = bds->userData;
    err = ATPWrite(atpproto,&atp,bds->buffPtr,bds->buffSize);
    if (err < 0)
      return(err);
  }
  return(0);
}



/*
 * abatpaux.c - Appletalk Transaction Protocol Auxillary routines
 *
 * Provides management of:
 *   o RspCB (response control block)
 *   o Atp responding sockets
 *   o Request control blocks
 *   o Transmission Control Blocks
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * Edit History:
 *
 *  June 30, 1986    CCKim	Created
 *  Aug 1, 1986      CCKim      Make p-v on sockets history
 *  March 1986	     CCKim	Merge into abatp module...
 *
*/

#ifdef notdef
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include "abatp.h"
#endif
/*
 * ATPSKT management routines - used to manage the sockets
 * opened by ATPOpenSocket.
 *
 * Dynamically allocate new space for sockets in multiples of NUMATPSKT
 *
*/
private AtpSkt *atpsktlist = NULL;
private int numatpskt = 0;

/*
 * Establish an ATP responding socket block
 * 
*/
private AtpSkt *
create_atpskt(skt, raddr)
int skt;
AddrBlock *raddr;
{
  int i;
  char *calloc();
  AtpSkt *atpskt;
  register AtpSkt *ap;

  for (i=0; i < numatpskt; i++)
    if (atpsktlist[i].inuse == 0)
      break;
  if (i == numatpskt) {
    /* make more, use calloc() instead of realloc() */
    if ((ap = (AtpSkt *)calloc(numatpskt+NUMATPSKT, sizeof(AtpSkt))) == NULL)
      return(NULL);
    if (numatpskt > 0)
      bcopy((char *) atpsktlist, (char *)ap, numatpskt*sizeof(AtpSkt));
    if (atpsktlist != NULL)
      free((char *) atpsktlist);
    atpsktlist = ap;
    numatpskt += NUMATPSKT;
  }
  atpskt = &atpsktlist[i];
  atpskt->inuse = 1;		/* true! */
  atpskt->skt = skt;
  atpskt->addr = *raddr;
  atpskt->usecount = 0;
  return(atpskt);
}

/*
 * Find a ATP responding socket and return the address of the block
 * if the address mask of the socket specified in the ATPOpenSocket
 * allows receiving requests from the specified remote address (raddr)
 *  Returns NULL o.w.
*/
private AtpSkt *
find_atpskt(skt, raddr)
int skt;
AddrBlock *raddr;
{
  int i;
  AddrBlock *ab;

  for (i=0; i < numatpskt; i++)
    if (atpsktlist[i].inuse != 0 && skt == atpsktlist[i].skt)
      break;
  if (i==numatpskt)
    return(NULL);
  ab = &atpsktlist[i].addr;
  if ((ab->net == 0 || ab->net == raddr->net) &&
      (ab->node==0 || ab->node == raddr->node) &&
      (ab->skt==0 || ab->skt==raddr->skt))
    return(&atpsktlist[i]);
  else return(NULL);
}

/*
 * remove a ATP responding socket block
 */
private int
delete_atpskt(skt)
int skt;
{
  int i;
  for (i=0; i < numatpskt; i++)
    if (atpsktlist[i].inuse && atpsktlist[i].skt == skt) {
      atpsktlist[i].inuse = 0;
      if (dbug.db_atp)
	fprintf(stderr,"atp: delete_atpskt: deleting socket %d\n",skt);
      return(0);
    }
  return(-1);
}


/*
 * Response Control Block handling routines
 *
 * Organized as a hash table with lists hanging off the buckets
 * 
 * Assumptions: the hash function assumes that skt's tend to be "clustered"
 *
*/
/* RSPCBLIST is a hash list with one bucket for each socket at present */
/* Bucket lists are not ordered */
private QElemPtr rspcblist[NUMRspCB]; 
private QElemPtr rspcb_free; /* list of free items */
#define rspcb_hash(skt)  ((skt)	% NUMRspCB)

/*
 * Create a rspcb and insert it into rspcblist at the access point
 * defined by socket.
 *
 * Doesn't check to see if the rspcb already exists
*/
private RspCB *
create_rspcb(skt, tid, raddr)
int skt, tid;
AddrBlock *raddr;
{
  RspCB *rspcb;

  if ((rspcb = (RspCB *)dq_head(&rspcb_free)) == NULL &&
      (rspcb = (RspCB *) malloc(sizeof(RspCB))) == NULL) {
	fprintf(stderr,"atp: panic: create_rspcb: out of memory\n");
	exit(8);
      }

  if (dbug.db_atp)
    fprintf(stderr,"atp: create_rspcb: create %x\n",rspcb);

  rspcb->atpTransID = tid;
  if (raddr)			/* dummy rspcb doesn't have */
    rspcb->atpAddress = *raddr;	/* save remote address */
  rspcb->atpsocket = skt;	/* remember this */
  rspcb->callback = NULL;
  rspcb->cbarg = (caddr_t)0;
  rspcb->abr = NULL;
  q_tail(&rspcblist[rspcb_hash(skt)], &rspcb->link); /* add to queue */
  return(rspcb);
}

/*
 * remove a rspcb from the active list
 *
*/
private
delete_rspcb(rspcb)
RspCB *rspcb;
{
  if (dbug.db_atp)
    fprintf(stderr,"atp: delete_rspcb: deleting %x\n",rspcb);

  dq_elem(&rspcblist[rspcb_hash(rspcb->atpsocket)], &rspcb->link);
  if (rspcb->callback != NULL)
    (*rspcb->callback)(rspcb->abr, rspcb->cbarg);
  q_tail(&rspcb_free, &rspcb->link); /* add to free list */
}

/*
 * Find the rscb corresponding to the specified TID and raddr
 *
*/
struct rspcb_match_info {
  int skt, tid;
  AddrBlock *addr;
};

private boolean
match_rspcb(rspcb, info)
RspCB *rspcb;
struct rspcb_match_info *info;
{
  return(info->skt == rspcb->atpsocket &&
	 info->tid == rspcb->atpTransID &&
	 bcmp(info->addr, &rspcb->atpAddress, sizeof(AddrBlock)) == 0);
   
}

private RspCB *
find_rspcb(skt, tid, raddr)
int skt, tid;
AddrBlock *raddr;
{
  struct rspcb_match_info info;

  info.tid = tid, info.skt = skt, info.addr = raddr;
  return((RspCB *)q_mapf(rspcblist[rspcb_hash(skt)], match_rspcb, &info));
}

/*
 * Find the rscb corresponding to the specified abr
 *
*/
private boolean
match_rspcb_abr(rspcb, abr)
RspCB *rspcb;
ABusRecord *abr;
{
  return(abr == rspcb->abr);
}

private RspCB *
find_rspcb_abr(abr)
ABusRecord *abr;
{

  return((RspCB *)q_mapf(rspcblist[rspcb_hash(abr->proto.atp.atpSocket)],
			 match_rspcb_abr, abr));
}

/*
 * find any rspcb associated with the specified socket
 *
*/
private boolean
match_rspcb_skt(rspcb, skt)
RspCB *rspcb;
void *skt;
{
  int sk = (int)skt;
  return(sk == rspcb->atpsocket);
}

private RspCB *
find_rspcb_skt(skt)
int skt;
{
  return((RspCB *)q_mapf(rspcblist[rspcb_hash(skt)], match_rspcb_skt, 
					(void *)skt));
}



/*
 * Request control block handling routines
 *
 * Organized as a hash table with queues off each bucket
 *
*/

/* RQCBLIST is a hash list with one bucket for each socket at present */
/* Bucket lists are not ordered */

private QElemPtr rqcblist[NUMRqCB]; 
private QElemPtr rqcb_free;	/* list of free items */
#define rqcb_hash(skt)  ((skt) % NUMRqCB)

/*
 * Create a rqcb and insert it into rqcblist at the access point
 * defined by socket.
 *
 * Doesn't check to see if the rqcb already exists
*/
private RqCB *
create_rqcb(skt, abr, callback, cbarg)
int skt;
ABusRecord *abr;
int (*callback)();
caddr_t cbarg;
{
  RqCB *rqcb;

  if ((rqcb = (RqCB *)dq_head(&rqcb_free)) == NULL &&
      (rqcb = (RqCB *) malloc(sizeof(RqCB))) == NULL) {
	fprintf(stderr,"atp: panic: create_rqcb: out of memory\n");
	exit(8);
      }

  if (dbug.db_atp)
    fprintf(stderr,"atp: creat_rqcb: create %x\n",rqcb);

  rqcb->atpsocket = skt;
  rqcb->abr = abr;
  rqcb->callback = callback;
  rqcb->cbarg = cbarg;
  q_tail(&rqcblist[rqcb_hash(skt)], &rqcb->link); /* add to queue */
  return(rqcb);
}

/*
 * Find the first RqCB found for the socket
 *
 */

private boolean
match_rqcb_abr(rqcb, abr)
RqCB *rqcb;
ABusRecord *abr;
{
  return(abr == rqcb->abr);
}

private RqCB *
find_rqcb_abr(abr)
ABusRecord *abr;
{
  return((RqCB *)q_mapf(rqcblist[rqcb_hash(abr->proto.atp.atpSocket)],
	match_rqcb_abr, abr));
}

private boolean
match_rqcb(rqcb, skt)
RqCB *rqcb;
int skt;
{
  return(skt == rqcb->atpsocket);
}

private RqCB *
find_rqcb(skt)
int skt;
{
  return((RqCB *)q_mapf(rqcblist[rqcb_hash(skt)], match_rqcb, (void *)skt));
}

/*
 * remove the specified rqcb from the active list
 *
 */

private
delete_rqcb(rqcb)
RqCB *rqcb;
{

  if (dbug.db_atp)
    fprintf(stderr,"atp: delete_rqcb: deleting %x\n",rqcb);

  dq_elem(&rqcblist[rqcb_hash(rqcb->atpsocket)], &rqcb->link);
  if (rqcb->callback != NULL)
    (*rqcb->callback)(rqcb->abr, rqcb->cbarg);
  q_tail(&rqcb_free, &rqcb->link); /* add to free list */
}




/*
 * TCBlist is a simple list of items
 *
*/

private QElemPtr tcblist;
private QElemPtr tcb_free;

private u_short next_TID = 0;	/* 16 bits of tids */
private int tidded = 0;		/* have we randomized the tid yet? */

private TCB *
create_tcb(skt, abr, callback, cbarg)
int skt;
ABusRecord *abr;
int (*callback)();
caddr_t cbarg;
{
  TCB *tcb;
  atpProto *atpproto;
  int i;

  if ((tcb = (TCB *)dq_head(&tcb_free)) == NULL &&
      (tcb = (TCB *) malloc(sizeof(TCB))) == NULL) {
	fprintf(stderr,"atp: panic: create_tcb: out of memory\n");
	exit(8);
      }

  if (dbug.db_atp)
    fprintf(stderr,"atp: create_tcb: creating %x\n",tcb);

  tcb->abr = abr;
  atpproto = &abr->proto.atp;
  if (tidded)  {
    for (i=0; i < 65536; i++) {
      next_TID = (u_short)((int)next_TID+1) % 65535; /* mod 2^16-1 */
      if (find_tcb(skt, next_TID) == NULL)
	break;
    }
    if (i==65536) {
      fprintf(stderr,"atp: Fatal error:\n");
      fprintf(stderr,"atp: All TIDs are in use, this is highly improbable\n");
      exit(9);
    }
  } else {
    /*
     * randomly get first tid (preferably not numerically
     * close to another process started at the same time)
     *
     */
    next_TID = (time(0) % 65535) ^ ((getpid() & 0xff) << 8);
    tidded = TRUE;
  }
  tcb->atp.transID = htons(next_TID);
  tcb->atp.bitmap = 0xff >> (8-atpproto->atpNumBufs);
  tcb->atp.userData = atpproto->atpUserData;
  tcb->callback = callback;
  tcb->cbarg = cbarg;
  tcb->skt = skt;
  q_tail(&tcblist, &tcb->link);	/* add to queue */
  return(tcb);
}


struct tcb_match_info {
  int tid;
  int skt;
};

private boolean
match_tcb(tcb, info)
TCB *tcb;
struct tcb_match_info *info;
{
  return(info->tid == tcb->atp.transID && info->skt == tcb->skt);
}

private TCB *
find_tcb(skt, tid)
int skt;
int tid;
{
  struct tcb_match_info info;
  info.tid = tid, info.skt = skt;
  return((TCB *)q_mapf(tcblist, match_tcb, &info));
}


private boolean
match_tcb_abr(tcb, abr)
TCB *tcb;
ABusRecord *abr;
{
  return(abr == tcb->abr);
}

private TCB *
find_tcb_abr(abr)
ABusRecord *abr;
{
  return((TCB *)q_mapf(tcblist, match_tcb_abr, abr));
}

private
delete_tcb(tcb)
TCB *tcb;
{
  if (dbug.db_atp)
    fprintf(stderr,"atp: delete_tcb: deleting %x\n",tcb);

  dq_elem(&tcblist, &tcb->link);
  if (tcb->callback != NULL)
    (*tcb->callback)(tcb->abr, tcb->cbarg);
  q_tail(&tcb_free, &tcb->link); /* add to free list */
}

private boolean
match_tcb_skt(tcb, skt)
TCB *tcb;
int skt;
{
  return(skt == tcb->skt);
}

private
delete_tcb_skt(skt)
int skt;
{
  TCB *tcb;

  while ((tcb = (TCB *)q_mapf(tcblist, match_tcb_skt, (void *)skt)) != NULL) {
    if (dbug.db_atp)
      fprintf(stderr,"atp: delete_tcb_skt: deleting %x\n",tcb);
    dq_elem(&tcblist, &tcb->link);
    q_tail(&tcb_free, &tcb->link); /* add to free list */
  }
}




private int
ATPWrite(ap,atp,dp,dl)
atpProto *ap;
ATP *atp;
char *dp;
int dl;
{
  ABusRecord abr;
  ddpProto *ddpr;
  struct iovec iov[IOV_ATPU_SIZE];	/* io vector upto atp user level */
  int lvl;

  ddpr = &abr.proto.ddp;  
  ddpr->ddpType = ddpATP;
  ddpr->ddpSocket = ap->atpSocket;
  ddpr->ddpAddress = ap->atpAddress;
  ddpr->ddpReqCount = dl+atpSize;

  iov[IOV_ATP_LVL].iov_base = (caddr_t) atp;
  iov[IOV_ATP_LVL].iov_len = atpSize;
  lvl = IOV_ATP_LVL;
  /* Don't include a data element if there is none - 4.2 doesn't like it */
  if (dl > 0) {
    lvl++;
    iov[lvl].iov_base = (caddr_t) dp;
    iov[lvl].iov_len = dl;
  } 
  lvl++;			/* make offset into count */
  return(DDPWriteIOV(&abr,iov,lvl));
}


