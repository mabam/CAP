static char rcsid[] = "$Author: djh $ $Date: 1996/06/18 10:51:30 $";
static char rcsident[] = "$Header: /mac/src/cap60/etc/RCS/atis.c,v 2.11 1996/06/18 10:51:30 djh Rel djh $";
static char revision[] = "$Revision: 2.11 $";

/*
 * atis.c - a simple appletalk information server
 *
 * This provides a simple name information and echo server.
 * The NBP it assumes is a slightly extended form (cf. nbp.ext).
 * 
 * Also acts as the RTMP listener for use with Native EtherTalk and
 * Kernel EtherTalk for which we also maintain a simple routing table.
 *
 * Needs some cleaning.  A quit signal causes it to dump it's database.
 * a HUP signal tells it to reload it.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *   in the City of New York.
 *
 * 
 * Edit History:
 *
 *   July 10, 1986    CCKim	Created
 *   August 2, 1986   CCKim     Add dump and load functionality
 *   December 17, 1986 CCKim	Revise to rev1086 of UDP code
 *   April 28, 1991   djh	Add Phase 2 support
 *
 */

#include <stdio.h>
#include <signal.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>
#endif
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include <netat/abnbp.h>
#include <netat/compat.h>
#ifdef USETIMES
# include <time.h>
#endif
#ifdef USEVPRINTF
# include <varargs.h>
#endif

#ifdef	linux
#define SIGEMT SIGUNUSED
#endif	/* linux */

/* signals in use */
#define DUMPSIG sigmask(SIGQUIT)
#define EXITSIG sigmask(SIGTERM)
#define DBUGSIG sigmask(SIGIOT)
#define NDBGSIG sigmask(SIGEMT)
#define LOADSIG sigmask(SIGHUP)

#ifdef NOSIGMASK
static int allsigs[] = { SIGIOT, SIGHUP, SIGTERM, SIGEMT, SIGQUIT, 0 };
#else NOSIGMASK
#define ALLSIGS (sigmask(SIGIOT)|sigmask(SIGHUP)|sigmask(SIGTERM)|\
		sigmask(SIGEMT)|sigmask(SIGQUIT))
#endif NOSIGMASK

/* logging flags */
#define L_UERR 0x20		/* want unix error message */
#define L_EXIT 0x10		/* exit after logging */
#define L_LVL 0xf		/* debug levels */
#define L_LVLMAX 15		/* maximum level */

/* good place to stick the copyright so it shows up in object files */
private char Columbia_Copyright[] = "Copyright (c) 1986,1987,1988 by The Trustees of Columbia University in the City of New York";

private int tempdebugfile = 0;
private int nbpskt = nbpNIS;
private int echoskt = echoSkt;
private int rtmpskt = rtmpSkt;

char *pidfile;

extern short lap_proto;		/* identifies the "LAP" level */

extern u_char bridge_node;
extern u_short this_net, nis_net, bridge_net;

#ifdef PHASE2
extern u_short net_range_start, net_range_end;
#endif PHASE2

#ifndef ETCDIR
# define ETCDIR "/etc"
#endif

#ifndef NISDUMPFILE
# define NISDUMPFILE "/usr/tmp/nis.db"
#endif

#ifndef ATISRUNFILE
# define ATISRUNFILE "/usr/tmp/atis.run"
#endif

private char *nisdumpfile = NISDUMPFILE;
private char *atisrunfile = ATISRUNFILE;
private char *progname;
import DBUG dbug;


/* each tuple is entity name + addrblock + enumerator */
#define NBPTUPSIZE (sizeof(EntityName)+sizeof(AddrBlock)+1)
/* for nbp, must offset for control word and nbp id */
#define NUMREPLYMAX ((ddpMaxData-2)/NBPTUPSIZE)
NBPTEntry reply[NUMREPLYMAX];

extern int get_debug_level();
extern int set_debug_level();

void nbp_extensions();
void nbp_reload();
void nbp_dump();
void nbp_listener();
void echo_listener();
void rtmp_listener();
void atis_end();
void atis_debuginc();
void atis_undebug();
private int nbpcpy(), c2pkt_ename(), pkt2c_ename();

void
nbp_reload()
{
  FILE *fd;
  int cnt;
  int mask;

#ifdef NOSIGMASK
  sighold_all();
#else NOSIGMASK
  mask = sigblock(ALLSIGS);	/* block so we don't get interrupted */
#endif NOSIGMASK
  (void)logit(1, "reloading from %s",nisdumpfile);
  nbptab_init();		/* reset tables */
  cnt = 0;
  if ((fd = fopen(nisdumpfile,"r")) != NULL) {
    cnt = nbptab_load(fd);
    (void)fclose(fd);
  } else logit(L_UERR|1, "dump file open failed");
  logit(1, "loaded %d entries",cnt);
#ifdef NOSIGMASK
  sigrelse_all();
#else NOSIGMASK
  sigsetmask(mask);
#endif NOSIGMASK
  (void)signal(SIGHUP, nbp_reload);
}

