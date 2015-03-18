/*
 * $Author: djh $ $Date: 1995/08/29 01:52:43 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abpaps.c,v 2.3 1995/08/29 01:52:43 djh Rel djh $
 * $Revision: 2.3 $
 *
 */

/*
 * abpaps.c - Printer Access Protocol access - server only routines.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Edit History:
 *
 *  July 12, 1986    CCKim	Created
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include "abpap.h"
#include "cap_conf.h"

/* Server functions */
int SLInit();		/* int SLInit(int*, char*, int, PAPStatusRec*) */
int GetNextJob();	/* int GetNextJob(int, int*, int*) */
int PAPRegName();	/* int PAPRegName(int, char *) */
int PAPPAPRemName();	/* int PAPPAPRemName(int, char *) */
int HeresStatus();	/* int HeresStatus(int, PAPStatusRec*) */
int SLClose();		/* int SLClose(int) */

/* internal functions */
#ifndef CPLUSPLUSSTYPLE
void psinit();		/* init any server stuff */
private void handle_server();
private void pap_arb_done();
private boolean isajob(); 
private void trynewjob();
private boolean findminjob();	
private void acceptnewjob(); 
private void cancelalljobs();
private void rb_freeup(); 
private void papsendstatus(); 
private void papopenreply();
#else
/* private void handle_server(ABusRecord *, int) */
/* private void pap_arb_done(PAPSSOCKET *) */
/* private boolean isajob(PAPSOCKET *) */
/* private void trynewjob(PAPSSOCKET *, ABusRecord *) */
/* private boolean findminjob(PAPSOCKET *) */
/* private void acceptnewjob(PAPSOCKET *, PAPSSOCKET *) */
/* private void cancelalljobs(PAPSSOCKET*) */
/* private void rb_freeup(ABusRecord*, ReplyBuf *) */
/* private void papsendstatus(PAPSSOCKET*, ABusRecord*) */
/* private void papopenreply(PAPSSOCKET*, unsigned char, AddrBlock*, */
/*                    unsigned short, unsigned short, byte*, int, int) */
#endif



/* PAP Server Socket info - used only here - so not placed in abpap.h */
typedef struct {
  int active;
  int state;			/* current state */
  int resskt;			/* our responding socket */
  int flowq;			/* our flow quantum */
  PAPStatusRec *sb;		/* status string */
  ABusRecord abr;		/* for getrequest listens */
  QElemPtr gnj_q;		/* get next job queue */
  PAP pap;			/* pap packet */
} PAPSSOCKET;

PAPSSOCKET papslist[NUMSPAP];

/*
 * Issued by PAP client in server to open service listening socket
 * and to register the server's name on this service listening socket.
 * The client can also provide an inital status string.
 * 
 *  Multiple SLInit call implies that the server can handle multiple
 *  printers - the maximum number is compiled in as NUMSPAP
 *
 * call parameters: entity name, flow quantum, status string
 * returned: result code, server refnum
 *
*/
SLInit(refnum, printername, flowquantum, statusbuff)
int *refnum;
char *printername;
int flowquantum;
PAPStatusRec *statusbuff;
{
  AddrBlock useraddr;
  int cno;
  int err;
  atpProto *ap;

  /* allocate server socket */
  for (cno = 0; cno < NUMSPAP; cno++)
    if (!papslist[cno].active)
      break;
  if (cno==NUMSPAP)
    return(-1);			/* not enough server processes */

  *refnum = cno;
  papslist[cno].active = TRUE;
  papslist[cno].state = PAP_BLOCKED;
  papslist[cno].flowq = flowquantum;
  papslist[cno].sb = statusbuff;

  useraddr.net = useraddr.node = useraddr.skt = 0; /* accept from anywhere */
  ATPOpenSocket(&useraddr, &papslist[cno].resskt);

  /* publish our name */
  if ((err = PAPRegName(cno, printername)) != noErr)
    return(err);
  
  /* start listener */
  ap = &papslist[cno].abr.proto.atp;
  ap->atpSocket = papslist[cno].resskt;
  ap->atpReqCount = sizeof(papslist[cno].pap);
  ap->atpDataPtr = (char *)&papslist[cno].pap;
  ap->atpNumBufs = flowquantum;		/* this is what ATP uses! */
  err = cbATPGetRequest(&papslist[cno].abr, handle_server, cno);
  return(err);
}


