/*
 * $Author: djh $ $Date: 1996/03/07 09:13:56 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abnbp.c,v 2.7 1996/03/07 09:13:56 djh Rel djh $
 * $Revision: 2.7 $
*/

/*
 * abnbp.c - nbp access.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 13, 1986    Schilit    Created
 *  June 15, 1986    CCKim	move to abnbp.c, add extract
 *  July  1, 1986    Schilit	rewrite with async and NBPConfirm
 *  July  9, 1986    CCKim	Clean up and rewrite create_entity
 *  July 15, 1986    CCKim	Add nbpregister, nbpdelete
 *  April 28,1991    djh	Added Phase 2 support
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <netat/appletalk.h>
#include <netat/abnbp.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else
# include <strings.h>
#endif

private nbpProto *nbpQ;		/* NBP queue of nbpProto records */
private byte next_nbpid = 0;	/* NBP transaction ID */
private int nbpSkt = -1;	/* NBP socket number */

/* Public functions */

OSErr nbpInit();		/* initialize NBP */
OSErr NBPLookup();		/* lookup a name or group of names */
OSErr NBPConfirm();		/* confirm a name/address pair */
OSErr NBPExtract();		/* extract entity information after lookup */

int nbpMatch();			/* match obj or type using wildcards */

/* Internal functions */

private OSErr nbpFcn();		/* common NBP function */
private void SndNBP();		/* send request to appletalk */
private void nbp_timeout();	/* timeout monitor */
private void nbp_listener();	/* DDP listener process */
private void LkUpReply();	/* handle LkUpReply response */
private int nbp_match();	/* find matching request upon response */
private int nbpcpy();		/* copy entity into user buffer */
private int c2pkt_ename();	/* convert entity name from c to packed */
private int pkt2c_ename();	/* convert entity name from packed to c */

/*
 * OSErr nbpInit()
 *
 * nbpInit initializes NBP; an DDP socket with an NBP listener is
 * opened and varaibles are initialized.
 *
 * Result Codes:
 *	as returned by DDPOpenSocket()
 *
 */

OSErr
nbpInit()
{
  int err;

  nbpSkt = 0;
  nbpQ = (nbpProto *) 0;
  next_nbpid = 0;			/* NBP transaction ID */
  err = DDPOpenSocket(&nbpSkt,nbp_listener);
  if (err != noErr) {
    nbpSkt = 0;				/* reset */
    fprintf(stderr,"NBPInit: DDPOpenSocket error %d\n",err);
  }
}
  
/*
 * Close currently open NBP socket
 *
*/
OSErr
nbpShutdown()
{
  int err;
  err = DDPCloseSocket(nbpSkt);
  nbpSkt = -1;
  return(err);
}

/*
 * NBPLookup(nbpProto *pr,int async)
 *
 * NBPLookup returns the address of all entities with a specified
 * name.
 *
 * nbpEntityPtr - points to a variable of type EntityName containing
 * the name of the entity whose address should be returned.
 *
 * nbpBufPtr, nbpBufSize - contain the location and size of an area
 * in memory in which the entities' address should be returned.
 *
 * nbpDataField - indicates the maximum number of matching names to 
 * find address for; the actual number of addresses is returned in 
 * nbpDataField.
 *
 * nbpRetransmitInfo - contains the retry interval and retry count.
 *
 * Result Codes:
 *	noErr		No error
 *	nbpBuffOvr	Buffer overflow
 *
*/
 
OSErr
NBPLookup(abr,async)
nbpProto *abr;
int async;
{
  int maxx;
  import word this_net;
  OSErr GetBridgeAddress();

  GetBridgeAddress(&abr->nbpAddress);
  if (abr->nbpAddress.node == 0x00) { /* no router */
    abr->nbpAddress.node = 0xff;
#ifdef PHASE2
    abr->nbpAddress.net = 0x0000; /* a local broadcast */
#else  PHASE2
    abr->nbpAddress.net = this_net;
#endif PHASE2
  }

  maxx = abr->nbpBufSize/sizeof(NBPTEntry); /* max entries */
  abr->nbpMaxEnt =			/* set it up */
    (maxx < (int)abr->nbpDataField) ? maxx : abr->nbpDataField;
  abr->nbpDataField = 0;		/* initially no entries */

  return(nbpFcn(abr,tNBPLookUp,async));
}