void
nbp_dump()
{
  FILE *fd;
  int cnt;
  int mask;

#ifdef NOSIGMASK
  sighold_all();
#else NOSIGMASK
  mask = sigblock(ALLSIGS);	/* block so we don't get interrupted */
#endif NOSIGMASK
  logit(1, "Dumping to %s",nisdumpfile); 
  (void)signal(SIGQUIT, SIG_IGN);
  cnt = 0;
  if ((fd = fopen(nisdumpfile,"w+")) != NULL) {
    (void)chmod(nisdumpfile, 0774);
    cnt = nbptab_dump(fd);
    (void)fclose(fd);
  } else logit(L_UERR|1, "dump file (write) open failed");
  logit(1, "Dumped %d entries",cnt);
#ifdef NOSIGMASK
  sigrelse_all();
#else NOSIGMASK
  sigsetmask(mask);
#endif NOSIGMASK
  (void)signal(SIGQUIT, nbp_dump);
}

void
atis_end()
{
#ifdef linux
  if (lap_proto == LAP_KERNEL) {
    void rtmp_release();
    rtmp_release();
  }
#endif /* linux */
  logit(0, "exiting");
  (void)unlink(pidfile);
  exit(1);
}

void
atis_undebug()
{
#ifdef NOSIGMASK
  sighold(DBUGSIG);
  sighold(NDBGSIG);
#else NOSIGMASK
  int mask = sigblock(DBUGSIG|NDBGSIG);
#endif NOSIGMASK
  set_debug_level(0);
  logit(0, "DEBUGGING OFF");
  if (tempdebugfile)
    nologitfile();
  signal(SIGEMT, atis_undebug);
#ifdef NOSIGMASK
  sigrelse(DBUGSIG);
  sigrelse(NDBGSIG);
#else NOSIGMASK
  sigsetmask(mask);
#endif NOSIGMASK
}

void
atis_debuginc()
{
  int dlevel;

#ifdef NOSIGMASK
  sighold(DBUGSIG);
  sighold(NDBGSIG);
#else NOSIGMASK
  int mask = sigblock(DBUGSIG|NDBGSIG);
#endif NOSIGMASK
  if (!islogitfile()) {
    tempdebugfile++;
    logitfileis(atisrunfile, "w+");
  }
  if ((dlevel = get_debug_level()) < L_LVLMAX)
    set_debug_level(++dlevel);
  logit(0, "DEBUG LEVEL %d", dlevel);
  signal(SIGIOT, atis_debuginc);
#ifdef NOSIGMASK
  sigrelse(DBUGSIG);
  sigrelse(NDBGSIG);
#else NOSIGMASK
  sigsetmask(mask);
#endif NOSIGMASK
}

usage()
{
  fprintf(stderr,"usage: atis -l logfile -Dlevel -d<args> -E -N -R [reload]\n");
  fprintf(stderr,"       [dump] [exit] [debug] [nodebug]\n");
  exit(1);
}

setuppidfilename()
{
  int el;
  char *p;

  el = strlen(ETCDIR);
  pidfile = (char *)malloc(el+sizeof("atis.pid")+5);
  if (pidfile == NULL) {
    fprintf(stderr,"Can't allocate memory for pid file");
    exit(999);
  }
  strcpy(pidfile, ETCDIR);
  if (el > 0) {
    p = pidfile+el - 1;		/* point to last char */
    while (el-- && *p == '/')	/* strip trailing slashes */
      *p-- = '\0';
    p++;			/* make sure at end of string */
  }
  strcpy(p, "/atis.pid");
#ifdef DEBUG
  printf("pid file name = %s\n", pidfile);
#endif
}

setatispid()
{
  FILE *fd;

  if ((fd = fopen(pidfile, "w")) != NULL) {
    fprintf(fd, "%d\n",getpid());
    (void)fclose(fd);
  }
}

getatispid()
{
  FILE *fp;
  int pid;

  if ((fp = fopen(pidfile, "r")) == NULL) {
    logit(L_UERR|0, "No pid file - maybe the daemon wasn't running?");
    return(-1);
  }
  if (fscanf(fp, "%d\n", &pid) != 1) {
    logit(0, "pid file was bad");
    return(-1);
  }
  return(pid);
}

#define NSIGACT 5

struct sigtab {
  char *s_name;
  int s_signal;
  char *s_action;
} sigtab[NSIGACT] = {
  "reload", SIGHUP, "reload nis database",
  "dump", SIGQUIT, "dump nis database",
  "debug", SIGIOT, "increment debug level",
  "nodebug", SIGEMT, "no debug level",
  "exit", SIGTERM, "stop running atis"
};

