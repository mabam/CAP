/*
 * $Author: djh $ $Date: 1993/01/14 12:57:35 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abpap.c,v 2.2 1993/01/14 12:57:35 djh Rel djh $
 * $Revision: 2.2 $
*/

/*
 * abpap.c - Printer Access protocol access.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Edit History:
 *
 *  June 22, 1986    CCKim	Created
 *
*/

/* PATCH: abpap.c.tickletimer, djh@munnari.OZ.AU, 15/11/90 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include "abpap.h"
#include "cap_conf.h"

#define PAP_SHUT_WRITE 0x1
#define PAP_SHUT_READ 0x2
#define PAP_SHUT_ALL 0x4


int PAPInit();		/* int PAPInit() */
int PAPGetNetworkInfo(); /* int PAPGetNetworkInfo(int, Addrblock *) */
int PAPRead();		/* int PAPRead(int, char*, int*, */
			/*  boolean*, int*) */
int PAPWrite();		/* int PAPWrite(int,char*,int,int,int*) */
int PAPUnload();
int PAPClose();		/* int PAPClose(int, boolean) */
int PAPHalfClose();
int PAPShutdown();
private void writedone(); /* private void writedone(ABusRecord *, u_long) */
private void readdone(); /* private void readdone(ABusRecord *, u_long) */
private OSErr start_papgetrequest();
private OSErr stop_papgetrequest();
private void papremoteclose();
void start_papc();
private void stop_papc();
private void handle_papgetrequest();
private void papsndresponse();
private void startpaptickle();
private void stoppaptickle();
private void ttimeout();
private void start_ttimer();
private void reset_ttimer();
private void stop_ttimer();

private void ppsktinit();
PAPSOCKET *cnotopapskt();
int ppskttocno();
int abskttocno();
private void ppfreeskt();
int ppgetskt();
private void psshutdown();
private boolean setup_prr();
private boolean setup_pwr();

int
PAPInit()
{
  ppsktinit();
  psinit();			/* init server code */
  pcinit();			/* init client code */
}

/*
 * returns address of remote
 *
*/
PAPGetNetworkInfo(cno, addr)
int cno;
AddrBlock *addr;
{
  PAPSOCKET *ps;
  if ((ps = cnotopapskt(cno)) == NULL)
    return(-1);
  if (ps->state != PAP_OPENED)
    return(-2);
  *addr = ps->addr;		/* return address */
  return(noErr);
}

/*
 * PAPRead - as documented
 *
*/
int
PAPRead(cno, databuff, datasize, eof, compstate)
int cno;
char *databuff;
int *datasize;
int *eof;
OSErr *compstate;
{
  PAPSOCKET *ps;
  int err;

  if (dbug.db_pap)
    fprintf(stderr, "pap: papread called: cno %d\n",cno);
  if ((ps = cnotopapskt(cno)) == NULL)
    return(-1);
  if (ps->state != PAP_OPENED)	/* connection closed */
    return(-2);			/* for now */

  /* only error is "request already active" */
  if (!setup_prr(cno, databuff, datasize, eof, compstate))
    return(tooManyReqs);

  if ((err = cbATPSndRequest(&ps->prr.abr, readdone, (u_long)cno)) < 0) {
    ps->prr.valid = FALSE;
    return(err);
  }
  return(noErr);
}



/*
 * PAPWrite - as documented
 * 
*/
int
PAPWrite(refnum, databuff, datasize, eof, compstate)
int refnum;
char *databuff;
int datasize;
int eof;
int *compstate;
{
  PAPSOCKET *ps;

  if (dbug.db_pap)
    fprintf(stderr,"pap: papwrite: cno %d, datasize %d, eof %s\n",
	    refnum, datasize, eof ? "yes" : "no");
  if ((ps = cnotopapskt(refnum)) == NULL)
    return(-1);
  if (ps->state != PAP_OPENED)	/* connection closed */
    return(-2);			/* for now */
  if (datasize > PAPSegSize*ps->rflowq)
    return(-3);

  /* only error is "request already active" */
  if (!setup_pwr(refnum, databuff, datasize, eof, compstate)) 
    return(tooManyReqs);
  papsndresponse(ps);		/* try to send if sdc here */
  return(noErr);
}

/*
 * PAPUnload()
 *
 * Kill off all pap stuff
*/
PAPUnload()
{

}

