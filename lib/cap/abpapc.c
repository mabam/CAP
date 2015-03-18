/*
 * $Author: djh $ $Date: 1996/06/18 10:48:17 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abpapc.c,v 2.4 1996/06/18 10:48:17 djh Rel djh $
 * $Revision: 2.4 $
*/

/*
 * abpapc.c - Printer Access Protocol access - client only routines.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July 12, 1986    CCKim	Created
 *
*/

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netat/appletalk.h>
#include "abpap.h"
#include "cap_conf.h"

OSErr PAPStatus();	/* OSErr PAPStatus(char *, PAPStatusRec *,  */
			/*  AddrBlock *) */
OSErr PAPOpen();	/* OSErr PAPOpen(int *, char *, int,  */
			/* PAPStatusRec *, int *) */
private void opendone(); /* private void opendone(ABusRecord *, u_long) */
private void res_open(); /* private void res_open(PRR *) */
void pcinit();
private boolean getpname();

/*
 * PAPStatus
 * 
 * As documented
 *
*/
OSErr
PAPStatus(printername, statusbuff, prtaddr)
char *printername;
PAPStatusRec *statusbuff;
AddrBlock *prtaddr;
{
  ABusRecord abr;
  atpProto *ap;
  PAPUserBytes *pub;
  BDS bds[1];			/* one bds */
  PAP p;
  int len;
  AddrBlock addr;
  EntityName en;

  if (prtaddr->net == 0 || prtaddr->node == 0 || prtaddr->skt == 0) {
    create_entity(printername, &en); /* name to entity */
    if (!getpname(&addr,&en))  {
      sprintf((char *)statusbuff->StatusStr+1,"?Can't find %s",
	      printername);
      statusbuff->StatusStr[0] = (byte)(strlen(statusbuff->StatusStr+1));
      return(-1);
    }
    *prtaddr = addr;
  } else addr = *prtaddr;

  ap = &abr.proto.atp;
  ap->atpUserData = 0;
  pub = (PAPUserBytes *)&ap->atpUserData;
  pub->PAPtype = papSendStatus;
  ap->atpAddress = addr;
  ap->atpSocket = 0;
  ap->atpReqCount = 0;
  ap->atpDataPtr = NULL;
  ap->atpRspBDSPtr = bds;
  bds[0].buffSize = sizeof(PAPpkt);
  bds[0].buffPtr = (char *)&p;
  ap->fatpXO = 0;		/* nyi */
  ap->atpTimeOut = 1;		/* retry in seconds */
  ap->atpRetries = 3;		/* 3 Retries */
  ap->atpNumBufs = 1;		/* number of bds segments */
  len = ATPSndRequest(&abr, FALSE);
  if (len == reqFailed) {
    strcpy(statusbuff->StatusStr+1, "%Not responding");
    statusbuff->StatusStr[0] = (byte)(sizeof("%Not responding")-1);
    return(reqFailed);
  }
  if (ap->atpNumRsp < 1 && (int)bds[0].dataSize < min_PAPpkt_size)
    return(-1);
  pub = (PAPUserBytes *) &bds[0].userData;
  if (pub->PAPtype != papStatusReply)
    return(-1);			/* should never happen? */
  bcopy(p.papS.status, statusbuff->StatusStr, (int)p.papS.status[0]+1);
  return(noErr);		/* return okay */
}