/*
 * getnextjob - call when ready to accept new job through specified
 * listener socket (idenitified by server refnum)
 *
 * call parameters: server refnum
 * returned parameters: result code, refnum to refer to connection in
 *  read, write, close calls
 *
*/
int
GetNextJob(srefnum, refnum, compstate)
int srefnum;
int *refnum;
int *compstate;
{
  PAPSOCKET *ps;
  PAPSSOCKET *pss = &papslist[srefnum];
  AddrBlock useaddr;
  int cno;

  useaddr.net = useaddr.node = useaddr.skt = 0; /* accept from anywhere */
  cno = ppgetskt(&useaddr);
  *refnum = cno;
  ps = cnotopapskt(cno);
  if (cno < 0 || ps == NULL)
    return(-1);
  ps->flowq = pss->flowq;
  ps->state = PAP_GNJOPENING;
  ps->comp = compstate;
  *compstate = 1;

  if (pss->state == PAP_BLOCKED) /* if blocked */
    pss->state = PAP_WAITING;	/* now waiting */
  q_tail(&papslist[srefnum].gnj_q, &ps->link); /* here's the list */
  return(noErr);
}


/* 
 * papregname - allow a secondary name to be registered
 *
*/
int
PAPRegName(refnum, printername)
int refnum;
char *printername;
{
  EntityName en;
  nbpProto nbpr;		/* nbp proto */
  NBPTEntry nbpt[1];		/* table of entity names */
  int err;

  create_entity(printername, &en);

  nbpr.nbpAddress.skt = papslist[refnum].resskt;
  nbpr.nbpRetransmitInfo.retransInterval = 4;
  nbpr.nbpRetransmitInfo.retransCount = 3;
  nbpr.nbpBufPtr = nbpt;
  nbpr.nbpBufSize = sizeof(nbpt);
  nbpr.nbpDataField = 1;	/* max entries */
  nbpr.nbpEntityPtr = &en;

  err = NBPRegister(&nbpr,FALSE);	/* try synchronous */
  return(err);
}

/*
 * remove a server name
 *
*/
int
PAPRemName(refnum, printername)
int refnum;
char *printername;
{
  EntityName en;
  int err;

  create_entity(printername, &en);
  err = NBPRemove(&en);
  return(err);
}

/*
 * hereisstatus - used to send new status string
 *
 * note: caller may modify status string at any time, but the status
 * string MUST be a pascal style string.
 *
*/
int
HeresStatus(srefnum, statusbuff)
int srefnum;
PAPStatusRec *statusbuff;
{
  if (papslist[srefnum].active)
    papslist[srefnum].sb = statusbuff;
  else return(-1);
  return(noErr);
}

/*
 * used to close down a server "process".  Note: it does NOT deregister
 * any names assocatied with that process.  Furthermore, it does not close
 * down any active pap connections.  You are expected to do this
 * before calling SLClose.  The function of this call is to prevent
 * further calls from being processed.
 *
*/
int
SLClose(srefnum)
int srefnum;
{
  int err;

  if (!papslist[srefnum].active)
    return(-1);		/* wasn't active */
  papslist[srefnum].active = FALSE; /* mark as unused now */
  err = ATPCloseSocket(papslist[srefnum].resskt);
  cancelalljobs(&papslist[srefnum]);
  /* possibly close down any active connections in the future */
  return(noErr);
}


/*
 * psinit 
 *
 * initialize server code
 *
*/
void
psinit()
{
}