/*
 * PAPClose
 *
 * - as documented
 * 
 *
*/
int
PAPClose(refnum)
int refnum;
{
  PAPSOCKET *ps;
  struct ABusRecord abr;
  struct atpProto *ap;
  PAPUserBytes *pub;
  BDS bds[1];			/* one bds */
  int err;

  if (dbug.db_pap)
    fprintf(stderr, "pap: papclose: cno %d\n",refnum);

  if ((ps = cnotopapskt(refnum)) == NULL)
    return(-1);

  ap = &abr.proto.atp;
  ap->atpUserData = 0;
  pub = (PAPUserBytes *)&ap->atpUserData;
  pub->connid = ps->connid;
  pub->PAPtype = papCloseConn;
  ap->atpAddress = ps->addr;
  ap->atpAddress.skt = ps->rrskt;
  ap->atpSocket = 0;
  ap->atpReqCount = 0;
  ap->atpDataPtr = 0;
  ap->atpRspBDSPtr = bds;
  bds[0].buffSize = 0;
  bds[0].buffPtr = NULL;
  ap->fatpXO = 0;		/* nyi */
  ap->atpTimeOut = PAPCLOSETIMEOUT; /* retry in seconds */
  ap->atpRetries = PAPCLOSERETRY; /* 3 Retries */
  ap->atpNumBufs = 1;		/* number of bds segments */
  err = ATPSndRequest(&abr, FALSE);

  if (err == reqFailed) {
    if (dbug.db_pap)
      fprintf(stderr,"pap: papclose: remote not responding");
    /* can't return here because we want to do the stop_papc below */
  }

  stop_papc(ps);

  if (err != reqFailed) {
    pub = (PAPUserBytes *) &bds[0].userData;
    if (pub->PAPtype != papCloseConnReply) {
      return(-1);
    }
  } else return(err);
  return(0);
}

/*
 * shutdown write ability on a pap socket
 *
 * leaves tickle timer (outgoing) active and ability to read from socket
 *
*/
int
PAPHalfClose(refnum)
int refnum;
{
  PAPSOCKET *ps;

  if ((ps = cnotopapskt(refnum)) == NULL)
    return(-1);
  stop_ttimer(ps);
  stop_papgetrequest(ps);
  psshutdown(ps, PAP_SHUT_WRITE);
  return(0);
}

/*
 * close down a pap connection without telling remote
 * useful for forking off the a pap fork without carrying the baggage
 *
*/
int
PAPShutdown(refnum)
int refnum;
{
  PAPSOCKET *ps;

  if ((ps = cnotopapskt(refnum)) == NULL)
    return(-1);
  stop_papc(ps);
  return(0);
}



/*
 * callback from atp after sndresp is done
 * (use cno as cbarg so we can check papsocket closed without explict
 * checks)
*/
private void
writedone(abr, cbarg)
ABusRecord *abr;
u_long cbarg;
{
  int cno = (int)cbarg;
  PAPSOCKET *ps;

  if ((ps = cnotopapskt(cno)) == NULL)	/* get pap socket */
    return;			/* drop it */

  if (!ps->pwr.valid)		/* nothing to do */
    return;

  *(ps->pwr.comp) = abr->abResult; /* all done! */
  ps->pwr.active = FALSE;
  ps->pwr.valid = FALSE;
}