/*
 * OSErr NBPConfirm(nbpProto *pr, int async)
 *
 * NBPConfirm confirms that an entity known by name and address still
 * exists.
 *
 * nbpEntityPtr - points to a variable of type EntityName that contains
 * the name to confirm.  No meta characters are allowed in the entity
 * name (otherwise the result would be ambigous).
 *
 * nbpAddress - specifies the address to be confirmed.
 * 
 * nbpRetransmitInfo - contains the retry interval and retry count.
 * 
 * The correct socket number is returned in nbpDataField.
 *
 * NBPConfirm is more efficient than NBPLookup in terms of network
 * traffic.  Since NBPConfirm only waits for a single response it
 * is also quicker (i.e. doesn't need to wait for the timeout period).
 *
 * Result Codes:
 *	noErr		No error
 *	nbpConfDiff	Name confirmed for different socket
 *	nbpNoConfirm	Name not confirmed
 *
*/
 
OSErr
NBPConfirm(abr,async)
nbpProto *abr;
int async;
{
  return(nbpFcn(abr,tNBPConfirm,async)); /* common function does the work */
}

/* 
 * OSErr NBPExtract(NBPTEntry t[],int nument,int whichone, 
 *		EntityName *en,	AddrBlock *addr)
 *
 * NBPExtract returns one address from the list of addresses returned
 * by NBPLookup.
 *
 * t - is a table of entries in the form NBPTEntry as returned by
 * NBPLookUp.
 *
 * nument - is the number of tuples in "t" as returned in nbpDataField 
 * by NBPLookUp
 *
 * whichone - specifies which one of the tuples in the buffer should
 * be returned.
 *
 * en, addr - are pointers to an EntityName and AddrBlock into which
 * NBPExtract stores the selected entity information.
 *
 * Result Codes:
 *	noErr		No error
 *	extractErr	Can't find tuple in buffer
 *
 */ 

OSErr
NBPExtract(t,nument,whichone,ent,addr)
NBPTEntry t[];
EntityName *ent;
AddrBlock *addr;
{
  if (whichone > nument) {
    fprintf(stderr,"NBPExtract: whichone too large!");
    return(extractErr);		/* return error code, not found */
  } else {
    *ent = t[whichone-1].ent;	/* pretty simple */
    *addr = t[whichone-1].addr;	/*  stuff... */
  }
  return(noErr);
}

/*
 * register a nve
 *
*/
NBPRegister(abr, async)
nbpProto *abr;
boolean async;
{
  import word this_net, nis_net;
  import byte this_node, nis_node;

  int err;

  if ((err = NBPLookup(abr, FALSE)) < 0) /* i guess this is the right */
    return(err);		/* thing to do */

  if (abr->nbpDataField != 0) 
    if (abr->nbpDataField != 1 ||
	abr->nbpBufPtr[0].addr.net != this_net ||
	abr->nbpBufPtr[0].addr.node != this_node ||
	abr->nbpBufPtr[0].addr.skt != abr->nbpAddress.skt)
      return(nbpDuplicate);

  abr->nbpAddress.net = nis_net;
  abr->nbpAddress.node = nis_node;
  /* socket is given */
 
  return(nbpFcn(abr,tNBPRegister,async)); /* common function does the work */
}

/*
 * remove a nve
 *
*/
NBPRemove(abEntity)
EntityName *abEntity;
{
  nbpProto abr;
  import word nis_net;
  import byte nis_node;
  int err;

  abr.nbpEntityPtr = abEntity;
  abr.nbpAddress.net = nis_net;
  abr.nbpAddress.node = nis_node;
  abr.nbpRetransmitInfo.retransInterval = 10;
  abr.nbpRetransmitInfo.retransCount = 2;

  err = nbpFcn(&abr,tNBPDelete,FALSE); /* common function does the work */
  if (err != noErr)
    return(err);
  return(abr.abResult);
}

/*
 * private OSErr nbpFcn(nbpProto *abr, int fcn, async)
 *
 * nbpFcn is a common function for NBP calls.  The function code is
 * stored into nbpProto, a unique transaction ID is set, and other
 * variables are initialized in the protocol record.  The record is
 * placed on a Q of NBP requests, the request is sent out to the
 * appletalk net, and a retransmission timer is started.
 *
 * If the async parameter is TRUE nbpFcn returns otherwise it waits
 * for abResult to go non-positive.
 *
*/