/*
 * Handle the requests coming to the server process
 *
*/
private void
handle_server(abr, cno)
ABusRecord *abr;
int cno;
{
  PAPSSOCKET *pss = &papslist[cno];
  PAPSOCKET *ps;
  PAPUserBytes *pub;
  atpProto *ap;
  int err;

  if (abr->abResult != noErr) {
    if (abr->abResult == sktClosed)
      return;
    fprintf(stderr, "pap: socket error for server socket %x\n",pss);
  }

  ap = &abr->proto.atp;
  pub = (PAPUserBytes *)&ap->atpUserData;

  if (dbug.db_pap)
    fprintf(stderr, "[HS: PAPTYPE %d, STATE %d, PCID %x]\n",
	    pub->PAPtype, pss->state, pub->connid);

  switch (pub->PAPtype) {
  default:
    fprintf(stderr,"PAP: error: unexpected request received\n");
    fprintf(stderr,"PAP: conn %d,paptype %d,eof %d, unused %d\n",
	    pub->connid, pub->PAPtype, pub->other.std.eof,
	    pub->other.std.unused);
    break;
  case papOpenConn:
    switch (pss->state) {
    case PAP_WAITING:
      if (dbug.db_pap)
	fprintf(stderr, "Moving from wait to arbitrate\n");
      pss->state = PAP_ARBITRATE;
      Timeout(pap_arb_done, pss, PAPARBITRATIONTIME);
      trynewjob(pss, abr);		/* check out new job */
      break;
    case PAP_ARBITRATE:
      if (dbug.db_pap)
	fprintf(stderr, "Arbritrating\n");
      trynewjob(pss, abr);		/* check out new job */
      break;
    case PAP_UNBLOCKED:
      if (dbug.db_pap)
	fprintf(stderr, "Unblocked and have new job\n");
      ps = (PAPSOCKET *)dq_tail(&pss->gnj_q);
      if (ps == NULL) {
	fprintf(stderr, "pap: in unblocked state, but no gnj ready!!!\n");
	break;
      }
      ps->addr = ap->atpAddress; /* remote address */
      ps->transID = ap->atpTransID;
      ps->connid = pub->connid;
      ps->rrskt = pss->pap.papO.atprskt;
      ps->rflowq = pss->pap.papO.flowq;
      acceptnewjob(ps, pss);	/* got new job */
      break;
    default:
      /* reply no good */
      break;
    }
    break;
  case papSendStatus:
    papsendstatus(pss, abr);
    break;
  }
  ap = &pss->abr.proto.atp;
  ap->atpSocket = pss->resskt;
  ap->atpReqCount = sizeof(pss->pap);
  ap->atpDataPtr = (char *)&pss->pap;
  err = cbATPGetRequest(&pss->abr, handle_server, cno);
  /* if erro ??? */
}

/*
 * called when arbitration is completed
 *
*/
private void
pap_arb_done(pss)
PAPSSOCKET *pss;
{
  PAPSOCKET *ps;

  if (dbug.db_pap)
    fprintf(stderr, "Arbritration done - accepting new jobs\n");
  ps = (PAPSOCKET *)q_mapf(pss->gnj_q, isajob, 0);
  while (ps != NULL) {
    acceptnewjob(ps, pss);
    dq_elem(&pss->gnj_q, ps);
    ps = (PAPSOCKET *)q_mapf(pss->gnj_q, isajob, 0);
  }
  pss->state = (pss->gnj_q == NULL) ? PAP_BLOCKED : PAP_UNBLOCKED;
}

private boolean
isajob(ps)
PAPSOCKET *ps;
{
  return(ps->state == PAP_GNJFOUND);
}