/*
 * call back from atp after sndrequest is done
 *
*/
private void
readdone(abr, cbarg)
ABusRecord *abr;
u_long cbarg;
{
  PRR *prr;
  int cno = (int)cbarg;		/* get connection */
  int i, cnt;
  PAPSOCKET *ps;
  PAPUserBytes *pub;

  if ((ps = cnotopapskt(cno)) == NULL)	/* get pap socket */
    return;			/* drop it */

  if (ps->state == PAP_OPENED)	/* If read is done and ps is open */
    reset_ttimer(ps);		/* then do normal reset on tickle timer */
  else {			/* else just stop the tickle timeout */
    if (dbug.db_pap)
	fprintf(stderr, "pap: readdone cancelling ttimer!\n");
    stop_ttimer(ps);		/* just stop the tickle timer! */
  }

  prr = &ps->prr;		/* get active prr */

  if (abr->abResult == noErr)  {
    /* buffSize < dataSize => data size error (e.g. incoming bigger */
    /* than buffer allocated) should never happen, but we'll simply */
    /* make sure we don't try to use data that's not there for now */
    for (i = 0, cnt = 0; i < (int)abr->proto.atp.atpNumRsp; i++)
      cnt += min(prr->bds[i].dataSize,prr->bds[i].buffSize);
    *prr->datasize = cnt;
    pub = (PAPUserBytes *)&prr->bds[abr->proto.atp.atpNumRsp-1].userData;
    if (pub->other.std.eof) {
      if (dbug.db_pap)
	fprintf(stderr,"pap: Remote signaled EOF\n");
      *prr->eof = 1;
      /* assume that any outstanding papwrite request should be canceled */
      if (ps->pwr.valid && ps->pwr.active) {
	if (dbug.db_pap)
	  fprintf(stderr, "pap: cancelling outstanding papwrite due to EOF on stream\n");
	(void)ATPRspCancel(&ps->pwr.abr, FALSE); /* ignore error */
      }
    }
  }
  *prr->comp = abr->abResult;
  prr->valid = FALSE;		/* not active anymore */
}



/*
 * start_papgetrequest(ps)
 *
 * issue a atp request call - control falls back and when 
 *  request comes in, we goto handle_papgetrequest
 *
*/
private OSErr
start_papgetrequest(ps)
PAPSOCKET *ps;
{
  atpProto *ap;
  int err;

  if (ps->active)		/* already started */
    return(noErr);

  ap = &ps->request_abr.proto.atp;
  ap->atpSocket = ps->paprskt;
  ap->atpReqCount = 0;
  ap->atpDataPtr = NULL;
  err = cbATPGetRequest(&ps->request_abr, handle_papgetrequest, ps);
  ps->request_active = (err == noErr);
  return(err);
}

private OSErr
stop_papgetrequest(ps)
PAPSOCKET *ps;
{
  OSErr err;

  err = ATPReqCancel(&ps->request_abr, FALSE);
  return(err);
}



/*
 * Used to signal new pap connection and start all requiste maintenace
 * functions.  Expects all information for the socket to be in place
 * at this point.
 *
*/
void
start_papc(ps)
PAPSOCKET *ps;
{
  if (dbug.db_pap) {
    fprintf(stderr, "pap: connection %d opened\n", (int)ps->connid);
  }
  ps->state = PAP_OPENED;
  *ps->comp = 0;
  startpaptickle(ps);		/* start tickler on local */
  start_ttimer(ps);		/* start tickle timer on remote */
  start_papgetrequest(ps);	/* start up request monitor */
}

/*
 * used to stop a pap connection maintenance functions
*/
private void
stop_papc(ps)
PAPSOCKET *ps;
{
  ps->state = PAP_CLOSED;
  stoppaptickle(ps);
  stop_ttimer(ps);
  stop_papgetrequest(ps);
  psshutdown(ps, PAP_SHUT_ALL);
}



