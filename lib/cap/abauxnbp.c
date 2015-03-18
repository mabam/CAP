/*
 * $Author: djh $ $Date: 91/02/15 22:46:46 $
 * $Header: abauxnbp.c,v 2.1 91/02/15 22:46:46 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * abauxnbp.c - nbp access for native appletalk under A/UX
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
 *  June 15, 1986    CCKim      move to abnbp.c, add extract
 *  July  1, 1986    Schilit    rewrite with async and NBPConfirm
 *  July  9, 1986    CCKim      Clean up and rewrite create_entity
 *  July 15, 1986    CCKim      Add nbpregister, nbpdelete
 *
 *  1990   William Roberts	Add support for A/UX native appletalk
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

/* Include files for A/UX NBP support
 *
 * at_nvestr_t corresponds to str32, but uses Pascal Strings
 * Subject to that proviso, and the changes of field names, the
 * sizes and layouts of the following are the same:
 *
 *      at_entity_t   matches EntityName
 *      at_inet_t     matches AddrBlock
 *      at_nbptuple_t matches NBPTEntry
 */
#include <at/appletalk.h>
#include <at/nbp.h>
#include <sys/errno.h>
extern int errno;

extern char *appletalk_ntoa();

private nbpProto *nbpQ;
private byte next_nbpid = 0;
private int nbpSkt = -1;

/* Public functions */

OSErr nbpInit();                /* initialize NBP */
OSErr NBPLookup();              /* lookup a name or group of names */
OSErr NBPConfirm();             /* confirm a name/address pair */
OSErr NBPExtract();             /* extract entity information after lookup */

void zoneset();			/* absorbed from atalkdbm.c */

/* Internal functions */

private OSErr nbpFcn();         /* common NBP function */
private void SndNBP();          /* send request to appletalk */
private void nbp_timeout();     /* timeout monitor */
private void nbp_listener();    /* DDP listener process */
private void LkUpReply();       /* handle LkUpReply response */
private int nbp_match();        /* find matching request upon response */
private int nbpcpy();           /* copy entity into user buffer */
private int c2pkt_ename();      /* convert entity name from c to packed */
private int pkt2c_ename();      /* convert entity name from packed to c */
private int c2pas_ename();      /* convert entity name from c to pascal */
private int pas2c_ename();      /* convert entity name from c to pascal */

/*
 * OSErr nbpInit()
 *
 * Redundant under A/UX AppleTalk support.
 */

OSErr
nbpInit()
{
    return noErr;
}

void
zoneset(zonename)
char *zonename;
{
    /* ignored */
}

/*
 * Close currently open NBP socket
 *
*/
OSErr
nbpShutdown()
{
  return(noErr);
}

/*
 * NBPLookup(nbpProto *pr,int async)
 *
 * NBPLookup returns the address of all entities with a specified
 * name. The nbpProto structure contains
 *
 * nbpBufSize, nbpBufPtr - data area for result
 * nbpDataField   - number of entries to look for/entries found.
 * nbpEntityName  - name of the entity we are interested in
 *
 * Result code noErr or -1 (for some major failure).
*/

