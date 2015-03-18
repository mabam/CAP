static char rcsid[] = "$Author: djh $ $Date: 91/02/15 23:04:09 $";
static char rcsident[] = "$Header: look.c,v 2.1 91/02/15 23:04:09 djh Rel $";
static char revision[] = "$Revision: 2.1 $";

/*
 * look - UNIX AppleTalk test program: lookup entities
 * with "LOOKS" defined, will also get pap status and confirm address.
 * with "PINGER" defined will "ping" remote with "echo request"
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  June 13, 1986    Schilit	Created.
 *  Dec  23, 1986    Schilit	Sort result, display in columns, clean up.
 *				add usage and options.
 *  Mar  16, 1987    CCKim	Clean up some more, merge with looks and pinger
 *
 */

char copyright[] = "Copyright (c) 1986,1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>			/* so htons() works for non-vax */
#include <netat/appletalk.h>		/* include appletalk definitions */

#define NUMNBPENTRY 100			/* max names we can lookup */

#ifndef PINGER
# define PINGER 0;
#endif
#ifndef LOOKS
# define LOOKS 0
#endif

int pinger = PINGER;
int lwstatus = LOOKS;
char *deftype = "=";		/* default entity type */

int nbpretry = 3;		/* 3 retries */
int nbptimeout = 3;		/* 3/4 second by default */
/*
 * int compare(NBPTEntry *n1, NBPTEntry *n2)
 *
 * This is the comparison routine for the qsort() library function. 
 * Our comparison is on the entity's type string, and then the object 
 * string. In otherwords primary sort is type, secondary sort is object.
 *
 */

int
comparebyname(n1,n2)
NBPTEntry *n1,*n2;
{
  int rslt;

  if ((rslt = strcmp(n1->ent.typeStr.s,	/* compare types */
		     n2->ent.typeStr.s)) != 0)
    return(rslt);			/* return if they differ */
  return(strcmp(n1->ent.objStr.s,	/* types are the same, return */
		n2->ent.objStr.s));	/* the comparison of objects */
}

int
comparebynet(n1,n2)
NBPTEntry *n1,*n2;
{
  if (n1->addr.net != n2->addr.net) {
    if (n1->addr.net > n2->addr.net)
      return(1);
    return(-1);
  }
  if (n1->addr.node != n2->addr.node) {
    if (n1->addr.node > n2->addr.node)
      return(1);
    return(-1);
  }
  if (n1->addr.skt != n2->addr.skt) {
    if (n1->addr.skt > n2->addr.skt)
      return(1);
    return(-1);
  }
  return(0);
}

int
comparebyskt(n1,n2)
NBPTEntry *n1,*n2;
{
  if (n1->addr.skt != n2->addr.skt) {
    if (n1->addr.skt > n2->addr.skt)
      return(1);
    return(-1);
  }
  if (n1->addr.net != n2->addr.net) {
    if (n1->addr.net > n2->addr.net)
      return(1);
    return(-1);
  }
  if (n1->addr.node != n2->addr.node) {
    if (n1->addr.node > n2->addr.node)
      return(1);
    return(-1);
  }
  return(0);
}

int (*sortcomp)() = comparebyname;


usage(s)
char *s;
{
  fprintf(stderr,"usage: %s [-d FLAGS] [-P] [-S] [-n] [-s] [-t n] [-r n] \
nbpentity\n",s);
  fprintf(stderr,"\t -n means sort by net numbers\n");
  fprintf(stderr,"\t -s means sort by socket numbers\n");
  fprintf(stderr,"\t -r n - specifies number of retries (>=0)\n");
  fprintf(stderr,"\t -t n - specifies nbp timeout between retries (1/4 second units) (>0)\n");
  fprintf(stderr,"\t -P makes look pings the entities\n");
  fprintf(stderr,"\t -S makes look get the \"laserwriter\" status\n");
  fprintf(stderr,"\t    and makes the default type \"LaserWriter\"\n");
  fprintf(stderr,"\t -d FLAGS - cap library debugging flags\n");
  exit(1);
}

getnum(s)
char *s;
{
  int r;
  if (*s == '\0')
    return(0);
  r = atoi(s);
  if (r == 0 && *s != '0')
    return(-1);
  return(r);
}

