/*
 * $Author: djh $ $Date: 91/03/14 13:45:20 $
 * $Header: abasp.c,v 2.2 91/03/14 13:45:20 djh Exp $
 * $Revision: 2.2 $
*/

/*
 * abasp.c - Appletalk Session Protocol
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July 28, 1986    CCKim	Created
 *  Aug   4, 1986    CCKim	Verified: level 0
*/

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include "abasp.h"

int aspInit();
int SPGetParms();
int SPInit();
int SPGetNetworkInfo();
private void handle_aspserver();
private void asp_doopensess();
private void sessopenreply();
int SPGetSession();
int SPCloseSession();
int SPGetRequest();
int SPCmdReply();
int SPWrtReply();
private int spreply();
int SPWrtContinue();
int SPNewStatus();
int SPAttention();

int SPGetStatus();
int SPOpenSession();
private void handle_aspclient();
int SPCommand();
int SPWrite();
private void asp_do_write();

private void handle_asp_sndreq();
private void handle_asp_getreq(); /* for SPGetRequest */
private void handle_asp_rspdone(); /* for SPWRtReply, SPCmdReply and  */
				/* intermediate part of SPWrite */
private void handle_asp_special();

private void do_sendclosesessreply();
private void do_sendreply();
private void start_client_aspskt();
private void shutdown_aspskt();

void delete_aq();
ASPQE *create_aq();
private ASPQE *get_aq();
private boolean match_aspwe();
private ASPQE *find_aspawe();

private void startasptickle();
void stopasptickle();
private void ttimeout();
private void start_ttimer();
private void reset_ttimer();
void stop_ttimer();

int SPFork();
OSErr SPShutdown();
#ifdef ASPPID
int SPFindPid();
#endif
private OSErr spshutdown();

private int aspskt_init();	/* initialize skts */
private OSErr aspskt_new();
private void aspskt_free();
private ASPSkt *aspskt_find_notrunning();
private ASPSkt *aspskt_find_sessid();
#ifdef ASPPID
private ASPSkt *aspskt_find_pid();
#endif
private OSErr aspsskt_new();
ASPSkt *aspskt_find_active();
ASPSkt *aspskt_find_sessrefnum();
ASPSSkt *aspsskt_find_slsrefnum();
private boolean aspsskt_isactive();

private void sizeof_abr_bds_and_req();
private void sizeof_bds_and_req();
private OSErr asp_cksndrq_err();

private int sessid_not_inited = TRUE;
private word next_sessid = 0;	/* use word to prevent overflows */
/* this allows us to keep code around in case this should be done */
/* differently at some point.  */

#define AD_SKT 1
#define AD_HANDLERS 2
#define AD_CALLS 4
#define AD_TICKLE 8
private int asp_dbug = AD_SKT|AD_HANDLERS|AD_CALLS|AD_TICKLE;

#define isdskt (dbug.db_asp && (asp_dbug & AD_SKT))
#define isdhand (dbug.db_asp && (asp_dbug & AD_SKT))
#define isdcalls (dbug.db_asp && (asp_dbug & AD_SKT))
#define isdtickle (dbug.db_asp && (asp_dbug & AD_SKT))

private char *asptypes[9] = {
  "Unknown",
  "aspCloseSession",
  "aspCommand",
  "aspGetStat",
  "aspOpenSess",
  "aspTickle",
  "aspWrite",
  "aspWriteData",
  "aspAttention"
};

private char *aspevents[] = {
  "tSPGetRequest",
  "tSPCmdReply",
  "tSPWrtContinue",
  "tSPWrtReply",
  "tSPAttention",
  "tSP_Special_DROP",
  "tSPGetStat",
  "tSPOpenSess",
  "tSPCommand",
  "tSPWrite",
  "tSPWrite2 ",
  "tSPClose"
};

/*
 * Initialize asp - only args is the minimun number of sessions to allow
 *
 * You don't have to call this, but if you do, be sure to do it before
 * any other ASP calls.
 *
*/
int
aspInit(n)
int n;
{
  return(aspskt_init(n));
}

/*
 * Get server operating parameters
 *
*/
SPGetParms(MaxCmdSize, QuantumSize)
int *MaxCmdSize;
int *QuantumSize;
{
  if (isdcalls)
    fprintf(stderr,"asp: SPGetParms\n");
  *MaxCmdSize = atpMaxData;
  *QuantumSize = atpMaxData * atpMaxNum;
}

/*
 * Initialize for Server Listening Socket
 *
*/
OSErr
SPInit(SLSEntityIdentifier, ServiceStatusBlock, ServiceStatusBlockSize,
       SLSRefNum)
AddrBlock *SLSEntityIdentifier; /* SLS Net id */
char *ServiceStatusBlock;	/* block with status info */
int ServiceStatusBlockSize;	/* size of status info */
int *SLSRefNum;			/* sls ref num return place */
{
  int err;
  atpProto *ap;
  ASPSSkt *sas;
  OSErr tmp;

  if (isdcalls)
    fprintf(stderr,"asp: SPInit called\n"); 
  if ((tmp = aspsskt_new(SLSRefNum, &sas)) != noErr)
    return(tmp);
  if (ServiceStatusBlockSize > atpMaxData*atpMaxNum)
    return(SizeErr);

  sas->ssb = ServiceStatusBlock;
  sas->ssbl = ServiceStatusBlockSize;
  sas->addr = *SLSEntityIdentifier;
  /* start listener */
  ap = &sas->abr.proto.atp;
  ap->atpSocket = sas->addr.skt;
  ap->atpReqCount = 0;		/* don't need to see the data */
  ap->atpDataPtr = NULL;
  err = cbATPGetRequest(&sas->abr, handle_aspserver, *SLSRefNum);
  if (err != noErr)
    return(noATPResource);
  return(err);
}

/*
 * returns address of remote ss
 *
*/
SPGetNetworkInfo(SessRefNum, addr)
int SessRefNum;
AddrBlock *addr;
{
  ASPSkt *as;

  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL)
    return(ParamErr);
  if (as->state != SP_STARTED)
    return(noATPResource);

  *addr = as->addr;
  return(noErr);
}

/*
 * Handle an incoming request on SLS socket.  Can only be of type:
 *  Tickle, GetStat, or OpenSess
*/
private void
handle_aspserver(abr, SLSRefNum)
ABusRecord *abr;
int SLSRefNum;
{
  ASPUserBytes *aub;
  atpProto *ap;
  ASPSkt *as;
  ASPSSkt *sas;
  OSErr err;

  if ((sas = aspsskt_find_slsrefnum(SLSRefNum)) == NULL) {
    if (isdhand)
      fprintf(stderr, "asp: ASP_SLS: SLS %d invalid (prob. child cleaning)\n",
	      SLSRefNum);
    return;			/* nothing to do - sls is invalid */
  }
  aub = (ASPUserBytes *)&abr->proto.atp.atpUserData;
  if (isdhand)
    fprintf(stderr, "asp: [ASP_SLS: ASPTYPE %s]\n", asptypes[aub->std.b1]);

  if (abr->abResult == noErr) {
    switch (aub->std.b1) {	/* get command */
    case aspOpenSess:
      asp_doopensess(SLSRefNum, aub, abr);
      break;
    case aspTickle:
      if ((as = aspskt_find_sessid(SLSRefNum, aub->std.b2)) == NULL) {
	if (isdhand)
	  fprintf(stderr,"asp: Got tickle for sessid %d, but no ses\n",
		  aub->std.b2);
	break;
      }
      if (isdhand)
	fprintf(stderr,"asp: Got tickle on %d\n",aub->std.b2);
      reset_ttimer(as);
      break;
    case aspGetStat:
      asp_dosendstatus(SLSRefNum, abr);
      break;
    default:
      if (isdhand)
	fprintf(stderr, "asp: Misdirected request on ASP SLS\n");
    }
  }

  if (abr->abResult == sktClosed) {
    while ((as = aspskt_find_active(SLSRefNum)) != NULL)
      *as->comp = sktClosed;
    return;
  }

  ap = &sas->abr.proto.atp;
  ap->atpSocket = sas->addr.skt;
  ap->atpReqCount = 0;		/* don't need to see the data */
  ap->atpDataPtr = NULL;
  err = cbATPGetRequest(&sas->abr, handle_aspserver, SLSRefNum);
  /* what to do with error?  should report if we get a really bad one */
  if (err != noErr)
    fprintf(stderr, "asp: GetRequest fails on SLS %d!  Server is dead!\n",
	    SLSRefNum);
}