/*
 * Called during arbitration state.  Trys out the new job and sees if it
 * has priority over any extant ones - priority based on job wait time 
 *
*/
private void
trynewjob(pss, abr)
PAPSSOCKET *pss;
ABusRecord *abr;
{
  atpProto *ap = &abr->proto.atp;
  PAPUserBytes *pub;
  int jobwaittime;
  PAPSOCKET *ps;

  pub = (PAPUserBytes *)&ap->atpUserData;
  jobwaittime = pss->pap.papO.wtime;

  ps = (PAPSOCKET *)q_map_min(pss->gnj_q, findminjob, 0);
  if (ps->state == PAP_GNJOPENING || ps->wtime.tv_sec < jobwaittime) {
    if (dbug.db_pap)
      fprintf(stderr, "Arbritration - got better job\n");
    if (ps->state == PAP_GNJFOUND)
      papopenreply(pss, (u_char)ps->connid, &ps->addr, ps->transID,
		   papPrinterBusy, pss->sb->StatusStr, 0, 0);
    ps->wtime.tv_sec =  jobwaittime;
    ps->state = PAP_GNJFOUND;
    ps->addr = ap->atpAddress;
    ps->transID = ap->atpTransID;
    ps->connid = pub->connid;
    ps->rrskt = pss->pap.papO.atprskt;
    ps->rflowq = pss->pap.papO.flowq;
  } else {
    papopenreply(pss, (u_char)pub->connid, &ap->atpAddress, ap->atpTransID,
		 papPrinterBusy, pss->sb->StatusStr, 0, 0);
  }
}

private boolean
findminjob(ps)
PAPSOCKET *ps;
{
  if (ps->state == PAP_GNJOPENING)
    return(-1);
  else return(ps->wtime.tv_sec);
}

/*
 * acceptnewjob - used to accept a job - after this is called, 
 *  a GetNextJob call has completed
 *
*/
private void
acceptnewjob(ps, pss)
PAPSOCKET *ps;
PAPSSOCKET *pss;
{
  if (dbug.db_pap)
    fprintf(stderr, "Accepting job: connid %d from [%d.%d]\n", ps->connid,
	    ntohs(ps->addr.net), ps->addr.node);

  papopenreply(pss, (u_char)ps->connid, &ps->addr, ps->transID,
	       papNoError, pss->sb->StatusStr, ps->flowq, ps->paprskt);
  start_papc(ps);
}

/*
 * cancelalljobs - reject any pending jobs, remove all from the queue
 *
*/
private void
cancelalljobs(pss)
PAPSSOCKET *pss;
{
  PAPSOCKET *ps;
  if (dbug.db_pap)
    fprintf(stderr, "canceling all jobs\n");

  while ((ps = (PAPSOCKET *)dq_tail(&pss->gnj_q)) != NULL) {
    if (ps->state == PAP_GNJFOUND)
      papopenreply(pss, (u_char)ps->connid, &ps->addr, ps->transID,
		   papPrinterBusy, pss->sb->StatusStr, ps->flowq, ps->paprskt);
    *ps->comp = -1;
  }
}



/*
 * The following functions require are used to send status messages and
 * open reply codes back.  Each requires an abr, bds, etc. to be allocated
 * while they are running...
 *
*/

typedef struct {
  QElem link;
  ABusRecord abr;		/* apple bus record */
  BDS bds[1];			/* single bds should be good enuf */
  PAP pap;			/* pap response packet */
} replybuf;

replybuf *free_replybuf;

/*
 * Free up reply buffer
 * - would like to make a macro, but since it's the callback from
 *   the ATP SndResp routines, not possible
 *
*/
private void
rb_freeup(abr, rb)
ABusRecord *abr;
replybuf *rb;
{
  q_tail(&free_replybuf, &rb->link);
}


/*
 * Send status back
 *
 * Note: we expect statusbuff to have a pascal string!
 *
 */

/*
 * callback functions for open reply and status
 *
 */
byte *((*papopencallback)()) = NULL;
byte *((*papstatuscallback)()) = NULL;