OSErr
NBPLookup(abr,async)
nbpProto *abr;
int async;
{
  int maxx;
  at_entity_t lookup_ent;
  at_nbptuple_t *ep;
  at_retry_t  retry_info;
  EntityName *entp;
  char buf[30];         /* for printing addresses */

  c2pas_ename(abr->nbpEntityPtr, &lookup_ent);

  maxx = abr->nbpBufSize/sizeof(NBPTEntry); /* max entries */
  abr->nbpMaxEnt =                      /* set it up */
    (maxx < abr->nbpDataField) ? maxx : abr->nbpDataField;
  abr->nbpDataField = 0;

  retry_info.interval = abr->nbpRetransmitInfo.retransInterval;
  retry_info.retries  = abr->nbpRetransmitInfo.retransCount;
  retry_info.backoff  = 1;      /* ??? */

  if (async) {
     fprintf(stderr, "NBP: async NBPLookup not implemented\n");
     return -1;
  }

  if (dbug.db_nbp) {
     printf("NBP: calling NBP lookup %s:%s@%s\n",
	abr->nbpEntityPtr->objStr.s,
	abr->nbpEntityPtr->typeStr.s,
	abr->nbpEntityPtr->zoneStr.s);
  }
  maxx = nbp_lookup(&lookup_ent,
		abr->nbpBufPtr, abr->nbpMaxEnt, &retry_info);
  if (maxx < 0) {
     return -1;
  }
  if (dbug.db_nbp) {
     printf("nbp: NBP lookup returned %d entries\n", maxx);
  }

  abr->nbpDataField = maxx;
  ep = (at_nbptuple_t *)(abr->nbpBufPtr);
  while (maxx--) {
     entp = (EntityName *)(&(ep->enu_entity));
     pas2c_ename(&(ep->enu_entity), entp);
     if (dbug.db_nbp) {
	printf("NBP:   tuple %02d = %s:%s@%s at %s\n",
		abr->nbpDataField - maxx,
		entp->objStr.s, entp->typeStr.s, entp->zoneStr.s,
		appletalk_ntoa(buf, (AddrBlock *)(&ep->enu_addr)));
     }
     ep++;
  }

  return(noErr);
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
 *      noErr           No error
 *      nbpConfDiff     Name confirmed for different socket
 *      nbpNoConfirm    Name not confirmed
 *
*/

OSErr
NBPConfirm(abr,async)
nbpProto *abr;
int async;
{
  int answer;
  at_entity_t lookup_ent;
  at_retry_t  retry_info;
  at_inet_t   address;
  char buf[30];

  c2pas_ename(abr->nbpEntityPtr, &lookup_ent);

  retry_info.interval = abr->nbpRetransmitInfo.retransInterval;
  retry_info.retries  = abr->nbpRetransmitInfo.retransCount;
  retry_info.backoff  = 1;      /* ??? */

  address.net    = abr->nbpAddress.net;
  address.node   = abr->nbpAddress.node;
  address.socket = abr->nbpAddress.skt;

  if (async) {
     fprintf(stderr, "NBP: async NBPConfirm not implemented\n");
     return -1;
  }
  if (dbug.db_nbp) {
     printf("NBP: calling NBP Confirm %s:%s@%s as %s\n",
	abr->nbpEntityPtr->objStr.s,
	abr->nbpEntityPtr->typeStr.s,
	abr->nbpEntityPtr->zoneStr.s,
	appletalk_ntoa(buf, &abr->nbpAddress));
  }

  answer = nbp_confirm(&lookup_ent, &address, &retry_info);
  switch (answer) {

     case 1:    /* successful */
	abr->nbpDataField = address.socket;
	if (dbug.db_nbp) {
	   printf("NBP: Confirmed OK on socket %d\n", address.socket);
	}
	return (address.socket == abr->nbpAddress.skt)?
		    noErr : nbpConfDiff;


     case 0:    /* not confirmed */
	if (dbug.db_nbp) {
	   printf("NBP: Not confirmed\n");
	}
	return(nbpNoConfirm);
  }
  if (dbug.db_nbp) {
     printf("NBP: confirm error (errno=%d)\n", errno);
  }
  return(-1);
}

/*
 * OSErr NBPExtract(NBPTEntry t[],int nument,int whichone,
 *              EntityName *en, AddrBlock *addr)
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
 *      noErr           No error
 *      extractErr      Can't find tuple in buffer
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
    return(extractErr);         /* return error code, not found */
  } else {
    *ent = t[whichone-1].ent;   /* pretty simple */
    *addr = t[whichone-1].addr; /*  stuff... */
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
  int err, fd;
  at_entity_t new_ent;
  at_retry_t  retry_info;
  char buf[30];

#ifdef no_longer_needed
  if ((err = NBPLookup(abr, FALSE)) < 0) /* i guess this is the right */
    return(err);                /* thing to do */
#endif

  c2pas_ename(abr->nbpEntityPtr, &new_ent);

  retry_info.interval = abr->nbpRetransmitInfo.retransInterval;
  retry_info.retries  = abr->nbpRetransmitInfo.retransCount;
  retry_info.backoff  = 1;      /* ??? */

  if (async) {
     fprintf(stderr, "NBP: async NBPRegister not implemented\n");
     return -1;
  }
  if (dbug.db_nbp) {
     printf("NBP: calling NBP Register %s:%s@%s for socket %d\n",
	abr->nbpEntityPtr->objStr.s,
	abr->nbpEntityPtr->typeStr.s,
	abr->nbpEntityPtr->zoneStr.s,
	abr->nbpAddress.skt);
  }

  fd = ddp_skt2fd(abr->nbpAddress.skt);
  if (fd < 0) {
     fprintf(stderr, "NBP: can't register unopened socket %d\n",
	abr->nbpAddress.skt);
     return -1;
  }

  err = nbp_register(&new_ent, fd, &retry_info);

  if (err == -1 && errno == EADDRNOTAVAIL) {
      if (dbug.db_nbp) {
	  printf("NBP: %s:%s@%s already registered\n",
		  abr->nbpEntityPtr->objStr.s,
		  abr->nbpEntityPtr->typeStr.s,
		  abr->nbpEntityPtr->zoneStr.s);
      }
      return nbpDuplicate;
  }

  /* record the entity name and the file descriptor for NBPRemove
   * Record the entity name in contiguous pascal form for simple
   * comparisons.
   */

  return err;
}