/*
 * handle_papgetrequest
 *
 * we handle the callback from atp to complete a getrequest
 * 
 *
*/
private void
handle_papgetrequest(abr, ps)
ABusRecord *abr;
PAPSOCKET *ps;
{
  PAPUserBytes *pub;

  ps->request_active = FALSE;

  if (abr->abResult != noErr) {
    if (abr->abResult == sktClosed)
      return;
    fprintf(stderr, "pap: socket error for socket %x\n",ps);
  }
  pub = (PAPUserBytes *)&ps->request_abr.proto.atp.atpUserData;
  switch (pub->PAPtype) {
  default:
    fprintf(stderr,"pap: error: unexpected request received\n");
    fprintf(stderr,"pap: conn %d,paptype %d,eof %d, unused %d)\n",
	    pub->connid, pub->PAPtype, pub->other.std.eof,
	    pub->other.std.unused);
    break;
  case papCloseConn:
    if (pub->connid != ps->connid) {
      if (dbug.db_pap) {
	fprintf(stderr, "pap: wrong connection id on incoming pkt\n");
	fprintf(stderr,"pap: correct %d, incoming %d\n", ps->connid,
		pub->connid);
      }
      break;
    }
    if (dbug.db_pap)
      fprintf(stderr,"remote sent close request\n");
    if (ps->state != PAP_OPENED) {
      if (dbug.db_pap) {
	fprintf(stderr,"pap: remote sent close and conn not open: state: %d\n",
		ps->state);
	fprintf(stderr, "pap: pap connection ids: rem: %d, local %d\n",
		(byte)pub->connid, (byte)ps->connid);
      }
      /* Correct action is to ignore? */
      break;
    }
    ps->state = PAP_CLOSED;
    /* we are actually obligated to send an immediate reply */
    /* what to do here? - should really start shutting down */
    papremoteclose(ps, abr);	/* remote signal shutdown */
    return;			/* okay - no don't queue another req */
  case papSendData:
    reset_ttimer(ps);
    pub->other.seqno = ntohs(pub->other.seqno);
    if (dbug.db_pap)
      fprintf(stderr,"pap: Seq %d, expected %d\n",
	      pub->other.seqno, ps->paprseq);
    if (pub->connid != ps->connid) {
      if (dbug.db_pap) {
	fprintf(stderr, "Pap: wrong connection id on incoming pkt\n");
	fprintf(stderr,"Pap: correct %d, incoming %d\n", ps->connid,
		pub->connid);
      }
      break;
    }
    if (pub->other.seqno != 0) { /* incoming is sequences */
      if (ps->paprseq == 0)	/* boundary condition */
	ps->paprseq = pub->other.seqno;
      else
	if (ps->paprseq != pub->other.seqno) {
	  if (dbug.db_pap) 
	    fprintf(stderr, "Bad sequence number: expected %d, got %d\n",
		    ps->paprseq, pub->other.seqno);
	  break;
	}
    }
    /* remember outstanding send data credit */
    /* could check to make sure that the send data credit isn't used */
    /* before we go on, but... (possible problem) */
#ifdef notdef
    if (dbug.db_pap) {
#endif
      if (ps->sdc.valid && (ps->sdc.transID != abr->proto.atp.atpTransID ||
			    ps->sdc.creditno != ps->paprseq))
	fprintf(stderr,
		"pap: new send data credit received with one outstanding\n");
      
#ifdef notdef
    }
#endif
    /* valid pwr and it is active (in a send response) */
    /* this is the "release" handler */
    /* MIGHT be able to do this even if unsequenced by comparing tids */
    if (pub->other.seqno != 0 && ps->pwr.valid && ps->pwr.active) {
      if (dbug.db_pap)
	fprintf(stderr,"pap: new send data credit: terminating previous papwrite\n");
      (void)ATPRspCancel(&ps->pwr.abr, FALSE); /* ignore error */
    }
    /* note: does new sdc means previous papwrite from this end was okay? */
    ps->sdc.transID = abr->proto.atp.atpTransID;
    ps->sdc.creditno = ps->paprseq;
    /* cannot guarantee that this came in from remote responding skt */
    /* specification doesn't say */
    ps->sdc.skt = abr->proto.atp.atpAddress.skt;
    ps->sdc.valid = TRUE;
    /* okay to bump here since a rspcb should have been created for */
    /* the current send data credit */
    if (pub->other.seqno != 0)	/* if sequenced */
      nextpapseq(ps->paprseq);	/* bump pap seqence */
    papsndresponse(ps);		/* send any outstanding */
    break;
  case papTickle:
    reset_ttimer(ps);
    break;
  }
  start_papgetrequest(ps);
}

/*
 * called when the remote signals a closeConn request
 *
 * we know that no getrequest is active since we can only be called
 * from handle_paprequest
 *
*/
private void
papremoteclose(ps, reqabr)
PAPSOCKET *ps;
ABusRecord *reqabr;
{
  atpProto *ap;
  ABusRecord abr;
  PAPUserBytes *pub;
  BDS bds[1];
  int err;

  ap = &abr.proto.atp;

  ap->atpSocket = ps->paprskt;
  ap->atpAddress = reqabr->proto.atp.atpAddress;
  ap->atpRspBDSPtr = bds;
  ap->atpTransID = reqabr->proto.atp.atpTransID;
  ap->fatpEOM = 1;		/* EOM */
  ap->atpNumBufs = 1;
  ap->atpBDSSize = 1;
  bds[0].buffSize = 0;
  bds[0].buffPtr = NULL;
  bds[0].userData = 0L;
  pub = (PAPUserBytes *)&bds[0].userData;
  pub->connid = ps->connid;
  pub->PAPtype = papCloseConnReply;
  err = ATPSndRsp(&abr, FALSE);	/* send the response */
  if (err < 0 && dbug.db_pap)
    fprintf(stderr,"pap: papremoteclose: atpsndrsp returns %d\n",err);
  stop_papc(ps);		/* close down connection */
}