/*
 * Try to open a session - server only
 *
*/
private void
asp_doopensess(SLSRefNum, aub, abr)
int SLSRefNum;
ASPUserBytes *aub;
ABusRecord *abr;
{
  ASPSkt *as;
  int err;

  if (isdhand)
    fprintf(stderr,"asp: Server: remote wants connection: protocol level %x\n",
	    ntohs(aub->std.data));
  if (ntohs(aub->std.data) != ASP_PROTOCOL_VERSION) {
    if (isdhand)
      fprintf(stderr,"asp: Server: connection refused - protocol level %x\n",
	      ASP_PROTOCOL_VERSION);
    sessopenreply(SLSRefNum, abr, BadVersNum, 0, (byte)0);
    return;
  }

  if ((as = aspskt_find_active(SLSRefNum)) == NULL) { 
    /* no getsessions active */
    if (isdhand)
      fprintf(stderr,"asp: Server: no get session active, server busy\n");
#ifdef DEBUGAUFS
    logit(0, "asp: Server %d: no get session active, server busy", SLSRefNum);
    dumpsockets(SLSRefNum);
#endif
    sessopenreply(SLSRefNum, abr, ServerBusy, 0, (byte)0);
    return;
  }

  as->addr = abr->proto.atp.atpAddress;
  as->addr.skt = 0;		/* accept for any socket on remote */

  if (as->ss == -1)
    as->ss = 0;			/* use zero to indicate dynamic allocation */
  if ((err = ATPOpenSocket(&as->addr, &as->ss)) < 0) {
    /* woops */
    as->ss = -1;
    as->state = SP_INACTIVE;	/* close down srn */
    *as->comp = NoMoreSessions;
    aspskt_free(as);		/* get rid of it */
    if (isdhand)
      fprintf(stderr,"asp: Server: out of sockets! atp err %d\n", err);
#ifdef DEBUGAUFS
    logit(0, "asp: Server %d: out of sockets: err %d", SLSRefNum, err);
#endif
    sessopenreply(SLSRefNum, abr, ServerBusy, 0, (byte)0);
    return;
  }
  as->addr.skt = aub->std.b2; /* wss */
  if (isdhand)
    fprintf(stderr,"asp: Server: conn. initiated: id %d on wss %d, ss %d\n",
	    as->SessID, as->addr.skt, as->ss);
#ifdef DEBUGAUFS
    logit(0, "asp: Server: conn. initiated: id %d on wss %d, ss %d",
	    as->SessID, as->addr.skt, as->ss);
#endif
  sessopenreply(SLSRefNum, abr, noErr, as->ss, (byte)as->SessID);
  as->state = SP_STARTED;
  as->tickle_abr.proto.atp.atpAddress = as->addr;
  as->tickle_abr.proto.atp.atpSocket = as->ss;	/* remote WSS */
  startasptickle(as);
  start_ttimer(as);
  *as->comp = noErr;		/* done */
}

/*
 * reply to an open session call from a client
 *
*/
private void
sessopenreply(SLSRefNum, abr, errcode, ss, sessid)
int SLSRefNum;
ABusRecord *abr;
int errcode;
int ss;
byte sessid;
{
  ASPQE *aspqe;
  atpProto *ap;
  ASPUserBytes *aub;
  int cnt;
  ASPSSkt *sas = aspsskt_find_slsrefnum(SLSRefNum);

  if (isdhand)
    fprintf(stderr,"asp: Server: opensessionreply\n");
  if (sas == NULL)		/* slsrefnum invalid */
    return;

  aspqe = create_aspaqe();
  aspqe->type = tSP_Special_DROP;
  ap = &aspqe->abr.proto.atp;
  ap->atpSocket = sas->addr.skt;
  ap->atpAddress = abr->proto.atp.atpAddress;
  ap->atpTransID = abr->proto.atp.atpTransID;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, (char *)NULL,0, (dword)0);
  aspqe->bds[0].userData = 0;
  aub = (ASPUserBytes *)&aspqe->bds[0].userData ;
  aub->std.b1 = ss;
  aub->std.b2 = sessid;
  aub->std.data = htons(errcode);
  ap->atpRspBDSPtr = aspqe->bds;
  ap->fatpEOM = (abr->proto.atp.atpBitMap >> cnt) != 0 ? 1 : 0 ;
  ap->atpNumBufs = cnt;
  ap->atpBDSSize = cnt;
  if (cbATPSndRsp(&aspqe->abr, handle_asp_special, aspqe) != noErr) {
    /* well, we can get rid of the unused pointer at least */
    delete_aspaqe(aspqe);
  }
}

/*
 *
 * Send a status report back
 *
*/
asp_dosendstatus(SLSRefNum, abr)
int SLSRefNum;
ABusRecord *abr;
{
  ASPSSkt *sas = aspsskt_find_slsrefnum(SLSRefNum);
  ASPQE *aspqe;
  atpProto *ap;
  int cnt;

  if (isdhand)
    fprintf(stderr,"asp: Server: sendstatus\n");
  if (sas == NULL)		/* nothing to do */
    return;

  aspqe = create_aspaqe();
  aspqe->type = tSP_Special_DROP;
  ap = &aspqe->abr.proto.atp;
  ap->atpSocket = sas->addr.skt;
  ap->atpAddress = abr->proto.atp.atpAddress;
  ap->atpTransID = abr->proto.atp.atpTransID;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, sas->ssb, sas->ssbl,
		  (dword)0);
  ap->atpRspBDSPtr = aspqe->bds;
  ap->fatpEOM = (abr->proto.atp.atpBitMap >> cnt) != 0 ? 1 : 0 ;
  ap->atpNumBufs = cnt;
  ap->atpBDSSize = cnt;
  if (cbATPSndRsp(&aspqe->abr, handle_asp_special, aspqe) != noErr) {
    delete_aspaqe(aspqe);	/* get rid */
  }
  /* what to do with err? just ignore*/
}

/*
 * Watch SLS for a open to transfer to the Server Service Socket (SSS)
 *
*/
OSErr
SPGetSession(SLSRefNum, SessRefNum, comp)
int SLSRefNum;
int *SessRefNum;
int *comp;
{
  ASPSkt *as;
  OSErr tmp;
  int i;

  if (isdcalls)
    fprintf(stderr,"asp: SPGetSession - SLS %d\n",SLSRefNum);
  if (!aspsskt_isactive(SLSRefNum)) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if ((tmp=aspskt_new(SessRefNum, &as)) != noErr) {
    *comp = tmp;
    return(tmp);
  }

  as->type = SP_SERVER;
  as->wqueue = NULL;
  as->state = SP_STARTING;
  as->SLSRefNum = SLSRefNum;
  as->ss = -1; /* unknown at present */
  as->comp = comp;
  /* check for in use?  should be no prob */
  if (sessid_not_inited) {
    next_sessid = time(0L) & 0xff; /* random hopefully */
    sessid_not_inited = FALSE;
  }
  /* make sure sessid is unique on sls refnum being careful to stop */
  /* after all the sessids have been checked */
  i = 0;
  while (aspskt_find_sessid(SLSRefNum, (byte)next_sessid) != NULL) {
    next_sessid = ++next_sessid & 0xff; /* single byte */
    if (i++ > 255)
      return(NoMoreSessions);
  }
  as->SessID = (byte)next_sessid;
  next_sessid = ++next_sessid & 0xff; /* single byte */
#ifdef DEBUGAUFS
  logit(0, "asp: getsession: looking for connection on %d with sessid %d",
      as->SessRefNum, as->SessID);
#endif
  *comp = 1;
  return(noErr);
}

/*
 * Close down a Service Socket socket
 *
*/
OSErr
SPCloseSession(SessRefNum, atpretries, atptimeout, comp)
int SessRefNum;
int atpretries;
int atptimeout;
int *comp;
{
  ASPSkt *as;
  atpProto *ap;
  ASPUserBytes *aub;
  ASPQE *aspqe;
  int cnt, err;

  if (isdcalls)
    fprintf(stderr,"asp: SPCloseSession - srn %d\n",SessRefNum);
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }

  switch (as->state) {
  case SP_STARTED:
    break;
  case SP_HALFCLOSED:
#ifdef notdef
    /* this is wrong, want the close to go out if server calls aspclose */
    return(spshutdown(SessRefNum));
#endif
    /* allow halfclosed sockets to be closed by server */
    break;
  default:
    aspskt_free(as);
    return(noErr);
  }
  
  aspqe = create_aspaqe();
  aspqe->type = tSPClose;
  aspqe->comp = comp;
  aspqe->SessRefNum = SessRefNum;

  ap = &aspqe->abr.proto.atp;
  ap->atpUserData = (dword)0;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspCloseSession;
  aub->std.b2 = as->SessID;
  ap->atpAddress = as->addr;
  if (as->state == SP_HALFCLOSED) {
    /* In case we are half-closed, we use the sls to send sp attn */
    ASPSSkt *sas = aspsskt_find_slsrefnum(as->SLSRefNum);
    if (sas)
      ap->atpSocket = sas->addr.skt;
    else {
      delete_aspaqe(aspqe);
      *comp = ParamErr;
      return(ParamErr);
    }
  } else 
    ap->atpSocket = as->ss;
  ap->atpReqCount = 0;
  ap->atpDataPtr = NULL;
  cnt = setup_bds(aspqe->bds, 1, atpMaxData, (char *)NULL, 0, (dword)0);
  ap->atpRspBDSPtr = aspqe->bds;
  ap->atpNumBufs = cnt;
  ap->fatpXO = FALSE;
  ap->atpRetries = atpretries;
  ap->atpTimeOut = atptimeout <= 0 ? ASPCLOSESESSIONTIMEOUT : atptimeout;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);		/* get rid of it */
  return(asp_cksndrq_err("ASPClose", err, comp));
}

/*
 * Get a request on a SSS
 *
*/
SPGetRequest(SessRefNum, ReqBuff, ReqBuffSize, ReqRefNum, SPReqType,
	     ActRcvdReqLen, comp)