tickout(n)
int n;
{
  int t = n % 4;		/* ticks */
  int s = n / 4;		/* seconds */
  int m = s / 60;		/* minutes */

  if (m != 0) {
    if (m >= 60)		/* an hour??????? */
      printf(" (are you crazy?)");
    printf(" %d %s", m, m > 1 ? "minutes" : "minute");
    s %= 60;			/* reset seconds to remainder */
  }
  if (s)			/* print seconds if any */
    printf(" %d",s);
  if (t) {			/* print ticks */
    if (t == 2) 
      printf(" and one half");
    else
      printf(" %d fourths",t);
  }
  if (s || t)
    printf(" second");
  if (s > 1 || (t && s))
    printf("s");
}

doargs(argc,argv)
int argc;
char **argv;
{
  char *whoami = argv[0];
  extern char *optarg;
  extern int optind;
  int c;

  while ((c = getopt(argc, argv, "d:D:r:t:nsSP")) != EOF) {
    switch (c) {
    case 'd':
    case 'D':
      dbugarg(optarg);		/* some debug flags */
      break;
    case 'r':
      nbpretry = getnum(optarg);
      if (nbptimeout <= 0)
	usage(whoami);
      printf("Number of NBP retries %d\n",nbpretry);
      break;
    case 't':
      nbptimeout = getnum(optarg);
      if (nbptimeout < 0)
	usage(whoami);
      printf("NBP Timeout");
      tickout(nbptimeout);
      putchar('\n');
      break;
    case 'P':
      pinger++;
      break;
    case 'S':
      lwstatus++;
      deftype = "LaserWriter";	/* switch over */
      break;
    case 'n':
      sortcomp = comparebynet;
      break;
    case 's':
      sortcomp = comparebyskt;
      break;
    case '?':
    default:
      usage(whoami);
    }
  }
  return(optind);
}

my_create_entity(s, en)
char *s;
EntityName *en;
{
  create_entity(s, en);	/* must be fully specified name */
  if (*en->objStr.s == '\0')
    en->objStr.s[0] = '=';
  if (*en->typeStr.s == '\0')
    strcpy(en->typeStr.s,deftype); /*  to lookup... */
  if (*en->zoneStr.s == '\0')
    en->zoneStr.s[0] = '*';
}
    
main(argc,argv)
int argc;
char **argv;
{
  int i;
  EntityName en;			/* network entity name */

  abInit(TRUE);				/* initialize appletalk driver */
  nbpInit();				/* initialize nbp */
  checksum_error(FALSE);		/* ignore these errors */

  i = doargs(argc,argv);		/* handle arguments */
  if (pinger)
    pingInit();
  if (lwstatus)
    deftype = "LaserWriter";
  strcpy(en.objStr.s,"=");		/* create default entity */
  strcpy(en.typeStr.s,deftype);		/*  to lookup... */
  strcpy(en.zoneStr.s,"*");


  if (i == argc) {
    dolookup(&en);
  } else {
    for (; i < argc ; i++) {
      my_create_entity(argv[i], &en);
      dolookup(&en);
    }
  }
}
dolookup(en)
EntityName *en;			/* network entity name */
{
  int err,i, len;
  AddrBlock addr;			/* Address of entity */
  NBPTEntry nbpt[NUMNBPENTRY];		/* table of entity names */
  nbpProto nbpr;			/* nbp protocol record */
  char name[sizeof(EntityName)*4*3+3];	/* for formatted entity name */
				/* 3 entries, of size entity name */
				/* each char can be up to 4 in length */
				/* +3 for :@ and null */


  nbpr.nbpRetransmitInfo.retransInterval = nbptimeout;
  nbpr.nbpRetransmitInfo.retransCount = nbpretry;
  nbpr.nbpBufPtr = nbpt;		/* place to store entries */
  nbpr.nbpBufSize = sizeof(nbpt);	/* size of above */
  nbpr.nbpDataField = NUMNBPENTRY;	/* max entries */
  nbpr.nbpEntityPtr = en;		/* look this entity up */

  len = dumptostr(name, en->objStr.s);
  name[len++] = ':';
  len += dumptostr(name+len, en->typeStr.s);
  name[len++] = '@';
  dumptostr(name+len, en->zoneStr.s);
  printf("Looking for %s ...\n",name);

  /* Find all objects in specified zone */
  err = NBPLookup(&nbpr,FALSE);		/* try synchronous */
  if (err != noErr)
    fprintf(stderr,"NBPLookup returned err %d\n",err);

  /* Sort the result for better viewing */

  qsort((char *)nbpt,(int) nbpr.nbpDataField,sizeof(NBPTEntry),sortcomp);

  /* Extract and print the items */
  for (i = 1; i <= nbpr.nbpDataField; i++) {
    NBPExtract(nbpt,nbpr.nbpDataField,i,en,&addr);
    len = dumptostr(name, en->objStr.s);
    name[len++] = ':';
    len += dumptostr(name+len, en->typeStr.s);
    name[len++] = '@';
    dumptostr(name+len, en->zoneStr.s);
    printf("%02d - %-40s [Net:%3d.%02d Node:%3d Skt:%3d]\n",
	   i,name,htons(addr.net)>>8, htons(addr.net)&0xff,addr.node,addr.skt);
    if (lwstatus)
      getstatus(en, &addr);
    if (pinger) {
      printf("Ping...");
      if (ping(&addr))
	printf("Okay\n");
      else
	printf("no response\n");
    }
  }
}