handlesigact(s, pid)
char *s;
{
  int i;
  struct sigtab *st;

  pid = getatispid();
  for (st = sigtab, i = 0; i < NSIGACT; i++, st++) {
    if (strcmp(s, st->s_name) != 0)
      continue;
    if (kill(pid, st->s_signal) < 0)
      logit(0, "Couldn't send %s signal to daemon[%d] - is it running?",
	  st->s_action, pid);
    else
      logit(0, "Sent %s signal to daemon[%d]",st->s_action,pid);
    return(0);
  }
  return(-1);
}

doargs(argc, argv)
int argc;
char **argv;
{
  int pid;
  int c, dlevel;
  extern char *optarg;
  extern int optind;
  extern boolean dochecksum;

  while ((c = getopt(argc, argv, "kENRD:d:l:")) != EOF) {
    switch (c) {
    case 'k':
      dochecksum = 0;
      break;
    case 'N':
      nbpskt = 0;
      logit(0, "no nis server will be established");
      break;
    case 'E':
      echoskt = 0;
      logit(0, "no echo listener will be established");
      break;
    case 'R':
      rtmpskt = 0;
      logit(0, "no rtmp listener will be established");
      break;
    case 'D':
      dlevel = atoi(optarg);
      if (dlevel > L_LVLMAX)
	dlevel = L_LVLMAX;
      set_debug_level(dlevel);
      break;
    case 'd':
      dbugarg(optarg);
      set_debug_level(1);
      break;
    case 'l':
      logitfileis(optarg, "w");
      break;
    }
  }

  if (optind == argc)
    return;
  if ((pid = getatispid()) < 0) {
    logit(L_EXIT|0, "Couldn't get pid of daemon - is it running?");
  }
  for (; optind < argc; optind++)
    if (handlesigact(argv[optind], pid) < 0)
      usage();
  exit(0);
}

disassociate()
{
  int i;
  /* disassociate */
  if (fork())
    _exit(0);			/* kill parent */
  for (i=0; i < 3; i++) close(i); /* kill */
  (void)open("/",0);
#ifdef NODUP2
  (void)dup(0);			/* slot 1 */
  (void)dup(0);			/* slot 2 */
#else  NODUP2
  (void)dup2(0,1);
  (void)dup2(0,2);
#endif NODUP2
#ifndef POSIX
#ifdef TIOCNOTTY
  if ((i = open("/dev/tty",2)) > 0) {
    (void)ioctl(i, TIOCNOTTY, (caddr_t)0);
    (void)close(i);
  }
#endif TIOCNOTTY
#ifdef xenix5
    /*
     * USG process groups:
     * The fork guarantees that the child is not a process group leader.
     * Then setpgrp() can work, whick loses the controllong tty.
     * Note that we must be careful not to be the first to open any tty,
     * or it will become our controlling tty.  C'est la vie.
     */
    setpgrp();
#endif xenix5
#else  POSIX
    (void) setsid();
#endif POSIX
}

main(argc, argv)
int argc;
char **argv;
{
  int nbperr = noErr;
  int echoerr = noErr;
  int rtmperr = noErr;

  set_debug_level(0);
  setuppidfilename();

  progname = argv[0];
  doargs(argc, argv);

  abInit(TRUE);			/* init driver */
  nbpInit();			/* init NBP */
  nbptab_init();
  (void)signal(SIGHUP, nbp_reload);
  (void)signal(SIGQUIT, nbp_dump);
  (void)signal(SIGTERM, atis_end);
  (void)signal(SIGIOT, atis_debuginc);
  (void)signal(SIGEMT, atis_undebug);

  logit(0, "Reply num max for lkup reply is %d (based on %d)",
      NUMREPLYMAX, NBPTUPSIZE);

  if (nbpskt) {
    if ((nbperr = DDPOpenSocket(&nbpskt, nbp_listener)) != noErr)
      logit(L_UERR|0, "NIS: NIS socket not available: error %d", nbperr);
  }

  if (echoskt) {
    if ((echoerr = DDPOpenSocket(&echoskt, echo_listener)) != noErr)
      logit(L_UERR|0, "ECHO: ECHO socket not available: error %d", echoerr);
  }

  if ((lap_proto == LAP_ETALK || lap_proto == LAP_KERNEL) && rtmpskt) {
    if ((rtmperr = DDPOpenSocket(&rtmpskt, rtmp_listener)) != noErr)
      logit(L_UERR|0, "RTMP: RTMP socket not available: error %d", rtmperr);
#ifdef linux
    if (lap_proto == LAP_KERNEL) {
      extern void rtmp_timer();
      Timeout(rtmp_timer, 0, 40);
    }
#endif /* linux */
  }

  if (echoerr != noErr && nbperr != noErr && rtmperr != noErr)
   logit(0|L_EXIT,"Couldn't establish nis, rtmp or echo socket, nothing to do");

  if (!dbug.db_flgs && (get_debug_level() == 0))
    disassociate();

  /* store pid where people can see it */
  setatispid();
  logit(0,"pid = %d",getpid());

  /* sleep for a day (but wake up on events) */

  for (;;)
    abSleep(sectotick(60*60*24), TRUE);

}