private OSErr
nbpFcn(abr,fcn,async)
nbpProto *abr;
int fcn,async;
{
  int rtim;

  if (nbpSkt <= 0) {
    fprintf(stderr,"NBP nbpFcn: nbpInit not called");
    exit(1);
  }

  abr->abOpcode = fcn;			/* set function code */
  abr->abResult = 1;			/* result not completed */
  next_nbpid += 1;			/* increment request ID */
  abr->nbpID = next_nbpid;		/* store request id */
  /* copy so we can modify */
  abr->retranscount = abr->nbpRetransmitInfo.retransCount;
  q_head(&nbpQ,abr);			/* add entry to Q */

  SndNBP(abr);				/* send out NBP the request */

  rtim = abr->nbpRetransmitInfo.retransInterval;
  Timeout(nbp_timeout,(caddr_t) abr,rtim);
  if (async)
    return(noErr);
  while (abr->abResult > 0)
    abSleep(rtim,TRUE);
  return(abr->abResult);
}

/*
 * private void SndNBP(nbpProto *abr)
 *
 * Send out NBP request over appletalk.
 *
 */

private void
SndNBP(nbpr)
nbpProto *nbpr;
{
  ABusRecord ddp;
  ddpProto *ddpr;
  NBP nbp;
  int nsize;
  import byte this_node;
  import word this_net;

  if (dbug.db_nbp)
    printf("NBP SndNBP: sending\n");

  ddpr = &ddp.proto.ddp;	/* handle on DDP protocol args */
  ddpr->ddpType = ddpNBP;
  ddpr->ddpDataPtr = (byte *) &nbp;

  ddpr->ddpSocket = (nbpr->abOpcode == tNBPRegister) ?
    nbpr->nbpAddress.skt : nbpSkt;
  ddpr->ddpAddress = nbpr->nbpAddress;
  ddpr->ddpAddress.skt = nbpNIS; /* always talk to NIS servers */

  nbp.tcnt = 1;			/* 1 tuple */
  nbp.id = nbpr->nbpID;
  nbp.tuple[0].enume = 0;
  nbp.tuple[0].addr.net = this_net;
  nbp.tuple[0].addr.node = this_node;
  nbp.tuple[0].addr.skt = nbpSkt; /* assume this as our listening socket */
  switch (nbpr->abOpcode) {
  case tNBPConfirm:		/* this is directed to the node... */
    nbp.control = nbpLkUp;
    break;
  case tNBPLookUp:
    if (nbpr->nbpAddress.node == 0xff)
      nbp.control = nbpLkUp;	/* no bridge, just do lookup */
    else
      nbp.control = nbpBrRq;	/* function is lookup with bridge */
    break;
  case tNBPDelete:
    nbp.tcnt = 0;		/* make sure zero */
    nbp.control = nbpDelete;
    nbp.tuple[0].addr.skt = nbpr->nbpAddress.skt;
    break;
  case tNBPTickle:		/* say eh for now */
    break;
  case tNBPRegister:
    nbp.tcnt = 0;		/* make sure zero */
    nbp.control = nbpRegister;
    break;
  }
  nsize = c2pkt_ename(nbpr->nbpEntityPtr,nbp.tuple[0].name);
#ifdef PHASE2
  { char *q;
    u_char *GetMyZone();
    /* add the zone name for outgoing NBP lookups  */
    if (nbpr->abOpcode == tNBPLookUp && nsize >= 2) {
      q = (char *) &nbp.tuple[0].name[nsize-2];
      if (*q == 0x01 && *(q+1) == '*') {	/* zone "*"  */
        strcpy(q+1, (char *)GetMyZone());
        *q = strlen(q+1);
        nsize += (*q - 1);
      } else {
        if (*(q+1) == '\0') {			/* null zone */
          strcpy(q+2, (char *)GetMyZone());
          *(q+1) = strlen(q+2);
          nsize += *(q+1);
        }
      }
    }
  }
#endif PHASE2
  ddpr->ddpReqCount = nbpBaseSize+nsize;
  DDPWrite(&ddp);		/* write it out... */
}

/*
 * private void nbp_timeout(nbpProto *nbpr)
 *
 * Timeout processor called called by the Timeout() mechanism.
 *
 * nbp_timeout decrements the retransmission counter and if greater
 * than zero, retransmits the nbpProto request and requeues the next
 * timeout.  If the retransmission counter has expired then nbp_timeout
 * dequeues the nbpProto request and sets the result code.
 *
*/ 

