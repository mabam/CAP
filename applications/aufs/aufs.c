/*
 * $Author: djh $ $Date: 1996/09/12 05:11:59 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/aufs.c,v 2.34 1996/09/12 05:11:59 djh Rel djh $
 * $Revision: 2.34 $
 *
 */

/*
 * aufs - UNIX AppleTalk Unix File Server
 *
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987 	Schilit		Created.
 *
 */

char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <sys/param.h>
#ifndef _TYPES
 /* assume included by param.h */
#include <sys/types.h>
#endif  _TYPES
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <netinet/in.h>		/* for htons, etc. */
#include <signal.h>

#include <netat/appletalk.h>		/* include appletalk definitions */
#include <netat/compat.h>
#include <netat/afp.h>
#include "afps.h"			/* server includes */

#ifdef NEEDFCNTLDOTH
#include <fcntl.h>
#endif NEEDFCNTLDOTH

#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef USEVPRINTF
# include <varargs.h>
#endif USEVPRINTF

#ifndef NOWAIT3
# define DORUSAGE
#endif NOWAIT3

#ifdef aux
# ifdef DORUSAGE
#  undef DORUSAGE
# endif DORUSAGE
#endif aux

#ifdef NOPGRP
# ifdef xenix5
#  define killpg(pid,sig)	kill(-(pid),sig)
# else xenix5
#  define NOSHUTDOWNCODE
# endif xenix5
#endif NOPGRP

#ifdef DEBUG_AFP_CMD
#ifndef DEBUG_AFP
#define DEBUG_AFP
#endif  /* DEBUG_AFP */
#endif /* DEBUG_AFP_CMD */

/* known attention codes */
#define AFPSHUTDOWNTIME(x) (0x8000|((x)&0xfff))
#define AFPSHUTDOWNCANCEL (0x8fff)
#define AFPSHUTDOWNNOW (0x8000)
#define AFPSERVERCRASH (0x4000)
#define AFPSERVERMESG (0x2000)
#define AFPDONTRECONN (0x1000)

export int statflg = FALSE;	/* default is not to show stats */
export u_char *srvrname = NULL;	/* NBP registered name */
export u_char *srvrtype = (u_char *)AFSTYPE;	/* NBP registered type */
export char *messagefile = NULL; /* AFP2.1 GetSrvrMsg srvr msg filename */
export char *motdfile = NULL; /* AFP2.1 GetSrvrMsg login msg filename */
export char *dsiTCPIPFilter = NULL; /* AFP2.2 AppleShareIP address filter */
export u_int asip_addr = INADDR_ANY; /* AFP2.2 AppleShare over TCP/IP */
export u_short asip_port = ASIP_PORT; /* AFP2.2 AppleShare TCP/IP port */
export int asip_enable = FALSE; /* AFP2.2 AppleShare TCP/IP default off */

private char *sysvolfile = NULL; /* system afpvols file */
private char *passwdlookaside = NULL; /* local password file??? */
private char *guestid = NULL;	/* any guest ids? */
private char *coredir = NULL;	/* place to put coredumps */

#ifdef DEBUG_AFP
private char *debugAFPFile = NULL;	/* AFP debug file name */
#endif DEBUG_AFP
#ifdef ISO_TRANSLATE
private u_char isoSrvrName[80];
private u_char isoSrvrType[80];
#endif ISO_TRANSLATE
#ifdef REREAD_AFPVOLS
private int readafpvols = FALSE;/* set by USR1 signal */
#endif REREAD_AFPVOLS
#ifdef LWSRV_AUFS_SECURITY
export char *userlogindir = NULL;	/* budd -- name of userlogin file */
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
export char *enforcelist = NULL;/* name of list for appln run control */
#endif APPLICATION_MANAGER
#ifdef USR_FILE_TYPES
export char *uftfilename = NULL;/* file with suffix -> creat&type mappings */
#endif USR_FILE_TYPES
#ifdef LOGIN_AUTH_PROG
export char *login_auth_prog = NULL; /* path to external login auth. prog */
#endif LOGIN_AUTH_PROG
export int mcs, qs;		/* maxcmdsize and quantum size */
export int sqs = atpMaxNum;	/* maximum send quantum */
export int n_rrpkts = atpMaxNum; /* maximum send quantum to allow remote */
				/* (used in spwrtcontinue) */
export int nousrvol = FALSE;	/* no user home dir/vol flag */
export int nopwdsave = FALSE;	/* allow user to save password */
#ifdef AUFS_README
export char *aufsreadme;	/* path of readme file */
export char *aufsreadmename;	/* pointer into aufsreadme with just name */
#endif AUFS_README

#ifdef DEBUG_AFP
export FILE *dbg = NULL;
#endif /* DEBUG_AFP */

private EntityName srvr_entity_name; /* our entity name */
private char zonetorun[34];	/* zone to run in if different than std. */
private char logfile[MAXPATHLEN]; /* log file name if any */
#ifdef	PID_FILE
private char pid_file[MAXPATHLEN]; /* pid file name if any */
#endif	/* PID_FILE */
private int parent_pid;		/* pid of main server */
private int mypid;		/* pid of running process */
private int nomoresessions = FALSE; /* set true of out of asp sessions */
private int sesscount = 0;	/* number of asp sessions active */
private int maxsess = 10;
private int cno = -1;		/* current connection */
#ifndef NOSHUTDOWNCODE
private int minutes_to_shutdown = -1;
#endif NOSHUTDOWNCODE;
#ifdef AUFS_IDLE_TIMEOUT
private int idletime = 0;	/* allowable no traffic period */
private int timeidle = 0;	/* have been idle for this time */
private int guestonly = 1;	/* default to guest idle timeouts only */
#endif AUFS_IDLE_TIMEOUT

/*
 * CNO TO PID handling
 *
 * Must keep a table to session reference numbers (aka connection
 * numbers) to pid mappings for the server since sessions are
 * "half"-closed.  This handling was previously in the asp protocol
 * handler, but this doesn't make sense and there is much better
 * control over things here
 *
 * internal strategy at this point is to record fork terminations
 * and handling via garbage collector at a later point.  maximum time
 * to scan is around 5 seconds at this point.  to minimize scan time
 * for gc, the "dead" ones are pushed onto a stack.
 *
 * an alternative strategy is to have the signal handler write to an
 * internal pipe watched by a fdlistener.  if this is done the sleeps
 * can be made much longer.  carefully handled this will work very
 * well, but it is a little ugly.  (note: best way to do is probably
 * to have the listener set a variable and the inferior termination
 * handler write the pids out.  the scan process would then pick up
 * the pids from the pipe.)
 *
*/

typedef struct cno_to_pid {
  int state;			/* back reference */
#define CP_NOTINUSE 0x1		/* to mark not in use */
#define CP_RUNNING 0x2		/* inferior is running? */
#define CP_TIMEDOUT 0x4		/* tickle timeout on inferior */
  int pid;			/* recorded pid */
  long timeval;			/* time when pid died */
  WSTATUS status;
#ifdef DORUSAGE
  struct rusage rusage;
#endif DORUSAGE
} CTP, *CTPP;

private struct cno_to_pid *ctp_tab;
private int *ctp_stack;
private int ctp_stack_cnt;

