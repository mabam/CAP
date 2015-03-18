/*
 * snitch.c: a mimic of the "responder" INIT for Inter-Poll from Apple.
 * Advertises itself as "machinename.UNIX/CAP@*" via NBP,
 * and responds to ATP system info requests.
 *
 * To kill this cleanly, send the process a QUIT or TERM signal.
 * It will nbp_delete itself (as all nbp services should).
 * 
 * To really make this work with Inter-Poll, you want to add the string
 * "UNIX/CAP" (or your snitchtype) to the STR# that specifies the kinds of 
 * expected machine types.  The first item in that STR# is a number that 
 * indicates the number of following valid entries - add one to it and then
 * add "UNIX/CAP" at the end.  The string list is STR# "NIP Devices".
 *
 * This code takes pieces from atistest.c and efsd.c from the CAP distribution.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * From atistest.c:
 *
 * Copyright (c) 1986,1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * snitch by Rob Chandhok, Computer Science Department, 
 *           Carnegie Mellon University
 *
 * Edit History:
 *
 *  March 16,1988    chandhok	        Created snitch.
 *  March 17, 1988   cckim		clean up a bit
 *
 */

#include <stdio.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#include <netat/appletalk.h>

#ifdef USESTRINGDOTH		/* based on sysvcompat.h in appletalk.h */
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#define MAXNBPSTRING 32

/* MAXSNITCHSTRING is the max string length for a packed string in
 * with interpoll (127) minus some slop 
 */

#define MAXSNITCHSTRING 127
#define USER_MAXSNITCHSTRING 100
private char	snitchname[MAXNBPSTRING + 1] = "";
private char	snitchtype[MAXNBPSTRING + 1] = "UNIX/CAP";
private int     snitchdebug = 0;

private int spawn = 0;		/* disassociate process? (def. no) */

private char	finderstring[USER_MAXSNITCHSTRING + 1] = "";
private char	lwstring[USER_MAXSNITCHSTRING + 1] = "lwsrv";

/* saved values for snitch handler */
private char    real_finderstring[MAXSNITCHSTRING + 1];
private char	real_lwstring[MAXSNITCHSTRING + 1];
private char	real_system[MAXSNITCHSTRING + 1];

private char	fullhostname[256] = "";
private int     gmypid = -1;

#ifndef kipnetnumber
/* high and low part of a "kip" network number - assume number in network */
/* format */
#define nkipnetnumber(x) ntohs((x))>>8
#define nkipsubnetnumber(x) ntohs((x))&0xff

/* same but assumes in host form */
#define kipnetnumber(x) (x)>>8
#define kipsubnetnumber(x) (x)&0xff
#endif

/* forward */
void snitch_handler();

void
cleanup()
{
  int err;

  if (snitchdebug) {
      fprintf(stderr,"snitch: Un-Registering \"%s:%s@*\"\n",
	      snitchname,snitchtype);
  }
  err = nbp_delete(snitchname, snitchtype, "*");
  if (err != noErr)
    aerror("nbp delete",err);
  else
    fprintf(stderr,"snitch: Bye\n");

  exit(1);
}

usage(name)
char *name;
{
  char *DBLevelOpts();

  fprintf(stderr,"usage: %s [-d cap-flags] [-n name] [-t type]\n",name);
  fprintf(stderr,"\t[-f finderstring] [-l laserwriterstring] [-S]\n");
  fprintf(stderr,"\t-d for CAP debugging flags:\n");
  fprintf(stderr,"\t   l = lap, d = ddp, a = atp, n = nbp, p = pap,");
  fprintf(stderr,"i = ini, s = asp\n");
  fprintf(stderr,"\t-Dn for %s debugging level n (writes to stderr)\n",name);
  fprintf(stderr,"\t-S to disassociate snitch process\n");
  fprintf(stderr,"\nExample: %s -n 'MyCapMachine'\n",
	  name);

  exit(1);
}

doargs(argc,argv)
int argc;
char **argv;
{
  int c;
  extern char *optarg;
  extern int optind;
  
  while ((c = getopt(argc,argv,"n:t:f:d:D:l:S")) != EOF) {
    switch (c) {
    case 'n':
      strncpy(snitchname, optarg, MAXNBPSTRING);
      break;
    case 't':
      strncpy(snitchtype, optarg, MAXNBPSTRING);
      break;
    case 'f':
      strncpy(finderstring, optarg, USER_MAXSNITCHSTRING);
      break;
    case 'l':
      strncpy(lwstring, optarg, USER_MAXSNITCHSTRING);
      break;
    case 'd':
      dbugarg(optarg);			/* '-d' is debug */
      break;
    case 'S':
      spawn++;
      break;
    case 'D':
      snitchdebug = atoi(optarg);
      if (!snitchdebug) snitchdebug++; /* make sure -D at least sets to 1*/
      break;
    default:
      usage(argv[0]);
      break;
    }
  }
}