private void
nbp_timeout(nbpr)
nbpProto *nbpr;
{
  if (dbug.db_nbp)
    printf("NBP nbp_timeout: %d tick timeout on %d, %d remain\n",
	   nbpr->nbpRetransmitInfo.retransInterval, nbpr, nbpr->retranscount);

  if (nbpr->retranscount-- > 0) {
    SndNBP(nbpr);		/* resend */
    Timeout(nbp_timeout,(caddr_t)nbpr,nbpr->nbpRetransmitInfo.retransInterval);
  } else {
    dq_elem(&nbpQ,nbpr);	/* timeout - dq this record */
    switch (nbpr->abOpcode) {
    case tNBPRegister:
    case tNBPConfirm:
      nbpr->abResult = nbpNoConfirm; /* timeout means no confirm */
      break;
    case tNBPDelete:
    case tNBPTickle:
    case tNBPLookUp:
      nbpr->abResult = noErr;	/* timeout is OK */
      break;
    }
  }
}

/*
 * private void nbp_listener()
 *
 * Listener for NBP protocol packets called from DDP level.
 *
*/

private void
nbp_listener(skt,type,nbp,len,addr)
byte skt;
byte type;
NBP *nbp;
AddrBlock *addr;
{
  /* packet must be large enough and ddp type NBP */
  if (len < nbpMinSize || type != ddpNBP) 
    return;

  switch (nbp->control) {
    case nbpLkUpReply:		/* lookup reply? */
      LkUpReply(nbp,len);	/* yes, handle it */
      break;
    case nbpStatusReply:
      StatusReply(nbp, len);
      break;
    default:
      /* anything else should be requests */
      break;
    }
}

/*
 * private void nbpspecialdone
 *
 *
*/
StatusReply(nbp, len)
NBP *nbp;
int len;
{
  nbpProto *pr;
  OSErr errstatus = -1;		/* default to generic one */

/* find the queued request which matches this NBP reply */

  pr = (nbpProto *) q_mapf(nbpQ,nbp_match,nbp->id); /* find matching id */
  if (pr == (nbpProto *) 0)		/* anything found? */
    return;				/* nope... */

  if (dbug.db_nbp)
    fprintf(stderr,"NBP status done: found %d\n",pr);

  switch (pr->abOpcode) {
  case tNBPRegister:
    switch (nbp->tcnt) {
    case nbpSR_noErr:
      errstatus = noErr;
      break;
    case nbpSR_access:
      errstatus = nbpDuplicate;
      break;
    case nbpSR_overflow:
      errstatus = nbpBuffOvr;
      break;
    }
    break;
  case tNBPDelete:
    switch (nbp->tcnt) {
    case nbpSR_noErr:
      errstatus = noErr;
      break;
    case nbpSR_access:
      errstatus = nbpNoConfirm; /* well, can't do any better??? */
      break;
    case nbpSR_nsn:
      errstatus = nbpNotFound;
      break;
    }
    break;
  }
  pr->abResult = errstatus;
  remTimeout(nbp_timeout,pr);	/* remove timeout as well */
  dq_elem(&nbpQ,pr);		/* and unit no longer needed */
}

/*
 * private void LkUpReply(NBP *nbp, int l)
 *
 * Called by nbp_listener when a LkUpReply packet comes down the pike.
 *
 * Take the following action:
 *
 *  1) Locate queued nbpProto request matching incoming reply's ID.
 *
 *  2) If this is a response to NBPConfirm, store the responding socket
 *     number in nbpDataField, set abResult field, remove pending 
 *     timeout, and dq the nbpProto request.  All done.
 *
 *  3) If this is a response to NBPLookUp, copy entity into user
 *     buffer.  If that fails (i.e. overflow or count of responses 
 *     acquired) then set abResult, remove pending timeout, and dq
 *     the nbpProto request.
 *         
 */