getstatus(en, addr)
EntityName *en;
AddrBlock *addr;
{
  PAPStatusRec statusbuf;
  char namebuf[100];		/* place to put name of entity */
  nbpProto nbpr;		/* NBP protocol record */
  int err;

  sprintf(namebuf, "%s:%s@%s", en->objStr.s, en->typeStr.s, en->zoneStr.s);
  PAPStatus(namebuf, &statusbuf, addr);
  printf("---");
  dumpstatus(&statusbuf);
  putchar('\n');

  nbpr.nbpRetransmitInfo.retransInterval = 4;
  nbpr.nbpRetransmitInfo.retransCount = 3;
  nbpr.nbpEntityPtr = en;	/* entity name */
  nbpr.nbpAddress = *addr;	/* all other old values */
    err = NBPConfirm(&nbpr,FALSE);
    if (err != noErr)
      printf("Confirm failed, code = %d...\n",err);
    else
      printf("Address confirmed for socket %d\n",nbpr.nbpDataField);
}

/*
 * Dump a PAP status message
*/
dumpstatus(statusbuf)
PAPStatusRec *statusbuf;
{
  int len = (int)(statusbuf->StatusStr[0]);
  unsigned char *s = &statusbuf->StatusStr[1];

  while (len--) {
    if (isprint(*s))
      putchar(*s);
    else
      printf("\\%o",*s&0xff);
    s++;
  }
}

static int myskt ;
pingInit()
{
  int echo_listener();
  int err;

  myskt = 0;
  err = DDPOpenSocket(&myskt, echo_listener);
  if (err != noErr)
    fprintf(stderr, "no dynamic sockets left for ping\n");
}

typedef struct {
  u_char echoFunction;
  char epdata[585];		/* cheap */
} ECHO;

ECHO echo;
ABusRecord abr;


echo_listener(skt, type, pkt, len, addr)
u_char skt;
u_char type;
char *pkt;
int len;
AddrBlock *addr;
{
  int i;

  if (type != ddpECHO)
    return;
  abr.abResult = 1;
#ifdef DEBUG
  printf("Got pkt from %d.%d (skt %d) of size %d\n",
	 ntohs(addr->net), addr->node, addr->skt, len);
#endif
  i = (u_char)*pkt;
  if (len != 6) {
    printf("bad length...");
    return;
  }
  if (i != echoReply) {
    printf("bad reply code...");
    return;
  }
  if (strncmp(pkt+1,"barf",4) != 0) {
    printf("data corruption...");
    return;
  }
}

ping(addr)
AddrBlock *addr;
{

  echo.echoFunction = echoRequest;
  strcpy(echo.epdata, "barf");
  abr.abResult = 0;
  abr.proto.ddp.ddpAddress = *addr;
  abr.proto.ddp.ddpAddress.skt = echoSkt;
  abr.proto.ddp.ddpSocket = myskt;
  abr.proto.ddp.ddpType = ddpECHO;
  abr.proto.ddp.ddpDataPtr = (u_char *)&echo;
  abr.proto.ddp.ddpReqCount = 6; /* code+4bytes of string+null tie */
  DDPWrite(&abr, FALSE);
  abSleep(4*5, TRUE);		/* wait 5 seconds */
  return(abr.abResult);
}

dumptostr(str, todump)
char *str;
char *todump;
{
  char c;
  int i = 0;

  while ((c = *todump++)) {
    if (c > 0 && isprint(c)) {
      *str++ = c;
      i++;
    } else {
      sprintf(str,"\\%3o",c&0xff);
      str+=4;
      i+=4;
    }
  }
  *str = '\0';
  return(i);
}