/*
 * papsndresponse - send all outstanding write requests if there are
 * outstanding sending credits.  Call anytime.  Will only send if
 * we have the requiste send data credits and there is stuff to send.
 * [Good places to call are in PAPWrite and the request handler when a
 *  send data credit is received]
 *
*/
private void
papsndresponse(ps)
PAPSOCKET *ps;
{
  PWR *pwr;
  int err;

  if (!ps->sdc.valid || !ps->pwr.valid)
    return;			/* nothing to do */
  pwr = &ps->pwr;
  /* find pwr->abr: get atp address and transid from request abr */
  pwr->abr.proto.atp.atpAddress = ps->addr;
  pwr->abr.proto.atp.atpAddress.skt = ps->sdc.skt;
  pwr->abr.proto.atp.atpTransID = ps->sdc.transID;
  if (dbug.db_pap) 
    fprintf(stderr,"pap: sending packet (pap %d, atp %d)\n", ps->sdc.creditno,
	    ps->sdc.transID);
  err = cbATPSndRsp(&pwr->abr, writedone, (u_long)pwr->cno);
  if (err < 0) {
    *pwr->comp = err;	/* errror, all done! */    
    ps->pwr.valid = FALSE;
    /* return here? */
  } else ps->pwr.active = TRUE;
  ps->sdc.valid = FALSE;	/* used */
}


/*
 * PAP Tickle managment functions
 *
*/

/* 
 * startpaptickle - start a tickle for the specified connection
 *
*/
private void
startpaptickle(ps)
PAPSOCKET *ps;
{
  atpProto *ap;
  PAPUserBytes *pub;
  int err;

  ap = &ps->abr.proto.atp;
  ap->atpUserData = 0;
  pub = (PAPUserBytes *) &ap->atpUserData;
  pub->connid = ps->connid;
  pub->PAPtype = papTickle;
  ap->atpAddress = ps->addr;
  ap->atpAddress.skt = ps->rrskt;
  ap->atpSocket = 0;
  ap->atpReqCount = 0;
  ap->atpDataPtr = 0;
  ap->atpRspBDSPtr = ps->bds;
  ps->bds[0].buffPtr = NULL;
  ps->bds[0].buffSize = 0;
  ap->atpNumBufs = 1;
  ap->fatpXO = 0;
  ap->atpTimeOut = PAPTICKLETIMEOUT;
  ap->atpRetries = 255;	/* means infinity */
  err = ATPSndRequest(&ps->abr, TRUE);
  if (err != noErr) {
    fprintf(stderr,"pap: problems starting tickle\n");
  }
}

/*
 * stoppaptickle - cancel the tickle on the specified connection
 *
*/
private void
stoppaptickle(ps)
PAPSOCKET *ps;
{
  ATPReqCancel(&ps->abr, FALSE); /* run async? */
}

/*
 * Timeout handler for remote tickle
 */
private void
ttimeout(arg)
u_long arg;
{
  PAPSOCKET *ps = (PAPSOCKET *)arg;

  if (dbug.db_pap)
    fprintf(stderr,"pap: *** TIMEOUT ON PAP SOCKET\n");
  PAPClose(ppskttocno(ps));	/* close off socket */
}

/*
 * Start the remote tickle timeout
 *
*/
private void
start_ttimer(ps)
PAPSOCKET *ps;
{
  if (dbug.db_pap)
    fprintf(stderr,"pap: starting timeout on PAP socket %d\n",
	    PAPCONNECTIONTIMEOUT);
  Timeout(ttimeout, (u_long)ps, PAPCONNECTIONTIMEOUT);
}

/* 
 * reset the remote tickle timeout
 *
*/
private void
reset_ttimer(ps)
PAPSOCKET *ps;
{
  remTimeout(ttimeout, (u_long)ps);
  Timeout(ttimeout, (u_long)ps, PAPCONNECTIONTIMEOUT);
}