/*
 * This is the NBP NIS listener
 *
*/
void
nbp_listener(skt, type, nbp, len, addr)
u_char skt;
u_char type;
NBP *nbp;
int len;
AddrBlock *addr;
{
  int cnt;
  NBPTEntry looks;

  if (len < nbpMinSize || type != ddpNBP) {
    logit(3, "Packet too small or bad packet type");
    return;
  }
  /* technically, we should check to see if it is ourselves and not */
  /* respond if so, but it is convient to do so and is part of the */
  /* nbp extensions */
  switch (nbp->control) {
  default:
  case nbpLkUpReply:
    logit(3, "Dropping nbp of type %d",nbp->control);
    return;			/* drop the packet */
  case nbpTickle:
  case nbpRegister:
  case nbpDelete:
  case nbpBrRq:
  case nbpLkUp:
    break;
  }
  /* at this point we know we have a lookup */
  logit(2, "Got nbp %d lkup from net %d.%d, node %d, skt %d",
      nbp->tcnt, ntohs(addr->net)>>8, ntohs(addr->net)&0xff,
      addr->node, addr->skt);
  /* should only get one entity in incoming packet */
  switch (nbp->control) {
  case nbpBrRq:			/* treat as lkup */
  case nbpLkUp:
    cnt = nbpcpy(&looks, 0, 1, nbp, (int)nbp->tcnt, FALSE);
    answer(&looks, &looks.addr, nbp->id);
    break;
  case nbpTickle:
    /* tickle may be implemented in the future */
    /* tickle may take list of entities in future */
    break;
  case nbpRegister:
  case nbpDelete:
    cnt = nbpcpy(&looks, 0, 1, nbp, 1, FALSE);
    nbp_extensions(&looks, addr, nbp, len);
    break;
  }
}


/*
 * reply to a NBP LkUp request
 *
*/
answer(en, addr, id)
NBPTEntry *en;
AddrBlock *addr;
u_char id;
{
  NBP nbp;
  ABusRecord ddp;
  ddpProto *ddpr;
  int cnt, nsize, tsize;
  NBPTuple *tp;
  int i, start = 0;
  int mask;
  
#ifdef ISO_TRANSLATE
  EntityName isoEn;
  void cMac2ISO();

  bcopy(en->ent.objStr.s, isoEn.objStr.s, sizeof(Str32));
  bcopy(en->ent.typeStr.s, isoEn.typeStr.s, sizeof(Str32));
  bcopy(en->ent.zoneStr.s, isoEn.zoneStr.s, sizeof(Str32));
  cMac2ISO(isoEn.objStr.s);
  cMac2ISO(isoEn.typeStr.s);
  cMac2ISO(isoEn.zoneStr.s);
  logit(2, "Looking for entities to answer a lookup with %s:%s@%s",
      isoEn.objStr.s, isoEn.typeStr.s, isoEn.zoneStr.s);
#else  ISO_TRANSLATE
  logit(2, "Looking for entities to answer a lookup with %s:%s@%s",
      en->ent.objStr.s, en->ent.typeStr.s, en->ent.zoneStr.s);
#endif ISO_TRANSLATE
#ifdef NOSIGMASK
  sighold(DUMPSIG);
  sighold(LOADSIG);
#else NOSIGMASK
  mask = sigblock(DUMPSIG|LOADSIG); /* prevent inconsistency */
#endif NOSIGMASK
  while ((cnt = nbpt_find(&en->ent, &start, reply, NUMREPLYMAX)) > 0) {
    if (cnt > 255) {
      logit(3, "Dropping count to 255 from %d",cnt);
      cnt = 255;
    }
    logit(2, "Answering lookup with %d entities",cnt);
    nbp.tcnt = cnt;
    nbp.control = nbpLkUpReply;
    nbp.id = id;
    for (i = 0, tsize = 0, tp = nbp.tuple; i < cnt; i++) {
      tp->enume = (byte)reply[i].enume;
      bcopy((caddr_t)&reply[i].addr, (caddr_t)&tp->addr, sizeof(AddrBlock));
      nsize = c2pkt_ename(&reply[i].ent, tp->name);
      nsize += sizeof(AddrBlock) + sizeof(tp->enume);
      tsize += nsize;
      tp = (NBPTuple *)(((char *)tp) + nsize);
    }
    ddpr = &ddp.proto.ddp;	/* handle on DDP protocol args */
    ddpr->ddpAddress = *addr;
    ddpr->ddpSocket = nbpskt;
    ddpr->ddpType = ddpNBP;
    ddpr->ddpDataPtr = (u_char *) &nbp;
    ddpr->ddpReqCount = 2+tsize;	/* control + id + rest */
    DDPWrite(&ddp,FALSE);		/* write it out... */
  }
#ifdef NOSIGMASK
  sigrelse(DUMPSIG);
  sigrelse(LOADSIG);
#else NOSIGMASK
  sigsetmask(mask);
#endif NOSIGMASK
}