main(argc,argv)
int argc;
char **argv;
{
  char *cp;
  AddrBlock useaddr;
  int skt, err;
  ABusRecord req_abr;
  atpProto *ap;
  char req_buf[atpMaxData];	/* max data to receive on request */

  doargs(argc,argv);
  if (snitchdebug) {
      fprintf(stderr,"snitch: debugging level %d\n",snitchdebug);
  }
  if (!snitchdebug && spawn) {
    /* disassociate */
    if (fork())
      _exit(0);			/* kill parent */
    {
      int i;
      
      for (i=0; i < 20; i++) close(i); /* kill */
      (void)open("/",0);
#ifndef NODUP2
      (void)dup2(0,1);
      (void)dup2(0,2);
#else
      (void)dup(0);		/* slot 1 */
      (void)dup(0);		/* slot 2 */
#endif
#ifdef TIOCNOTTY
      if ((i = open("/dev/tty",2)) > 0) {
	(void)ioctl(i, TIOCNOTTY, (caddr_t)0);
	(void)close(i);
      }
#endif TIOCNOTTY
#ifdef POSIX
      (void) setsid();
#endif POSIX
    }
  }

  abInit(snitchdebug);
  nbpInit();

  gmypid = getpid();

  if (snitchdebug) {
      fprintf(stderr,"snitch: my pid is %d\n",gmypid);
  }

  /* Find our host name */
  gethostname(fullhostname, sizeof(fullhostname));
  if (! *snitchname) {
      strcpy(snitchname,fullhostname);
      /* strip off any domain name */
      if ((cp = index(snitchname, '.')) != NULL) *cp = 0;
  }
  if (! *finderstring) {
      struct in_addr thishost;
      
      if (getipaddr(fullhostname, &thishost) < 0)
	sprintf(finderstring,"%s @ <unknown ip address>",fullhostname);
      else
	sprintf(finderstring,"%s @ %s",fullhostname,inet_ntoa(thishost));
  }
  /* setup snitch vars */
  setup_snitch();

  useaddr.net = useaddr.node = useaddr.skt = 0; /* accept from anywhere */
  skt = 0;			/* dynamically allocate skt please */
  if ((err = ATPOpenSocket(&useaddr, &skt)) < 0) {
    perror("ATP Open Socket");
    aerror("ATP Open Socket",err);
    exit(1);
  }

  if (snitchdebug) {
      fprintf(stderr,"Registering \"%s:%s@*\" on socket %d\n",
	      snitchname,snitchtype,skt);
  }

  err = nbp_register(snitchname, snitchtype, "*", skt);
  if (err != noErr)
    aerror("nbp register",err);
  else {
      if (snitchdebug) {
	  fprintf(stderr,"snitch ready\n");
      }
  }

  signal(SIGQUIT, cleanup);
  signal(SIGTERM, cleanup);

  ap = &req_abr.proto.atp;
  do {
    ap->atpSocket = skt;
    ap->atpReqCount = atpMaxData;
    ap->atpDataPtr = req_buf;
    err = ATPGetRequest(&req_abr, FALSE);
    if (err != noErr)
      fprintf(stderr,"ATPGetRequest error: %d\n",err);
    else
      snitch_handler(&req_abr);
  } while (1);

}