/* 
 * cancel the remote tickle timeout
 *
*/
private void
stop_ttimer(ps)
PAPSOCKET *ps;
{
  if (dbug.db_pap)
    fprintf(stderr,"pap: cancel timeout on PAP socket\n");
  remTimeout(ttimeout, (u_long)ps);
}

/*
 * The following would be in a seperate file, except we want to
 * keep as much as possible private...
 *
*/


/*
 * abpapaux.c - Printer Access protocol access auxillary routines
 *
 *   pap socket management
 *   pap read request management
 *   pap write request management 
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July 1, 1986    CCKim	Created
 *
*/
#ifdef singlemodulemode
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netat/appletalk.h>
#include "abpap.h"
#include "cap_conf.h"
#endif

/*
 * pap socket management 
 *
 * Assumes very small list of possible sockets
 *
*/
PAPSOCKET paplist[NUMPAP];	/* list of "sockets" */

/*
 * initalize pap socket management
 *
*/
private void
ppsktinit()
{
  int i;

  for (i=0; i < NUMPAP; i++)
    paplist[i].state = PAP_INACTIVE;
}

/*
 * Convert a refnum to a papsocket 
 *
*/
PAPSOCKET *
cnotopapskt(cno)
int cno;
{
  if (cno >= 0 && cno < NUMPAP && paplist[cno].state != PAP_INACTIVE)
    return(&paplist[cno]);
  else return(NULL);
}

/*
 * convert a pap socket to a refnum
 *
*/
int
ppskttocno(skt)
PAPSOCKET *skt;
{
  int i;
  for (i=0; i < NUMPAP; i++)
    if (&paplist[i] == skt)
      return(i);
  return(-1);
}

/*
 * convert a ddp socket to a pap refnum
 *
*/
int
abskttocno(skt)
int skt;
{
  int i;
  for (i=0; i < NUMPAP; i++)
    if (paplist[i].paprskt == skt)
      return(i);
  return(-1);
}

/*
 * Free a pap socket
 *
*/
private void
ppfreeskt(cno)
int cno;
{
  ATPCloseSocket(paplist[cno].paprskt);
  paplist[cno].state = PAP_INACTIVE;
}

/*
 * get a pap socket - return the refnum
 *
*/
int
ppgetskt(addr)
AddrBlock *addr;
{
  int i;
  AddrBlock useaddr;
  PAPSOCKET *ps;

  for (i=0; i < NUMPAP; i++) 
    if (paplist[i].state == PAP_INACTIVE)
      break;
  if (i==NUMPAP) return(-1);	/* no sockets left */
  ps = &paplist[i];
  ps->state = PAP_OPENING;
  ps->connid = (byte)time(0);
  ps->papseq = 1;	/* start at one */
  ps->paprseq = 0;	/* assume zero to start */
  ps->papuseq = 0;
  ps->paprskt = 0;
  ps->pwr.valid = ps->prr.valid = FALSE; /* no valid writes/reads */
  ps->pwr.active = FALSE;	/* pwr is not in a send (overkill) */
  ps->sdc.valid = FALSE;	/* no valid send data credit */
  ps->request_active = 0;
  useaddr = *addr;
  useaddr.skt = 0;
  ATPOpenSocket(&useaddr,&ps->paprskt);
  if (paplist[i].paprskt < 0) {
    ppfreeskt(i);
    return(-1);
  }
  return(i);			/* return connection number */
}

/*
 * shutdown the pap socket 
*/
private void
psshutdown(ps, which)
PAPSOCKET *ps;
int which;
{
  int cno = ppskttocno(ps);

  /* dequeue write elements - note, what about one in transit? (e.g. */
  /* one with a sndrsp active) */
  /* turns out to be a good question */
  if ((which & PAP_SHUT_WRITE) || (which & PAP_SHUT_ALL))
    if (ps->pwr.valid) {
      *(ps->pwr.comp) = -1;	/* just in case (what should it be?) */
      if (ps->pwr.active)
	(void)ATPRspCancel(&ps->pwr.abr, FALSE); /* ignore error */
      ps->pwr.valid = FALSE;
      /* else problem */
    }
  /* dequeue read elements */
  if ((which & PAP_SHUT_READ) || (which & PAP_SHUT_ALL))
    if (ps->prr.valid) {
      ATPReqCancel(&ps->prr.abr, FALSE); /* not async! */
      *(ps->prr.comp) = ps->prr.abr.abResult;
      ps->prr.valid = FALSE;
    }
  if (which & PAP_SHUT_WRITE)
    ps->sdc.valid = FALSE;	/* mark as bad */

  if (which & PAP_SHUT_ALL)
    ppfreeskt(cno);	/* close off socket here */
  else
    ATPCloseSocket(paplist[cno].paprskt); /* close off req rec socket */
}