private void
LkUpReply(nbp,len)
NBP *nbp;
int len;
{
  nbpProto *pr;
  int skt,rslt;

/* find the queued request which matches this NBP reply */

  pr = (nbpProto *) q_mapf(nbpQ,nbp_match,nbp->id); /* find matching id */
  if (pr == (nbpProto *) 0)		/* anything found? */
    return;				/* nope... */

  if (dbug.db_nbp)
    printf("NBP LkUpReply: found %d\n",pr);

  switch(pr->abOpcode) {		/* handle according to req type */

    case tNBPConfirm:			/* response to NBPConfirm */
      skt = pr->nbpDataField =		/* set datafield to be */
	nbp->tuple[0].addr.skt;		/*  confirmed socket */
      pr->abResult = (pr->nbpAddress.skt == skt) ?
	noErr : nbpConfDiff;		/* set completion code */
      remTimeout(nbp_timeout,pr);	/* remove timeout as well */
      dq_elem(&nbpQ,pr);		/* and unit no longer needed */
      break;				/* all done with record */

     case tNBPLookUp:			/* response to NBPLookup */
       rslt = nbpcpy(pr,nbp);		/* copy and get result */
       if (rslt <= 0) {			/* indicates finished condition */
	 pr->abResult = rslt;		/* overflow or no error.. */
	 remTimeout(nbp_timeout,pr);	/* remove timeouts */
	 dq_elem(&nbpQ,pr);		/* remove request */
       }
      break;
    }
}

/*
 * private int nbp_match(nbpProto *pr, int id)
 *
 * nbp_match is the filter routine called via q_mapf (which applies
 * the function across all records in a queue).  nbp_match locates
 * the nbpProto record matching the argument ID.
 *
*/ 

private int
nbp_match(pr,id)
nbpProto *pr;
byte id;
{
  return(pr->nbpID == id);
}


/*
 * private int nbpcpy(nbpProto *pr, NBP *nbp)
 *
 * nbpcpy copies entities into the user buffer (nbpDataPtr) from
 * a network NBP packet.
 *
 * nbpcpy first checks to see if the entity exists, in which case it
 * returns without copying (prevents duplicates).
 *
 * Returns:
 *   1			success
 *   nbpBuffOvr		No room for entity.
 *   noErr		Have acquired requested number of entities.
 *
 * **BUG** Check for entity already in table should probably use
 * more than just the address (name and enum?).  In addition the
 * entity should always be updated so that a very long term 
 * NBPLookup can get network changes.
 *
 */

private int
nbpcpy(pr,nbp)
nbpProto *pr;
NBP *nbp;
{
  NBPTuple *ep;
  NBPTEntry *en;
  int i,len;

/* Add NBP tuples to user's data structure */

  en = (NBPTEntry *)pr->nbpBufPtr;

  for (ep = &nbp->tuple[0]; nbp->tcnt != 0; nbp->tcnt--) {
  /* check if this tuple entry is already in the table... */
    for (i = 0; i < (int)pr->nbpDataField; i++)
      if (bcmp(&en[i].addr, &ep->addr, sizeof(AddrBlock)) == 0)
	break; /* found identical */
    if (i != pr->nbpDataField)
      continue; /* try next entry */
    if (i > pr->nbpMaxEnt)		/* more than wanted? */
      return(nbpBuffOvr);		/* yes, return now... */
    bcopy(&ep->addr, &en[i].addr, sizeof(AddrBlock));
    len = pkt2c_ename(ep->name,&en[i].ent);
    pr->nbpDataField++;			/* one more entry */
    ep = (NBPTuple *)((char *)ep+(len+sizeof(AddrBlock)+1));
  }

  if (pr->nbpDataField == pr->nbpMaxEnt)
    return(noErr);

  return(1);
}


/*
 * private int c2pkt_ename(EntityName *cn, u_char *pn)
 * 
 * Copy entity name from c form into contiguous Apple Pascal
 * form (packet form).
 *
 * return: length of pascal form entity name
 *
 */
private int
c2pkt_ename(cn,pn)
byte *pn;
EntityName *cn;
{
  int i, cnt;
  byte *s;
  byte *pc;

  cnt = 0;
  for (s = cn->objStr.s, pc = pn++, i = 0; i < ENTITYSIZE; i++, pn++, s++) {
    *pn = *s;
    if (*s == '\0')
      break;
  }
  if (i > ENTITYSIZE)		/* increment to cnt and check aginst cutoff */
    i = ENTITYSIZE;		/* too large: turncated to 32 chars */
  *pc = i;
  cnt += (i+1);
  for (s = cn->typeStr.s, pc = pn++, i = 0; i < ENTITYSIZE; i++, pn++, s++) {
    *pn = *s;
    if (*s == '\0')
      break;
  }
  if (i > ENTITYSIZE)		/* increment to cnt and check aginst cutoff */
    i = ENTITYSIZE;		/* too large: turncated to 32 chars */
  *pc = i;
  cnt += (i+1);
  for (s = cn->zoneStr.s, pc = pn++, i = 0; i < ENTITYSIZE; i++, pn++, s++) {
    *pn = *s;
    if (*s == '\0')
      break;
  }
  if (i > ENTITYSIZE)		/* increment to cnt and check aginst cutoff */
    i = ENTITYSIZE;		/* too large: turncated to 32 chars */
  *pc = i;
  cnt += (i+1);
  return(cnt);		/* return number of bytes used */
}