int SessRefNum;
char *ReqBuff;
int ReqBuffSize;
ASPQE **ReqRefNum;
int *SPReqType;
int *ActRcvdReqLen;
int *comp;
{
  atpProto *ap;
  ASPSkt *as;
  ASPQE *aspqe;
  int err;

  if (isdcalls)
    fprintf(stderr,"asp: SPGetRequest - srn %d\n",SessRefNum);
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state != SP_STARTED) {
    *comp = SessClosed;
    return(SessClosed);
  }
  if (as->ss == -1) {		/* bad call */
    *comp = ParamErr;
    return(ParamErr);
  }
  aspqe = create_aspaqe();	/* will never return bad, dies instead */
  aspqe->SessRefNum = SessRefNum;
  aspqe->type = tSPGetRequest;
  aspqe->ReqRefNum = ReqRefNum;
  aspqe->SPReqType = SPReqType;
  aspqe->ActRcvdReqLen = ActRcvdReqLen;
  aspqe->comp = comp;

  ap = &aspqe->abr.proto.atp;
  ap->atpReqCount = ReqBuffSize;
  ap->atpDataPtr = ReqBuff;
  ap->atpSocket = as->ss;

  *comp = 1;
  err = cbATPGetRequest(&aspqe->abr, handle_asp_getreq, aspqe);
  if (err != noErr) {
    delete_aspaqe(aspqe);
    *comp = noATPResource;
    return(noATPResource);
  }
  return(noErr);
}

/*
 * handle completion of the SPGetRequest command
 *
*/
private void
handle_asp_getreq(abr, aspqe)
ABusRecord *abr;
ASPQE *aspqe;
{
  ASPUserBytes *aub;
  ASPSkt *as;

  if (isdhand) 
    fprintf(stderr, "asp: handle_sndreq with aspqe %x\n",aspqe);
  if (aspqe == NULL)
    return;			/* drop */
  if (abr == NULL || aspqe == NULL) {
    fprintf(stderr,"asp: fatal error: handle_asp_getreq - abr or aspqe NIL\n");
    exit(255);
    return;
  }

  if (aspqe->type != tSPGetRequest) {
    if (isdhand)
      fprintf(stderr,"asp: GetReq handler with bad aspqe %x - type %s\n",
	      aspqe, aspevents[aspqe->type]);
    delete_aspaqe(aspqe);
  }

  as = aspskt_find_sessrefnum(aspqe->SessRefNum);

  switch (abr->abResult) {
  case noErr:
    if (!as) {			/* no sess? ugh */
#ifdef DEBUGAUFS
      logit(0, "Session %d not active!!! Return SessClosed", aspqe->SessRefNum);
#endif
      *aspqe->comp = SessClosed;
      break;
    }
    aub = (ASPUserBytes *)&abr->proto.atp.atpUserData;
    /*** change  aub->std.data to aub->std.b2 ****/
    if (as->SessID != aub->std.b2) {
      if (isdhand)
	fprintf(stderr,"asp: Bad Req - Sessid = %d, ours is %d\n",
		aub->std.b2, as->SessID);
#ifdef DEBUGAUFS
      logit(0, "asp: Bad Req - Sessid = %d, ours is %d",
		aub->std.b2, as->SessID);
#endif
      *aspqe->comp = BadReqRcvd;
      break;
    }
    *aspqe->comp = abr->abResult;
    if (isdhand)
      fprintf(stderr, "asp: hgetreq: Sessid %d, reqrefnum %x, type %s\n",
	      as->SessID, aspqe, asptypes[aub->std.b1]);
    *aspqe->ReqRefNum = aspqe; /* cheap, but very bad */
    *aspqe->ActRcvdReqLen = abr->proto.atp.atpActCount;
    *aspqe->comp = noErr;
    *aspqe->SPReqType = aub->std.b1; /* mark */
    switch (aub->std.b1) {
    case aspCommand:
    case aspWrite:
      return;			/* just return, everything else is done */
    case aspCloseSession:
#ifdef DEBUGAUFS
      logit(0, "asp: Close on sessid %d, session %d",
	  aub->std.b2, as->SessRefNum);
#endif
      *aspqe->comp = SessClosed;
      do_sendclosesessreply(as, abr);
      break;
    default:
      /* what to do? */
      if (isdhand)
	fprintf(stderr,"asp: SPGetReq: Received unexpected request %d\n",
		aub->std.b1);
      *aspqe->comp = BadReqRcvd;
      break;
    }
    break;
  case sktClosed:
    *aspqe->comp = SessClosed;
    break;
  default:
    if (isdhand)
      fprintf(stderr, "asp: SPGetReq: bad atp completion %d\n",abr->abResult);
    *aspqe->comp = aspFault;
    break;
  }
  delete_aspaqe(aspqe);
}



/*
 * Reply to a request to an SSS from a WSS
 *
*/
SPCmdReply(SessRefNum, ReqRefNum, CmdResult, CmdReplyData, CmdReplyDataSize,
	   comp)
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  if (isdcalls)
    fprintf(stderr,"asp: SPCmdReply - srn %d, rrn %x, reply size %d\n",
	    SessRefNum, ReqRefNum, CmdReplyDataSize);
  return(spreply(tSPCmdReply, SessRefNum, ReqRefNum, CmdResult, CmdReplyData,
	  CmdReplyDataSize, comp));
}

/*
 *  final reply to a SPWrite request to an SSS from an WSS.  
 * 
*/
SPWrtReply(SessRefNum, ReqRefNum, CmdResult, CmdReplyData, CmdReplyDataSize,
	   comp)
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  if (isdcalls)
    fprintf(stderr,"asp: SPWrtReply - srn %d, rrn %x, reply size %d\n",
	    SessRefNum, ReqRefNum, CmdReplyDataSize);
  return(spreply(tSPWrtReply, SessRefNum, ReqRefNum, CmdResult, CmdReplyData,
	  CmdReplyDataSize, comp));
}

private int
spreply(type, SessRefNum, ReqRefNum, CmdResult, CmdReplyData, CmdReplyDataSize,
	comp)
int type;
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  atpProto *ap;
  ASPSkt *as;  
  ASPQE *aspqe;
  int cnt, err;

  if (CmdReplyDataSize < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (CmdReplyDataSize > atpMaxNum*atpMaxData) {
    *comp = SizeErr;
    return(SizeErr);
  }
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state != SP_STARTED) { /* really means srn isn't active yet */
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->ss == -1) {		/* bad call */
    *comp = ParamErr;
    return(ParamErr);
  }

  aspqe = create_aspaqe();
  aspqe->type = type;
  aspqe->SessRefNum = SessRefNum;
  aspqe->comp = comp;

  /* setup bds */
  ap = &aspqe->abr.proto.atp;
  ap->atpSocket = as->ss;

  ap->atpAddress = ReqRefNum->abr.proto.atp.atpAddress;
  ap->atpTransID = ReqRefNum->abr.proto.atp.atpTransID;

  /* We blithely attempt to send out all the data, regardless of the */
  /* bitmap sent by the remote.  According to the ASP document, the */
  /* client should have been smart enough to ask for one more response */
  /* than data if we are on a 578 (atpmaxdata) boundary and will be able */
  /* figure out there is size error - the extra pkts outside the bitmap */
  /* should simply be dropped */
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, CmdReplyData,
		  CmdReplyDataSize, (dword)0);
  aspqe->bds[0].userData = htonl(CmdResult);	/* only for first */
  ap->atpRspBDSPtr = aspqe->bds;
  /* since we only send a response once, we should always set EOM */
  ap->fatpEOM = 1;
  ap->atpNumBufs = cnt;
  ap->atpBDSSize = cnt;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRsp(&aspqe->abr, handle_asp_rspdone, aspqe);
  delete_aspaqe(ReqRefNum);	/* is this right?  Suppose so... */
  if (err != noErr) {
    delete_aspaqe(aspqe);	/* get rid of it */
    if (err == badBuffNum) {
      *comp = ParamErr;		/* bad ReqRefNum */
      return(ParamErr);
    }
    *comp = noATPResource;
    return(noATPResource);
  }
  return(noErr);
}


/*
 * Allow a write to continue (equiv - this is a read call) based upon
 * request to an SSS from a WSS (client)
 *
*/
SPWrtContinue(SessRefNum, ReqRefNum, Buffer, BufferSize, ActLenRcvd, 
	      atptimeout, comp)
int SessRefNum;
ASPQE *ReqRefNum;
char *Buffer;
int BufferSize;
int *ActLenRcvd;
int atptimeout;
int *comp;
{
  atpProto *ap;
  ASPUserBytes *aub;
  ASPSkt *as;  
  ASPQE *aspqe;
  int cnt, err;

  if (isdcalls)
    fprintf(stderr,"asp: SPWrtContinue: srn %d, rrn %x, bufsize %d\n",
	    SessRefNum, ReqRefNum, BufferSize);
  if (BufferSize < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state != SP_STARTED) {
    *comp = SessClosed;
    return(SessClosed);
  }
  if (as->ss == -1) {		/* bad call */
    *comp = ParamErr;
    return(ParamErr);
  }

  aspqe = create_aspaqe();
  aspqe->type = tSPWrtContinue;
  aspqe->SessRefNum = SessRefNum;
  aspqe->ActRcvdReplyLen = ActLenRcvd; /* overload */
  aspqe->comp = comp;

  ap = &aspqe->abr.proto.atp;
  /* get sessid, seqno info */
  ap->atpUserData = ReqRefNum->abr.proto.atp.atpUserData;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspWriteData;
/*  ap->atpAddress = ReqRefNum->abr.proto.atp.atpAddress; */
  ap->atpAddress = as->addr;
  ap->atpSocket = as->ss;


  ap->atpReqCount = sizeof(word);
  aspqe->availableBufferSize = htons((word)BufferSize);
  ap->atpDataPtr = (char *)&aspqe->availableBufferSize;
  ap->atpRspBDSPtr = aspqe->bds;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, Buffer,
		  BufferSize, (dword) 0);
  ap->atpNumBufs = cnt;
  ap->fatpXO = TRUE;
  ap->atpRetries = 255;		/* infinite retries */
  ap->atpTimeOut = atptimeout <= 0 ? ASPWRITETIMEOUT : atptimeout;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);		/* get rid of it */
  return(asp_cksndrq_err("SPWrtContinue", err, comp));
}