/*
 * register the specified entity
 *
*/
nbp_register(sobj, stype, szone, skt)
char *sobj, *stype, *szone;
int skt;
{
  EntityName en;
  nbpProto nbpr;		/* nbp proto */
  NBPTEntry nbpt[1];		/* table of entity names */
  int err;

  strcpy((char *)en.objStr.s, sobj);
  strcpy((char *)en.typeStr.s, stype);
  strcpy((char *)en.zoneStr.s, szone);


  nbpr.nbpAddress.skt = skt;
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
 * delete the specified entity
 *
*/
nbp_delete(sobj, stype, szone)
char *sobj, *stype, *szone;
{
  EntityName en;
  int err;

  strcpy((char *)en.objStr.s, sobj);
  strcpy((char *)en.typeStr.s, stype);
  strcpy((char *)en.zoneStr.s, szone);

  err = NBPRemove(&en);
  return(err);
}

aerror(msg, err)
char *msg;
int err;
{
  fprintf(stderr,"%s error because: ",msg);
  switch (err) {
  case tooManySkts:
    fprintf(stderr,"too many sockets open already\n");
    break;
  case noDataArea:
    fprintf(stderr,"internal data area corruption - no room to \
create socket\n");
    break;
  case nbpDuplicate:
    fprintf(stderr,"name already registered\n");
    break;
  case nbpNoConfirm:
    fprintf(stderr,"couldn't register name - is atis running?\n");
    break;
  case nbpBuffOvr:
    fprintf(stderr,"couldn't register name - too many names already \
registered\n");
    break;
  default:
    fprintf(stderr,"error: %d\n",err);
    break;
  }
}

/* append to appendto as pascal string, update appendto pointer */
int packstring(appendto,cstring)
u_char **appendto;
char *cstring;
{
    int len = strlen(cstring);
    register u_char *app = *appendto;

    if (len > MAXSNITCHSTRING) len = MAXSNITCHSTRING;

    *app++ = (u_char)len;
    bcopy(cstring, app, len);
    *appendto += (len+1);
    return(len+1);
}

/*
 * setup snitch strings
 *
*/
setup_snitch()
{
  AddrBlock nisaddr;			/* nis network and node number */
  AddrBlock thisaddr;			/* our network and node number */
  import struct in_addr bridge_addr;	/* the kbox we use */

  GetNisAddr(&nisaddr);
  GetMyAddr(&thisaddr);

  if (thisaddr.net != nisaddr.net || thisaddr.node != nisaddr.node)
    sprintf(real_system, "System: CAP: bridge %s atis: net %3d.%02d node %d",
	    inet_ntoa(bridge_addr), nkipnetnumber(nisaddr.net),
	    nkipsubnetnumber(nisaddr.net), nisaddr.node);
  else 
    sprintf(real_system, "System: CAP: bridge %s", inet_ntoa(bridge_addr));
  sprintf(real_finderstring,"Finder: %s",finderstring);
  sprintf(real_lwstring,"LaserWriter: %s",lwstring);
}

struct snitch_userbytes {
  byte su_code;			/* snitch code */
#define SNITCH_REQUEST 0
#define SNITCH_REPLY 1
  byte su_xxxx;			/* unknown */
  byte su_version;		/* snitch version */
  byte su_subversion;		/* snitch subversion */
};

struct snitch_buf {
  word sb_atalk_version;	/* 0: atalk version: note: it may only */
				/* be the second byte */
  byte sb_dummy[8];		/* 2: unknown? */
#define SB_STRING_OFFSET 10
  byte sb_strings[1];		/* 10: start of string area */
};

void snitch_handler(req_abr)
ABusRecord *req_abr;
{
  int cnt;
  atpUserDataType userData;
  char buffer[atpMaxData];	/* room for translated message */
  struct atpProto *ap = &req_abr->proto.atp;
  struct snitch_userbytes *su;
  struct snitch_buf *sb;
  byte *p;

  /* Check out the user data field */
  su = (struct snitch_userbytes *)&ap->atpUserData;
  if (snitchdebug > 1) {
    u_char *p = (u_char *)&ap->atpUserData;
    int i;

    for ( i = 0;i < 4; i++) {
	fprintf(stderr,"snitch: user data as bytes[%d]=%x\n",i,p[i]);
    }
  }
 
  if ((su->su_code != SNITCH_REQUEST)) {
    if (snitchdebug) {
      fprintf(stderr,"snitch: bad request code = %x\n",su->su_code);
    }
    return;
  }

  
  su = (struct snitch_userbytes *)&userData;
  su->su_code = SNITCH_REPLY;
  su->su_xxxx = 0;		/* ??? */
  su->su_version = 0;
  su->su_subversion = 0xCA;	/* subversion (as close as I can get to CAP)*/

  /* now fill the user bytes with info. */

  /* atalk driver version is buffer[1] (maybe a short from 0 to 1)*/
  sb = (struct snitch_buf *)buffer;
  sb->sb_atalk_version = htons(2); /* random? */

  /* packed strings */
  p = sb->sb_strings;
  cnt = SB_STRING_OFFSET;
  cnt += packstring(&p,real_system); /* system name */
  cnt += packstring(&p,real_finderstring); /* finder name */
  cnt += packstring(&p,real_lwstring); /* laserwriter driver */

  do_reply(req_abr, userData, buffer, cnt);
}

/*
 * Take need information from the ATP request and turn it into 
 * an ATP response of the given data.
 *
 *
*/
do_reply(req_abr, userdata,data,datalength)
ABusRecord *req_abr;
atpUserDataType userdata;
char *data;
int datalength;
{
  ABusRecord res_abr;
  atpProto *ap;
  BDS aBDS[1];

  ap = &res_abr.proto.atp;
  ap->atpAddress = req_abr->proto.atp.atpAddress;
  ap->atpTransID = req_abr->proto.atp.atpTransID;
  ap->atpSocket = req_abr->proto.atp.atpSocket;
  ap->atpNumBufs = setup_bds(aBDS,sizeof(aBDS),atpMaxData,
			     data,datalength,userdata);
  ap->atpRspBDSPtr = aBDS;
  ap->atpBDSSize = ap->atpNumBufs; /* usually equal in an ATP response */
  ap->fatpEOM = 1; /* is always EOM */

  return(ATPSndRsp(&res_abr, FALSE));
}


/*
 * Get the ip address based on the hostname
 *
*/
getipaddr(hostname, sin)
char *hostname;
struct in_addr *sin;
{
  struct hostent *host;

  if ((host = gethostbyname(hostname)) == NULL)
    return(-1);
  bcopy(host->h_addr, (caddr_t)sin, host->h_length);
  return(0);
}