/*
 * remove a nve
 *
*/
NBPRemove(abEntity)
EntityName *abEntity;
{
  int err, fd;
  at_entity_t new_ent;

  c2pas_ename(abEntity, &new_ent);

  /* need to look up a file descriptor to go with the entity name
   * Either we keep the list of names we've registered, or we
   * use NBPLookup to find them. The former is probably better.
   */

  err = nbp_remove(&new_ent, fd);

  if (dbug.db_nbp) {
     printf("NBP: NBP Remove %s:%s@%s (fd=%d) returns %d\n",
	abEntity->objStr.s, abEntity->objStr.s,
	abEntity->zoneStr.s, fd, err);
  }
  return err;
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
  if (i > ENTITYSIZE)           /* increment to cnt and check aginst cutoff */
    i = ENTITYSIZE;             /* too large: turncated to 32 chars */
  *pc = i;
  cnt += (i+1);
  for (s = cn->typeStr.s, pc = pn++, i = 0; i < ENTITYSIZE; i++, pn++, s++) {
    *pn = *s;
    if (*s == '\0')
      break;
  }
  if (i > ENTITYSIZE)           /* increment to cnt and check aginst cutoff */
    i = ENTITYSIZE;             /* too large: turncated to 32 chars */
  *pc = i;
  cnt += (i+1);
  for (s = cn->zoneStr.s, pc = pn++, i = 0; i < ENTITYSIZE; i++, pn++, s++) {
    *pn = *s;
    if (*s == '\0')
      break;
  }
  if (i > ENTITYSIZE)           /* increment to cnt and check aginst cutoff */
    i = ENTITYSIZE;             /* too large: turncated to 32 chars */
  *pc = i;
  cnt += (i+1);
  return(cnt);          /* return number of bytes used */
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

  ol = *pn;                     /* length of object */
  tl = *(pn+ol+1);              /* length of type */
  zl = *(pn+ol+tl+2);           /* length of zone */
  if (ol > ENTITYSIZE || tl > ENTITYSIZE || zl > ENTITYSIZE) {
    fprintf(stderr,"pkt2c_entity_names: invalid length!\n");
    return(0);
  }
  cpyp2cstr(cn->objStr.s,pn);   /* copy them... */
  cpyp2cstr(cn->typeStr.s,pn+ol+1);
  cpyp2cstr(cn->zoneStr.s,pn+ol+tl+2);
  return(ol+tl+zl+3);           /* return length */
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
  strncpy(en->objStr.s, name, min(ENTITYSIZE, ol)); /* just copy */
  if (ts)
    strncpy(en->typeStr.s, ts+1, min(ENTITYSIZE, tl));
  if (zs)
    strncpy(en->zoneStr.s, zs+1, min(ENTITYSIZE, zl));
}



/* c2pas_str(unsigned char *cn, at_nvestr_t *pn)
 * pas2c_str(at_nvestr_t *pn, unsigned char *cn);
 *
 * Converts between C strings and Pascal strings. Will work
 * correctly even if cn and pn point to the same place.
 */

private void c2pas_str(cn, pn)
unsigned char *cn;
at_nvestr_t *pn;
{
   int length;
   register unsigned char *from, *to;

   length = strlen(cn);
   if (length > NBP_NVE_STR_SIZE) length = NBP_NVE_STR_SIZE;

   /* copy from last character, working backwards */
   if (length) {
       from = cn + length - 1;  /* last char */
       to = (pn->str) + length - 1;
       do {
	   *to-- = *from;
       } while (from-- >= cn);
   }
   pn->len = length;
}

private void pas2c_str(pn, cn)
at_nvestr_t *pn;
register unsigned char *cn;
{
   int length;
   register unsigned char *from;

   from = pn->str;
   length = pn->len;

   while (length--) {
       *cn++ = *from++;
   }
   *cn = '\0';
}

private int c2pas_ename(cn, pn)
EntityName *cn;
at_entity_t *pn;
{
    c2pas_str(&cn->objStr,  &pn->object);
    c2pas_str(&cn->typeStr, &pn->type);
    c2pas_str(&cn->zoneStr, &pn->zone);
}

private int pas2c_ename(cn, pn)
at_entity_t *pn;
EntityName *cn;
{
    pas2c_str(&pn->object, &cn->objStr);
    pas2c_str(&pn->type,   &cn->typeStr);
    pas2c_str(&pn->zone,   &cn->zoneStr);
}