/*
 * establish new status on the SSS
 *
*/
SPNewStatus(SLSRefNum, ServiceStatusBlock, ServiceStatusBlockSize)
int SLSRefNum;
char *ServiceStatusBlock;
int ServiceStatusBlockSize;
{
  ASPSSkt *sas = aspsskt_find_slsrefnum(SLSRefNum);

  if (isdcalls)
    fprintf(stderr,"asp: SPNewStatus: SLS %d\n",SLSRefNum);
  if (sas == NULL)
    return(ParamErr);

  sas->ssb = ServiceStatusBlock;
  sas->ssbl = ServiceStatusBlockSize;
  return(noErr);
}

/*
 * Send attn signal to WSS.
 *
*/
SPAttention(SessRefNum, AttentionCode, atpretries, atptimeout, comp)
int SessRefNum;
word AttentionCode;
int atpretries;
int *comp;
{
  atpProto *ap;
  ASPUserBytes *aub;
  ASPSkt *as;  
  ASPQE *aspqe;
  int cnt, err;

  if (isdcalls)
    fprintf(stderr,"asp: SPattention: srn %d, code %x\n",SessRefNum, AttentionCode);

  if (AttentionCode == (word)0) {
    *comp = ParamErr;
    return(ParamErr);
  }

  if ((as=aspskt_find_sessrefnum(SessRefNum))==NULL ||as->state==SP_STARTING) {
    *comp = ParamErr;
    return(ParamErr);
  }

  aspqe = create_aspaqe();
  aspqe->type = tSPAttention;
  aspqe->comp = comp;

  ap = &aspqe->abr.proto.atp;
  /* get sessid, seqno info */
  ap->atpUserData = 0;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspAttention;
  aub->std.b2 = as->SessID;
  aub->std.data = htons(AttentionCode);
  ap->atpAddress = as->addr;
  if (as->state == SP_HALFCLOSED) {
    /* In case we are half-closed, we use the sls to send sp attn */
    ASPSSkt *sas = aspsskt_find_slsrefnum(as->SLSRefNum);
    if (sas)
      ap->atpSocket = sas->addr.skt;
    else {
      delete_aspaqe(aspqe);
      *comp = ParamErr;
      return(ParamErr);
    }
  } else 
    ap->atpSocket = as->ss;

  ap->atpReqCount = 0;
  ap->atpDataPtr = NULL;
  ap->atpRspBDSPtr = aspqe->bds;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, (char *)NULL,0,(dword)0);
  ap->atpNumBufs = cnt;
  ap->fatpXO = FALSE;
  ap->atpRetries = atpretries;
  ap->atpTimeOut = atptimeout <= 0 ? ASPATTENTIONTIMEOUT : atptimeout;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);
  return(asp_cksndrq_err("SPAttention", err, comp));
}

/* workstation calls */

/* spgetparms as above */
SPGetStatus(SLSEntityIdentifier, StatusBuffer, StatusBufferSize,
	    ActRcvdStatusLen, atpretries, atptimeout, comp)
AddrBlock *SLSEntityIdentifier;
char *StatusBuffer;
int StatusBufferSize;
int *ActRcvdStatusLen;
int atpretries;
int atptimeout;
int *comp;
{
  atpProto *ap;
  ASPUserBytes *aub;
  ASPQE *aspqe;
  int cnt, err;


  if (isdcalls)
    fprintf(stderr,"asp: SPGetStatus called\n");
  aspqe = create_aspaqe();
  aspqe->type = tSPGetStat;
  aspqe->ActRcvdStatusLen = ActRcvdStatusLen;
  aspqe->comp = comp;

  ap = &aspqe->abr.proto.atp;
  ap->atpUserData = (dword)0;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspGetStat;
  ap->atpSocket = 0;
  ap->atpAddress = *SLSEntityIdentifier;
  ap->atpReqCount = 0;
  ap->atpDataPtr = NULL;
  ap->atpRspBDSPtr = aspqe->bds;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, StatusBuffer,
		  StatusBufferSize, (dword)0);
  /* we need this to ensure that we can figure out if a size error occurs */
  if (cnt < atpMaxNum && ((StatusBufferSize % atpMaxData) == 0)) {
    /* empty bds entry */
    aspqe->bds[cnt].buffPtr = NULL;
    aspqe->bds[cnt].dataSize = 0;	/* init */
    aspqe->bds[cnt].buffSize = 0;	/* no data here */
    cnt++;
  }
  ap->fatpXO = FALSE;
  ap->atpTimeOut = atptimeout <= 0 ? ASPGETSTATTIMEOUT : atptimeout;
  ap->atpRetries = atpretries;
  ap->atpNumBufs = cnt;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);
  return(asp_cksndrq_err("SPGetStatus", err, comp));
}

SPOpenSession(SLSEntityIdentifier, AttnRoutine, SessRefNum, atpretries,
	      atptimeout, comp)
AddrBlock *SLSEntityIdentifier;
int (*AttnRoutine)();
int *SessRefNum;
int atptimeout;
int *comp;
{
  atpProto *ap;
  ASPUserBytes *aub;
  ASPSkt *as;  
  ASPQE *aspqe;
  int cnt, err;
  OSErr tmp;
  AddrBlock useaddr;

  if (isdcalls)
    fprintf(stderr,"asp: SPOpenSession called\n");

  if ((tmp=aspskt_new(SessRefNum, &as)) != noErr) {
    *comp = tmp;
    return(tmp);
  }

  as->type = SP_CLIENT;
  as->next_sequence = 0;
  as->wqueue = NULL;
  as->state = SP_STARTING;
  as->addr = *SLSEntityIdentifier;

  as->ss = 0;
  useaddr = *SLSEntityIdentifier;
  useaddr.skt = 0;
  if ((err = ATPOpenSocket(&useaddr, &as->ss)) != noErr) {
    aspskt_free(as);		/* get rid of this */
    *comp = noATPResource;
    return(noATPResource);
  }

  as->attnroutine = AttnRoutine;

  aspqe = create_aspaqe();
  aspqe->type = tSPOpenSess;
  aspqe->SessRefNum = *SessRefNum;
  aspqe->comp = comp;

  ap = &aspqe->abr.proto.atp;
  ap->atpUserData = (dword)0;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspOpenSess;
  aub->std.b2 = as->ss;
  aub->std.data = htons(ASP_PROTOCOL_VERSION);
  ap->atpAddress = *SLSEntityIdentifier;
  ap->atpSocket = as->ss;

  ap->atpReqCount = 0;
  ap->atpDataPtr = NULL;
  ap->atpRspBDSPtr = aspqe->bds;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, (char *)NULL, 0,(dword)0);
  ap->fatpXO = TRUE;
  ap->atpTimeOut = atptimeout <= 0 ? ASPOPENSESSTIMEOUT : atptimeout;
  ap->atpRetries = atpretries;
  ap->atpNumBufs = cnt;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);
  return(asp_cksndrq_err("SPOpenSess",err,comp));
}

/*
 * Handle incoming requests for a client process
 *
*/
private void
handle_aspclient(abr, SessRefNum)
ABusRecord *abr;
int SessRefNum;
{
  ASPSkt *as;
  ASPUserBytes *aub; 
  atpProto *ap;

  aub = (ASPUserBytes *)&abr->proto.atp.atpUserData;
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    if (isdhand)
      fprintf(stderr, "asp: ASP_WSS: srn %d not found, sessid sent %d\n",
	      SessRefNum, aub->std.b2);
    return;
  }
  if (isdhand)
    fprintf(stderr,"asp: [ASP_WSS: ASPTYPE %s, SESSID sent %d, local %d]\n",
	    asptypes[aub->std.b1], aub->std.b2,as->SessID);


  switch (abr->abResult) {
  case noErr:
    if (aub->std.b2 != as->SessID) {
      if (isdhand)
	fprintf(stderr, "asp:  Misdirected request on ASP WSS\n");
      break;
    }
    switch (aub->std.b1) {
    case aspTickle:
      reset_ttimer(as);
      break;
    case aspCloseSession:
      do_sendclosesessreply(as, abr);
      return;			/* don't restart */
    case aspWriteData:
      asp_do_write(abr, (word)ntohs(aub->std.data), as);
      break;
    case aspAttention:
      do_sendreply(as, abr);
      (*as->attnroutine)(SessRefNum, aub->std.b2, (word)ntohs(aub->std.data));
      break;
    }
    break;
  case sktClosed:
    if (isdhand)
      fprintf(stderr, "asp: handle_aspclient: skt closed\n");
    return;
  default:
    if (isdhand)
      fprintf(stderr, "asp: handle_aspclient: bad atp completion %d\n",
	      abr->abResult);
    break;
  }

  ap = &as->rabr.proto.atp;
  ap->atpSocket = as->ss;
  ap->atpReqCount = sizeof(as->reqdata);
  ap->atpDataPtr = (char *)&as->reqdata;
  cbATPGetRequest(&as->rabr, handle_aspclient, SessRefNum);
  /* ignore error */
}