/* 
 * private int pkt2c_enames(u_char *pn, EntityName *cn);
 *
 * Copy entity names from packet form (abutting Apple Pascal
 * strings) to c form into structure of type EntityName.
 *
 * return: the length of the packed string.
 *
 */

private int
pkt2c_ename(pn,cn)
byte *pn;
EntityName *cn;
{
  int ol,tl,zl;

  ol = *pn;			/* length of object */
  tl = *(pn+ol+1);		/* length of type */
  zl = *(pn+ol+tl+2);		/* length of zone */
  if (ol > ENTITYSIZE || tl > ENTITYSIZE || zl > ENTITYSIZE) {
    fprintf(stderr,"pkt2c_entity_names: invalid lengths! %d:%d@%d\n",ol,tl,zl);
    return(0);
  }
  cpyp2cstr(cn->objStr.s,pn);	/* copy them... */
  cpyp2cstr(cn->typeStr.s,pn+ol+1);
  cpyp2cstr(cn->zoneStr.s,pn+ol+tl+2);
  return(ol+tl+zl+3);		/* return length */
}

/*
 * Convert name in the form 'LaserWriter:LaserWriter@*' (object, type,
 * zone) to entity form (LaserWriter, LaserWriter, *).
 *
 * Assumes no ':' in object name , and no '@' in object or type name
 *
*/
void
create_entity(name, en)
char *name;
EntityName *en;
{
  char *zs, *ts;
  int ol, tl, zl;

  ts = index(name, ':');
  zs = index(name, '@');
  ol = ts ? (ts - name) : (zs ? (zs - name) : strlen(name));
  tl = ts == NULL ? 0 : ((zs == NULL) ? strlen(ts+1) : (zs - ts - 1));
  zl = zs == NULL ? 0 : strlen(zs+1);
  /* make foo@bar be =:foo@bar */
  /* make foo be =:=@foo */
  /* make foo@ be =:foo@* */
  if (ol != 0 && tl == 0 && ts == NULL) {
    if (zl != 0 || zs)
      tl = ol, ts = name - 1;
    else
      zs = name - 1, zl = ol;
    ol = 0;
  }

  bzero(en->objStr.s, sizeof(en->objStr.s));
  bzero(en->typeStr.s, sizeof(en->typeStr.s));
  bzero(en->zoneStr.s, sizeof(en->zoneStr.s));
  strncpy((char *)en->objStr.s, name, min(ENTITYSIZE, ol)); /* just copy */
  if (ts)
    strncpy((char *)en->typeStr.s, ts+1, min(ENTITYSIZE, tl));
  if (zs)
    strncpy((char *)en->zoneStr.s, zs+1, min(ENTITYSIZE, zl));
}

#ifdef AUTHENTICATE
isNBPInited()
{
  return(nbpSkt > 0);
}
#endif AUTHENTICATE

/*
 * NBP object/type match (taking wild-cards in "pat" into account)
 */

int
nbpMatch(pat, thing)
register byte *pat, *thing;
{
	register byte *p;
	register short pl, tl, hl;

	if ((pat[0] == nbpEquals || pat[0] == nbpWild) && pat[1] == '\0')
		return (1);

	if (p = (byte *)index((char *)pat, nbpWild)) {
		hl = p - pat;
		if (hl && strncmpci((char *)pat, (char *)thing, hl) != 0)
			return (0);
		pl = strlen((char *)pat) - 1;
		tl = strlen((char *)thing);
		if (tl < pl)
			return (0);
		if (hl < pl &&
		    strcmpci((char *)p+1, (char *)thing+(tl-(pl-hl))) != 0)
			return (0);
		return (1);
	}
	if (strcmpci((char *)pat, (char *)thing) != 0)
		return (0);
	return (1);
}