/*
 * Handle the extended NBP functions
 *
*/
void
nbp_extensions(nbptent, addr, nbp, len)
NBPTEntry *nbptent;
AddrBlock *addr;
NBP *nbp;
int len;
{
  ABusRecord abr;
  int mask;
  int rc;
  char objStr[33], typeStr[33], zoneStr[33];

  switch (nbp->control) {
  case nbpRegister:
    logit(2, "Register from net %3d.%02d node %d skt %d ",
	ntohs(addr->net)>>8, ntohs(addr->net)&0xff,addr->node, addr->skt);
    if (get_debug_level() > 1) {
      objStr[32] = typeStr[32] = zoneStr[32] = '\0'; /* tie off? */
      strcpy(objStr, (char *)nbptent->ent.objStr.s);
      strcpy(typeStr, (char *)nbptent->ent.typeStr.s);
      strcpy(zoneStr, (char *)nbptent->ent.zoneStr.s);
      logit(2, "\tfor %s:%s@%s, net %3d.%02d node %d skt %d ",
	  objStr, typeStr, zoneStr,
	  ntohs(nbptent->addr.net)>>8,
	  ntohs(nbptent->addr.net)&0xff, nbptent->addr.node,
	  nbptent->addr.skt);
    }
	  

#ifdef NOSIGMASK
    sighold(DUMPSIG);
    sighold(LOADSIG);
#else NOSIGMASK
    mask = sigblock(DUMPSIG|LOADSIG); /* prevent inconsistency */
#endif NOSIGMASK
    rc = nbptab_insert(addr, &nbptent->addr, &nbptent->ent);
#ifdef NOSIGMASK
    sigrelse(DUMPSIG);
    sigrelse(LOADSIG);
#else NOSIGMASK
    sigsetmask(mask);
#endif NOSIGMASK
    logit(2, "Register return code %d",rc);
    /* return address */
    bcopy((caddr_t)&nbptent->addr, (caddr_t)&abr.proto.ddp.ddpAddress,
	  sizeof(AddrBlock));
    break;
  case nbpDelete:
    if (get_debug_level() > 1) {
      objStr[32] = typeStr[32] = zoneStr[32] = '\0'; /* tie off? */
      strcpy(objStr, (char *)nbptent->ent.objStr.s);
      strcpy(typeStr, (char *)nbptent->ent.typeStr.s);
      strcpy(zoneStr, (char *)nbptent->ent.zoneStr.s);
      logit(2, "Delete %32s:%32s@%32s, net %3d.%02d node %d skt %d",
	  objStr, typeStr, zoneStr, ntohs(addr->net)>>8,
	  ntohs(addr->net)&0xff,addr->node, addr->skt);
    }
#ifdef NOSIGMASK
    sighold(DUMPSIG);
    sighold(LOADSIG);
#else NOSIGMASK
    mask = sigblock(DUMPSIG|LOADSIG); /* prevent inconsistency */
#endif NOSIGMASK
    rc = nbptab_delete(addr, &nbptent->ent) ;
#ifdef NOSIGMASK
    sigrelse(DUMPSIG);
    sigrelse(LOADSIG);
#else NOSIGMASK
    sigsetmask(mask);
#endif NOSIGMASK
    logit(2, "Delete returns code %d",rc);
    abr.proto.ddp.ddpAddress = *addr; /* return address */
    break;
  case nbpTickle:
    logit(2, "Got a tickle");
    return;
  default:
    logit(2, "Unknown NBP type %d",nbp->control);
    return;
  }
  nbp->control = nbpStatusReply;
  nbp->tcnt = rc;
  abr.proto.ddp.ddpSocket = nbpskt;
  abr.proto.ddp.ddpType = ddpNBP;
  abr.proto.ddp.ddpDataPtr = (u_char *)nbp;
  abr.proto.ddp.ddpReqCount = len;
  DDPWrite(&abr, FALSE);
}