SPCommand(SessRefNum, CmdBlock, CmdBlockSize, ReplyBuffer, ReplyBufferSize,
	  CmdResult, ActRcvdReplyLen, atptimeout, comp)
int SessRefNum;
char *CmdBlock;
int CmdBlockSize;
char *ReplyBuffer;
int ReplyBufferSize;
dword *CmdResult;
int *ActRcvdReplyLen;
int atptimeout;
int *comp;
{
  atpProto *ap;
  ASPUserBytes *aub;
  ASPSkt *as;  
  ASPQE *aspqe;
  int cnt, err;

  if (isdcalls)
    fprintf(stderr,"asp: SPCommand: srn %d, cmdsize %d, replysize %d\n",
	    SessRefNum, CmdBlockSize, ReplyBufferSize);
  if ((as=aspskt_find_sessrefnum(SessRefNum))==NULL ||as->state==SP_STARTING) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->ss == -1) {		/* bad call */
    *comp = ParamErr;
    return(ParamErr);
  }

  aspqe = create_aspaqe();
  aspqe->type = tSPCommand;
  aspqe->SessRefNum = SessRefNum;
  aspqe->comp = comp;
  aspqe->CmdResult = CmdResult;
  aspqe->ActRcvdReplyLen = ActRcvdReplyLen;

  ap = &aspqe->abr.proto.atp;
  ap->atpUserData = (dword)0;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspCommand;
  aub->std.b2 = as->SessID;
  aub->std.data = htons(as->next_sequence);
  as->next_sequence = ++as->next_sequence % 65536;
  ap->atpAddress = as->addr;
  ap->atpSocket = as->ss;

  ap->atpReqCount = CmdBlockSize;
  ap->atpDataPtr = CmdBlock;
  ap->atpRspBDSPtr = aspqe->bds;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, ReplyBuffer,
		  ReplyBufferSize, (dword)0);
  /* we need this to ensure that we can figure out if a size error occurs */
  if (cnt < atpMaxNum && ((ReplyBufferSize % atpMaxData) == 0)) {
    /* empty bds entry */
    aspqe->bds[cnt].buffPtr = NULL;
    aspqe->bds[cnt].dataSize = 0;	/* init */
    aspqe->bds[cnt].buffSize = 0;	/* no data here */
    cnt++;
  }
  ap->fatpXO = TRUE;
  ap->atpTimeOut = atptimeout <= 0 ? ASPCOMMANDTIMEOUT : atptimeout;
  ap->atpRetries = 255;		/* infinite */
  ap->atpNumBufs = cnt;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);
  return(asp_cksndrq_err("SPCommand", err, comp));
}


SPWrite(SessRefNum, CmdBlock, CmdBlockSize, WriteData, WriteDataSize,
	ReplyBuffer, ReplyBufferSize, CmdResult, ActLenWritten,
	ActRcvdReplyLen, atptimeout, comp)
int SessRefNum;
char *CmdBlock;
int CmdBlockSize;
char *WriteData;
int WriteDataSize;
char *ReplyBuffer;
int ReplyBufferSize;
dword *CmdResult;
int *ActLenWritten;
int *ActRcvdReplyLen;
int atptimeout;
int *comp;
{
  atpProto *ap;
  ASPUserBytes *aub;
  ASPSkt *as;  
  ASPQE *aspqe, *aspwe;
  int cnt, err;

  if (isdcalls)
    fprintf(stderr,"asp: SPWrite: srn %d, cmdsize %d, wds %d, replysize %d\n",
	    SessRefNum, CmdBlockSize, WriteDataSize, ReplyBufferSize);

  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state == SP_STARTING) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->ss == -1) {		/* bad call */
    *comp = ParamErr;
    return(ParamErr);
  }

  aspwe = create_aspawe(as);
  aspwe->type = tSPWrite2;
  aspwe->SessRefNum = SessRefNum;
  aspwe->WriteData = WriteData;
  aspwe->WriteDataSize = WriteDataSize;
  aspwe->ActLenWritten = ActLenWritten;

  aspqe = create_aspaqe();
  aspqe->type = tSPWrite;
  aspqe->SessRefNum = SessRefNum;
  aspqe->comp = comp;
  aspqe->CmdResult = CmdResult;
  aspqe->ActRcvdReplyLen = ActRcvdReplyLen;

  ap = &aspqe->abr.proto.atp;
  ap->atpUserData = (dword)0;
  aub = (ASPUserBytes *)&ap->atpUserData;
  aub->std.b1 = aspWrite;
  aub->std.b2 = as->SessID;
  aub->std.data = htons(as->next_sequence);
  aspwe->seqno = as->next_sequence;
  as->next_sequence = ++as->next_sequence % 65536;
  ap->atpAddress = as->addr;
  ap->atpSocket = as->ss;

  ap->atpReqCount = CmdBlockSize;
  ap->atpDataPtr = CmdBlock;
  ap->atpRspBDSPtr = aspqe->bds;
  cnt = setup_bds(aspqe->bds, atpMaxNum, atpMaxData, ReplyBuffer,
		  ReplyBufferSize, (dword)0);
  /* we need this to ensure that we can figure out if a size error occurs */
  if (cnt < atpMaxNum && ((ReplyBufferSize % atpMaxData) == 0)) {
    /* empty bds entry */
    aspqe->bds[cnt].buffPtr = NULL;
    aspqe->bds[cnt].dataSize = 0;	/* init */
    aspqe->bds[cnt].buffSize = 0;	/* no data here */
    cnt++;
  }
  ap->fatpXO = TRUE;
  ap->atpTimeOut = atptimeout <= 0 ? ASPWRITETIMEOUT : atptimeout;
  ap->atpRetries = 255;		/* infinite */
  ap->atpNumBufs = cnt;
  *comp = 1;			/* mark waiting */
  err = cbATPSndRequest(&aspqe->abr, handle_asp_sndreq, aspqe);
  if (err == noErr)
    return(noErr);
  delete_aspaqe(aspqe);
  delete_aspawe(aspwe, as);
  return(asp_cksndrq_err("SPWrite", err, comp));
}

/*
 * continue the write started by SPWrite - send the data after
 * a wrtcontinue from the remote
 *
*/
private void
asp_do_write(abr, seqno, as)
ABusRecord *abr;
word seqno;
ASPSkt *as;
{
  ASPQE *aspwe;
  atpProto *ap;
  int cnt, towrite;


  if (isdhand)
    fprintf(stderr,"asp: Respond to wrtcontinue: with seqno %d on as %x\n",
	    seqno, as);
  aspwe = find_aspawe(as, seqno);
  if (aspwe == NULL)		/* drop then */
    return;

  if (isdhand)
    fprintf(stderr,"asp: Response is with aspawe %x\n",aspwe);

  if (abr->proto.atp.atpActCount < sizeof(as->reqdata)) {
    /* This means we got a bad request here */
    /* because we don't have the count to write */
    return;
  }

  /* setup bds */
  ap = &aspwe->abr.proto.atp;
  ap->atpSocket = as->ss;

  ap->atpAddress = abr->proto.atp.atpAddress;
  ap->atpTransID = abr->proto.atp.atpTransID;

  towrite = min(ntohs(as->reqdata), aspwe->WriteDataSize);
  if (isdhand)
    fprintf(stderr,"asp: Writting %d on aspawe %x\n",towrite,aspwe);
  *aspwe->ActLenWritten = towrite;
  cnt = setup_bds(aspwe->bds, atpMaxNum, atpMaxData, aspwe->WriteData,
		  towrite, (dword)0);
  ap->atpRspBDSPtr = aspwe->bds;
  ap->fatpEOM = 1;
  ap->atpNumBufs = cnt;
  ap->atpBDSSize = cnt;
  cbATPSndRsp(&aspwe->abr, handle_asp_special, aspwe);
  /* we should really figure out how to handle errors here */
}