usage(name)
char *name;
{
  char *DBLevelOpts();

  fprintf(stderr,"usage: %s [-d cap-flags] [-a afp-levels] ",name);
#ifdef LWSRV_AUFS_SECURITY
  fprintf(stderr,"[-n name] [-t packets] [-s] [-V afpvols] [-X usrfile]\n");
#else LWSRV_AUFS_SECURITY
  fprintf(stderr,"[-n name] [-t packets] [-s] [-V afpvols]\n");
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
  fprintf(stderr,"[-A applistfile] ");
#endif APPLICATION_MANAGER
#ifdef AUFS_README
  fprintf(stderr,"[-r readme_path] ");
#endif AUFS_README
#ifdef AUFS_IDLE_TIMEOUT
  fprintf(stderr,"[-[i|I] idle_timeout] ");
#endif AUFS_IDLE_TIMEOUT
#ifdef USR_FILE_TYPES
  fprintf(stderr,"[-F typelist] ");
#endif USR_FILE_TYPES
#ifdef LOGIN_AUTH_PROG
  fprintf(stderr,"[-L authprog] ");
#endif LOGIN_AUTH_PROG
  fprintf(stderr,"[-m login_message_file] [-M srvr_message_file] ");
  fprintf(stderr,"[-p] [-u] ");
  fprintf(stderr,"\n\t-d for CAP debugging flags:\n");
  fprintf(stderr,"\t   l = lap, d = ddp, a = atp, n = nbp, p = pap,");
  fprintf(stderr,"i = ini, s = asp\n");
  fprintf(stderr,"\t-Ds[shct] for ASP debug limiting flags:\n");
  fprintf(stderr,"\t   s = socket, h = handlers, c = call, t = tickle\n");
  fprintf(stderr,"\t-a for AFP debugging by level (or setenv AUFSDEBUG):\n");
  fprintf(stderr,"\t   %s\n",DBLevelOpts());
  fprintf(stderr,"\t-t for packet traces (or setenv AUFSTRACE):\n");
  fprintf(stderr,"\t   [I|O|B]CmdName\n");
  fprintf(stderr,"\t-n for setting the server's name\n");
  fprintf(stderr,"\t-s for statistics\n");
  fprintf(stderr,"\t-u do not show user home directory or vols\n");
  fprintf(stderr,"\t-p do not allow AFP client to save password\n");
  fprintf(stderr,"\t-V VolsFile for server wide afp volumes\n");
  fprintf(stderr,"\t-G <username> to set guest id for <anonymous> logins\n");
  fprintf(stderr,"\t-P LookAsidePasswordFile for scrambled transactions\n");
  fprintf(stderr,"\t-T enable AFP connections via TCP/IP (default addr)\n");
  fprintf(stderr,"\t-B <addr[:port]> enable AFP over TCP/IP & set address\n");
  fprintf(stderr,"\t-f <filterfile> set the AFP over TCP-IP address filter\n");
  fprintf(stderr,"\t-U <number> to allow <number> sessions\n");
  fprintf(stderr,"\t-m|M <file> specifies login or server message file\n");
#ifndef STAT_CACHE
  fprintf(stderr,"\t-c directory to specify a directory to coredump to\n");
#endif STAT_CACHE
  fprintf(stderr,"\t-l file to send logfile to other than <servername>.log\n");
#ifdef PID_FILE
  fprintf(stderr,"\t-w file to write pid to other than ./%s\n", PID_FILE);
#endif /* PID_FILE */
  fprintf(stderr,"\t-S <n> limit responses to <n> packets\n");
  fprintf(stderr,"\t-R <n> limit remote to sending <n> packets\n");
#ifdef LWSRV_AUFS_SECURITY
  fprintf(stderr,"\t-X datafile of logged in users\n"); /* budd */
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
  fprintf(stderr,"\t-A <file> application run manager\n");
#endif APPLICATION_MANAGER
#ifdef AUFS_README
  fprintf(stderr,"\t-r readme file for new users\n");
#endif AUFS_README
#ifdef USR_FILE_TYPES
  fprintf(stderr,"\t-F <file> to map from suffix to creator/translation\n");
#endif USR_FILE_TYPES
#ifdef LOGIN_AUTH_PROG
  fprintf(stderr,"\t-L <prog> to provide login authorization service\n");
#endif LOGIN_AUTH_PROG
#ifdef DEBUG_AFP
  fprintf(stderr, "\t-Z <file> to dump parameters from AFP commands\n");
#endif DEBUG_AFP
  fprintf(stderr,"\nExample: %s -t 'bdelete irename' -a 'file fork dir' -s\n",
	  name);
  exit(1);
}

/*
 * generate default name from host name
 *
*/
char *
afs_default_name()
{
  char hostname[255];
  static char afsname[255];
  char *ap;

  gethostname(hostname,(int) sizeof(hostname));
  if ((ap = index(hostname,'.')) != NULL)
    *ap = '\0';				/* remove trailing domain name */
  sprintf(afsname,"%s Aufs",hostname);
  return(afsname);
}

doargs(argc,argv)
int argc;
char **argv;
{
  int c;
  char *p;
  u_char *parsename();
  extern char *optarg;
  extern int optind;
  extern boolean dochecksum;
  static char optlist[100] = "a:B:d:f:D:n:N:t:kpsTuV:U:G:P:c:l:z:S:R:M:m:";
#ifdef ISO_TRANSLATE
  void cISO2Mac();
#endif ISO_TRANSLATE
  
#ifdef LWSRV_AUFS_SECURITY
  strcat(optlist, "X:");
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
  strcat(optlist, "A:");
#endif APPLICATION_MANAGER
#ifdef AUFS_README
  strcat(optlist, "r:");
#endif AUFS_README
#ifdef AUFS_IDLE_TIMEOUT
  strcat(optlist, "i:I:");
#endif AUFS_IDLE_TIMEOUT
#ifdef USR_FILE_TYPES
  strcat(optlist, "F:");
#endif USR_FILE_TYPES
#ifdef LOGIN_AUTH_PROG
  strcat(optlist, "L:");
#endif LOGIN_AUTH_PROG
#ifdef PID_FILE
  pid_file[0] = '\0';
  strcat(optlist, "w:");
#endif /* PID_FILE */
#ifdef	DEBUG_AFP
  strcat(optlist, "Z:");
#endif	/* DEBUG_AFP */

  while ((c = getopt(argc,argv,optlist)) != EOF) {
    switch (c) {
    case 'z':
      strncpy(zonetorun, optarg, 32);
      break;
    case 'l':
      strncpy(logfile, optarg, sizeof(logfile)-1);
      break;
    case 'c':
      coredir = optarg;
      break;
    case 'k':
      dochecksum = 0;			/* no DDP checksum */
      break;
    case 'p':
      nopwdsave = TRUE;			/* don't allow save password */
      break;
    case 's':
      statflg = TRUE;
      break;
    case 'u':
      nousrvol = TRUE;			/* don't show home dir */
      break;
    case 'd':
      dbugarg(optarg);			/* '-d' is debug */
      break;
    case 'D':
      if (optarg[0] != 's')
	usage(argv[0]);
      aspdebug(optarg+1);		/* call asp debug */
      break;
    case 'a':
      if (!SetDBLevel(optarg))		/* -a for afp debug */
	usage(argv[0]);
      break;
    case 'm':
      motdfile = optarg;		/* -m message file path */
      break;
    case 'M':
      messagefile = optarg;		/* -M message file path */
      break;
    case 'n':
    case 'N':
      srvrname = (u_char *)optarg;	/* '-n' to set server name */
      srvrtype = parsename(srvrname);	/* (optional "-n name:type") */
#ifdef ISO_TRANSLATE
      bcopy(srvrname, isoSrvrName, strlen(srvrname)+1);
      cISO2Mac(srvrname);
      bcopy(srvrtype, isoSrvrType, strlen(srvrtype)+1);
      cISO2Mac(srvrtype);
#endif ISO_TRANSLATE
      break;
    case 't':
      if (!SetPktTrace(optarg))
	usage(argv[0]);
      break;
    case 'f':				/* AppleShareIP IP address filter */
      dsiTCPIPFilter = optarg;
      break;
    case 'B':				/* Bind AppleShare TCP/IP address */
      if ((p = (char *)index(optarg, ':')) != NULL) {
	asip_port = atoi(p+1);
	*p = '\0';
      }
      if ((asip_addr = (u_int)ntohl(inet_addr(optarg))) == -1)
	asip_addr = INADDR_ANY;
      if (p != NULL)
	*p = ':';
      /* fall through */
    case 'T':				/* Enable AppleShare TCP/IP */
      asip_enable = TRUE;
      break;
    case 'V':				/* system afpvols file */
      sysvolfile = optarg;
      break;
    case 'U':
      maxsess = atoi(optarg);
      break;
    case 'P':
      passwdlookaside = optarg;
      break;
    case 'G':
      guestid = optarg;
      break;
    case 'S':
      sqs = atoi(optarg);
      if (sqs <= 0) {
	fprintf(stderr, "Must have at least one packet in a response, resetting to one\n");
	sqs = 1;
      }
      if (sqs > atpMaxNum) {
	fprintf(stderr, "No more than %d packets allowed in a response\n",
		atpMaxNum);
	sqs = atpMaxNum;
      }
      break;
    case 'R':
      n_rrpkts = atoi(optarg);
      if (n_rrpkts <= 0) {
	fprintf(stderr, "Must have at least one packet in a response, resetting to one\n");
	n_rrpkts = 1;
      }
      if (n_rrpkts > atpMaxNum) {
	fprintf(stderr, "No more than %d packets allowed in a response\n",
		atpMaxNum);
	n_rrpkts = atpMaxNum;
      }
      break;
#ifdef LWSRV_AUFS_SECURITY
    case 'X':				/* budd... */
      userlogindir = optarg;
      break;				/* ...budd */
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
    case 'A':
      enforcelist = optarg;
      break;
#endif APPLICATION_MANAGER
#ifdef AUFS_README
    case 'r':
      aufsreadme = optarg;
      if (*aufsreadme != '/') {
	fprintf(stderr, "The -r parameter must be a full path name\n");
	exit(1);
      }
      aufsreadmename = (char *)rindex(aufsreadme, '/') + 1;
      break;
#endif AUFS_README
#ifdef AUFS_IDLE_TIMEOUT
    case 'I': /* also users */
      guestonly = 0;
      /* fall thro' */
    case 'i': /* guests only */
      idletime = atoi(optarg) * 6;
      break;
#endif AUFS_IDLE_TIMEOUT
#ifdef USR_FILE_TYPES
    case 'F': /* file suffix mapping */
      uftfilename = optarg;
      break;
#endif USR_FILE_TYPES
#ifdef LOGIN_AUTH_PROG
    case 'L': /* login authorization */
      login_auth_prog = optarg;
      if (*login_auth_prog != '/') {
	fprintf(stderr, "The -L parameter must be a full path name\n");
	exit(1);
      }
      break;
#endif LOGIN_AUTH_PROG
#ifdef PID_FILE
    case 'w': /* pid file */
      strncpy(pid_file, optarg, sizeof(pid_file)-1);
      break;
#endif /* PID_FILE */
#ifdef	DEBUG_AFP
    case 'Z': /* command debug file */
      debugAFPFile = optarg;
      break;
#endif	/* DEBUG_AFP */
    default:
      usage(argv[0]);
      break;
    }
  }
}