/*
 * This is the Echo Protocol listener
 *
*/
void
echo_listener(skt, type, pkt, len, addr)
u_char skt;
u_char type;
char *pkt;
int len;
AddrBlock *addr;
{
  u_char ec;			/* echo command */
  ABusRecord abr;

  if (type != ddpECHO) {
    logit(1, "Got non-echo pkt in echolistener");
    return;			/* drop packet */
  }
  ec = (u_char)*pkt;
  if (ec != echoRequest) {
    printf("got %u when expecting echorequest\n",ec);
    return;			/* drop packet */
  }
  *pkt = echoReply;
  abr.proto.ddp.ddpAddress = *addr;
  abr.proto.ddp.ddpSocket = echoskt;
  abr.proto.ddp.ddpType = ddpECHO;
  abr.proto.ddp.ddpDataPtr = (u_char *)pkt;
  abr.proto.ddp.ddpReqCount = len;
  DDPWrite(&abr, FALSE);
}

/*
 * This is the RTMP listener
 *
 */

void
rtmp_listener(skt, type, pkt, len, addr)
u_char skt;
u_char type;
u_char *pkt;
int len;
AddrBlock *addr;
{
  u_char rc;			/* rtmp command */
  time_t now;
  u_short net;
  ABusRecord abr;
  static int goodness = 0;
  static AddrBlock last_addr;
  static time_t last_time = 0;
  static AddrBlock current_addr;
  static time_t current_time = 0;
  static int current_goodness = -1;
#ifdef PHASE2
  u_short new_net_range_start, new_net_range_end, increment;
#endif PHASE2

  if (type != ddpRTMP) {
    logit(1, "Got non-rtmp pkt in rtmplistener");
    return;	/* drop packet */
  }

  net = htons((pkt[0] << 8) | pkt[1]);

  logit(5, "Got RTMP pkt net %d from %d.%d",
	ntohs(net), ntohs(addr->net), addr->node);

  if (bridge_net == 0 && net != 0) {
    addr->net = net;
    SetBridgeAddress(addr);
    logit(1, "Gleaned network number %d from bridge %d",ntohs(net),addr->node);
  }

#ifdef PHASE2
  if (ntohs(net)>=ntohs(net_range_start) && ntohs(net)<=ntohs(net_range_end)) {
#else  PHASE2
  if (net == addr->net) {
#endif PHASE2
/*
 * Compute the goodness of this router.  We prefer routers that do
 * split horizon, and among them, those that have the most routes.
 * The goodness is thus the number of routes, and 0 for non-split horizon.
 * At Rutgers the effect is to prefer cisco (which does split horizon)
 * over Kinetics, and to pick the cisco that is the most "central".
 * This should tend to produce the best routes.  It will probably
 * work reasonably at most other places as well.  We expire an old
 * router after 15 sec.  This allows us to miss one update, but no
 * more.  Apple wants us to use the most recent router we heard from,
 * but that allows no selectivity at all, and also tends to lead to
 * inconsistent results.  Unfortunately Apple specifies that when
 * several routes have the same metric, routers choose the most recently
 * heard.  This leads to rapidly changing entries.  The result can be
 * small changes in goodness.  If two routers are very close, we could
 * end up going between them.  If this is a problem for you, redefine
 * MARGIN to be 10% or more of the current_goodness.
 *
 * Be aware that very good routers can send a full RTMP packet followed
 * by a small "overflow" RTMP packet, be careful not to overreact.
 *
 */

    /*
     * go to beginning of routing triples
     *
     */
#ifdef PHASE2
    pkt += 4;
    len -= 4;
    if (pkt[0] == 0 && pkt[1] == 0 && pkt[2] == 0x82) {	/* non-extended net */
      new_net_range_start = net;			/* net byte order */
      new_net_range_end   = net;			/* net byte order */
    } else if (pkt[2] & 0x80 && pkt[5] == 0x82) {	/* extended network */
      new_net_range_start = htons((pkt[0] << 8) | pkt[1]);
      new_net_range_end   = htons((pkt[3] << 8) | pkt[4]);
      pkt += 3;
      len -= 3;
    } else {
      logit(2, "RTMP: unknown format packet, dropped!");
      return;
    }
    pkt += 3;
    len -= 3;
#ifdef notdef
    /* ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ */
    /* we should be doing this, in case the router was down when */
    /* we started up. However, doing so will proably confuse our */
    /* clients too much. We should also do it to ensure that the */
    /* correct zone multicast address gets enabled on the intrfc */
    /* ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ */
    if (net_range_start == 0x00 && net_range_end == htons(0xfffe)) {
      this_net = nis_net = net_range_start = new_net_range_start;
      net_range_end = new_net_range_end;
      SetNetRange(net_range_start, net_range_end);
    }
#endif notdef
#else  PHASE2
    pkt += 7;
    len -= 7;
#endif PHASE2

#ifdef linux
    /*
     * build RTMP table for kernel
     *
     */
    if (lap_proto == LAP_KERNEL) {
      void rtmp_data();
      rtmp_data(addr->node, addr->net, pkt, len);
    }
#endif /* linux */

    now = time(NULL);

    /*
     * reset goodness if more than 2 seconds
     * old or this is from a different router.
     *
     */
    if (now > last_time+2
     || last_addr.net != addr->net
     || last_addr.node != addr->node)
      goodness = 0;

    /*
     * loop over routing triples, counting them.
     * If we find the router network, this router
     * isn't doing split horizon.
     *
     */
    while (len > 0) {
      net = htons((pkt[0] << 8) | pkt[1]);	/* pkt data is not aligned */
      if (net == addr->net) {			/* not doing split horizon */
	logit (10, "No split horizon in router %d.%d",
	  ntohs(addr->net), addr->node);
	goodness = 0;
      }
      goodness++;
#ifdef PHASE2
      if (pkt[2] & 0x80 && pkt[5] == 0x82)
        increment = 3;
      else
        increment = 0;
      len -= increment;
      pkt += increment;
#endif PHASE2
      len -= 3;
      pkt += 3;
    }

    logit (8, "Router %d.%d has goodness %d",
	ntohs(addr->net), addr->node, goodness);

    last_time = now;
    last_addr.net = addr->net;
    last_addr.node = addr->node;

#define MARGIN (current_goodness / 20)	/* currently 5% */

    /*
     * this router knows more than 5% more
     * routes than the current one, adopt it
     * (may be subsequent packets from current)
     *
     */
    if (goodness > (current_goodness+MARGIN)) {
      if (addr->node != current_addr.node
       || addr->net != current_addr.net) {
        SetBridgeAddress(addr);
        logit (1, "New default router %d.%d, goodness %d -> %d",
	  ntohs(addr->net), addr->node, current_goodness, goodness);
        current_addr.node = addr->node;
        current_addr.net = addr->net;
      }
      current_goodness = goodness;
      current_time = now;
      return;
    }

    /*
     * same router, update heard-from time
     *
     */
    if (addr->node == current_addr.node
     && addr->net == current_addr.net) {
      logit (1, "Current default router %d.%d, goodness %d",
        ntohs(addr->net), addr->node, current_goodness);
      current_time = now;
      return;
    }

    /*
     * different router, and we haven't heard
     * from our current router for more than
     * 15 seconds, adopt it
     *
     */
    if ((addr->node != current_addr.node
     || addr->net != current_addr.net)
     && (now - current_time) > 15) {
      SetBridgeAddress(addr);
      logit (1, "New default router %d.%d (old expired)",
	ntohs(addr->net), addr->node);
      current_addr.node = addr->node;
      current_addr.net = addr->net;
      current_goodness = goodness;
      current_time = now;
      return;
    }
  }
}

/*
 * The following should be integrated into ABNBP.C someday
 * (Looks like that someday will never come :-).
*/

/*
 * private int nbpcpy(en, enc, start, nbp, nbptcnt, unique)
 *
 * nbpcpy copies the entities in nbp to the NBP Table entry array
 * pointed to by en.  Maximum number of entities is enc.  Start specifies
 * where in the array to start.  Unique is used as a flag: if true
 * then make sure the incoming items are unique (e.g. don't duplicate
 * items in the table pointed to by en).
 * 
 * at end number of entries inserted is returned 
 *
*/
private int
nbpcpy(entab, enstart, entabmax, nbp, nbptcnt, unique)
NBPTEntry *entab;
int enstart;
int entabmax;
NBP *nbp;
int nbptcnt;
int unique;
{
  NBPTuple *ep;
  int i, tcount, len;
  NBPTEntry curr;

  /* Add NBP tuples to user's data structure */
  /* make sure curr's ent is empty */
  bzero((caddr_t)&curr.ent, sizeof(curr.ent));
  for (i=enstart, tcount=nbptcnt, ep = nbp->tuple;
       tcount != 0 && i < entabmax;
       tcount--) {
    bcopy((caddr_t)&ep->addr,(caddr_t)&curr.addr, sizeof(AddrBlock));
    bcopy((caddr_t)&ep->enume,(caddr_t)&curr.enume, sizeof(curr.enume));
    len = pkt2c_ename(ep->name,&curr.ent);
    ep = (NBPTuple *) ((char *) ep+(len+sizeof(AddrBlock)+1));
    i += nbpinsertentry(entab, i, &curr, unique);
  }
  return(i-enstart);
}


/* insert entry into specified NBP Table.  Check previous entries for */
/* conflict - if conflict - then update entry if update flag is on */
/* ow. insert at specified point */
/* returns 1 if inserted item, zero o.w. - no errors possible */
nbpinsertentry(entab, point, nbptentry, update)
NBPTEntry entab[];
int point;
NBPTEntry *nbptentry;
boolean update;
{
  NBPTEntry *cetp = entab;
  int i;

  if (update) {			/* update==false ==> always insert */
    for (i=0; i < (point-1); i++) {
      if (bcmp((caddr_t)&cetp->addr, (caddr_t)&nbptentry->addr,
	       sizeof(cetp->addr)) == 0)
	if (cetp->enume == nbptentry->enume) {
	  bcopy((caddr_t)&nbptentry->ent,(caddr_t)&cetp->ent,
		sizeof(cetp->ent));
	  return(0);		/* done */
	}
    }
  }
  bcopy((caddr_t)nbptentry, (caddr_t)cetp, sizeof(NBPTEntry));
  return(1);
}



/*
 * Private int c2pkt_ename(EntityName *cn, u_char *pn)
 * 
 * Copy entity name from c form into contiguous Apple Pascal
 * form (packet form).
 *
 * return: length of pascal form entity name
 *
 */

private int
c2pkt_ename(cn,pn)
u_char *pn;
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
 * Private int pkt2c_enames(u_char *pn, EntityName *cn);
 *
 * Copy entity names from packet form (abutting Apple Pascal
 * strings) to c form into structure of type EntityName.
 *
 * return: the length of the packed string.
 *
 */

private int
pkt2c_ename(pn,cn)
u_char *pn;
EntityName *cn;
{
  int ol,tl,zl;

  ol = *pn;			/* length of object */
  tl = *(pn+ol+1);		/* length of type */
  zl = *(pn+ol+tl+2);		/* length of zone */
  if (ol > ENTITYSIZE || tl > ENTITYSIZE || zl > ENTITYSIZE) {
    logit(3,"pkt2c_entity_names: invalid length!");
    return(0);
  }
  cpyp2cstr(cn->objStr.s,pn);	/* copy them... */
  cpyp2cstr(cn->typeStr.s,pn+ol+1);
  cpyp2cstr(cn->zoneStr.s,pn+ol+tl+2);
  return(ol+tl+zl+3);		/* return length */
}

#ifdef notdef

/*
 * print message - use vprintf whenever possible (solves the problem
 * of using the varargs macros -- you must interpret the format).
 * This is something all machine should, but don't have :-)
 */

private FILE *lfp = stderr;


#ifndef USEVPRINTF
/* Bletch - gotta do it because pyramids don't work the other way */
/* (using _doprnt and &args) and don't have vprintf */
/* of course, there will be something that is just one arg larger :-) */
/*VARARGS1*/
logit(level, fmt, a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af)
int level;
char *fmt;
#else
logit(va_alist)
va_dcl
#endif
{
  long time(), tloc;
  char *timestr;
#ifdef USEVPRINTF
  register char *fmt;
  va_list args;
  int level;
#endif
  int saveerr;
  extern int errno;
  extern int sys_nerr;
  extern char *sys_errlist[];

  if (lfp == NULL)		/* no logging? */
    return;

  saveerr = errno;
#ifdef USEVPRINTF
  va_start(args);
  level = va_arg(args, int);
  fmt = va_arg(args, char *);
#endif

  if (dlevel < (level & L_LVL))
    return;
  (void)time(&tloc);
  timestr = (char *)ctime(&tloc);
  timestr[24] = '\0';		/* hokey */
  fprintf(lfp,"atis: %s ",timestr);

#ifdef USEVPRINTF
  vfprintf(lfp, fmt, args);
  va_end(args);
#else
  fprintf(lfp, fmt, a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af);
#endif
  if (level & L_UERR) {
    if (saveerr < sys_nerr)
      fprintf(lfp, ": %s", sys_errlist[saveerr]);
    else
      fprintf(lfp, ": error %d\n", saveerr);
  }
  putc('\n', lfp);
  fflush(lfp);
  if (level & L_EXIT)
    exit(1);
}

islogitfile()
{
  if (lfp == stderr)
    return(FALSE);
  return(lfp != NULL);
}

logitfileis(filename, mode)
char *filename;
char *mode;
{
  FILE *fp;

  if ((fp = fopen(filename, mode)) != NULL) {
    logit(0, "log file name %s", filename);
  } else {
    logit(0|L_UERR, "couldn't open logfile %s", filename);
  }
  lfp = fp;			/* reset */
}

nologitfile()
{
  if (lfp && lfp != stderr)
    fclose(lfp);
}
#endif notdef

#ifdef NOSIGMASK

sighold_all()
{
	int i;

	for (i = 0; allsigs[i]; ++i)
		sighold(allsigs[i]);
}

sigrelse_all()
{
	int i;

	for (i = 0; allsigs[i]; ++i)
		sigrelse(allsigs[i]);
}

#endif NOSIGMASK