/*
 * handle asp protocol events on ss sockets for sndrequests
 *
 * For now: SPWrite, SPCommand, SPWrtContinue, SPGetStat, SPOpenSession,
 *          SPAttention
 *
*/
private void
handle_asp_sndreq(abr, aspqe)
ABusRecord *abr;
ASPQE *aspqe;
{
  atpProto *ap;
  ASPSkt *as;
  ASPUserBytes *aub;
  int rds, rrs;

  if (isdhand)
    fprintf(stderr, "asp: handle_asp_sndreq with aspqe %x\n",aspqe);
  if (aspqe == NULL)
    return;			/* drop */
  if (abr == NULL || aspqe == NULL) {
    fprintf(stderr,"asp: fatal error: handle_asp_sndreq - abr or aspqe NIL\n");
    exit(255);
    return;
  }
  if (isdhand)
    fprintf(stderr, "asp: hsndreq: event code %s\n",aspevents[aspqe->type]);

  if ((as = aspskt_find_sessrefnum(aspqe->SessRefNum)) == NULL)
    if (isdhand)
      fprintf(stderr, "asp: hsndreq: srn %d not found\n",aspqe->SessRefNum);

  /* should check type (server/client) */
  switch (aspqe->type) {
  case tSPWrite:
  case tSPCommand:
  case tSPWrtContinue:
    switch (abr->abResult) {
    case noErr:
      if (aspqe->type != tSPWrtContinue)
	*aspqe->CmdResult = ntohl(aspqe->bds[0].userData);
      sizeof_abr_bds_and_req(abr, &rds, &rrs);
      if (rds > rrs) {
	/* Data should be okay since each bds element is atpMaxData */
	/* except for the last, however, last bit of data is missing */
	*aspqe->comp = BufTooSmall;
	*aspqe->ActRcvdReplyLen = rrs; /* really only got this */
      } else {
	*aspqe->comp = noErr;
	*aspqe->ActRcvdReplyLen = rds;	/* phew! everything okay */
      }
      break;
    case sktClosed:
      *aspqe->comp = SessClosed;
      break;
    case reqFailed:
      if (isdhand)
	fprintf(stderr, "asp: hsndreq: reqFailed %x\n",aspqe);
      *aspqe->comp = NoAck;
      break;
#ifdef notdef
      /* Request failed - could just die like here, but rather return */
      /* an error and let upper layer decide */
      *aspqe->comp = SessClosed;
      if (!as)			/* can't */
	break;
      as->state = SP_INACTIVE;
      shutdown_aspskt(as);
      aspskt_free(as);
      break;
#endif
    default:
      if (isdhand)
	fprintf(stderr,"asp: hsndreq: bad atp completion %d\n",abr->abResult);
      *aspqe->comp = aspFault;
    }
    break;
  case tSPGetStat:		/* almost like above... */
    /* probably should check that the atp user bytes are all zero, but.. */
    if (abr->abResult == noErr) {
      sizeof_abr_bds_and_req(abr, &rds, &rrs);
      if (rds > rrs) {
	*aspqe->comp = BufTooSmall;
	*aspqe->ActRcvdStatusLen = rrs; /* really only got this */
      } else  {
	*aspqe->comp = noErr;
	*aspqe->ActRcvdStatusLen = rds;	/* phew! everything okay */
      }
    } else if (abr->abResult == reqFailed || abr->abResult == sktClosed)
      *aspqe->comp = NoServers;	/* assume atpReqFailed */
    else {
      if (isdhand)
	fprintf(stderr, "asp: SPGetStat: bad atp completion %d\n",abr->abResult);
      *aspqe->comp = aspFault;
    }
    break;
  case tSPOpenSess:
    switch (abr->abResult) {
    case noErr:
      if (!as) {			/* no matching session???? */
	*aspqe->comp = NoServers;
	break;
      }
      aub = (ASPUserBytes *)&aspqe->bds[0].userData;
      *aspqe->comp = (sword)ntohs(aub->std.data);
      if (*aspqe->comp != noErr) 
	break;
      ap = &as->rabr.proto.atp;
      ap->atpSocket = as->ss;
      ap->atpReqCount = sizeof(as->reqdata);
      ap->atpDataPtr = (char *)&as->reqdata;
      as->state = SP_STARTED;
      as->SessID = aub->std.b2;	/* remember sessid!! */
      /* set address for tickle to go to */
      as->tickle_abr.proto.atp.atpAddress = as->addr; /* remote SLS not SSS! */
      as->tickle_abr.proto.atp.atpSocket = as->ss;
      startasptickle(as);
      /* this is purposely deferred until after the startasptickle */
      as->addr.skt = aub->std.b1; /* get remote sss */
      cbATPGetRequest(&as->rabr, handle_aspclient, aspqe->SessRefNum);
      start_ttimer(as);
      break;
    case reqFailed: 
    case sktClosed:
      *aspqe->comp = NoServers;
      break;
    default:
      if (isdhand)
	fprintf(stderr, "asp: SPOpenSess: bad atp completion %d\n",
		abr->abResult);
      *aspqe->comp = aspFault;
    }
    break;
  case tSPAttention:
    /* should probably check that atp user bytes are all zero, but.. */
    if (abr->proto.atp.atpUserData != 0)
      if (isdhand)
	fprintf(stderr, "asp: SPAttention: bad userdata in attention ack\n");
    switch (abr->abResult) {
    case noErr:
      *aspqe->comp = noErr;
      break;
    case reqFailed:
    case sktClosed:
      *aspqe->comp = NoAck;
      break;
    default:
      if (isdhand)
	fprintf(stderr, "asp: SPAttenion: bad atp completion %d\n",
		abr->abResult);
      *aspqe->comp = aspFault;
    }
    break;
  case tSPClose:
    switch (abr->abResult) {
    case sktClosed:
      *aspqe->comp = SessClosed;
      break;
    case reqFailed:
    case noErr:
      if (isdhand)
	fprintf(stderr, "asp: hsndreq: remote close %x\n",aspqe);
      /* ignore error, etc */
      if (!as) {
	*aspqe->comp = SessClosed;
	break;
      }
      /* Inactive before so we protocol running won't cause as many problems */
      if (as->state == SP_INACTIVE) /* someone has already inactived */
	break;
      as->state = SP_INACTIVE;	/* close should inactive session :-) */
      shutdown_aspskt(as);
      aspskt_free(as);
      *aspqe->comp = noErr;
      break;
    default:
      if (isdhand)
	fprintf(stderr, "asp: SPClose: bad atp completion %d\n",abr->abResult);
      *aspqe->comp = aspFault;
    }
  default:
    break;			/* just drop the thing */
  }
  delete_aspaqe(aspqe);		/* get rid of it */
}

/*
 * Handle response done event - right now - just SPCmdReply and SPWrtReply
 *
*/
private void
handle_asp_rspdone(abr, aspqe)
ABusRecord *abr;
ASPQE *aspqe;
{
  ASPSkt *as;

  if (isdhand) 
    fprintf(stderr, "asp: handle_rspdone with aspqe %x\n",aspqe);

  if (aspqe == NULL)
    return;			/* drop */
  if (abr == NULL || aspqe == NULL) {
    fprintf(stderr,"asp: fatal error: handle_rspdone - abr or aspqe NIL\n");
    exit(255);
    return;
  }

  if (isdhand)
    fprintf(stderr, "asp: rspdone: event code %s\n",aspevents[aspqe->type]);

  if ((as = aspskt_find_sessrefnum(aspqe->SessRefNum)) == NULL) {
    if (isdhand)
      fprintf(stderr, "asp:rspdone - session %s not found\n",
	      aspqe->SessRefNum);
    *aspqe->comp = SessClosed;
    delete_aspaqe(aspqe);
    return;
  }

  switch (abr->abResult) {
  case noErr:
    switch (aspqe->type) {
    case tSPWrtReply:
    case tSPCmdReply:
      *aspqe->comp = noErr;
      break;
    }
    break;
  case sktClosed:
    *aspqe->comp = SessClosed;
    break;
  case noRelErr:
    /* is this the right thing to do????  */
    if (isdhand)
      fprintf(stderr, "asp: hrspdone: no rel received %x\n",aspqe);
    *aspqe->comp = NoAck;	/* overload */
#ifdef notdef
    /* This is a viable option, but will not do it.... */
    *aspqe->comp = SessClosed;
    as->state = SP_INACTIVE;
    shutdown_aspskt(as);
    aspskt_free(as);
#endif
    break;
  default:
    if (isdhand)
      fprintf(stderr, "asp: hrspdone: Unexpected comp %d\n",abr->abResult);
    *aspqe->comp = aspFault;
    break;
  }
  delete_aspaqe(aspqe);
}

/*
 * Handle "Special" events - really handle cases where we wanted to
 * run ATP async and need to drop the aspqe
*/
private void
handle_asp_special(abr, aspqe)
ABusRecord *abr;
ASPQE *aspqe;
{
  ASPSkt *as;

  if (isdhand)
    fprintf(stderr, "asp:  hspecial: aspqe %x event code %s\n",
	    aspqe, aspevents[aspqe->type]);

  if (aspqe->type == tSPWrite2) {
    if ((as = aspskt_find_sessrefnum(aspqe->SessRefNum)) == NULL) {
      if (isdhand)
      fprintf(stderr,"asp: WriteContinue reply done, but session %s invalid\n",
	      aspqe->SessRefNum);
    } else delete_aspawe(aspqe, as);
  } else delete_aspaqe(aspqe);
}

/*
 * send a reply to a close session call
 *
*/
private void
do_sendclosesessreply(as, abr)
ASPSkt *as;
ABusRecord *abr;
{
  if (isdhand)
    fprintf(stderr, "asp: responding to remote closesession\n");
  if (as->state == SP_INACTIVE)
    return;			/* already done */
  /* Inactive before so we protocol running won't cause as many problems */
  abr->proto.atp.atpUserData = 0; /* clear these */
  as->state = SP_INACTIVE;
  do_sendreply(as, abr);
  shutdown_aspskt(as);
  aspskt_free(as);
}

/*
 * Send an empty reply message
 *
*/
private void
do_sendreply(as, abr)
ASPSkt *as;
ABusRecord *abr;
{
  ABusRecord rabr;
  BDS bds[1];
  atpProto *ap;
  int cnt;

  ap = &rabr.proto.atp;
  ap->atpSocket = as->ss;

  ap->atpAddress = abr->proto.atp.atpAddress;
  ap->atpTransID = abr->proto.atp.atpTransID;
  cnt = setup_bds(bds, atpMaxNum, atpMaxData, (char *)NULL, 0, (dword)0);
  ap->atpRspBDSPtr = bds;
  ap->fatpEOM = (abr->proto.atp.atpBitMap >> cnt) != 0 ? 1 : 0 ;
  ap->atpNumBufs = cnt;
  ap->atpBDSSize = cnt;
  ATPSndRsp(&rabr, FALSE);
}