private void
papsendstatus(pss, abr)
PAPSSOCKET *pss;
ABusRecord *abr;
{
  replybuf *rb;
  atpProto *ap;
  int err, sslen;
  PAPUserBytes *pub;
  byte *message;

  if ((rb = (replybuf *)dq_head(&free_replybuf)) == NULL &&
      (rb = (replybuf *)malloc(sizeof(replybuf))) == NULL) {
	fprintf(stderr,"panic: replybuf: out of memory\n");
	exit(8);
      }

  ap = &rb->abr.proto.atp;
  /* find responding address and transid from request abr */
  ap->atpAddress = abr->proto.atp.atpAddress;
  ap->atpTransID = abr->proto.atp.atpTransID;
  ap->atpSocket = pss->resskt;
  ap->atpRspBDSPtr = rb->bds;
  message = pss->sb->StatusStr;
  if (!papstatuscallback ||
   (message = (*papstatuscallback)((pss - papslist), &ap->atpAddress)) == NULL)
    message = pss->sb->StatusStr;
  sslen = (int)(message[0]);
  rb->bds[0].buffSize = sslen + 4 +1 ; /* ugh */
  rb->bds[0].buffPtr = (char *)&rb->pap;
  rb->bds[0].userData = 0L;
  pub = (PAPUserBytes *)&rb->bds[0].userData;
  pub->PAPtype = papStatusReply;
  ap->atpNumBufs = 1;
  ap->atpBDSSize = 1;
  ap->fatpEOM = 1;		/* eom */
  /* include string length byte */
  bcopy(message, rb->pap.papS.status, sslen + 1);
  if (dbug.db_pap) {
    message[(int)message[0]+1] = 0; /* null terminate */
    fprintf(stderr,"Sending %s to [%d,%d]\n",
	    message + 1,
	    ntohs(abr->proto.atp.atpAddress.net),
	    abr->proto.atp.atpAddress.node&0xff);
  }

  err = cbATPSndRsp(&rb->abr, rb_freeup, rb);
  if (err != noErr) {
    rb_freeup(0, rb);
    if (dbug.db_pap)
      fprintf(stderr, "pap: sendstatus: ATPSndRsp returns %d\n",err);
  }
}


/*
 * Send a reply back to a PAPOpenConn request
 * 
 * note: message is expected to be a "pascal" string (first byte is length)
 *
*/
private void
papopenreply(pss, connid, addr, transID, result, message, flowq, rskt)
PAPSSOCKET *pss;
u_char connid;
AddrBlock *addr;
u_short transID;
u_short result;
byte *message;
int flowq;
int rskt;
{
  replybuf *rb;
  atpProto *ap;
  int err, sslen;
  PAPUserBytes *pub;
  byte *message2;

  if ((rb = (replybuf *)dq_head(&free_replybuf)) == NULL &&
      (rb = (replybuf *)malloc(sizeof(replybuf))) == NULL) {
	fprintf(stderr,"panic: replybuf: out of memory\n");
	exit(8);
      }
  ap = &rb->abr.proto.atp;
  /* find responding address and transid from request abr */
  ap->atpAddress = *addr;
  ap->atpSocket = pss->resskt;
  ap->atpTransID = transID;
  ap->atpRspBDSPtr = rb->bds;
  if (papopencallback &&
   (message2 = (*papopencallback)((pss - papslist), &ap->atpAddress, result)))
    message = message2;
  sslen = (int)(message[0]);
  rb->bds[0].buffSize = sslen + 4 + 1; /* ugh */
  rb->bds[0].buffPtr = (char *)&rb->pap;
  rb->bds[0].userData = 0L;
  pub = (PAPUserBytes *)&rb->bds[0].userData;
  pub->connid = connid;
  pub->PAPtype = papOpenConnReply;
  rb->pap.papOR.result = result;
  rb->pap.papOR.atprskt = rskt;
  rb->pap.papOR.flowq = flowq;
  bcopy(message, rb->pap.papOR.status, sslen+1);
  if (dbug.db_pap) {
    message[(int)message[0]+1] = 0; /* null terminate */
    fprintf(stderr,"Sending reply (%d) mesg %s to [%d,%d,%d,%x]\n", 
	    result,message+1,ntohs(addr->net),addr->node,addr->skt,connid);
  }
  ap->atpNumBufs = 1;
  ap->atpBDSSize = 1;
  ap->fatpEOM = 1;		/* eom */
  err = cbATPSndRsp(&rb->abr, rb_freeup, rb);
  if (err != noErr) {
    rb_freeup(0, rb);
    if (dbug.db_pap)
      fprintf(stderr, "pap: sendopenreply: ATPSndRsp returns %d\n",err);
  }
}