/*
 * check name for optional ':' delimited type
 *
 */

u_char *
parsename(name)
u_char *name;
{
  u_char *cp;

  if ((cp = (u_char *)index((char *)name, ':')) != NULL) {
    *cp++ = '\0'; /* NULL terminate name */
    return(cp);
  }
  return((u_char *)AFSTYPE);
}

export AddrBlock addr;			/* budd */

main(argc,argv)
int argc;
char **argv;
{
  int timedout();
  void inferiordone(), dienow(), dying();
#ifdef REREAD_AFPVOLS
  void setreadafpvols();
  void dorereadafpvols();
#endif REREAD_AFPVOLS
#ifdef CLOSE_LOG_SIG
  void closelog();
#endif /* CLOSE_LOG_SIG */
#ifndef NOSHUTDOWNCODE
  void killnow(), killin5(), diein5();
  void msgnotify(), msgavail();
#endif NOSHUTDOWNCODE;
  int err,atpskt,slsref;
  int comp,comp2;
  byte *srvinfo;
  int srvinfolen;
  char *getenv(),*env;
  int pid;
  int mask;
  byte *buf;			/* [atpMaxData]; */
  byte *rspbuf;			/* [atpMaxData*atpMaxNum]; */
  import byte *aufsicon;	/* aufs icon */
  import int aufsiconsize;	/* and its size */
  import char *aufs_versiondate;
  import int aufs_version[];
  extern int errno;
#ifdef ISO_TRANSLATE
  void cISO2Mac();
#endif ISO_TRANSLATE
  
#ifdef DIGITAL_UNIX_SECURITY
  set_auth_parameters(argc, argv);
#endif DIGITAL_UNIX_SECURITY

  OSEnable();			/* enable OS dependent items */

  IniServer();
  srvrname = (u_char *)afs_default_name();	/* set default server name */
#ifdef ISO_TRANSLATE
  bcopy(srvrname, isoSrvrName, strlen(srvrname)+1);
  cISO2Mac(srvrname);
  bcopy(AFSTYPE,  isoSrvrType, strlen(AFSTYPE)+1);
#endif ISO_TRANSLATE
  logfile[0] = '\0';
  zonetorun[0] = '\0';
  doargs(argc,argv);			/* handle command line */
#ifdef AUTHENTICATE
#ifdef ISO_TRANSLATE
  initauthenticate(*argv, (char *)isoSrvrName);
#else  ISO_TRANSLATE
  initauthenticate(*argv, (char *)srvrname);
#endif ISO_TRANSLATE
#endif AUTHENTICATE

  env = getenv("AUFSTRACE");		/* See if user wants to */
  if (env != NULL)			/*  trace some packets */
    SetPktTrace(env);
  env = getenv("AUFSDEBUG");
  if (env != NULL)
    SetDBLevel(env);

  if (!DBDEB) {
#ifdef ISO_TRANSLATE
    fprintf(stderr,"Apple Unix File Server (%s:%s@*) starting\n",
	    (char *)isoSrvrName, (char *)isoSrvrType);
#else  ISO_TRANSLATE
    fprintf(stderr,"Apple Unix File Server (%s:%s@*) starting\n",
	    (char *)srvrname, (char *)srvrtype);
#endif ISO_TRANSLATE
  }

  /* here's the place to fork off */
  if (!DBDEB) {
    if (fork())
      _exit(0);
    {
      int f;
      for (f = 0; f < 10; f++)
	(void) close(f);
    }
    (void) open("/", 0);
#ifndef NODUP2
    (void) dup2(0, 1);
    (void) dup2(0, 2);
#else  NODUP2
    (void)dup(0);		/* for slot 1 */
    (void)dup(0);		/* for slot 2 */
#endif NODUP2
#ifndef POSIX
#ifdef TIOCNOTTY
    {
      int t;
      t = open("/dev/tty", 2);	
      if (t >= 0) {
	ioctl(t, TIOCNOTTY, (char *)0);
	(void) close(t);
      }
    }
#endif TIOCNOTTY
#ifdef linux
    (void) setsid();
#endif linux
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
  
#ifdef DEBUG_AFP
  if (debugAFPFile != NULL)
    if ((dbg = fopen(debugAFPFile, "w")) != NULL)
      fprintf(dbg, "AUFS Debug Session\n");
#endif /* DEBUG_AFP */

#ifdef FIXED_DIRIDS
  InitDID();				/* init directory stuff  */
#endif FIXED_DIRIDS

  mypid = parent_pid = getpid();	/* remember who we are */

#ifdef	PID_FILE
  { FILE *pidfd;
    if (pid_file[0] == '\0')
      strncpy(pid_file, PID_FILE, sizeof(pid_file)-1);
    if( (pidfd = fopen(pid_file, "w")) == NULL ) {
      logit(0,"Can't open pid file %s", pid_file);
      exit(-1);
    }
    fprintf(pidfd, "%d\n", mypid);
    fclose(pidfd);
  }
#endif /* PID_FILE */

  if (logfile[0] == '\0') {
#ifdef ISO_TRANSLATE
    sprintf(logfile, "%s.log", (char *)isoSrvrName);
#else  ISO_TRANSLATE
    sprintf(logfile, "%s.log", (char *)srvrname);
#endif ISO_TRANSLATE
  }
  logitfileis(logfile, "a+");
  logit(0,"****************************************");
#ifdef ISO_TRANSLATE
  logit(0,"Apple Unix File Server (%s:%s@*) starting",
    (char *)isoSrvrName, (char *)isoSrvrType);
#else  ISO_TRANSLATE
  logit(0,"Apple Unix File Server (%s:%s@*) starting",
    (char *)srvrname, (char *)srvrtype);
#endif ISO_TRANSLATE
  logit(0,"Aufs version: %d(%d) as of %s",aufs_version[0], aufs_version[1],
      aufs_versiondate);
  if (sysvolfile != NULL) {		/* if known system vols file */
    FILE *fd = fopen(sysvolfile, "r");

    if (fd == NULL)
      logit(0,"Aufs: no such system volumes file %s", sysvolfile);
    else if (!VRdVFile(fd))	/* then try to read it now */
      logit(0,"Aufs: system volumes file %s bad",sysvolfile);
  }
  if (zonetorun[0] != '\0')
    zoneset(zonetorun);
  abInit(TRUE);				/* initialize applebus */
  nbpInit();				/* initialize nbp */
  maxsess = aspInit(maxsess);	/* init asp */
  logit(0,"%d sessions allowed",maxsess);
  if ((ctp_tab = (CTPP)malloc(sizeof(struct cno_to_pid)*maxsess)) == NULL) {
    logit(0,"couldn't malloc table for pid recording, fatal!");
  }
  if ((ctp_stack = (int *)malloc(sizeof(int)*maxsess)) == NULL) {
    logit(0,"couldn't malloc stack for pid recording, fatal!");
  }
  if (asip_enable)
    if (asip_addr != INADDR_ANY)
      logit(0,"AFP over TCP/IP enabled (IP %08x Port %d)",asip_addr,asip_port);
    else
      logit(0,"AFP over TCP/IP enabled (IP INADDR_ANY Port %d)", asip_port);
#ifdef LWSRV_AUFS_SECURITY
  if (userlogindir != NULL) {			/* budd... */
      logit(0,"Aufs: user login database in %s", userlogindir);
  } /* ...budd */
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
  if (enforcelist != NULL) {
    logit(4,"aufs: application manager database in %s", enforcelist);
    readAppList(enforcelist);
  }
#endif APPLICATION_MANAGER
#ifdef AUFS_IDLE_TIMEOUT
  if (idletime)
    logit(0, "Idle timeout set to %d minutes for %s sessions",
      idletime/6, (guestonly) ? "guest" : "all");
#endif AUFS_IDLE_TIMEOUT
#ifdef LOGIN_AUTH_PROG
  if (login_auth_prog)
    logit(0, "Login authorization program: %s\n", login_auth_prog);
#endif LOGIN_AUTH_PROG
  ctp_stack_cnt = 0;
  { int i;
    for (i = 0 ; i < maxsess; i++) {
      ctp_tab[i].state = CP_NOTINUSE;
      ctp_tab[i].pid = -1;
    }
  }
  
  tellaboutos();		/* tell about os stuff */

  if (sqs < atpMaxNum)
    logit(0,"maximum of %d packet%s will be sent on a response", sqs,
	sqs > 1 ? "s" : "");
  sqs *= atpMaxData;
  if (n_rrpkts < atpMaxNum)
    logit(0,"remote limited to %d packet%s in a response", n_rrpkts,
	n_rrpkts > 1 ? "s" : "");
  dsiGetParms(&mcs, &qs);
  if (DBDEB)
    printf("Command buffer size is %d, Quantum size is %d\n", mcs, qs);
  buf = (byte *)malloc(mcs);
  rspbuf = (byte *)malloc(qs);
  if (buf == NULL || rspbuf == NULL) {
    logit(0,"memory allocation failure!\n");
    exit(999);
  }

  addr.net = addr.node = addr.skt = 0;	/* use any */
  atpskt = 0;			/* make sure we use dynamic skt */
  err = ATPOpenSocket(&addr,&atpskt);
  if (err != noErr) {
    logit(0,"ATPOpenSocket failed with error %d\n",err);
    exit(0);
  }
  
  GetMyAddr(&addr);	/* set net and node */
  addr.skt = atpskt;

  if (SrvrRegister(atpskt,srvrname,srvrtype,"*", &srvr_entity_name) != noErr) {
    logit(0,"SrvrRegister for %s:%s failed...", srvrname, srvrtype); 
    exit(2);
  }

  /*
   * set available User Authentication Methods
   *
   */
  if (guestid != NULL)
    allowguestid(guestid);
  allowcleartext();
#ifdef DISTRIB_PASSWDS
  allow2wayrand(passwdlookaside);
#else  /* DISTRIB_PASSWDS */
  if (passwdlookaside != NULL) 
    allowrandnum(passwdlookaside);
#endif /* DISTRIB_PASSWDS */

#ifndef STAT_CACHE
  if (coredir)
    if (chdir(coredir) < 0) {
      perror("chdir for coredumps");
      logit(0,"chdir to %s for coredumps failed",coredir);
    } else
      logit(0,"***CORE DUMPS WILL BE IN %s",coredir);
#endif STAT_CACHE
    
  /* Get server info once and stash away*/
  srvinfolen = GetSrvrInfo(rspbuf,srvrname,aufsicon,aufsiconsize);
  srvinfo = (byte *) malloc(srvinfolen);
  bcopy(rspbuf,srvinfo,srvinfolen);

  if (DBSRV)
    PrtSrvrInfo(srvinfo,srvinfolen);
  
  /* Init asp */
  err = dsiInit(&addr,srvinfo,srvinfolen,&slsref);
  if (err != noErr) {
    logit(0,"dsiInit failed with code %d, fatal",err);
    exit(0);
  }

  logit(0,"Aufs Starting (%s)",srvrname);
  if (sysvolfile)
    logit(0,"System vols in '%s'",sysvolfile);
  logit(0,"dsiInit Completed.  Waiting for connection...");

#ifndef NOSHUTDOWNCODE
# ifndef NOPGRP
  setpgrp(0, parent_pid);	/* set our process group */
				/* (inherited) */
# endif NOPGRP
#endif NOSHUTDOWNCODE;

#ifndef DEBUGFORK
  if (!DBDEB)
#endif
    signal(SIGCHLD, inferiordone);
#ifndef NOSHUTDOWNCODE
  signal(SIGHUP, killnow);	/* kill -HUP -- Immediate shutdown */
  signal(SIGTERM, killin5);	/* kill -TERM -- Timed (5 min) shutdown */
  signal(SIGURG, msgnotify);	/* kill -URG -- A srvr message is available */
#endif NOSHUTDOWNCODE;
#ifdef REREAD_AFPVOLS
  signal(SIGUSR1, setreadafpvols); /* kill -USR1 -- force afpvols re-read */
#endif REREAD_AFPVOLS
#ifdef CLOSE_LOG_SIG
  signal(CLOSE_LOG_SIG, closelog); /* kill -USR2 -- force close/reopen log */
#endif /* CLOSE_LOG_SIG */

  do {
    pid = -1;			/* make sure zero at start */
    dsiGetSession(slsref,&cno,&comp);
    if (comp > 0)
      logit(0,"Waiting for session %d to activate", cno);
    /* won't wait if we set comp above */
    while (comp > 0) {
      abSleep(sectotick(5),TRUE);
#ifdef REREAD_AFPVOLS
      if (readafpvols == TRUE)
	dorereadafpvols();
#endif REREAD_AFPVOLS
      gcinferior();
    }
    if (comp == NoMoreSessions) {
      nomoresessions = TRUE;
      logit(0,"Woops, no more sessions");
      while (nomoresessions) {
	gcinferior();
	abSleep(sectotick(5), TRUE);
      }
      logit(0,"Okay, sessions should be available");
      continue;
    }
    if (comp != noErr) {
      logit(0,"GetSession returned %d on server socket %d",comp,slsref);
      continue;
    } else {
      sesscount++;
      logit(0,"New session %d started on server socket %d, count %d",
	  cno,slsref,sesscount);
      if ((err = dsiGetNetworkInfo(cno, &addr)) != noErr) {
	if (err > 0) { /* AppleShareIP session */
	logit(0,"Session %d from [IP addr %d.%d.%d.%d, port %d]",
	    cno, addr.net>>8, addr.net&0xff, addr.node, addr.skt, err);
	  err = noErr;
	} else
	  logit(0,"Get Network info failed with error %d", err);
      } else {
#ifdef AUTHENTICATE
	err = (authenticate(ntohs(addr.net), addr.node)) ? noErr : ~noErr;
	logit(0,"%session %d from [Network %d.%d, node %d, socket %d]",
	    (err == noErr) ? "S" : "Authentication failed, s",
#else AUTHENTICATE
	logit(0,"Session %d from [Network %d.%d, node %d, socket %d]",
#endif AUTHENTICATE
	    cno,
	    nkipnetnumber(addr.net), nkipsubnetnumber(addr.net),
	    addr.node, addr.skt);
      }
#ifdef AUTHENTICATE
      if(err != noErr) {
	dsiCloseSession(cno, 1, 1, &comp2);
	continue;
      }
#endif AUTHENTICATE
    }
#ifndef DEBUGFORK
    if (!DBDEB) {
#endif
#ifdef NOSIGMASK
      mask = 0;
      sighold(SIGCHLD);
      sighold(SIGHUP);
# ifndef NOSHUTDOWNCODE
      sighold(SIGTERM);
      sighold(SIGURG);
# endif NOSHUTDOWNCODE;
#else NOSIGMASK
# ifndef NOSHUTDOWNCODE
      mask = sigblock(sigmask(SIGCHLD)|sigmask(SIGHUP)
	|sigmask(SIGTERM)|sigmask(SIGURG));
# else NOSHUTDOWNCODE;
      mask = sigblock(sigmask(SIGCHLD)|sigmask(SIGHUP));
# endif NOSHUTDOWNCODE;
#endif NOSIGMASK
      /* fork on connection - only tickle from parent */
      if ((pid = dsiFork(cno, TRUE, FALSE)) < 0) {
	logit(0,"dsiFork failed on session %d, last system error %d",cno,errno);
	/* try to close, but don't worry too much */
	dsiCloseSession(cno, 1, 1, &comp2);
#ifdef NOSIGMASK
	sigrelse(SIGCHLD);
	sigrelse(SIGHUP);
# ifndef NOSHUTDOWNCODE
	sigrelse(SIGTERM);
	sigrelse(SIGURG);
# endif NOSHUTDOWNCODE;
#else NOSIGMASK
	sigsetmask(mask);
#endif NOSIGMASK
	continue;
      }
      dsiTickleUserRoutine(cno, timedout, pid);
      if (pid) {
	logit(0,"pid %d starting for session %d",pid, cno);
	addinferior(cno, pid);	/* addinferior scans (phew) */
      } else {
#ifndef NOSHUTDOWNCODE
	alarm(0);		/* make sure off */
#endif NOSHUTDOWNCODE;
	nbpShutdown();		/* don't need this in inferior */
	signal(SIGCHLD, SIG_DFL);
#ifndef NOSHUTDOWNCODE
	signal(SIGTERM, diein5); /* die in 5 minutes */
	signal(SIGURG, msgavail); /* message is available */
#endif NOSHUTDOWNCODE;
	signal(SIGHUP, dienow);	/* superior signaled us to shutdown */
	mypid = getpid();
#ifndef NOSHUTDOWNCODE
#ifdef REREAD_AFPVOLS
	signal(SIGUSR1, SIG_DFL);/* ignore in inferior */
#endif REREAD_AFPVOLS
# ifndef NOPGRP
	setpgrp(0, parent_pid);	/* set our process group */
# endif NOPGRP
	dying();		/* are we dying? if so, handle it*/
#endif NOSHUTDOWNCODE;
      }
#ifdef NOSIGMASK
	sigrelse(SIGCHLD);
	sigrelse(SIGHUP);
# ifndef NOSHUTDOWNCODE
	sigrelse(SIGTERM);
	sigrelse(SIGURG);
# endif NOSHUTDOWNCODE;
#else NOSIGMASK
	sigsetmask(mask);
#endif NOSIGMASK
#ifndef DEBUGFORK
    } else pid = 0;
#endif
  } while (pid != 0);		/* pid = 0 implies inferior process */

  if (pid == 0)
    inferior_handler(buf, rspbuf);

  if (statflg)
    SrvrPrintStats();			/* print stats */
  if (mypid == parent_pid)
    if ((err = SrvrShutdown(&srvr_entity_name)) != noErr)
      logit(0,"NBPRemove failed: code %d\n", err);
  exit(0);
}

#ifndef TREL_TIMEOUT
inferior_handler(buf, rspbuf)
byte *buf;
byte *rspbuf;
#else TREL_TIMEOUT
inferior_handler(buf1, rspbuf1)
byte *buf1;
byte *rspbuf1;
#endif TREL_TIMEOUT
{
  int mask;
  OSErr err;
#ifndef TREL_TIMEOUT
  int comp;
  int type,rlen,rsplen;
  ReqRefNumType reqref;
#else TREL_TIMEOUT
  int comp1,comp2;
  int type1,rlen1,rsplen1;
  int type2,rlen2,rsplen2;
  ReqRefNumType reqref1;
  ReqRefNumType reqref2;
  byte * buf2;
  byte * rspbuf2;
#endif TREL_TIMEOUT

#ifdef TREL_TIMEOUT
  buf2 = (byte *)malloc(mcs);
  rspbuf2 = (byte *)malloc(qs);
  if (buf2 == NULL || rspbuf2 == NULL) {
    logit(0,"memory allocation failure!\n");
    exit(999);
  }

  comp1 = 0;
  comp2 = 0;
#endif TREL_TIMEOUT
  umask(0);			/* file creates have explict modes */
  for (;;) {
#ifndef TREL_TIMEOUT
    dsiGetRequest(cno,buf,mcs,&reqref,&type,&rlen,&comp);
    while (comp > 0) {
      abSleep(sectotick(60),TRUE);
#ifdef AUFS_IDLE_TIMEOUT
      if (idletime)
        (void) checkIdle(cno,buf,comp);
#endif AUFS_IDLE_TIMEOUT
    }
    if (comp == SessClosed || comp == ParamErr) {
#else TREL_TIMEOUT
    if (comp1 > 0 && comp2 > 0) {
	while (comp1 > 0 && comp2 > 0) {
           abSleep(sectotick(60),TRUE); 
	}
        if (DBSRV)
          printf("done\n");
    }
	    
   if (comp1 <= 0) {
    dsiGetRequest(cno,buf1,mcs,&reqref1,&type1,&rlen1,&comp1);
    while (comp1 > 0) {
      abSleep(sectotick(60),TRUE);
#ifdef AUFS_IDLE_TIMEOUT
      if (idletime)
        (void) checkIdle(cno,buf1,comp1);
#endif AUFS_IDLE_TIMEOUT
    }
    if (comp1 == SessClosed || comp1 == ParamErr) {
#endif TREL_TIMEOUT
      logit(0,"Session (%d) closed",cno);
#ifdef LWSRV_AUFS_SECURITY
      clearuserlogin();			/* budd */
#endif LWSRV_AUFS_SECURITY
      return;
    }
#ifndef TREL_TIMEOUT
    if (comp < 0) {
      logit(0,"dsiGetRequest failed %d",comp);
#else TREL_TIMEOUT
    if (comp1 < 0) {
      logit(0,"dsiGetRequest failed %d",comp1);
#endif TREL_TIMEOUT
      continue;
    }
#ifndef TREL_TIMEOUT
    if (rlen == 0)
#else TREL_TIMEOUT
    if (rlen1 == 0)
#endif TREL_TIMEOUT
      continue;
#ifndef NOSHUTDOWNCODE
    /* mask off potential race condition */
# ifdef NOSIGMASK
    sighold(SIGTERM);
    sighold(SIGURG);
    sighold(SIGALRM);
# else NOSIGMASK
    mask = sigblock(sigmask(SIGTERM)|sigmask(SIGURG)|sigmask(SIGALRM));
# endif NOSIGMASK
#endif NOSHUTDOWNCODE;
#ifndef TREL_TIMEOUT
    switch (type) {
#else TREL_TIMEOUT
    switch (type1) {
#endif TREL_TIMEOUT
    case aspWrite:
    case aspCommand:
#ifndef TREL_TIMEOUT
      err = SrvrDispatch(buf,rlen,rspbuf,&rsplen,cno,reqref);
#else TREL_TIMEOUT
      err = SrvrDispatch(buf1,rlen1,rspbuf1,&rsplen1,cno,reqref1);
#endif TREL_TIMEOUT
      if (DBSRV) {
#ifndef TREL_TIMEOUT
	printf("Sending reply len=%d err=%s ...\n",rsplen,afperr(err));
#else TREL_TIMEOUT
	printf("Sending reply len=%d err=%s ...\n",rsplen1,afperr(err));
#endif TREL_TIMEOUT
	fflush(stdout);			/* force out */
      }
#ifndef TREL_TIMEOUT
      if (type == aspWrite)
	dsiWrtReply(cno,reqref,(dword) err,rspbuf,rsplen,&comp);
#else TREL_TIMEOUT
      if (type1 == aspWrite)
	dsiWrtReply(cno,reqref1,(dword) err,rspbuf1,rsplen1,&comp1);
#endif TREL_TIMEOUT
      else
#ifndef TREL_TIMEOUT
	dsiCmdReply(cno,reqref,(dword) err,rspbuf,rsplen,&comp);
      while (comp > 0) {
	abSleep(sectotick(60),TRUE);
      }
      if (DBSRV)
	printf("done\n");
#else TREL_TIMEOUT
	dsiCmdReply(cno,reqref1,(dword) err,rspbuf1,rsplen1,&comp1);
#endif TREL_TIMEOUT
      break;
    case aspCloseSession:
      logit(0,"Closing ASP Session...");
#ifndef TREL_TIMEOUT
      dsiCloseSession(cno,10,3,&comp); /* 5 times, .75 seconds */
      while (comp > 0)
#else TREL_TIMEOUT
      dsiCloseSession(cno,10,3,&comp1); /* 5 times, .75 seconds */
      while (comp1 > 0)
#endif TREL_TIMEOUT
	abSleep(1, TRUE);
#ifndef NOSHUTDOWNCODE
#ifdef NOSIGMASK
      sigrelse(SIGCHLD);
      sigrelse(SIGHUP);
#else NOSIGMASK
      sigsetmask(mask);
#endif NOSIGMASK
#endif NOSHUTDOWNCODE;
      return;
    default:
#ifndef TREL_TIMEOUT
      logit(0,"Unknown asp command type = %d",type);
#else TREL_TIMEOUT
      logit(0,"Unknown asp command type = %d",type1);
#endif TREL_TIMEOUT
      break;
    }
#ifndef NOSHUTDOWNCODE
#ifdef NOSIGMASK
    sigrelse(SIGCHLD);
    sigrelse(SIGHUP);
#else NOSIGMASK
    sigsetmask(mask);
#endif NOSIGMASK
#endif NOSHUTDOWNCODE;
#ifdef TREL_TIMEOUT
   } else {  /* comp2 */
    dsiGetRequest(cno,buf2,mcs,&reqref2,&type2,&rlen2,&comp2);
    while (comp2 > 0) {
      abSleep(sectotick(60),TRUE);
#ifdef AUFS_IDLE_TIMEOUT
      if (idletime)
        (void) checkIdle(cno,buf2,comp2);
#endif AUFS_IDLE_TIMEOUT
    }
    if (comp2 == SessClosed || comp2 == ParamErr) {
      logit(0,"Session (%d) closed",cno);
#ifdef LWSRV_AUFS_SECURITY
      clearuserlogin();			/* budd */
#endif LWSRV_AUFS_SECURITY
      return;
    }
    if (comp2 < 0) {
      logit(0,"dsiGetRequest failed %d",comp2);
      continue;
    }
    if (rlen2 == 0)
      continue;
#ifndef NOSHUTDOWNCODE
    /* mask off potential race condition */
# ifdef NOSIGMASK
    sighold(SIGTERM);
    sighold(SIGURG);
    sighold(SIGALRM);
# else NOSIGMASK
    mask = sigblock(sigmask(SIGTERM)|sigmask(SIGURG)|sigmask(SIGALRM));
# endif NOSIGMASK
#endif NOSHUTDOWNCODE;
    switch (type2) {
    case aspWrite:
    case aspCommand:
      err = SrvrDispatch(buf2,rlen2,rspbuf2,&rsplen2,cno,reqref2);
      if (DBSRV) {
	printf("Sending reply len=%d err=%s ...\n",rsplen2,afperr(err));
	fflush(stdout);			/* force out */
      }
      if (type2 == aspWrite)
	dsiWrtReply(cno,reqref2,(dword) err,rspbuf2,rsplen2,&comp2);
      else
	dsiCmdReply(cno,reqref2,(dword) err,rspbuf2,rsplen2,&comp2);
      break;
    case aspCloseSession:
      logit(0,"Closing ASP Session...");
      dsiCloseSession(cno,10,3,&comp2); /* 5 times, .75 seconds */
      while (comp2 > 0)
	abSleep(1, TRUE);
#ifndef NOSHUTDOWNCODE
#ifdef NOSIGMASK
      sigrelse(SIGCHLD);
      sigrelse(SIGHUP);
#else NOSIGMASK
      sigsetmask(mask);
#endif NOSIGMASK
#endif NOSHUTDOWNCODE;
      return;
    default:
      logit(0,"Unknown asp command type = %d",type2);
      break;
    }
#ifndef NOSHUTDOWNCODE
#ifdef NOSIGMASK
    sigrelse(SIGCHLD);
    sigrelse(SIGHUP);
#else NOSIGMASK
    sigsetmask(mask);
#endif NOSIGMASK
#endif NOSHUTDOWNCODE;
   }
#endif TREL_TIMEOUT
  }
}

/*
 * Deals with inferior process termination - just close off the
 * "half closed" socket
 *
*/
void
inferiordone()
{
  WSTATUS status;
#ifdef DORUSAGE
  struct rusage rusage;
# define RUSAGEVAR &rusage
#else  DORUSAGE
# define RUSAGEVAR 0		/* may not be used, but.. */
#endif DORUSAGE
  int pid;
  int i;
  struct cno_to_pid *cp;

#ifndef NOWAIT3
# define DOWAIT while ((pid=wait3(&status, WNOHANG, RUSAGEVAR)) > 0) 
#else   NOWAIT3
# define DOWAIT  if ((pid=wait(&status)) > 0)
#endif  NOWAIT3

  DOWAIT {
    /* remember for later */
    for (i = 0, cp = ctp_tab; i < maxsess; i++, cp++) {
      if (cp->pid == pid && (cp->state & CP_NOTINUSE) == 0) {
	/* one of alive, dead or timedout */
	/* if alive, move to dead */
	/* if dead, shouldn't be here */
	/* if timedout, then leave state */
	if ((cp->state & CP_RUNNING) == 0)
	  logit(0,"Internal error: pid %d has died twice according to wait",pid);
	cp->state &= ~CP_RUNNING; /* not running anymore */
	(void)time(&cp->timeval); /* log time */
	bcopy(&status, &cp->status, sizeof(status)); /* copy status */
#ifdef DORUSAGE
	bcopy(&rusage, &cp->rusage, sizeof(rusage));
#endif DORUSAGE
	ctp_stack[ctp_stack_cnt++] = i;	/* mark cno */
	logit(0,"Recorded terminated inferior Aufs PID %d", pid);
	break;
      }
    }
    if (i == maxsess)
      logit(0,"Unknown terminating inferior pid %d ignored", pid);
  }
  signal(SIGCHLD, inferiordone);
}

/*
 * scan for dead inferiors and handle gracefully.  Know we are never
 * called from a "bad" context.  e.g. we won't be called while in a
 * "critical" section where data structures are being updated
 *
*/
gcinferior()
{
  char tmpbuf[30];		/* reasonable */
  int srn, comp;
  struct cno_to_pid *cp;
  int mask;

  if (ctp_stack_cnt <= 0) {	/* stack of died pids empty? */
    if (ctp_stack_cnt < 0)
      logit(0,"internal error: unsafe condition: ctp stack count less than zero");
    return;			/* yes, just return */
  }

#ifdef NOSIGMASK
  sighold(SIGCHLD);
#else NOSIGMASK
  mask = sigblock(sigmask(SIGCHLD));
#endif NOSIGMASK
  do {
    srn = ctp_stack[--ctp_stack_cnt]; /* get pid */
    cp = &ctp_tab[srn];		/* pointer to info on died pid */
    if (cp->state & CP_NOTINUSE) /* nothing to do */
      continue;
    /* fork termination has three cases: */
    /* [?,0177] - stopped */
    /* [?, 0] - exit status */
    /* [0, ?] - terminated due to signal */
    if (cp->state & CP_TIMEDOUT) {
      logit(0,"process %d, session %d was terminated due to a timeout",
	  cp->pid, srn);
    } else if (WIFSTOPPED(cp->status)) {
      logit(0,"process %d, session %d was suspended! gads what is happening?",
	  cp->pid, srn);
    } else if (WIFSIGNALED(cp->status)) {
      dsiAttention(srn, AFPSHUTDOWNNOW, 1, -1, &comp);
      while (comp > 0) {abSleep(30, TRUE);} /* ignore error */
      dsiCloseSession(srn, 3, 2, &comp); /* try 3 times every .5 seconds */
      while (comp > 0) { abSleep(30, TRUE); } /* close down if we can */
      logit(0,"process %d, session %d was terminated on signal %d",
	  cp->pid, srn, W_TERMSIG(cp->status));
      if (W_COREDUMP(cp->status)) {
	logit(0,"woops: we just dumped core on pid %d", cp->pid);
	sprintf(tmpbuf, "core%d",cp->pid);
	if (link("core", tmpbuf) == 0) { /* making copy */
	  if (unlink("core") != 0)
	    logit(0,"woops: couldn't unlink core for pid %d", cp->pid);
	  logit(0,"core dump for pid %d is in %s",cp->pid, tmpbuf);
	} else
	  logit(0,"core dump for pid %d is in core - plz be careful though!",
	      cp->pid);
      }
    } else {
      logit(0,"process %d, session %d terminated with exit code %d",
	  cp->pid, srn, W_RETCODE(cp->status));
    }
    dsiShutdown(srn);
    nomoresessions = FALSE;	/* if this was set, unset now */
#ifdef DORUSAGE
    logit(0,"%d messages out, %d in, CPU %.2f user %.2f system",
	cp->rusage.ru_msgsnd,cp->rusage.ru_msgrcv,
	((float)cp->rusage.ru_utime.tv_sec)+
	((float)cp->rusage.ru_utime.tv_usec)/1000000.0,
	((float)cp->rusage.ru_stime.tv_sec)+
	((float)cp->rusage.ru_stime.tv_usec)/1000000.0);
#endif DORUSAGE
    logit(0,"Process %d terminated", cp->pid);
    cp->state = CP_NOTINUSE;
    cp->pid = -1;		/* make -1 to speed up sigchld handler */
  } while (ctp_stack_cnt);
#ifdef NOSIGMASK
  sigrelse(SIGCHLD);
#else NOSIGMASK
  sigsetmask(mask);		/* restore mask */
#endif NOSIGMASK
}

/*
 *
 * record inferior pid for later handling
 *
 * must be called with sigchild and other signals,etc. that could
 * affect ctp_tab off. 
 * 
 *
*/
addinferior(cno, pid)
int cno;
int pid;
{
  struct cno_to_pid *cp;

  /* need to scan here in case something timed out */
  gcinferior();
  cp = &ctp_tab[cno];		/* get pointer to table entry */
  /* If was in table, just smash because we have a new pid on cno */
  /* should we scan table for duplicates? */
  cp->state = CP_RUNNING;
  cp->pid = pid;
}

/*
 * Connection timed because remote didn't tickle us enough
 * Kill inferior and shutdown half closed skt.
 *
 * Note: called from abSleep scheduler.
 *
*/
int
timedout(srn, pid)
int srn;
int pid;
{
  nomoresessions = FALSE;	/* if this was set, unset now */
  logit(0,"Server timeout on session %d pid %d, not talking to remote anymore",
      srn, pid);
  /* shouldn't need to do anymore here since remote stopped talking */
  /* assume sigchild interlocked here */
  if (ctp_tab[srn].state & CP_RUNNING)
    ctp_tab[srn].state |= CP_TIMEDOUT;
  dsiShutdown(srn);		/* ignore errors */
  kill(pid, SIGHUP);		/* hangup inferior */
}

/* got a hup and are inferior of server */
void
dienow()
{
  int comp;

  logit(0,"Superior told us to shutdown - probably tickle timeout");
  /* The following shouldn't really do anything since remote should be gone */
  /* be in case it really isn't, let's go through this rigamorle */
  /* Tell remote we are shutting down */
  dsiAttention(cno, AFPSHUTDOWNNOW, 1, -1, &comp);
  while (comp > 0) {abSleep(30, TRUE);} /* ignore error */
  /* Try closing just in case */
  dsiCloseSession(cno, 3, 2, &comp); /* 3 times, .5 seconds */
  while (comp > 0) { abSleep(30, TRUE); } /* close down if we can */
#ifdef LWSRV_AUFS_SECURITY
  clearuserlogin(); /* gtw: delete auth-file entry for dead users */
#endif LWSRV_AUFS_SECURITY
  exit(0);
}

#ifndef NOSHUTDOWNCODE

/*
 * inferior handler: setup and handle: terminate in minutes_to_shutdown
 *
 */
void
dying()
{
  int comp;
  void dying();
  void dienow();

  if (minutes_to_shutdown < 0)		/* not in die mode */
    return;
  if (!minutes_to_shutdown)
    dienow();
  signal(SIGALRM, dying);
  /* Tell remote we are shutting down */
  if (minutes_to_shutdown % 2) {		/* all odd minutes */
    /* there is a potential race condition here */
    dsiAttention(cno, AFPSHUTDOWNTIME(minutes_to_shutdown), 1, -1, &comp);
    while (comp > 0) {abSleep(30, TRUE);} /* ignore error */
  }
  minutes_to_shutdown--;
  alarm(60);
}

/*
 * inferior handler: setup and handle: terminate in 5 minutes 
 *
 */
void
diein5()
{
  void dying();

  signal(SIGTERM, SIG_IGN);
  if (minutes_to_shutdown >= 0)		/* already in shutdown mode */
    return;
  logit(0,"Superior told us to shutdown by time -- initiating 5 minute shutdown");
  minutes_to_shutdown = 5;
  dying();			/* start */
}

/*
 * inferior handler: advise client that a message is available
 *
 */
void
msgavail()
{
  int comp;

  signal(SIGURG, SIG_IGN);
  dsiAttention(cno, AFPSERVERMESG, 1, -1, &comp);
  while (comp > 0)
    abSleep(30, TRUE);
  signal(SIGURG, msgavail);
  return;
}

/*
 * Call when the master process receives a SIGHUP -- immediate shutdown
 *
 */
void
killnow()
{
  OSErr err;

  signal (SIGHUP, SIG_IGN);
  logit(0,"Parent received SIGHUP -- immediate shutdown");
#ifdef linux
  killlist(SIGHUP);
#else  linux
  killpg (parent_pid, SIGHUP);
#endif linux
  if ((err = SrvrShutdown(&srvr_entity_name)) != noErr)
    logit(0,"NBPRemove failed: code %d\n", err);
  exit(0);
}

/*
 * Called with the master process receives a SIGTERM
 */
void
killin5()
{
  void killinn();

#ifdef REREAD_AFPVOLS
  signal(SIGUSR1, SIG_IGN);
#endif REREAD_AFPVOLS
  signal (SIGTERM, SIG_IGN);
  logit(0,"Shutdown by time -- initiating 5 minute shutdown");
#ifdef linux
  killlist(SIGTERM);
#else  linux
  killpg (parent_pid, SIGTERM);
#endif linux
  minutes_to_shutdown = 4;
  /* in case children get blocked up */
  signal(SIGALRM, killinn);
  alarm(68);			/* a litte more than a minute */
}

void
killinn()
{
  void killinn();
  void killnow();

  if (minutes_to_shutdown < 0)		/* not in die mode */
    return;
  if (!minutes_to_shutdown)
    killnow();
#ifdef linux
  killlist(SIGTERM);
#else  linux
  killpg (parent_pid, SIGTERM);	/* in case inferior was blocked up */
#endif linux
  signal(SIGALRM, killinn);
  minutes_to_shutdown--;
  alarm(60);
}

/*
 * called when the master process receives a SIGURG
 *
 */
void
msgnotify()
{
  signal(SIGURG, SIG_IGN);
  logit(0, "Server Message available -- notifying clients");
#ifdef linux
  killlist(SIGURG);
#else  linux
  killpg(parent_pid, SIGURG);
#endif linux
  signal(SIGURG, msgnotify);
  return;
}

#ifdef linux
/*
 * send specified signal to child processes
 * (process groups appear to be broken)
 *
 */
killlist(sig)
int sig;
{
  int i;
  struct cno_to_pid *cp;

  for (i = 0, cp = ctp_tab; i < maxsess; i++, cp++)
    if (cp->state == CP_RUNNING)
      kill(cp->pid, sig);
  return;
}
#endif /* linux */
#endif /* NOSHUTDOWNCODE */

#ifdef REREAD_AFPVOLS
/*
 * a SIGUSR1 sets the 'readafpvols' flag which causes
 * dorereadafpvols() to be called next time through the loop
 *
 */

void
setreadafpvols()
{
  signal(SIGUSR1, SIG_IGN);
  readafpvols = TRUE;
  signal(SIGUSR1, setreadafpvols);
}

void
dorereadafpvols()
{
  FILE *fd, *fopen();

  readafpvols = FALSE;
  if (sysvolfile != NULL) {
    if ((fd = fopen(sysvolfile, "r")) == NULL)
      logit(0, "aufs: system volumes file %s disappeared", sysvolfile);
    else
      if (!VRRdVFile(fd))
	logit(0, "aufs: bad format system volumes file %s", sysvolfile);
  }
}
#endif REREAD_AFPVOLS

#ifdef	CLOSE_LOG_SIG
/*
 * A SIGUSR2 requests that the log be closed and reopened.  This is
 * important if you wish to rotate your logs on a regular basis.
 *
 */

void
closelog()
{
  signal(CLOSE_LOG_SIG, SIG_IGN);
  nologitfile();
  logitfileis(logfile, "a+");
  signal(CLOSE_LOG_SIG, closelog);
  return;
}
#endif /* CLOSE_LOG_SIG */

#ifdef notdef
/*
 * log an error message 
 */
#ifndef USEVPRINTF
/* Bletch - gotta do it because pyramids don't work the other way */
/* (using _doprnt and &args) and don't have vprintf */
/* of course, there will be something that is just one arg larger :-) */
/*VARARGS1*/
logit(fmt, a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af)
char *fmt;
#else /* USEVPRINTF */
logit(va_alist)
va_dcl
#endif /* USEVPRINTF */
{
  static FILE *fp = NULL;
#ifdef USEVPRINTF
  register char *fmt;
  va_list args;
#endif

  if (fp == NULL)
    if (DBDEB)
      fp = stderr;
    else
      if ((fp = fopen(logfile, "a+")) == NULL) 
	return;
  
  if (mypid != parent_pid)
    fprintf(fp, "%05d: ", mypid);
  else
    fprintf(fp, "%05d* ",mypid);
  tmprt(fp);
#ifdef USEVPRINTF
  va_start(args);
  fmt = va_arg(args, char *);
  vfprintf(fp, fmt, args);
  va_end(args);
#else /* USEVPRINTF */
  fprintf(fp, fmt,  a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af);
#endif /* USEVPRINTF */
  putc('\n', fp);
  fflush(fp);
}

tmprt(fp)
FILE *fp;
{
  fprintf(fp, "%s", mytod(0L));
}

/*
 * return tod in a static buffer, rotate among <n> buffers to
 * allow at least <n> calls "uses" to be active at a time
 *
 * <n> is currently 2
 *
*/
char *
mytod(ptime)
long ptime;
{
  long tloc;
  struct tm *tm, *localtime();
#define NTODBUF 2
  char * buf;
  static char buffers[NTODBUF][100];
  static int idx = 0;

  if (ptime == 0L)
    (void)time(&tloc);
  else
    tloc = ptime;
  tm = localtime(&tloc);
  buf = buffers[idx];
  ++idx;			/* move to next */
  idx %= NTODBUF;		/* make sure in range */
  if (tm->tm_year > 99)
    sprintf(buf, "%02d:%02d:%02d %02d/%02d/%04d ",
	    tm->tm_hour, tm->tm_min, tm->tm_sec,
	    tm->tm_mon+1, tm->tm_mday, tm->tm_year+1900);
  else
    sprintf(buf, "%02d:%02d:%02d %02d/%02d/%02d ",
	    tm->tm_hour, tm->tm_min, tm->tm_sec,
	    tm->tm_mon+1, tm->tm_mday, tm->tm_year);
  return(buf);
}
#endif notdef

#ifdef LWSRV_AUFS_SECURITY
/**************** budd... ****************/
clearuserlogin()
{
#ifdef HIDE_LWSEC_FILE
    char protecteddir[MAXPATHLEN];
    char dir_fname[MAXPATHLEN];
#endif HIDE_LWSEC_FILE

    if( userlogindir != NULL ) {
	char fname[ 100 ];
	int fd;

#ifdef HIDE_LWSEC_FILE
	strcpy(protecteddir, userlogindir);
	make_userlogin(fname, protecteddir, addr);
	strcpy(protecteddir, fname);
	strcpy(dir_fname, fname);
	fname[0] = '\0';
	make_userlogin(fname, protecteddir, addr);
	if (unlink(fname) < 0) {
	  logit(0, "clearuserlogin: unlink failed for %s", fname);
	  if ((fd = open(fname, O_WRONLY|O_TRUNC)) >= 0)
	    close(fd);
	} else
	  if (rmdir(dir_fname) < 0)
	    logit(0, "clearuserlogin: rmdir failed for %s", dir_fname);
#else  HIDE_LWSEC_FILE
	make_userlogin( fname, userlogindir, addr );
	if( unlink( fname ) < 0 ) {
	    if( (fd = open( fname, O_WRONLY|O_TRUNC )) != -1 )
		close( fd );
	} /* unlink failed */
#endif HIDE_LWSEC_FILE
    } /* have userlogindir */
} /* clearuserlogin */

/* duplicated in aufs.c and lwsrv.c sigh */
make_userlogin( buf, dir, addrb )
    char *buf, *dir;
    AddrBlock addrb;
{
    sprintf( buf, "%s/net%d.%dnode%d",
	    dir,
	    nkipnetnumber(addrb.net), nkipsubnetnumber(addrb.net),
	    addrb.node
	    );
} /* make_userlogin */
/**************** ...budd ****************/
#endif LWSRV_AUFS_SECURITY
#ifdef APPLICATION_MANAGER
/*
 * read the -A specified file for a list of pathnames
 * and an indication of the maximum number of times that
 * the file (resource fork) can be opened (colon separated).
 * Build a sorted list so that searching can be more efficient.
 * When the file is opened, read lock the next available byte,
 * return 'aeLockErr' if no locks available. If the maximum number
 * is specified with a trailing 'P', then the file is PROTECTED from
 * Finder copying.
 *
 * djh@munnari.OZ.AU
 * September, 1991
 *
 */

struct flist *applist = NULL;	/* head ptr for file list */
int fdplist[NOFILE];		/* fd list for protections */

readAppList(file)
char *file;
{
  char *c;
  int qty = 0;
  int num, protect;
  FILE *fp, *fopen();
  char linebuf[MAXPATHLEN*2];
  struct flist *newapplp, *p, *q;

  for (num = 0; num < NOFILE; num++)
    fdplist[num] = -1;

  if ((fp = fopen(file, "r")) == NULL) {
    logit(0, "readAppList(): cannot open %s for reading", file);
    return;
  }
  while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
    if (linebuf[0] == '#')
      continue;
    if ((c = (char *) rindex(linebuf, ':')) == NULL) {
      logit(0, "readAppList(): bad format (line %d) in %s", qty+1, file);
      fclose(fp);
      return;
    }
    *c++ = '\0';
    if ((num = atoi(c)) <= 0) {
      logit(0, "readAppList(): illegal value (%d) in %s", num, file);
      fclose(fp);
      return;
    }
    protect = ((char *)index(c, 'P') == NULL) ? 0 : 1;
    if ((newapplp = (struct flist *)malloc(sizeof(struct flist))) == NULL) {
      logit(0, "readAppList(): malloc(%d) failed", sizeof(struct flist));
      fclose(fp);
      return;
    }
    if ((newapplp->filename = (char *)malloc(strlen(linebuf)+12)) == NULL) {
      logit(0, "readAppList(): malloc(%d) failed", strlen(linebuf)+12);
      fclose(fp);
      return;
    }
    if ((c = (char *)rindex(linebuf,'/')) == NULL)
      continue;
    *c++ = '\0';
    strcpy(newapplp->filename, linebuf);
    strcat(newapplp->filename, "/.resource/");
    strcat(newapplp->filename, c);
    newapplp->incarnations = num;
    newapplp->protected = protect;
    newapplp->next = NULL;
    qty++;

    /* start list if none */
    if (applist == NULL) {
      applist = newapplp;
      continue;
    }
    /* else insert in-order */
    q = NULL;
    p = applist;
    while (p != NULL) {
      if (strcmp(p->filename, newapplp->filename) > 0)
        break;
      q = p;
      p = p->next;
    }
    newapplp->next = p;
    if (q == NULL)
      applist = newapplp;
    else
      q->next = newapplp;
  }
  fclose(fp);
  p = applist;
  logit(0, "Read %d application restrictions from %s", qty, file);
  while (p != NULL) {
    logit(0, " %s, %d%s", p->filename, p->incarnations,
      (p->protected) ? " (no copy)" : "");
    p = p->next;
  }
  return;
}
#endif APPLICATION_MANAGER
#ifdef AUFS_IDLE_TIMEOUT
/*
 * If an idle time is set ('-i' for guests, '-I' for everyone) then
 * check for number of minutes of no AFP activity. Give warnings
 * at 5, 3 & 1 minute marks. If anything happens then we reset the
 * timer and notify user that shutdown is aborted.
 *
 * djh@munnari.OZ.AU
 * February, 1992
 *
 */
int
checkIdle(cno, buf, cmp)
int cno, cmp;
unsigned char *buf;
{
  int i, comp;
  extern int guestlogin;
  static int sentshutdown = 0;

  if (guestonly && !guestlogin)
    return;

  if (cmp == 0 && *buf != 17) { /* periodic GetVolParms AFP call */
    if (sentshutdown) {
      logit(0, "Session %d: Aborting Idle Timeout", cno);
      dsiAttention(cno, AFPSHUTDOWNCANCEL, 1, -1, &comp);
      while (comp > 0) { abSleep(30, TRUE); } /* ignore error */
      sentshutdown = 0;
    }
    timeidle = 0;
    return;
  }

  switch (cmp) {
    case 0: /* noErr */
      timeidle++;
      break;
    case 1: /* abSleep() timeout */
      if (!sentshutdown) return;
      timeidle += 6;
      break;
    default: /* some error */
      return;
      break;
  }

  if (timeidle < idletime)
    return;

  switch ((i = timeidle-idletime)) {
    case 0:  /* shutdown in 5 min */
    case 12: /* shutdown in 3 min */
    case 24: /* shutdown in 1 min */
      logit(0, "Session %d: sending %d minute idle timeout warning",cno,5-i/6);
      dsiAttention(cno, AFPSHUTDOWNTIME(5-i/6), 1, -1, &comp);
      while (comp > 0) { abSleep(30, TRUE); } /* ignore error */
      sentshutdown++;
      return;
      break;
    case 30: /* shutdown now */
      logit(0, "Session %d: Idle Timeout Shutdown", cno);
      dsiAttention(cno, AFPSHUTDOWNNOW, 1, -1, &comp);
      while (comp > 0) { abSleep(30, TRUE); } /* ignore error */
      dsiCloseSession(cno, 3, 2, &comp); /* 3 times, .5 seconds */
      while (comp > 0) { abSleep(30, TRUE); } /* close down if we can */
#ifdef LWSRV_AUFS_SECURITY
      clearuserlogin(); /* gtw: delete auth-file entry for dead users */
#endif LWSRV_AUFS_SECURITY
      exit(0);
      break;
  }
}
#endif AUFS_IDLE_TIMEOUT