/*
 * Close down activity on an ASP connection.
 *
*/
private void
shutdown_aspskt(as)
ASPSkt *as;
{
  ASPQE *aspqe;

  if (isdhand)
    fprintf(stderr, "asp: socketshutdown on srn %d/%d (sess %d) started\n",
	    as->SLSRefNum, as->SessRefNum, as->SessID);
  if (as->tickling)
    stopasptickle(as);	/* stop tickling remote */
  stop_ttimer(as);
  /* close down getrequests */
  while ((aspqe = get_aspaqe()) != NULL) {
    switch (aspqe->type) {
      /* atpsndResponse */
    case tSPWrite2:
      /* shouldn't be in this queue */
      break;
    case tSPCmdReply:
    case tSPWrtReply:
    case tSP_Special_DROP:
      /* just break - will be shutup by atpclose */
      break;
      /* atpgetrequest */
    case tSPGetRequest:
      /* just break - will be shutup by atpclose */      
      break;
      /* atpsndReqeust */
    case tSPWrtContinue:
    case tSPGetStat:
    case tSPAttention:
    case tSPOpenSess:
    case tSPCommand:
    case tSPWrite:
    case tSPClose:
      ATPReqCancel(&aspqe->abr, TRUE); /* run async, say why not? */
      break;
    }
  }
  while ((aspqe = get_aspawe(as)) != NULL)
    ;
  if (as->ss != -1)
    ATPCloseSocket(as->ss);	/* close the socket */
  as->ss = -1;			/* nothing here anymore */
}

/*
 * SPFork(SessRefNum)
 *
 * Do a fork, returning -1 on an error
 *
 * For Client:
 *   Close off everything in toplevel
 * For Server:
 *   Close off everything but handling for incoming tickles in
 *   toplevel - the SLS which remains in top-level is the one that
 *    sees them.
 *   In child, remember that we no longer wish to see the SLS
 *
 * Outgoing tickles are under the control of stickle, ctickle -- TRUE
 * for either means to tickle or not to tickle for the base process
 * (half-closed) connection and forked processes respectively
 *
*/
int
SPFork(SessRefNum, stickle, ctickle)
int SessRefNum;
int stickle;
int ctickle;
{
  int pid;
  ASPSkt *as, *as1;
  ASPSSkt *sas;

  if (isdcalls)
    fprintf(stderr, "asp: SPFork called\n");
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL)
    return(-1);
  if (as->state != SP_STARTED)
    return(-1);			/* nothing to do */

  if ((pid = fork()) < 0)	/* check for error */
    return(pid);
  if (pid) {
    if (isdhand)
      fprintf(stderr, "asp: child pid is %d\n",pid);
    /* Parent doesn't want to know about the SSS */
#ifdef ASPPID
    as->pid = pid;
#endif
    shutdown_aspskt(as);
    if (as->type == SP_SERVER) {
      /* an tickle user routines are still there though! */
      start_ttimer(as);		/* restart tickle timer */
      as->state = SP_HALFCLOSED;
      /* in parent: want to tickle them :-) */
      if (stickle) {
	if (isdtickle)
	  fprintf(stderr, "asp: parent is tickling\n");
	/* remote address should have been set already */
	as->tickle_abr.proto.atp.atpSocket = 0;
	startasptickle(as);
      }
    } else {
      aspskt_free(as);
    }
  } else {
    /* Child shouldn't see SLS if it exists */
    if (isdhand)
      fprintf(stderr, "asp: child is running\n");
    if (as->type != SP_SERVER)
      return(pid);
    if ((sas = aspsskt_find_slsrefnum(as->SLSRefNum)) == NULL)
      return(pid);

    /* kill off any unstarted ones - usually left over half-closed */
    /* if srn isn't started, then had no business forking anyway */
    while ((as1 = aspskt_find_notrunning()) != NULL) {
      if (as1->state == SP_HALFCLOSED)
	spshutdown(as1);
      else {
	shutdown_aspskt(as1);
	aspskt_free(as1);
      }
    }
    /* this is at the crux - we are not privy to incoming tickles on */
    /* session since they go to the sss!  */
    stop_ttimer(as);		/* close tickle timer since we are not */
				/* privy to incoming request */
    if (!ctickle) {
      if (isdtickle)
	fprintf(stderr, "asp: no child tickling\n");
      stopasptickle(as);
    }
    ATPCloseSocket(sas->addr.skt); /* close down server listener here */
    dsiTCPIPCloseSLS(); /* and the AppleShareIP SLS */
  }
  return(pid);
}

/*
 * Complete shutdown of half-closed session.  (E.g. parent side of
 * spfork session).  Should only be done when known child is dead.
 *
*/
OSErr
SPShutdown(srn)
int srn;
{
  ASPSkt *as = aspskt_find_sessrefnum(srn);
  if (isdcalls)
    fprintf(stderr, "asp: SPShutdown called with srn %d\n",srn);
  return(spshutdown(as));
}

private OSErr
spshutdown(as)
ASPSkt *as;
{
  if (isdcalls)
    fprintf(stderr, "asp: spshutdown called\n");

  if (as == NULL || as->state != SP_HALFCLOSED)
    return(ParamErr);
  /* gotta do stopasptickle because server sends tickles as well */
  /* as child.  This keeps client from timing out when child blocks */
  /* assumes that we trust that we will "know" when child processes die */
  if (as->tickling)
    stopasptickle(as);		/* stop tickling remote */
  stop_ttimer(as);
  as->state = SP_INACTIVE;
  aspskt_free(as);
  return(noErr);
}

#ifdef ASPPID
int
SPFindPid(pid)
int pid;
{
  ASPSkt *as = aspskt_find_pid(pid);
  if (as == NULL)
    return(-1);
  return(as->SessRefNum);
}
#endif

/*
 * Set callback for Tickle Timeout - our timer expired for tickles
 * from remote
 *
*/
OSErr
SPTickleUserRoutine(SessRefNum, routine, arg)
int SessRefNum;
int (*routine)();
int arg;
{
  ASPSkt *as = aspskt_find_sessrefnum(SessRefNum);

  if (isdcalls)
    fprintf(stderr, "asp: SPTickleUser called\n");
  if (as == NULL)
    return(ParamErr);
  as->tickle_timeout_user = routine;
  as->ttu_arg = arg;
  return(noErr);
}

/* mappings for sessrefnum/slsrefnum to aspskt/aspsskt */
private ASPSkt *asplist = NULL;
private ASPSSkt aspslist[NUMSASP];
int numasp = -1;		/* initially */

private int
aspskt_init(n)
int n;
{
  ASPSkt *as;

  if (n < NUMASP)
    n = NUMASP;			/* min to alloc */
  if (asplist != NULL && isdskt)
    fprintf(stderr, "asp: asplist wasn't empty - AWK\n");
  asplist = (ASPSkt *)malloc(sizeof(ASPSkt)*n);
  numasp = n;
  if (asplist == NULL) {
    fprintf(stderr, "asp: PANIC! out of memory in aspskt_init\n");
    exit(998);
  }
  as = asplist;
  while (n--) {
    as->active = FALSE;
    as++;
  }
  return(numasp);
}

private OSErr
aspskt_new(srn, retas)
int *srn;
ASPSkt **retas;
{
  int i;
  ASPSkt *as;

  if (asplist == NULL)		/* hasn't been inited yet? */
    aspskt_init(NUMASP);	/* then do it */
  for (i = 0, as = asplist; i < numasp; i++, as++)
    if (!as->active) {
      as->active = TRUE;
      as->tickle_timeout_user = NILPROC; /* make sure null */
#ifdef ASPPID
      as->pid = 0;	/* base process */
#endif
      as->tickling = FALSE;		/* we are not tickling yet! */
      as->SessRefNum = i;
      *retas = as;
      *srn = i;
      return(noErr);
    }
  return(NoMoreSessions);
}

private void
aspskt_free(as)
ASPSkt *as;
{
  if (as == NULL)
    return;
  as->active = FALSE;
  /* possibly do a callback */
}

private ASPSkt *
aspskt_find_sessid(SLSRefNum, sessid)
int SLSRefNum;
byte sessid;
{
  int i;
  ASPSkt *as;

  for (i = 0, as=asplist; i < numasp; i++, as++)
    if (as->active && SLSRefNum == as->SLSRefNum && as->SessID == sessid)
      return(as);
  return(NULL);
}

private ASPSkt *
aspskt_find_notrunning()
{
  int i;
  ASPSkt *as;

  for (i = 0, as=asplist; i < numasp; i++, as++)
    if (as->active && as->state != SP_STARTED)
      return(as);
  return(NULL);
}

#ifdef ASPPID
private ASPSkt *
aspskt_find_pid(pid)
int pid;
{
  ASPSkt *as;
  int i;
  for (i=0, as=asplist; i < numasp; i++, as++)
    if (as->active && as->state == SP_HALFCLOSED && as->pid == pid)
      return(as);
  return(NULL);
}
#endif

#ifdef DEBUGAUFS
dumpsockets(sls)
{
  int i;
  ASPSkt *as;
  for (i = 0, as = asplist ; i < numasp; i++, as++) {
#ifdef ASPPID
    logit(0, "aspskt %d %sactive, sls %d, state %d, type %d, ss %d, pid %d",
	i, as->active ? "" : "not ",
	as->SLSRefNum, as->state, as->type, as->ss, as->pid);
#else
    logit(0, "aspskt %d %sactive, sls %d, state %d, type %d, ss %d",
	i, as->active ? "" : "not ",
	as->SLSRefNum, as->state, as->type, as->ss);
#endif
  }
}
#endif