/*
 * pap read request management
 *
 * read request are placed on a list
 *
*/

private boolean
setup_prr(cno,rbuf,rlen,eof,comp)
int cno;
char *rbuf;
int *rlen;
int *eof;
int *comp;
{
  PAPSOCKET *ps = cnotopapskt(cno);
  PRR *prr = &ps->prr;
  struct atpProto *ap;
  PAPUserBytes *pub;

  if (prr->valid)
    return(FALSE);
  prr->numbds = setup_bds(prr->bds, sizeof(prr->bds), PAPSegSize, rbuf,
			  ps->flowq*PAPSegSize, (atpUserDataType)0);
/*  prr->cno = ps->cno; */		/* is this needed? */
  prr->eof = eof;
  prr->comp = comp;
  *eof = FALSE;			/* assume */
  *comp = 1;			/* assume */
  prr->datasize = rlen;
  ap = &prr->abr.proto.atp;

  ap->atpUserData = 0;
  pub = (PAPUserBytes *)&ap->atpUserData;
  /* lock ps */
  pub->connid = ps->connid;
  pub->PAPtype = papSendData;
  pub->other.seqno = htons(ps->papseq);
  nextpapseq(ps->papseq);
  ap->atpAddress = ps->addr;
  ap->atpAddress.skt = ps->rrskt;
  /* unlock ps */
  ap->atpSocket = 0;
  ap->atpReqCount = 0;
  ap->atpDataPtr = 0;
  ap->atpRspBDSPtr = prr->bds;
  ap->fatpXO = 1;
  ap->atpNumBufs = prr->numbds;	/* number of bds segments */
  ap->atpTimeOut = PAPREADTIMEOUT;
  ap->atpRetries = PAPREADRETRIES;
  prr->valid = TRUE;

  /* lock ps */
  if (dbug.db_pap)
    fprintf(stderr,"pap: prr entry on cno %d, seq %d\n",cno, ps->papseq);
  return(TRUE);
}




/*
 * PAP write request management
 *
 * write request are organized into queue's
 *
*/
private boolean
setup_pwr(cno, sbuf,slen, eof, compstate)
int cno;
char *sbuf;
int slen;
int eof;
int *compstate;
{
  int cnt;
  PAPSOCKET *ps = cnotopapskt(cno);
  PWR *pwr = &ps->pwr;
  atpProto *ap;
  PAPUserBytes *pub;
  atpUserDataType udata;

  if (pwr->valid)
    return(FALSE);
  /* setup reply */
  udata = 0;			/* expect to expand to fit */
  pub = (PAPUserBytes *)&udata;
  pub->connid = ps->connid;
  pub->PAPtype = papData;
  pub->other.std.eof = eof ? 1 : 0;
  cnt = setup_bds(pwr->bds, sizeof(pwr->bds), PAPSegSize, sbuf, slen,
		  (atpUserDataType)udata);
#ifdef notdef
  /* this is probably the right way */
  pub = (PAPUserBytes *)&pwr->bds[cnt-1].userData;
  pub->other.std.eof = eof ? 1 : 0;
#endif
  ap = &pwr->abr.proto.atp;
  ap->atpSocket = ps->paprskt;	/* socket we're coming from */
  ap->atpRspBDSPtr = pwr->bds;	/* buffer data  */
  ap->fatpEOM = 1;		/* mark as last message in transaction */
  ap->atpNumBufs = cnt;		/* count of buffer */
  ap->atpBDSSize = cnt;		/* same */
  pwr->comp = compstate;
  pwr->cno = cno;
  pwr->valid = TRUE;		/* valid pwr entry */
  pwr->active = FALSE;		/* not in send response */
  *compstate = 1;

  if (dbug.db_pap) {
    fprintf(stderr, "pap: pwr entry with %d bytes\n", slen);
    if (eof)
      fprintf(stderr, "pap: EOF SET\n");
  }
  return(TRUE);
}