/*
 * PAPOpen
 *
 * as documented
 *
*/
OSErr
PAPOpen(refnum, printername, flowquantum, statusbuff, compstate)
int *refnum;
char *printername;
int flowquantum;
PAPStatusRec *statusbuff;
int *compstate;
{
  atpProto *ap;
  PAPUserBytes *pub;
  int cno, err;
  PAPSOCKET *ps;
  AddrBlock addr;
  EntityName en;

  create_entity(printername, &en); /* name to entity */

  if (!getpname(&addr,&en)) {
    sprintf((char *)statusbuff->StatusStr+1,
	    "%%Can't find %s",printername);
    statusbuff->StatusStr[0] = (byte)(strlen(statusbuff->StatusStr+1)+1);
    return(-1);
  }

  cno = ppgetskt(&addr);
  if (cno < 0)			/* no slots left */
    return(cno);
  if ((ps = cnotopapskt(cno)) == NULL) {
    fprintf(stderr,"AWK\n");
    exit(9999);
  }

  /* initialize status string */
  sprintf((char *)statusbuff->StatusStr+1, "%%no status");
  /* -2 instead of -1 to account for the eaten percent */
  statusbuff->StatusStr[0] = (byte)(sizeof("%%no status")-2);
  *compstate = 1;		/* start it out */
  ps->comp = compstate;
  ps->statusbuff = statusbuff;
  ps->addr = addr;
  ps->flowq = flowquantum;
  time((time_t *)&ps->wtime.tv_sec);	/* get current time */
  ps->state = PAP_OPENING;
  *refnum = cno;		/* send back refnum */

  ps->bds[0].buffPtr = (char *)&ps->por; /* establish bds here */
  ps->bds[0].buffSize = sizeof(PAP);

  ap = &ps->abr.proto.atp;
  ap->atpUserData = 0;
  pub = (PAPUserBytes *)&ap->atpUserData;
  pub->PAPtype = papOpenConn;
  pub->connid = ps->connid;
  ps->po.papO.atprskt = ps->paprskt;
  ps->po.papO.flowq = ps->flowq; /* until further notice */
  ps->po.papO.wtime = 1;
  ap->atpAddress = ps->addr;
  ap->atpSocket = 0;
  ap->atpReqCount = 4;		/* cheating */
  ap->atpDataPtr = (char *)&ps->po;
  ap->atpRspBDSPtr = ps->bds;
  ap->fatpXO = 1;		/* set xo on */
  ap->atpNumBufs = 1;		/* number of bds segments */

  ap->atpTimeOut = PAPOPENTIMEOUT;
  ap->atpRetries = PAPOPENRETRY;
  err = cbATPSndRequest(&ps->abr, opendone, (u_long)ps);
  return(err);
}

/*
 * Callback routine for open - called after sndrequest gets response
 * or a timeout occurs and no response received
 *
*/
private void
opendone(abr, ps)
ABusRecord *abr;
PAPSOCKET *ps;
{
  PAPUserBytes *pub;
  int sslen;

  if (ps->state != PAP_OPENING) {
    if (dbug.db_pap)
      fprintf(stderr,"papc: unexpected state change on session\n");
    /* sigh, what else to do ???? */
    *ps->comp = -1;
    return;
  }
  pub = (PAPUserBytes *) &ps->bds[0].userData;
  if (abr->abResult != reqFailed) {
    if (abr->abResult == sktClosed) {
      *ps->comp = sktClosed;	/* only way to abort... */
      return;
    }
    if (abr->abResult == noErr && pub->PAPtype == papOpenConnReply) {
      ps->rrskt = ps->por.papOR.atprskt; /* remote responding socket */
      sslen = (int)(ps->por.papS.status[0]);
      bcopy(ps->por.papS.status, ps->statusbuff->StatusStr,  sslen+1);
      if (dbug.db_pap)
	fprintf(stderr,"pap: open return from remote: %d\n",
		ps->por.papOR.result);
      if (ps->por.papOR.result == 0) {
	ps->rflowq = ps->por.papOR.flowq; /* save remote flow quantum */
	start_papc(ps);
	return;
      }
    }
    /* wait two seconds */
    Timeout(res_open, ps, 4*2);	/* will always.. */
    return;
  }
  res_open(ps);
}

/*
 * res_open(prr) - resume open call using prr.  Really part of open done.
 *
*/
private void
res_open(ps)
PAPSOCKET *ps;
{
  long t;
  int err;
  atpProto *ap = &ps->abr.proto.atp;

  time(&t);
  ps->po.papO.wtime = t - ps->wtime.tv_sec;

  ap->atpTimeOut = PAPOPENTIMEOUT;
  ap->atpRetries = PAPOPENRETRY;
  err = cbATPSndRequest(&ps->abr, opendone, (u_long)ps);
  if (err < 0) {
    *ps->comp = err;		/* die */
  }
}


void
pcinit()
{
  /* nothing to do */
}


/*
 * Find the specified entity - return true and addr if found, false
 * o.w.
 *
*/
private boolean
getpname(addr,ent)
AddrBlock *addr;
EntityName *ent;
{
  nbpProto nbpr;
  NBPTEntry nbpt[1];		/* should be exact match */
  
  nbpr.nbpRetransmitInfo.retransInterval = 8;
  nbpr.nbpRetransmitInfo.retransCount = 3;
  nbpr.nbpDataField = 1;
  nbpr.nbpEntityPtr = ent;
  nbpr.nbpBufPtr = nbpt;
  nbpr.nbpBufSize = sizeof(nbpt);

  NBPLookup(&nbpr, FALSE);
  if (nbpr.nbpDataField != 1)
    return(FALSE);
  NBPExtract(nbpt, nbpr.nbpDataField, 1, ent, addr); /* get lw entry */
  return(TRUE);
}
  