ASPSkt *
aspskt_find_active(SLSRefNum)
int SLSRefNum;
{
  int i;
  ASPSkt *as;

  for (i = 0, as=asplist; i < numasp; i++, as++)
    if (as->active && as->SLSRefNum == SLSRefNum && as->state == SP_STARTING)
      return(as);
  return(NULL);
}

ASPSkt *
aspskt_find_sessrefnum(srn)
int srn;
{
  ASPSkt *as;

  if (srn < 0 || srn >= numasp)
    return(NULL);
  as = &asplist[srn];
  if (as->active)
    return(as);
  return(NULL);
}

private OSErr
aspsskt_new(sls, sas)
int *sls;
ASPSSkt **sas;
{
  int cno;
  for (cno = 0; cno < NUMSASP; cno++)
    if (!aspslist[cno].active) {
      aspslist[cno].active = TRUE;
      *sls = cno;
      *sas = &aspslist[cno];
      return(noErr);
    }
  return(TooManyClients);
}


/*
 * locate SLSRefNum structure
 * (non-private for DSI access)
 *
 */

ASPSSkt *
aspsskt_find_slsrefnum(sls)
int sls;
{
  if (sls >= NUMSASP || sls < 0 || !aspslist[sls].active)
    return(NULL);
  return(&aspslist[sls]);
}

private boolean
aspsskt_isactive(sls)
{
  if (sls > NUMSASP || sls < 0 || !aspslist[sls].active)
    return(FALSE);
  return(TRUE);
}


/*
 * ASP Tickle managment functions
 *
*/

/* 
 * startpaptickle - start a tickle for the specified connection outgoing
 *  on the designated socket
 * remote address and local socket MUST be set before calling
 *
*/
private void
startasptickle(as)
ASPSkt *as;
{
  atpProto *ap;
  ASPUserBytes *aub;
  static BDS bds[1];
  int err;


  ap = &as->tickle_abr.proto.atp;
  ap->atpUserData = 0;
  aub = (ASPUserBytes *) &ap->atpUserData;
  aub->std.b1 = aspTickle;
  aub->std.b2 = as->SessID;
#ifdef notdef
  /* must be set before called */
  ap->atpAddress = as->addr;	/* remote wss or sls socket */
  ap->atpSocket = skt;
#endif
  if (isdtickle) {
    fprintf(stderr, "asp: starting tickle on connection %d/%d socket %d\n",
	    as->SLSRefNum, as->SessRefNum, ap->atpSocket);
  }

  ap->atpReqCount = 0;
  ap->atpDataPtr = 0;
  ap->atpRspBDSPtr = bds;
  bds[0].buffPtr = NULL;
  bds[0].buffSize = 0;
  ap->atpNumBufs = 1;
  ap->fatpXO = FALSE;
  ap->atpTimeOut = ASPTICKLETIMEOUT;
  ap->atpRetries = 255;	/* means infinity */
  err = ATPSndRequest(&as->tickle_abr, TRUE);
  if (err != noErr) {
    fprintf(stderr,"asp: Problems starting tickle\n");
    as->tickling = FALSE;
  } else
    as->tickling = TRUE;
}

/*
 * stopasptickle - cancel the tickle on the specified connection
 *
*/
void
stopasptickle(as)
ASPSkt *as;
{
  OSErr err;

  if (isdtickle) {
    fprintf(stderr, "asp: killing tickle on connection %d/%d\n",
	    as->SLSRefNum, as->SessRefNum);
  }
  err = ATPReqCancel(&as->tickle_abr, FALSE); /* run async? */
  if (err == cbNotFound && err != sktClosed) {
    if (isdtickle)
      fprintf(stderr, "asp: Tickle request completed - should never happen\n");
  }
  as->tickling = FALSE;
}

/*
 * Timeout handler for remote tickle
 */
private void
ttimeout(as)
ASPSkt *as;
{
  if (isdtickle) {
    fprintf(stderr, "asp: Tickle timeout\n");
    fprintf(stderr, "asp: Timeout on connection %d\n", as->SessID);
  }
  if (as->tickle_timeout_user != NULL)
    (*as->tickle_timeout_user)(as->SessRefNum, as->ttu_arg);
  else {
    as->state = SP_INACTIVE;
    shutdown_aspskt(as);
    aspskt_free(as);
  }
}

/*
 * Start the remote tickle timeout
 *
*/
private void
start_ttimer(as)
ASPSkt *as;
{
  Timeout(ttimeout, (u_long)as, ASPCONNECTIONTIMEOUT);
}

/* 
 * reset the remote tickle timeout
 *
*/
private void
reset_ttimer(as)
ASPSkt *as;
{
  remTimeout(ttimeout, (u_long)as);
  Timeout(ttimeout, (u_long)as, ASPCONNECTIONTIMEOUT);
}

/* 
 * cancel the remote tickle timeout
 *
*/
void
stop_ttimer(as)
ASPSkt *as;
{
  remTimeout(ttimeout, (u_long)as);
}





private ASPQE *aspqe_list;
private QElemPtr aspqe_free;

ASPQE *
create_aq(which, as)
int which;
ASPSkt *as;
{
  ASPQE *aspqe;
  QHead head;

  switch (which) {
  case ASPAQE:
    head = (QHead)&aspqe_list;
    break;
  case ASPAWE:
    head = &as->wqueue;
    break;
  }

  if ((aspqe = (ASPQE *)dq_head(&aspqe_free)) == NULL &&
      (aspqe =(ASPQE *)malloc(sizeof(ASPQE))) == NULL) {
	fprintf(stderr,"asp: panic: create_aq: out of memory\n");
	exit(8);
      }
  if (isdskt)
    fprintf(stderr,"asp: create_aq: create %x on %s\n",
	    aspqe,which == ASPAQE ? "main queue" : "local writeq");
  q_tail(head, &aspqe->link);
  return(aspqe);
}

void
delete_aq(aspqe, which, as)
ASPQE *aspqe;
int which;
ASPSkt *as;
{
  QHead head;

  switch (which) {
  case ASPAQE:
    head = (QHead)&aspqe_list;
    break;
  case ASPAWE:
    head = &as->wqueue;
    break;
  }
  if (isdskt)
    fprintf(stderr,"asp: delete_aq: delete %x on %s\n",
	    aspqe,which == ASPAQE ? "main queue" : "local writeq");
  dq_elem(head, &aspqe->link);
  q_tail(&aspqe_free, &aspqe->link);
}

private ASPQE *
get_aq(which, as)
int which;
ASPSkt *as;
{
  ASPQE *aspqe;
  QHead head;

  switch (which) {
  case ASPAQE:
    head = (QHead)&aspqe_list;
    break;
  case ASPAWE:
    head = &as->wqueue;
    break;
  }

  if ((aspqe = (ASPQE *)dq_head(head)) != NULL)
    q_tail(&aspqe_free, &aspqe->link);
  return(aspqe);
}

private boolean
match_aspwe(aspqe, seqno)
ASPQE *aspqe;
word seqno;
{
  return(aspqe->seqno == seqno);
}

private ASPQE *
find_aspawe(as, seqno)
ASPSkt *as;
word seqno;
{
  return((ASPQE *)q_mapf(as->wqueue, match_aspwe, seqno));
}


/* some aux stuff that should eventually be moved out */
/*
 * return size of dataSize and size of original requested data
 *
*/
private void
sizeof_bds_and_req(bds, numbds, rds, rrs)
BDS bds[];
int numbds;
int *rds;
int *rrs;
{
  int i, ds, rs;

  if (bds == NULL || numbds < 0) {
    *rds = 0;
    *rrs = 0;
    return;
  }
  if (numbds > atpMaxNum)
    numbds = atpMaxNum;		/* cheap */

  for (i = 0, ds = 0, rs=0; i < numbds; i++) {
    ds += bds[i].dataSize;
    rs += bds[i].buffSize;
  }
  *rds = ds;
  *rrs = rs;
}

/*
 * Same as sizeof_bds_and_req, but takes ABusrecord instead of
 * bds and bdssize
 *
*/
private void
sizeof_abr_bds_and_req(abr, rds, rrs)
ABusRecord *abr;
int *rds;
int *rrs;
{
  if (abr == NULL) {
    *rds = 0;
    *rrs = 0;
    return;
  }
  sizeof_bds_and_req(abr->proto.atp.atpRspBDSPtr, abr->proto.atp.atpNumRsp,
		     rds, rrs);
}

/*
 * Check for possible ATPsendrequest errors and convert to an approriate
 * asp error
*/
private OSErr
asp_cksndrq_err(s, err, comp)
char *s;
OSErr err;
int *comp;
{
  if (err == noErr)
    return(noErr);
  if (err == atpLenErr) {
    *comp = SizeErr;
    return(SizeErr);
  }
  if (err == tooManySkts) {
    *comp = noATPResource;
    return(noATPResource);
  }
  else if (err == badATPSkt) {
    fprintf(stderr, "asp: ASP Internal error: %s: badATPSkt\n",s);
    exit(1);
  }
  fprintf(stderr, "asp: ASP Internal error: %s: unexpected error %d\n",s,err);
  exit(1);
}

/* set asp debug flags */
aspdebug(s)
char *s;
{
  asp_dbug = 0;			/* default to zero */
  while (*s) {
    switch (*s) {
    case 's':
      asp_dbug |= AD_SKT;
      break;
    case 'h':
      asp_dbug |= AD_HANDLERS;
      break;
    case 'c':
      asp_dbug |= AD_CALLS;
      break;
    case 't':
      asp_dbug |= AD_TICKLE;
      break;
    }
    s++;
  }
}
