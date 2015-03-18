static char rcsid[] = "$Author: djh $ $Date: 1996/09/10 13:56:31 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/lwsrv.c,v 2.47 1996/09/10 13:56:31 djh Rel djh $";
static char revision[] = "$Revision: 2.47 $";

/*
 * lwsrv - UNIX AppleTalk spooling program: act as a laserwriter
 *  driver: handles setup and farms out incoming jobs
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Feb  15, 1987	Schilit		Created, based on lsrv
 *  Mar  17, 1987	Schilit		Fixed nonprintables, added -r
 *  May  15, 1987	CCKim		Add support for LaserPrep 4.0
 *					Make multifork by default (turn
 *					off by defining SINGLEFORK)
 *  Feb	 15, 1991	djh		Various cleanups & patches
 *  Feb  20, 1991	rapatel		improve wait code, add lpr logging
 *  Jan  21, 1992	gkl300		add simple pass thru (for PC's)
 *
 */

char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>			/* assume included by param.h */
#endif  _TYPES
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netat/appletalk.h>		/* include appletalk definitions */
#include <netat/sysvcompat.h>
#include <netat/compat.h>
#include <netinet/in.h>
#include "../../lib/cap/abpap.h"	/* urk, puke, etc */

#ifdef USEDIRENT
#include <dirent.h>
#else  USEDIRENT
#ifdef xenix5
#include <sys/ndir.h>
#else xenix5
#include <sys/dir.h>
#endif xenix5
#endif USEDIRENT

#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
#endif NEEDFCNTLDOTH

#ifdef SOLARIS
#include <sys/systeminfo.h>
#define gethostname(n,l) (sysinfo(SI_HOSTNAME,(n),(l)) == (1) ? 0 : -1)
#endif SOLARIS

#ifdef LWSRV8
#include "list.h"
#include "query.h"
#include "parse.h"
#include "procset.h"
#include "fontlist.h"
#endif /* LWSRV8 */
#include "papstream.h"

#if defined (LWSRV_AUFS_SECURITY) | defined (RUN_AS_USER)
#include <pwd.h>
#endif LWSRV_AUFS_SECURITY | RUN_AS_USER
#ifdef RUN_AS_USER
#ifndef USER_FILE
#define USER_FILE "/usr/local/lib/cap/macusers"
#endif  USER_FILE
#ifndef REFUSE_MESSAGE
#define REFUSE_MESSAGE "/usr/local/lib/cap/refused"
#endif  REFUSE_MESSAGE
#else  RUN_AS_USER
#undef USER_REQUIRED
#endif RUN_AS_USER

#ifdef LWSRV8
#define	STATUSFILE	".lwsrvstatus"
#define	STATUSSIZE	256
typedef struct Slot {
  AddrBlock addr;
  int slot;
  int pid;
  int prtno;
  byte status[STATUSSIZE];
} Slot;
#endif /* LWSRV8 */

private char *logfile = NULL;

#ifdef LWSRV8
#ifdef LW_TYPE
#define	LW_AT_TYPE	"LaserWriter"
#else LW_TYPE
private char *prttype = "LaserWriter";
#endif LW_TYPE
#else  /* LWSRV8 */
private u_char *prtname = NULL;
private char *tracefile = NULL;
private char *fontfile = NULL;
private char *unixpname = NULL;
private char *prttype = "LaserWriter";
private char *dictdir = ".";		/* assume local dir */
#endif /* LWSRV8 */

private int rflag = FALSE;		/* remove print file */
private int hflag = TRUE;		/* default to print banner */
private int singlefork = FALSE;
private PAPStatusRec *statbuffp;	/* status buffer */
export int capture = TRUE;
export char *myname;
export int verbose = 0;

#ifdef LWSRV8
#ifdef PAGECOUNT
export int pcopies;
#endif PAGECOUNT
export int pagecount;
export boolean tracing = FALSE;		/* set if tracing */
export char *queryfile = NULL;
#else  /* LWSRV8 */
#ifdef PAGECOUNT
export int pagecount;
export int pcopies;
#endif PAGECOUNT
#endif /* LWSRV8 */

#ifdef __hpux
#define MAXJOBNAME 64
private char username[32],jobname[MAXJOBNAME],jobtitle[MAXJOBNAME];
#else  __hpux
private char username[128],jobname[1024];
#endif __hpux

#ifndef LWSRV8
private char buf[1024];
private int srefnum;
#endif /* not LWSRV8 */

#ifndef TEMPFILE
# define TEMPFILE "/tmp/lwsrvXXXXXX"	/* temporary file holds job */
#endif TEMPFILE

private char *tmpfiledir = NULL;

#define RFLOWQ atpMaxNum
#define BUFMAX (PAPSegSize*RFLOWQ)+10

#ifdef LWSRV_AUFS_SECURITY
private int requid, reqgid;
private char *aufsdb = NULL;		/* budd */
#ifndef LWSRV8
char *bin, *tempbin;
int tempbinlen;
#else /* LWSRV8 */
private char *bin, *tempbin;
private boolean aufssecurity[NUMSPAP];
private char openReplyMessage[STATUSSIZE];
#endif /* LWSRV8 */
#endif LWSRV_AUFS_SECURITY

#ifdef LWSRV_LPR_LOG
private char *requname = NULL;
#endif LWSRV_LPR_LOG

#ifdef NeXT
char *nextdpi = NULL;			/* NeXT printer resolution */
#endif NeXT

#ifndef LWSRV8
#ifdef DONT_PARSE
export int dont_parse = FALSE;
#endif DONT_PARSE
#endif /* not LWSRV8 */

#ifdef USESYSVLP
# ifndef LPRCMD
#  define LPRCMD "/usr/bin/lp"
# endif  LPRCMD
#endif USESYSVLP

#ifndef LPRCMD
# if defined(xenix5) || defined(__FreeBSD__)
#  define LPRCMD "/usr/bin/lpr"
# else /* xenix5 || __FreeBSD__ */
#  define LPRCMD "/usr/ucb/lpr"
# endif /* xenix5 || __FreeBSD__ */
#endif  LPRCMD

private char *lprcmd = LPRCMD;

#ifdef LPRARGS
#ifndef LWSRV8
private char *lprargsbuf[16];
private char **lprargs = lprargsbuf;
#else /* LWSRV8 */
private int lprstarted;
private List *lprargs;
#endif /* LWSRV8 */
#endif LPRARGS

#ifndef LWSRV8
private struct printer_instance {
    u_char *prtname;	/* NBP registered printername */
    char *unixpname;	/* UNIX printer name */
    char *tracefile;	/* .. individual tracefile option */
    PAPStatusRec statbuf;
    int srefnum;	/* returned by SLInit */
    char nbpbuf[128];   /* registered name:type@zone */
    int rcomp;		/* flag: waiting for job? */
    int cno;		/* connection number of next job */
} prtlist[NUMSPAP], *prtp;
#else /* LWSRV8 */
private struct printer_instance prtlist[NUMSPAP];
struct printer_instance *prtp;
#endif /* LWSRV8 */

private int nprt = 0;

#ifdef LWSRV8
private struct printer_instance _default = {
    NULL,	/* default is local dir	(-a) */
    0,		/* always scan dictdir */
    NULL,	/* dictionary list */
    TRUE,	/* doing checksum	(-k) */
    TRUE,	/* print banner		(-h) */
    FALSE,	/* don't remove file	(-r) */
    NULL,	/* tracefile		(-t) */
    FALSE,	/* maybe "eexec" in code(-e) */
    0,		/* Transcript options	(-T) */
#ifdef ADOBE_DSC2_CONFORMANT
    TRUE,	/* DSC2 compatibility	(-A) */
#else  ADOBE_DSC2_CONFORMANT
    FALSE,	/* DSC2 compatibility	(-A) */
#endif ADOBE_DSC2_CONFORMANT
#ifdef LPRARGS
    NULL,	/* lpr arguments	(-L) */
#endif LPRARGS
#ifdef PASS_THRU
    FALSE,	/* pass through		(-P) */
#endif PASS_THRU
#ifdef NeXT
    NULL,	/* NeXT resolution	(-R) */
#endif NeXT
#ifdef LW_TYPE
    LW_AT_TYPE,	/* default AppleTalk type (-Y)  */
#endif LW_TYPE
    TRUE,	/* capture procset	(-N) */
};
private int prtno;
private int prtcopy;

private int statusfd = -1;
private char statusfile[256];
private char statusidle[] = "status: idle";
private char statusprocessing[] = "status: processing job(s)";
private List *slots;
private int freeslot;
private int myslot;

private void readstatus();
private void resetstatus();
private void writestatus();
private Slot *GetSlot();
private void FreeSlot();
private byte *statusreply();
#ifdef LWSRV_AUFS_SECURITY
private byte *openreply();
private char *deniedmessage();
#endif LWSRV_AUFS_SECURITY
#ifdef TIMESTAMP
private char *timestamp();
#endif TIMESTAMP
#endif /* LWSRV8 */

private void
#ifndef LWSRV8
usage(s, err) 
char *s, *err;
#else /* LWSRV8 */
usage(err) 
char *err;
#endif /* LWSRV8 */
{
#ifndef LWSRV8
  if (err != NULL)
    fprintf(stderr,"%s: %s\n",s,err);
  fprintf(stderr,"usage: %s -n <PrinterName> -p <unix printer name>\n",s);
#else /* LWSRV8 */
  if (err != NULL)
    fprintf(stderr,"%s: %s\n",myname,err);
  fprintf(stderr,"usage: %s <ConfigFile> [<Database>]\n",myname);
  fprintf(stderr,"\t<ConfigFile> contains options and printer descriptions\n");
  fprintf(stderr,"\t<Database> of printer templates\n\n");
#ifdef LW_TYPE
  fprintf(stderr,"usage: %s -n <PrinterName> -p <unix printer name>",myname);
  fprintf(stderr," -Y <AppleTalkType>\n");
#else  LW_TYPE
  fprintf(stderr,"usage: %s -n <PrinterName> -p <unix printer name>\n",myname);
#endif LW_TYPE
#endif /* LWSRV8 */
  fprintf(stderr,"usage:\t\t-a <DictionaryDirectory> -f <FontFile>\n");
  fprintf(stderr,"usage:\t\t[-l <LogFile>] [-t <TraceFile>] [-r] [-h]\n");
#ifndef LWSRV_AUFS_SECURITY
  fprintf(stderr,"usage:\t\t[-T crtolf] [-T quote8bit]\n\n");
#else LWSRV_AUFS_SECURITY
  fprintf(stderr,"usage:\t\t[-T crtolf] [-T quote8bit] [-X userlogindb]\n\n");
#endif LWSRV_AUFS_SECURITY
  fprintf(stderr,"\t-p*<unix printer name> is the unix printer to print to\n");
  fprintf(stderr,"\t-n*<PrinterName> specifies the name %s registers.\n",
   myname);
#ifdef LWSRV8
#ifdef LW_TYPE
  fprintf(stderr,"\t-Y <AppleTalkType> specifies an alternate AppleTalk\n");
  fprintf(stderr,"\t   type for this printer, default is: ");
  fprintf(stderr,"%s\n", LW_AT_TYPE);
#endif LW_TYPE
#endif /* LWSRV8 */
  fprintf(stderr,"\t-a*<DictionaryDirectory> is the ProcSet directory.\n");
  fprintf(stderr,"\t-f*<FontFile> contains an font coordination list.\n");
  fprintf(stderr,"\t-t <Tracefile> stores session and appledict in\n");
  fprintf(stderr,"\t   <Tracefile> without printing.\n");
  fprintf(stderr,"\t-F <TmpFileDir> directory to store temporary files\n");
  fprintf(stderr,"\t-l <LogFile> specifies a file to log the %s session\n",
   myname);
#ifdef LWSRV8
  fprintf(stderr,"\t-q <QueryFile> log unknown queries\n");
#endif /* LWSRV8 */
  fprintf(stderr,"\t-e Allow an eexec to occur in a procset\n");
  fprintf(stderr,"     warning: this may cause problems, use carefully\n");
  fprintf(stderr, "\t-N Turns off capturing of new procsets\n");
  fprintf(stderr,"\t-r Will retain temp print file for inspection\n");
  fprintf(stderr,"\t-h means to print without a banner page\n");
  if (is_simple_dsc()) 
    fprintf(stderr,"\t-A [on*|off] means to turn on or off Adobe document\n");
  else
    fprintf(stderr,"\t-A [on|off*] means to turn on or off Adobe document\n");
  fprintf(stderr,"\tstructuring revision 2 compatibility\n");
  fprintf(stderr,"\t(this can cause problems, *'ed item is default)\n");
  fprintf(stderr,"\t-C <cmd> Specify print spooler rather than lp/lpr\n");
#ifdef LPRARGS
  fprintf(stderr,"\t-L <arg> Argument to pass to lpr (multiple use)\n");
#endif LPRARGS
#ifdef PASS_THRU
  fprintf(stderr,"\t-P %s pass through (no adobe preprocessing)\n", myname);
#endif PASS_THRU
#ifdef NeXT
  fprintf(stderr,"\t-R <dpi> Specify resolution for NeXT printer\n");
#endif NeXT
  fprintf(stderr,"\t-S single %s fork (default is multiforking)\n", myname);
#ifndef LWSRV8
#ifdef DONT_PARSE
  fprintf(stderr,"\t-D don't parse any of the file, but allow crtolf if set\n");
#endif DONT_PARSE
#endif /* LWSRV8 */
  fprintf(stderr,"\t-T Transcript compatibilty options\n");
  fprintf(stderr,"\t-T crtolf: translate cr to lf for Transcript filters\n");
  fprintf(stderr,"\t-T quote8bit: quote 8 bit chars for Transcript\n");
  fprintf(stderr,"\t-T makenondscconformant: make non DSC conformant: use\n");
  fprintf(stderr,"\t   if psrv only works with DSC 1.0 conventions\n");
#ifdef LWSRV_AUFS_SECURITY
 fprintf(stderr,"\t-X <database directory> use aufs for validation\n");/*budd*/
#endif LWSRV_AUFS_SECURITY
  fprintf(stderr,"\nexample: %s -n Laser -p ps -a/usr/lib/ADicts\n",myname);
  fprintf(stderr,"\t\t-f /usr/lib/LW+Fonts\n");
  fprintf(stderr,"\t(note the starred items above are required)\n");
  exit(0);
}

private void
#ifndef LWSRV8
inc_nprt(progname)
char *progname;
#else /* LWSRV8 */
inc_nprt()
#endif /* LWSRV8 */
{
#ifndef LWSRV8
    if (prtp->unixpname == NULL)
      usage(progname,"Missing Unix Printer Name");
#endif /* LWSRV8 */

#ifdef LWSRV8
  if (prtp == &_default) {	/* doing default */
    prtp = prtlist;
    if (_default.dictdir == NULL) {
      _default.dictdir = ".";
      _default.dictlist = scandicts(_default.dictdir, &_default.lastried);
      AddToKVTree(_default.querylist, "procset", _default.dictlist, strcmp);
    }
  } else {
    if (prtp->unixpname == NULL)
      usage("Missing Unix Printer Name");
#endif /* LWSRV8 */
#ifndef LWSRV8
    if (prtp->prtname == NULL)
      usage(progname,"Missing AppleTalk Printer Name");
#else /* LWSRV8 */
    if (prtp->prtname == NULL)
      usage("Missing AppleTalk Printer Name");
    if (SearchKVTree(prtp->querylist, "font", strcmp) == NULL)
      usage("No fonts specified");
#ifdef LW_TYPE
    if (prtp->prttype == NULL)
      usage("Missing AppleTalk type");
#endif LW_TYPE
#endif /* LWSRV8 */
#ifndef LWSRV8
    if (++nprt >= NUMSPAP)
	usage("lwsrv","too many printers");
#else /* LWSRV8 */
    if (++nprt >= NUMSPAP)
	usage("too many printers");
#endif /* LWSRV8 */
    prtp++;
#ifndef LWSRV8
    unixpname = tracefile = NULL;
    prtname = NULL;
#else /* LWSRV8 */
  }
  bcopy((char *)&_default, (char *)prtp, prtcopy);
  thequery = prtp->querylist = DupKVTree(_default.querylist);
#ifdef LPRARGS
  lprstarted = FALSE;
#endif LPRARGS
#endif /* LWSRV8 */
}
    
#ifdef LWSRV8
static char optlist[64] = "a:f:l:p:t:d:n:krehNT:A:C:F:Svq:";

private void
lwsrv_init(name)
char *name;
{
  if (myname = rindex(name, '/'))
    myname++;
  else
    myname = name;
#ifdef LWSRV_AUFS_SECURITY
  strcat(optlist, "X:");
#endif LWSRV_AUFS_SECURITY
#ifdef LPRARGS
  strcat(optlist, "L:");
#endif LPRARGS
#ifdef PASS_THRU
  strcat(optlist, "P");
#endif PASS_THRU
#ifdef NeXT
  strcat(optlist, "R:");
#endif NeXT
#ifdef LW_TYPE
  strcat(optlist, "Y:");
#endif LW_TYPE

  thequery = _default.querylist = CreateKVTree();
  prtcopy = (char *)&_default.prtname - (char *)&_default;
  nprt = 0;
  prtp = &_default;
#ifdef LPRARGS
  lprstarted = FALSE;
#endif LPRARGS
}

export void
doargs(argc,argv)
int argc;
char **argv;
{
  register int c;
  register KVTree **kp;
  register List *lp;
  time_t ltime;
  extern char *optarg;
  extern int optind;
#ifdef ISO_TRANSLATE
  void cISO2Mac();
#endif ISO_TRANSLATE
  
  while ((c = getopt(argc,argv,optlist)) != EOF) {
    switch (c) {
    case 'a':
      if (index(optarg, '/') == NULL) {
	prtp->dictdir = (char *)malloc(strlen(optarg)+4);
	strcpy(prtp->dictdir, "./");
	strcat(prtp->dictdir, optarg);
      } else 
	prtp->dictdir = optarg;	/* remember dictionary directory */
      ltime = 0;
      if (kp = scandicts(prtp->dictdir, &ltime)) {
	prtp->lastried = ltime;
        AddToKVTree(prtp->querylist, "procset", (prtp->dictlist = kp), strcmp);
      }
      break;
    case 'd':
      dbugarg(optarg);
      break;
    case 'f':
      if ((lp = LoadFontList(optarg)) == NULL) { /* -f fontfile */
	fprintf(stderr,"%s: Bad FontList from %s\n", myname, optarg);
	break;
      }
      AddToKVTree(prtp->querylist, "font", lp, strcmp);
      fprintf(stderr,"%s: Font list from file %s\n", myname, optarg);
      break;
    case 'l':				/* -l logfile */
      logfile = optarg;
      break;
    case 'q':				/* -q queryfile */
      queryfile = optarg;
      break;
    case 'n':				/* lwsrv printer name */
      if (prtp->prtname || prtp == &_default)
	inc_nprt();
#ifdef ISO_TRANSLATE
      cISO2Mac(optarg);
#endif ISO_TRANSLATE
      prtp->prtname = optarg;
      break;
    case 'p':				/* -p unix printer name */
      if (prtp->unixpname || prtp == &_default)
	inc_nprt();
      prtp->unixpname = optarg;
      break;
    case 'k':				/* no DDP checksum */
      prtp->dochecksum = FALSE;
      break;
    case 'h':
      prtp->hflag = FALSE;		/* do not print banner */
      break;
    case 'r':				/* do not remove file */
      prtp->rflag = TRUE;
      break;
    case 't':				/* -t tracefile */
      prtp->tracefile = optarg;
      break;
    case 'e':
      prtp->eflag = TRUE;		/* maybe "eexec" in code */
      break;
    case 'T':
      if ((c = simple_TranscriptOption(optarg)) < 0)
	usage(NULL);
      prtp->toptions |= c;
      break;
    case 'A':
      if ((c = simple_dsc_option(optarg)) < 0)
	usage(NULL);
      prtp->dsc2 = c;
      break;
    case 'C':
      lprcmd = optarg;
      break;
#ifdef LPRARGS
    case 'L':
      if (!lprstarted) {
	if (prtp->lprargs)
	  prtp->lprargs = DupList(prtp->lprargs);
	else
	  prtp->lprargs = CreateList();
	lprstarted = TRUE;
      }
      AddToList(prtp->lprargs, (void *)optarg);
      break;
#endif LPRARGS
#ifdef PASS_THRU
    case 'P':
      prtp->passthru = TRUE;		/* -P pass through PC jobs */
      break;
#endif PASS_THRU
    case 'S':
      singlefork = TRUE;
      fprintf(stderr, "%s: single fork\n", myname);
      break;
#ifdef LWSRV_AUFS_SECURITY
    case 'X':				/* budd... */
      aufsdb = optarg;
      fprintf(stderr, "%s: aufs db directory in %s\n", myname, aufsdb);
      break;				/* ...budd */
#endif LWSRV_AUFS_SECURITY
#ifdef NeXT
    case 'R':
      prtp->nextdpi = optarg;
      break;
#endif NeXT
#ifdef LW_TYPE
    case 'Y':
      prtp->prttype = optarg;
      break;
#endif LW_TYPE
    case 'F':
      tmpfiledir = optarg;
      break;
    case 'N':
      prtp->capture = FALSE;
      break;
    case 'v':
      verbose++;
      break;
    case '?':				/* illegal character */
      usage(NULL);			/* usage and exit */
      break;
    }
  }
  if (optind < argc) {
    fprintf(stderr, "%s: surplus arguments starting from \"%s\"\n",
      myname,argv[optind]);
    usage(NULL);
  }
}

void 
set_printer(p)
register struct printer_instance *p;
{
    extern boolean dochecksum;

    set_dict_list(p->dictlist);
    dochecksum = p->dochecksum;
    hflag = p->hflag;
    rflag = p->rflag;
    setflag_encrypted_instream(p->eflag);
    set_toptions(p->toptions);
    set_simple_dsc(p->dsc2);
#ifdef LPRARGS
    lprargs = p->lprargs;
#endif LPRARGS
#ifdef PASS_THRU
    set_simple_pass_thru(p->passthru);
#endif PASS_THRU
#ifdef NeXT
    nextdpi = p->nextdpi;
#endif NeXT
    capture = p->capture;
    tracing = (p->tracefile != NULL);
    statbuffp = &(p->statbuf);
    thequery = p->querylist;
    spool_setup(p->prtname, prtp->dictdir);
}
    
private void
childdone(s)
int s;
{
  WSTATUS status;

  resetstatus(wait(&status));
  signal(SIGCHLD, childdone);
}

#else  /* LWSRV8 */

private void
doargs(argc,argv)
int argc;
char **argv;
{
  int c;
  extern char *optarg;
  extern int optind;
  extern boolean dochecksum;
  static char optlist[64] = "a:f:l:p:t:d:n:krehNT:A:C:F:Sv";
#ifdef ISO_TRANSLATE
  void cISO2Mac();
#endif ISO_TRANSLATE
  
#ifdef LWSRV_AUFS_SECURITY
  strcat(optlist, "X:");
#endif LWSRV_AUFS_SECURITY
#ifdef LPRARGS
  strcat(optlist, "L:");
#endif LPRARGS
#ifdef PASS_THRU
  strcat(optlist, "P");
#endif PASS_THRU
#ifdef NeXT
  strcat(optlist, "R:");
#endif NeXT
#ifdef DONT_PARSE
  strcat(optlist, "D");
#endif DONT_PARSE

  nprt = 0; prtp = prtlist;

  while ((c = getopt(argc,argv,optlist)) != EOF) {
    switch (c) {
    case 'a':
      if (index(optarg, '/') == NULL) {
	dictdir = (char *)malloc(strlen(optarg)+4);
	strcpy(dictdir, "./");
	strcat(dictdir, optarg);
      } else 
	dictdir = optarg;	/* remember dictionary directory */
      break;
    case 'd':
      dbugarg(optarg);
      break;
    case 'f':
      fontfile = optarg;		/* -f fontfile */
      break;
    case 'l':				/* -l logfile */
      logfile = optarg;
      break;
    case 'n':				/* lwsrv printer name */
      if (prtname != NULL)
	inc_nprt(argv[0]);
#ifdef ISO_TRANSLATE
      cISO2Mac(optarg);
#endif ISO_TRANSLATE
      prtp->prtname = prtname = (u_char *)optarg;
      break;
    case 'p':				/* -p unix printer name */
      if (unixpname != NULL)
	inc_nprt(argv[0]);
      prtp->unixpname = unixpname = optarg;
      break;
    case 'k':				/* no DDP checksum */
      dochecksum = 0;
      break;
    case 'h':
      hflag = FALSE;			/* do not print banner */
      break;
    case 'r':				/* do not remove file */
      rflag = TRUE;
      break;
    case 't':				/* -t tracefile */
      prtp->tracefile = tracefile = optarg;
      break;
    case 'e':
      setflag_encrypted_instream(TRUE);	/* maybe "eexec" in code */
      break;
    case 'T':
      if (simple_TranscriptOption(optarg) < 0)
	usage(argv[0], NULL);
      break;
    case 'A':
      if (simple_dsc_option(optarg) < 0)
	usage(argv[0], NULL);
      break;
    case 'C':
      lprcmd = optarg;
      break;
#ifdef LPRARGS
    case 'L':
      *lprargs++ = optarg;
      break;
#endif LPRARGS
#ifdef PASS_THRU
    case 'P':
      set_simple_pass_thru();		/* -P pass through PC jobs */
      break;
#endif PASS_THRU
    case 'S':
      singlefork = TRUE;
      fprintf(stderr, "lwsrv: single fork\n");
      break;
#ifdef LWSRV_AUFS_SECURITY
    case 'X':				/* budd... */
      aufsdb = optarg;
      fprintf(stderr, "lwsrv: aufs db directory in %s\n", aufsdb );
      break;				/* ...budd */
#endif LWSRV_AUFS_SECURITY
#ifdef NeXT
    case 'R':
      nextdpi = optarg;
      break;
#endif NeXT
#ifdef DONT_PARSE
    case 'D':
      dont_parse = TRUE;
      break;
#endif DONT_PARSE
    case 'F':
      tmpfiledir = optarg;
      break;
    case 'N':
      capture = FALSE;
      break;
    case 'v':
      verbose++;
      break;
    case '?':				/* illegal character */
      usage(argv[0],NULL);		/* usage and exit */
      break;
    }
  }
  if (optind < argc) {
    fprintf(stderr, "%s: surplus arguments starting from \"%s\"\n",
      argv[0],argv[optind]);
    usage(argv[0], NULL);
  }

  inc_nprt(argv[0]);

  if (fontfile == NULL)
    usage(argv[0],"No FontFile specified");    
#ifdef LPRARGS
  *lprargs = NULL;
  lprargs = lprargsbuf;
#endif LPRARGS

}

void 
set_printer(p)
struct printer_instance *p;
{
    prtname = p->prtname;
    unixpname = p->unixpname;
    tracefile = p->tracefile;
    statbuffp = &(p->statbuf);
    srefnum = p->srefnum;
}
    
private void
childdone()
{
  WSTATUS status;

  (void)wait(&status);
  signal(SIGCHLD, childdone);
}
#endif /* LWSRV8 */

#ifdef LWSRV8
private Slot *
GetSlot(prtno)
int prtno;
{
  register Slot *slotp, **sp;
  register int i, n;

  if ((slotp = (Slot *)malloc(sizeof(Slot))) == NULL)
    errorexit(1, "GetSlot: Out of memory\n");
  i = freeslot;
  sp = (Slot **)AddrList(slots) + i;
  *sp = slotp;
  slotp->slot = i;
  slotp->prtno = prtno;

  n = NList(slots);
  for ( ; ; ) {
    if (++i >= n) {	/* all slots in use, make a new one */
      AddToList(slots, NULL);
      freeslot = n;
      break;
    }
    if (*++sp == NULL) {
      freeslot = i;
      break;
    }
  }
  return(slotp);
}

private void
FreeSlot(n)
int n;
{
  register Slot **sp;

  sp = (Slot **)AddrList(slots) + n;
  free((char *)*sp);
  *sp = NULL;
  if (freeslot > n)
    freeslot = n;
}

private void
resetstatus(pid)
register int pid;
{
  register Slot **sp;
  register int i, n;
  register int *children;

  n = NList(slots);
  for (sp = (Slot **)AddrList(slots), i = 0; i < n; sp++, i++) {
    if (*sp == NULL)
      continue;
    if ((*sp)->pid == pid) {
      children = &prtlist[(*sp)->prtno].children;
      if (*children > 0 && --(*children) == 0)
	cpyc2pstr(prtlist[(*sp)->prtno].statbuf.StatusStr, statusidle);
      FreeSlot(i);
      return;
    }
  }
}

private void
readstatus(n)
int n;
{
  register Slot **sp;

  sp = (Slot **)AddrList(slots);
  lseek(statusfd, n * STATUSSIZE, L_SET);
  read(statusfd, sp[n]->status, STATUSSIZE);
}

private void
writestatus(n, str)
int n;
char *str;
{
  byte message[STATUSSIZE];

  lseek(statusfd, n * STATUSSIZE, L_SET);
  cpyc2pstr(message, str);
  write(statusfd, message, *message + 1);
}

private byte *
statusreply(cno, from)
int cno;
register AddrBlock *from;
{
  register Slot **sp;
  register int i, n;

  n = NList(slots);
  sp = (Slot **)AddrList(slots);
  for (i = 0; i < n; sp++, i++) {
    if (*sp == NULL)
      continue;
    if (from->net == (*sp)->addr.net && from->node == (*sp)->addr.node) {
      readstatus(i);
      return((*sp)->status);
    }
  }
  return(NULL);
}

#ifdef LWSRV_AUFS_SECURITY
private char *
deniedmessage(cno)
{
  register List *lp;
  register char *sp, *cp;
  /*
   * We rely on the fact that the cno passed by the callback corresponds to
   * the index into the prtlist array.
   */
  if ((lp = SearchKVTree(prtlist[cno].querylist, "DeniedMessage", strcmp)) &&
   NList(lp) >= 1) {
    /* remove the trailing newline, if any */
    if (cp = index((sp = *(char **)AddrList(lp)), '\n'))
      *cp = 0;
    return(sp);
  }
  return("Denied: Please login to host %s via AppleShare");
}

private byte *
openreply(cno, from, result)
int cno;
AddrBlock *from;
int result;
{
  if (result == papPrinterBusy)
    return(NULL);
  /*** budd.... */
  fprintf(stderr,
#ifdef TIMESTAMP
     "%s: %s: \"%s\" open request from [Network %d.%d, node %d, socket %d]\n",
	myname, timestamp(), prtlist[cno].prtname, nkipnetnumber(from->net),
#else TIMESTAMP
     "%s: \"%s\" open request from [Network %d.%d, node %d, socket %d]\n",
	myname, prtlist[cno].prtname, nkipnetnumber(from->net),
#endif TIMESTAMP
	nkipsubnetnumber(from->net),
	from->node, from->skt);

  if (aufsdb != NULL) {
    char fname[ 256 ];
    char filename[ 256 ];
    int f, cc, ok = 0;
    struct passwd *pw;
    struct stat statbuf;
#ifdef HIDE_LWSEC_FILE
    char protecteddir[MAXPATHLEN];
    (void) strcpy(protecteddir, aufsdb);
    make_userlogin(filename, protecteddir, from);
    (void) strcpy(protecteddir, filename);
    filename[0] = '\0';
    make_userlogin(filename, protecteddir, from);
#else  HIDE_LWSEC_FILE
    make_userlogin( filename, aufsdb, from);
#endif HIDE_LWSEC_FILE

    if ((f = open( filename, 0)) >= 0) {
      if ((cc = read( f, fname, sizeof(fname)-1)) > 0) {
	if (fname[cc-1] == '\n')
	  fname[cc-1] = '\0';
	fprintf( stderr, "%s: Found username in aufsdb (%s): %s\n",
		myname, aufsdb, fname);

	if ((tempbin = rindex(fname,':')) != NULL) {
	  *tempbin='\0';
	  tempbin++;
	  bin = (char *)malloc(strlen(tempbin)+1);
	  strcpy(bin,tempbin);
	}
	    
	(void) stat(filename, &statbuf);
	if ((pw = getpwuid((int) statbuf.st_uid)) != NULL) {
	  if (strcmp(pw->pw_name, fname) == 0) {
	    requid = pw->pw_uid;
	    reqgid = pw->pw_gid;
#ifdef LWSRV_LPR_LOG
	    requname = pw->pw_name;
#endif LWSRV_LPR_LOG
	    ok = 1;
	  }
	} else if ((pw = getpwnam(fname)) != NULL) {
	  requid = pw->pw_uid;
	  reqgid = pw->pw_gid;
#ifdef LWSRV_LPR_LOG
	  requname = pw->pw_name;
#endif LWSRV_LPR_LOG
	  ok = 1;		/* true */
	} /* pwnam ok */
      } /* read OK */
      close(f);  /* close the file */
    } /* open OK */
    if (!ok) {	/* dump connection with error message */
      static byte message[STATUSSIZE];
      fprintf( stderr, "%s: No username (or invalid) confirmation.\n",
	myname);
      gethostname( fname, sizeof(fname)-1); /* ick */

      sprintf(openReplyMessage, deniedmessage(cno), fname);
      cpyc2pstr(message, openReplyMessage);
      return(message);
    } /* not ok */
  } /* have aufsdb */
    /* ....budd ***/
  aufssecurity[cno] = TRUE;
  return(NULL);
}
#endif LWSRV_AUFS_SECURITY
#endif /* LWSRV8 */

/*
 * delete the NBP name
 *
 */

private int is_child = 0;

private void
#ifndef LWSRV8
cleanup()
#else /* LWSRV8 */
cleanup(s)
int s;
#endif /* LWSRV8 */
{
#ifndef LWSRV8
    int i;
#else /* LWSRV8 */
    int i, srefnum;
#endif /* LWSRV8 */

    if (is_child)
      exit(0);

    for (i=0; i<nprt; i++) {
	srefnum = prtlist[i].srefnum;
	SLClose(srefnum);		/* close server for child */
	PAPRemName(srefnum, prtlist[i].nbpbuf);
    }
#ifdef LWSRV8
    close(statusfd);
    unlink(statusfile);
#endif /* LWSRV8 */
    exit(0);
}

openlogfile()
{
    int i;

    if (logfile != NULL) {
      if ((i = open(logfile,O_WRONLY|O_APPEND)) < 0) {
	if ((i = creat(logfile,0666)) < 0) {
	  fprintf(stderr, "%s: Cannot open/create logfile (%s)\n",
	   myname, logfile);
	  exit(1);
	}
      }
      if (i != 2) {
#ifndef NODUP2
	(void) dup2(i,2);
#else   NODUP2
	close(2);			/* try again */
	(void) dup(0);			/* for slot 2 */
#endif  NODUP2
	(void) close(i);
      }
#ifdef LWSRV_LPR_LOG	/* Set up logfile to be also stdout/stderr for lpr */
#ifdef NODUP2
      close(1);
      dup(2);
#else  NODUP2
      dup2 (2, 1);
#endif NODUP2
#endif LWSRV_LPR_LOG
    }
}

#ifdef LWSRV8
main(argc,argv)
int argc;
char **argv;
{
  register KVTree **kp;
  time_t ltime;
  int err,i,srefnum,cno,errcount;
  Slot *slotp;
  void cleanup();
  void childdone();
#ifdef ISO_TRANSLATE
  void cMac2ISO(), cISO2Mac();
#endif ISO_TRANSLATE
  int free();

  lwsrv_init(argv[0]);
  if ((argc == 2  || argc == 3) && *(argv[1]) != '-') {	/* configuration file */
    extern FILE *yyin;
    FILE *fp;

    if ((fp = fopen(argv[1], "r")) == NULL) {
      fprintf(stderr, "%s: Can't open %s\n", myname, argv[1]);
      exit(1);
    }
    yyin = fp;				/* set for flex(1) */
    initkeyword(fp);
    if (yyparse()) {
      fprintf(stderr, "%s: Giving up...\n", myname);
      exit(1);
    }
    fclose(fp);
    configargs(argc == 3 ? argv[2] : libraryfile);
  } else
    doargs(argc,argv);			/* handle args */
  inc_nprt();			/* check last printer */
  
#ifdef LWSRV_AUFS_SECURITY
  requid = getuid();			/* budd */
  reqgid = getgid();			/* budd */
#endif LWSRV_AUFS_SECURITY

#ifdef AUTHENTICATE
  if (nprt == 1)
    initauthenticate(*argv, (char *)prtlist[0].prtname);
#endif AUTHENTICATE

  signal(SIGHUP, cleanup);
  signal(SIGINT, cleanup);
  signal(SIGQUIT, cleanup);
  signal(SIGTERM, cleanup);

  if (!dbug.db_flgs) {			/* disassociate */
    if (fork())
      exit(0);				/* kill off parent */
    for (i=0; i < 20; i++)
      close(i);				/* close all files */
    (void) open("/dev/null",0);
#ifndef NODUP2
    (void) dup2(0,1);
#else   NODUP2
    (void)dup(0);			/* for slot 1 */
#endif  NODUP2
    if (logfile == NULL) {
#ifndef NODUP2
      (void) dup2(0,2);
#else   NODUP2
      (void) dup(0);			/* for slot 2 */
#endif  NODUP2
    } else
      openlogfile();
#ifndef POSIX
#ifdef TIOCNOTTY
    if ((i = open("/dev/tty",2)) > 0) {
      ioctl(i, TIOCNOTTY,(char *) 0);
      close(i);
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

/*  dbug.db_pap = TRUE; */

  if (!singlefork) {
    sprintf(statusfile, "%s%05d", STATUSFILE, getpid());
    if ((statusfd = open(statusfile, O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0) {
      fprintf(stderr, "%s: Can't open %s\n", myname, statusfile);
      exit(1);
    }
    signal(SIGCHLD, childdone);
    slots = CreateList();
    AddToList(slots, NULL);	/* make sure there is one slot set up */
    freeslot = 0;
    papstatuscallback = statusreply;
  }
#ifdef LWSRV_AUFS_SECURITY
  papopencallback = openreply;
#endif LWSRV_AUFS_SECURITY
  
  abInit(FALSE);		/* initialize appletalk driver */
  nbpInit();			/* initialize NBP routines */
  PAPInit();			/* init PAP printer routines */

  errcount = 0;
  for (prtp=prtlist,i=0; i<nprt; prtp++,i++) {
#ifdef LW_TYPE
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      cMac2ISO(prtp->prttype);
      fprintf(stderr,"%s: Spooler starting for %s:%s@*\n",
       myname,prtp->prtname,prtp->prttype);
      sprintf(prtp->nbpbuf,"%s:%s@*",prtp->prtname,prtp->prttype);
      cISO2Mac(prtp->prttype);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"%s: Spooler starting for %s:%s@*\n",
       myname,prtp->prtname,prtp->prttype);
      sprintf(prtp->nbpbuf,"%s:%s@*",prtp->prtname,prtp->prttype);
#endif ISO_TRANSLATE
#else LW_TYPE
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      fprintf(stderr,"%s: Spooler starting for %s:%s@*\n",
       myname,prtp->prtname,prttype);
      sprintf(prtp->nbpbuf,"%s:%s@*",prtp->prtname,prttype);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"%s: Spooler starting for %s:%s@*\n",
       myname,prtp->prtname,prttype);
      sprintf(prtp->nbpbuf,"%s:%s@*",prtp->prtname,prttype);
#endif ISO_TRANSLATE
#endif LW_TYPE
      cpyc2pstr(prtp->statbuf.StatusStr, statusidle);
      err = SLInit(&(prtp->srefnum), prtp->nbpbuf, 8, &(prtp->statbuf));
      if (err != noErr) {
         fprintf(stderr,"%s: SLInit %s failed: %d\n",myname,prtp->nbpbuf,err);
	 errcount++;
      }
      /* GetNextJob is asynchronous, so call it for each printer */
      err = GetNextJob(prtp->srefnum, &(prtp->cno), &(prtp->rcomp));
      if (err != noErr) {
#ifdef ISO_TRANSLATE
	cMac2ISO(prtp->prtname);
        fprintf(stderr,"%s: GetNextJob %s failed: %d\n",myname,prtp->prtname,
         err);
	cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
        fprintf(stderr,"%s: GetNextJob %s failed: %d\n",myname,prtp->prtname,
         err);
#endif ISO_TRANSLATE
        errcount++;
      }
      fprintf(stderr, "%s: %s ready and waiting\n", myname, prtp->nbpbuf);
  }

  if (errcount == nprt) {
	fprintf(stderr, "%s: no printers successful registered!\n", myname);
	exit(1);
 }

  prtno = 0;
  while (TRUE) {
    openlogfile();
    NewStatus("idle");

  /* scan each printer in turn, processing each one as "normal" if there
   * is work available. If no work was available then abSleep for a while
   */
    while (prtno<nprt && prtlist[prtno].rcomp > 0) prtno++;
    if (prtno == nprt) {
      abSleep(20,TRUE);	/* wait for some work */
      prtno = 0;	/* start scan again */
      continue;
    }

    prtp = &prtlist[prtno];
    ltime = prtp->lastried;
    if (kp = scandicts(prtp->dictdir, &ltime)) {
      if (prtp->dictlist)
	FreeKVTree(prtp->dictlist, free, free);
      prtp->dictlist = kp;
      prtp->lastried = ltime;
    }
    set_printer(prtp); 	/* set the global variables etc */
    srefnum = prtp->srefnum;
    cno = prtp->cno;

    /* GetNextJob is asynchronous, so announce readiness for more work */
    /* WTR: is this right - should it depend on singlefork? */
    err = GetNextJob(prtp->srefnum, &(prtp->cno), &(prtp->rcomp));
    if (err != noErr) {
#ifdef TIMESTAMP
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      fprintf(stderr,"%s: %s: GetNextJob %s failed: %d\n",
       myname, timestamp(), prtp->prtname, err);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"%s: %s: GetNextJob %s failed: %d\n",
       myname, timestamp(), prtp->prtname, err);
#endif ISO_TRANSLATE
#else TIMESTAMP
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      fprintf(stderr,"%s: GetNextJob %s failed: %d\n",
       myname, prtp->prtname, err);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"%s: GetNextJob %s failed: %d\n",
       myname, prtp->prtname, err);
#endif ISO_TRANSLATE
#endif TIMESTAMP
    }
#ifdef LWSRV_AUFS_SECURITY
    /*
     * abpaps.c and lwsrv.c uses "cno" to refer to different things.  The
     * "cno" used by the callbacks is really "srefnum" here.  Confusing!
     */
    if (aufssecurity[srefnum]) {
#endif LWSRV_AUFS_SECURITY
#ifdef TIMESTAMP
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      fprintf(stderr,"%s: %s: Starting print job for \"%s\", Connection %d\n",
       myname, timestamp(), prtp->prtname, cno);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"%s: %s: Starting print job for \"%s\", Connection %d\n",
       myname, timestamp(), prtp->prtname, cno);
#endif ISO_TRANSLATE
#else TIMESTAMP
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      fprintf(stderr,"%s: Starting print job for \"%s\", Connection %d\n",
       myname, prtp->prtname, cno);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"%s: Starting print job for \"%s\", Connection %d\n",
       myname, prtp->prtname, cno);
#endif ISO_TRANSLATE
#endif TIMESTAMP
      cpyc2pstr(prtp->statbuf.StatusStr, statusprocessing);
#ifdef LWSRV_AUFS_SECURITY
    }
#endif LWSRV_AUFS_SECURITY
    if (!singlefork) {
      slotp = GetSlot(prtno);
      myslot = slotp->slot;
      if ((err = PAPGetNetworkInfo(cno, &slotp->addr)) != noErr) {
	fprintf(stderr, "%s: Get Network info failed with error %d", myname,
	 err);
	PAPClose(cno, TRUE);
	FreeSlot(slotp->slot);
	continue;
      }
      if (logfile != NULL)
	close(2);		/* close log file */
      if ((i = fork()) != 0) {
	PAPShutdown(cno);	/* kill off connection */
	slotp->pid = i;		/* for resetting status */
	prtp->children++;
#ifdef LWSRV_AUFS_SECURITY
	aufssecurity[srefnum] = FALSE;
#endif LWSRV_AUFS_SECURITY
	continue;
      }
      for (i = 0; i < nprt; i++) {
	SLClose(prtlist[i].srefnum);	/* close all servers for child */
	PAPShutdown(prtlist[i].cno);	/* kill off connection */
      }
      nbpShutdown();		/* shutdown nbp for child */
    }
#ifdef LWSRV_AUFS_SECURITY
    if (!aufssecurity[srefnum]) {
      NewStatus(openReplyMessage);
      abSleep(sectotick(20),FALSE);
      PAPClose( cno, TRUE);	/* arg2 is ignored!! */
					/* what does it mean? */
      if (singlefork)
	continue;
      else
	exit(1);
    }
    aufssecurity[srefnum] = FALSE;
#endif LWSRV_AUFS_SECURITY
    /* need for multi-forking, nice for single forking */
    /* handle the connection in cno */
    openlogfile();
#ifdef SOLARIS
    signal(SIGCHLD,SIG_DFL);
#else  SOLARIS
    signal(SIGCHLD,SIG_IGN);
#endif SOLARIS
#ifdef AUTHENTICATE
    if (nprt != 1)
      initauthenticate(*argv, prtp->prtname);
#endif AUTHENTICATE
    childjob(p_opn(cno, BUFMAX)); 
    if (!singlefork)
      exit(0);
    else {
      cpyc2pstr(prtp->statbuf.StatusStr, statusidle);
      *username = 0;
    }
  }
}

#else  /* LWSRV8 */

main(argc,argv)
int argc;
char **argv;
{
  int err,i,cno,errcount;
  int prtno,prtcount;
  void cleanup();
  void childdone();
#ifdef ISO_TRANSLATE
  void cMac2ISO(), cISO2Mac();
#endif ISO_TRANSLATE

  if (myname = rindex(argv[0], '/'))
    myname++;
  else
    myname = argv[0];

  doargs(argc,argv);			/* handle args */
  
#ifdef LWSRV_AUFS_SECURITY
  requid = getuid();			/* budd */
  reqgid = getgid();			/* budd */
#endif LWSRV_AUFS_SECURITY

#ifdef AUTHENTICATE
  if (nprt == 1)
    initauthenticate(*argv, (char *)prtlist[0].prtname);
#endif AUTHENTICATE

  signal(SIGHUP, cleanup);
  signal(SIGINT, cleanup);
  signal(SIGQUIT, cleanup);
  signal(SIGTERM, cleanup);

  if (!dbug.db_flgs) {			/* disassociate */
    if (fork())
      exit(0);				/* kill off parent */
    for (i=0; i < 20; i++)
      close(i);				/* close all files */
    (void) open("/dev/null",0);
#ifndef NODUP2
    (void) dup2(0,1);
#else   NODUP2
    (void)dup(0);			/* for slot 1 */
#endif  NODUP2
    if (logfile == NULL) {
#ifndef NODUP2
      (void) dup2(0,2);
#else   NODUP2
      (void) dup(0);			/* for slot 2 */
#endif  NODUP2
    } else
      openlogfile();
#ifndef POSIX
#ifdef TIOCNOTTY
    if ((i = open("/dev/tty",2)) > 0) {
      ioctl(i, TIOCNOTTY,(char *) 0);
      close(i);
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

/*  dbug.db_pap = TRUE; */
  
  abInit(FALSE);		/* initialize appletalk driver */
  nbpInit();			/* initialize NBP routines */
  PAPInit();			/* init PAP printer routines */

  set_printer(&prtlist[0]);	/* for definiteness */
#ifdef ISO_TRANSLATE
  cMac2ISO(prtname);
  fprintf(stderr,"lwsrv: Spooler starting for %s.%s.*\n",
    (char *)prtname, prttype);
  cISO2Mac(prtname);
#else  ISO_TRANSLATE
  fprintf(stderr,"lwsrv: Spooler starting for %s.%s.*\n",
    (char *)prtname, prttype);
#endif ISO_TRANSLATE
  if (!spool_setup(tracefile, fontfile, (char *)prtname, dictdir)) {
    usage(argv[0]);
    exit(1);
  }

  if (!singlefork)
    signal(SIGCHLD, childdone);

  errcount = 0;
  for (prtp=prtlist,i=0; i<nprt; prtp++,i++) {
      sprintf(prtp->nbpbuf,"%s:%s@*",(char *)prtp->prtname,prttype);
      cpyc2pstr(prtp->statbuf.StatusStr,"status: idle");
      err = SLInit(&(prtp->srefnum), prtp->nbpbuf, 8, &(prtp->statbuf));
      if (err != noErr) {
#ifdef ISO_TRANSLATE
	 cMac2ISO(prtp->nbpbuf);
         fprintf(stderr,"lwsrv: SLInit %s failed: %d\n", prtp->nbpbuf, err);
	 cISO2Mac(prtp->nbpbuf);
#else  ISO_TRANSLATE
         fprintf(stderr,"lwsrv: SLInit %s failed: %d\n", prtp->nbpbuf, err);
#endif ISO_TRANSLATE
	 errcount++;
      }
      /* GetNextJob is asynchronous, so call it for each printer */
      err = GetNextJob(prtp->srefnum, &(prtp->cno), &(prtp->rcomp));
      if (err != noErr) {
#ifdef ISO_TRANSLATE
	cMac2ISO(prtp->prtname);
        fprintf(stderr,"lwsrv: GetNextJob %s failed: %d\n",
	  (char *)prtp->prtname, err);
	cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
        fprintf(stderr,"lwsrv: GetNextJob %s failed: %d\n",
	  (char *)prtp->prtname, err);
#endif ISO_TRANSLATE
        errcount++;
      }
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->nbpbuf);
      fprintf(stderr, "lwsrv: %s ready and waiting\n", prtp->nbpbuf);
      cISO2Mac(prtp->nbpbuf);
#else  ISO_TRANSLATE
      fprintf(stderr, "lwsrv: %s ready and waiting\n", prtp->nbpbuf);
#endif ISO_TRANSLATE
  }

  if (errcount == nprt) {
    fprintf(stderr, "lwsrv: no printers successfully registered!\n");
    exit(1);
  }

  prtno = 0; prtcount = 0;
  while (TRUE) {
#ifdef LWSRV_AUFS_SECURITY
    AddrBlock addr;			/* budd */
#endif LWSRV_AUFS_SECURITY
    openlogfile();
    NewStatus("idle");

  /* scan each printer in turn, processing each one as "normal" if there
   * is work available. If no work was available then abSleep for a while
   */
    while (prtno<nprt && prtlist[prtno].rcomp > 0) prtno++;
    if (prtno == nprt) {
	if (prtcount == 0) {
	    abSleep(20,TRUE);	/* wait for some work */
        }
        prtno = 0;	/* start scan again */
        continue;
    }

    prtp = &prtlist[prtno];
    set_printer(prtp); 	/* set the global variables etc */
    cno = prtp->cno;

    /* GetNextJob is asynchronous, so announce readiness for more work */
    /* WTR: is this right - should it depend on singlefork? */
    err = GetNextJob(prtp->srefnum, &(prtp->cno), &(prtp->rcomp));
    if (err != noErr) {
#ifdef ISO_TRANSLATE
      cMac2ISO(prtp->prtname);
      fprintf(stderr,"lwsrv: GetNextJob %s failed: %d\n",
	(char *)prtp->prtname, err);
      cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
      fprintf(stderr,"lwsrv: GetNextJob %s failed: %d\n",
	(char *)prtp->prtname, err);
#endif ISO_TRANSLATE
    }

#ifdef ISO_TRANSLATE
    cMac2ISO(prtname);
    fprintf(stderr,"lwsrv: Starting print job for %s\n", (char *)prtname);
    cISO2Mac(prtname);
#else  ISO_TRANSLATE
    fprintf(stderr,"lwsrv: Starting print job for %s\n", (char *)prtname);
#endif ISO_TRANSLATE

#ifdef LWSRV_AUFS_SECURITY
    /*** budd.... */
    if ((err = PAPGetNetworkInfo(cno, &addr)) != noErr)
      fprintf(stderr, "Get Network info failed with error %d", err);
    else
      fprintf(stderr,"Connection %d from [Network %d.%d, node %d, socket %d]\n",
	      cno, nkipnetnumber(addr.net),
	      nkipsubnetnumber(addr.net),
	      addr.node, addr.skt);

    if( aufsdb != NULL ) {
	char fname[ 256 ];
	char filename[ 256 ];
	int f, cc, ok = 0;
	struct passwd *pw;
	struct stat statbuf;
#ifdef HIDE_LWSEC_FILE
	char protecteddir[MAXPATHLEN];
	(void) strcpy(protecteddir, aufsdb);
	make_userlogin(filename, protecteddir, addr);
	(void) strcpy(protecteddir, filename);
	filename[0] = '\0';
	make_userlogin(filename, protecteddir, addr);
#else  HIDE_LWSEC_FILE
	make_userlogin( filename, aufsdb, addr );
#endif HIDE_LWSEC_FILE

	if( (f = open( filename, 0)) >= 0) {
	   if( (cc = read( f, fname, sizeof( fname )-1 )) > 0 ) {
	    if( fname[cc-1] == '\n' )
		fname[cc-1] = '\0';
	    fprintf( stderr, "Found username in aufsdb (%s): %s\n",
		    aufsdb, fname );

	    if ((tempbin = rindex(fname,':')) != NULL) {
	      *tempbin='\0';
	      tempbin++;
	      bin=(char *) malloc(strlen(tempbin)+1);
	      strcpy(bin,tempbin);
	    }
	    
	    (void) stat(filename, &statbuf);
	    if ( (pw = getpwuid((int) statbuf.st_uid)) != NULL ) {
	      if ( strcmp(pw->pw_name, fname) == 0) {
		requid = pw->pw_uid;
		reqgid = pw->pw_gid;
#ifdef LWSRV_LPR_LOG
		requname = pw->pw_name;
#endif LWSRV_LPR_LOG
		ok = 1;
	      }
	    }
	    else
	      if( (pw = getpwnam( fname )) != NULL ) {
		requid = pw->pw_uid;
		reqgid = pw->pw_gid;
#ifdef LWSRV_LPR_LOG
		requname = pw->pw_name;
#endif LWSRV_LPR_LOG
		ok = 1;		/* true */
	      } /* pwnam ok */
	    } /* read OK */
	    close(f);  /* close the file */
	  } /* open OK */
	if( !ok ) {	/* dump connection with error message */
	    char message[ 512 ];
	    fprintf( stderr, "No username (or invalid) confirmation.\n");
	    gethostname( fname, sizeof( fname )-1 ); /* ick */

	    /* THIS DOES NOT WORK! */
	    sprintf(message, "error: please login to host %s via appleshare",
		    fname );
	    NewStatus( message );
	    sleep( 3 );			/* bleh!! */
/*	    PAPShutdown( cno );		/*  */
	    PAPClose( cno, TRUE );	/* arg2 is ignored!! */
					/* what does it mean? */

	    continue;
	} /* not ok */
    } /* have aufsdb */
    /* ....budd ***/
#endif LWSRV_AUFS_SECURITY

    if (!singlefork) {
      if (logfile != NULL)
	close(2);		/* close log file */
      if (fork() != 0) {
	PAPShutdown(cno);	/* kill off connection */
	continue;
      }
      SLClose(srefnum);		/* close server for child */
      nbpShutdown();		/* shutdown nbp for child */
    } else NewStatus("busy, processing job");
    /* need for multi-forking, nice for single forking */
    /* handle the connection in cno */
    openlogfile();
#ifdef SOLARIS
    signal(SIGCHLD,SIG_DFL);
#else  SOLARIS
    signal(SIGCHLD,SIG_IGN);
#endif SOLARIS
#ifdef AUTHENTICATE
    if (nprt != 1)
      initauthenticate(*argv, (char *)prtlist[prtno].prtname);
#endif AUTHENTICATE
    childjob(p_opn(cno, BUFMAX)); 
    if (!singlefork)
      exit(0);
  }
}
#endif /* LWSRV8 */

#ifdef RUN_AS_USER
/*
 * Conversion table, macintosh ascii with diacriticals to plain ascii.
 * In addition, an ':' also maps to an underscore for obvious reasons.
 * All other junk maps to an underscore.
 *
 */

static unsigned char convert [256] = {
/*       0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
/* 0 */ '_','_','_','_','_','_','_','_','_','_','_','_','_','_','_','_',
/* 1 */ '_','_','_','_','_','_','_','_','_','_','_','_','_','_','_','_',
/* 2 */ ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
/* 3 */ '0','1','2','3','4','5','6','7','8','9','_',';','<','=','>','?',
/* 4 */ '@','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
/* 5 */ 'p','q','r','s','t','u','v','w','x','y','z','[','\\',']','^','_',
/* 6 */ '`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
/* 7 */ 'p','q','r','s','t','u','v','w','x','y','z','{','|','}','~','_',
/* 8 */ 'a','a','c','e','n','o','u','a','a','a','a','a','a','c','e','e',
/* 9 */ 'e','e','i','i','i','i','n','o','o','o','o','o','u','u','u','u',
/* a */ '_','_','c','$','_','*','_','_','r','c','_','_','_','_','_','o',
/* b */ '_','_','_','_','_','m','d','s','p','p','_','a','o','_','_','o',
/* c */ '?','!','_','_','_','_','_','_','_','_','_','a','a','o','_','_',
/* d */ '_','_','_','_','_','_','_','_','y','_','_','_','_','_','_','_',
/* e */ '_','_','_','_','_','_','_','_','_','_','_','_','_','_','_','_',
/* f */ '_','_','_','_','_','_','_','_','_','_','_','_','_','_','_','_'
};

static void
unixize(name)
char *name;
{
  register unsigned char *p = (unsigned char *)name;

  while (*p) {
    *p = convert[*p];
    p += 1;
  }
}

static char *
getmacname(name, line, file)
char *name; /* mac name */
char *line; /* line buffer to use */
char *file; /* name of file containing table */
{
  FILE *f;
  char *m, *nl;

  if ((f = fopen(file, "r")) == NULL)
    return(NULL);

  while (fgets(line, 255, f) != NULL) {
    if (*line == '#' || *line == '\n')
      continue;		/* skip comment line */
    if ((m = index(line, ':')) == NULL)
      continue;		/* line in error */
    *m++ = '\0';
    if ((nl = index(m, '\n')) != NULL)
      *nl = '\0';	/* take trailing newline */
    unixize(m);
    if (strcmp(m, name) == 0) {
      fclose(f);
      return(line);
    }
  }
  fclose(f);
  return(NULL);
}

/*
 * We try to find a unix username to match the name from the mac.
 * The approach is simple. We use a simple file to convert mac names
 * to unix login names. The file needs only contain mac names that can't
 * be resolved into unix names by simple means. The other ones are found
 * automaticly. This means that 'Maarten' is resolved into 'maarten' without
 * intervention, whereas 'El Gringo' needs to be in the file to be resolved
 * to 'dolf'.
 *
 * Example file:
 * #comment line
 * dolf:El Gringo
 *
 */

static int
unixuid(macname)
char *macname;
{
  struct passwd  *pw;
  char line[256];
  char name[256];
  char *n;

  strcpy(name, macname);
  unixize(name);

  if ((n = getmacname(name, line, USER_FILE)) == NULL)
    n = name;

  if ((pw = getpwnam(n)) == NULL)
    return(0);
  else
    return(pw->pw_uid);
}
#endif RUN_AS_USER

#ifdef LWSRV8
export
childjob(pf)
PFILE *pf;
{
  long t;
  int argc, i;
  FILE *outfile;
  char tname[256],status[256];
  char pbuf[256],rhbuf[16],jbuf[1024];
  char *childargv[64];
#ifdef LWSRV_AUFS_SECURITY
  char bbuf[256];
#endif LWSRV_AUFS_SECURITY
#ifdef NeXT
  char dpistring[6];
#endif NeXT
#ifdef RUN_AS_USER
  int uid;
#endif RUN_AS_USER

  int waitret;
  WSTATUS waitstatus;

  int actualprintout;
  int doeof;

#ifdef AUTHENTICATE
  register PAPSOCKET *ps;
  register unsigned net;
#ifdef ISO_TRANSLATE
  void cMac2ISO(), cISO2Mac();
#endif ISO_TRANSLATE

  if ((ps = cnotopapskt(pf->p_cno)) == NULL || !authenticate(net =
   ntohs(ps->addr.net), ps->addr.node)) {
    p_cls(pf);				/* close out the pap connection */
    (void)time(&t);
#ifdef TIMESTAMP
    fprintf(stderr,"%s: %s: Authentication failed: net %u.%u node %u\n",
     myname, timestamp(), ((unsigned)net >> 8), (unsigned)(net & 0xff),
     (unsigned)ps->addr.node);
#else TIMESTAMP
    fprintf(stderr,"%s: Authentication failed: net %u.%u node %u\n",
     myname, ((unsigned)net >> 8), (unsigned)(net & 0xff),
     (unsigned)ps->addr.node);
#endif TIMESTAMP
    return;
  }
#endif AUTHENTICATE
  
  jobname[0] = username[0] = '\0';

  if (tracing) {		/* is this a trace? */
    strcpy(tname,prtp->tracefile);	/* yes... then output is tracefile */
    if ((outfile = fopen(tname, "w+")) == NULL)
      perror(tname);
#ifndef aux
#if defined(__hpux) || defined(SOLARIS)
    if (setvbuf(outfile, NULL, _IOLBF, 0) != 0)
      perror(tname);
#else  /* __hpux || SOLARIS */
    setlinebuf(outfile);
#endif /* __hpux || SOLARIS */
#endif aux
    chmod(tname, 0644);
  } else {				/* otherwise use a temp file */
    if (tmpfiledir != NULL) {
      strcpy(tname,tmpfiledir);
      strcat(tname,"/lwsrvXXXXXX");
    } else
      strcpy(tname,TEMPFILE);
    mktemp(tname);
    if ((outfile = fopen(tname, "w+")) == NULL)
      perror(tname);
    chmod(tname, 0600);
  }

  NewStatus(prtp->children <= 0 ? "initializing" : statusprocessing);
#ifdef ISO_TRANSLATE
  cMac2ISO(prtp->prtname);
  sprintf(status,"receiving job for %s", prtp->prtname);
  cISO2Mac(prtp->prtname);
#else  ISO_TRANSLATE
  sprintf(status,"receiving job for %s", prtp->prtname);
#endif ISO_TRANSLATE

  pagecount = -1;
#ifdef PAGECOUNT
  pcopies = 0;
#endif PAGECOUNT

  /*
   * Set actualprintout to -1 by default (for non-dcs conforming documents);
   * If we get %!PS-Adobe, set it to zero if it is still negative.
   * On a %%Page, set it to 1.
   *
   */
  actualprintout = -1;
  while (getjob(pf,outfile,&actualprintout,&doeof)) { /* while still open... */
    NewStatus(status);
    /* don't send out real eof - causes real problems */
    /* given that we are prepending procsets */
    if (doeof)
      fprintf(outfile,"%% *EOF*\n");
  }

  fclose(outfile);

  (void) time(&t);

#ifdef RUN_AS_USER
#ifdef LWSRV_AUFS_SECURITY
  if (((uid = requid) != 0)
   || (uid = unixuid(username))) {
#else  LWSRV_AUFS_SECURITY
  if ((uid = unixuid(username))) {
#ifdef LWSRV_LPR_LOG
    requname = username;
#endif LWSRV_LPR_LOG
#endif LWSRV_AUFS_SECURITY
    chown(tname, uid, -1);
  }
#ifdef USER_REQUIRED
  else if (actualprintout) {
    int n;
    FILE *infile;
    char buffer[BUFSIZ];

#ifdef TIMESTAMP
    fprintf(stderr, "%s: %s: Job refused for macuser %s\n",
     myname, timestamp(), username);
#else TIMESTAMP
    fprintf(stderr, "%s: Job refused for macuser %s\n", myname, username);
#endif TIMESTAMP
    NewStatus ("Unknown user, job refused");
    unlink(tname);
    if ((outfile = fopen(tname, "w+")) != NULL) {
      fprintf(outfile, "\n\nMacintosh user name: %s\nJob: %s\n",
          username, jobname);
      fprintf(outfile, "\nJob refused. ");
      fprintf(outfile,
        "Can't map Macintosh user name to a Unix user name\n\n");
      if ((infile = fopen(REFUSE_MESSAGE, "r")) != NULL) {
        while ((n = fread(buffer, 1, BUFSIZ, infile)) > 0)
          fwrite(buffer, 1, n, outfile);
        fclose(infile);
      } else
        fprintf(outfile, "No detailed message available\n");
      fclose(outfile);
    }
  }
#endif USER_REQUIRED
#endif RUN_AS_USER

  if (tracing)
#ifdef TIMESTAMP
    fprintf(stderr,"%s: %s: Tracing to file: %s; job %s; user %s\n",
	    myname,timestamp(),prtp->tracefile,jobname,username);
#else TIMESTAMP
    fprintf(stderr,"%s: Tracing to file: %s; job %s; user %s\n",
	    myname,prtp->tracefile,jobname,username);
#endif TIMESTAMP
  else if (!actualprintout) {
#ifdef TIMESTAMP
    fprintf(stderr,"%s: %s: No actual output for job %s; user %s\n",
	    myname,timestamp(),jobname,username);
#else TIMESTAMP
    fprintf(stderr,"%s: No actual output for job %s; user %s\n",
	    myname,jobname,username);
#endif TIMESTAMP
    unlink(tname);
  } else {
    if (rflag)
      fprintf(stderr,"%s: Preserving file in %s\n",myname,tname);

/*
 * this way lies madness ...
 */

#ifdef RUN_AS_USER
    if (uid)
#endif RUN_AS_USER
#ifdef TIMESTAMP
      fprintf(stderr,"%s: %s: Printing job: %s; user %s\n",
	    myname,timestamp(),jobname,username);
#else TIMESTAMP
      fprintf(stderr,"%s: Printing job: %s; user %s\n",
	    myname,jobname,username);
#endif TIMESTAMP
#ifdef RUN_AS_USER
    else
#ifdef TIMESTAMP
      fprintf(stderr,"%s: %s: Printing notification: %s; user %s\n",
	    myname,timestamp(),jobname,username);
#else TIMESTAMP
      fprintf(stderr,"%s: Printing notification: %s; user %s\n",
	    myname,jobname,username);
#endif TIMESTAMP
#endif RUN_AS_USER

	argc = 0;
#ifdef USESYSVLP
	childargv[argc++]="lp";
#else  USESYSVLP
	childargv[argc++]="lpr";
#endif USESYSVLP
#ifdef MELBOURNE
	childargv[argc++]="-v";
#endif MELBOURNE
#ifdef VUW
	childargv[argc++]="-l";
#endif VUW
#ifdef xenix5
	childargv[argc++]="-og";
	sprintf(pbuf,"-d%s",prtp->unixpname); /* name of the printer */
	childargv[argc++]=pbuf;
#else xenix5
#ifdef USESYSVLP
	sprintf(pbuf,"-d%s",prtp->unixpname); /* name of the printer */
#else  USESYSVLP
	sprintf(pbuf,"-P%s",prtp->unixpname); /* name of the printer */
#endif USESYSVLP
	childargv[argc++]=pbuf;
	if (hflag) {			/* want a burst page */
#ifdef USESYSVLP
#ifdef DOCNAME
#ifdef __hpux
	  for (i = 0; i < MAXJOBNAME-1; i++)
	    if (isspace(jobname[i]))
	      jobtitle[i] = '_';
	    else
	      jobtitle[i] = jobname[i];
	  jobtitle[MAXJOBNAME-1] = '\0';
	  sprintf(jbuf,"-tMacJobName:%s",jobtitle);
#else  __hpux
	  sprintf(jbuf,"-tMacUser: %s Job: %s",username,jobname); /* job name */
#endif __hpux
#else  DOCNAME
	  sprintf(jbuf,"-tMacUser: %s",username); /* job name */
#endif DOCNAME
#else  USESYSVLP
	  childargv[argc++]="-J";
#ifdef DOCNAME
	  sprintf(jbuf,"MacUser: %s Job: %s",username,jobname); /* job name */
#else  DOCNAME
	  sprintf(jbuf,"MacUser: %s",username); /* job name */
#endif DOCNAME
#endif USESYSVLP
#ifdef PAGECOUNT
	  if (pagecount >= 0) {
	    if (pcopies <= 0)
	      pcopies = 1;
#ifdef __hpux
	    sprintf(&jbuf[strlen(jbuf)], "_Pages:_%04d", pcopies*pagecount);
#else  __hpux
	    sprintf(&jbuf[strlen(jbuf)], " Pages: %d", pcopies*pagecount);
#endif __hpux
	  }
#endif PAGECOUNT
	  childargv[argc++]=jbuf;
        }
#ifndef hpux
	else
#ifdef USESYSVLP
# ifdef SOLARIS
	  childargv[argc++]="-onobanner";	/* suppress burst page */
# else  SOLARIS
	  childargv[argc++]="-o-h";		/* suppress burst page */
# endif SOLARIS
#else  USESYSVLP
	  childargv[argc++]="-h";		/* suppress burst page */
#endif USESYSVLP
#endif hpux
#endif xenix5
#ifdef LWSRV_AUFS_SECURITY
	if (aufsdb && bin != NULL) {
#ifdef RUTGERS
	  sprintf(bbuf, "-B%s", bin);
#else  RUTGERS
	  sprintf(bbuf, "-C%s", bin);
#endif RUTGERS
	  childargv[argc++]=bbuf;
	}
#endif LWSRV_AUFS_SECURITY
#ifdef NeXT
        if (nextdpi) {
	  sprintf(dpistring, "-R%s", nextdpi);
	  childargv[argc++]=dpistring;
	}
#endif NeXT
        rhbuf[0] = rhbuf[1] = '\0';
#ifdef xenix5
	/* will this work ... ? */
	sprintf(rhbuf,"-%s%s",rflag ? "" : "r",hflag ? "" : "ob");
#else  xenix5
#if defined(__hpux) || defined(SOLARIS)
	sprintf(rhbuf,"-c");
#else  /* __hpux || SOLARIS */
	sprintf(rhbuf,"-%s",rflag ? "" : "r");
#endif /* __hpux || SOLARIS */
#endif xenix5
	if (rhbuf[1] != '\0') 
	  childargv[argc++]=rhbuf;	/* include h and/or r flags */
#ifdef USELPRSYM
#ifndef USESYSVLP
	childargv[argc++]="-s";		/* better for > 1M files */
#endif USESYSVLP
#endif USELPRSYM
#ifdef LPRARGS
	if (lprargs) {
	  register int n = NList(lprargs);
	  register char **ip = (char **)AddrList(lprargs);

	  while (n-- > 0)
	    childargv[argc++] = *ip++;
	}
#endif LPRARGS
        childargv[argc++]=tname;	/* our temporary file name */
        childargv[argc]=(char *) 0;	/* end of argument list */

	switch (fork()) {
	case 0:
#ifdef RUN_AS_USER
#ifndef LWSRV_AUFS_SECURITY
	    setuid(uid);
#endif  LWSRV_AUFS_SECURITY
#endif RUN_AS_USER
#ifdef LWSRV_AUFS_SECURITY
	    if (aufsdb) {
	/*
	 * dissassociate from any tty to make sure
	 * lpr uses our requid instead of getlogin()
	 *
	 */
#ifndef LWSRV_LPR_LOG
		for (i = 0 ; i < 10 ; i++)
	          (void) close(i);
	 	(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
#else LWSRV_LPR_LOG
		for (i = 3 ; i < 10 ; i++)
		  (void) close(i);
	/* 
	 * stderr is the logfile; stdin and stdout are already 
	 * using /dev/null, therefore disassociated from any tty;
	 * The Rutgers hack requires stdout to be the logfile if
	 * one exists.
	 *
	 */
		/* (void) open("/", 0);	*/
		/* (void) dup2(0, 1);	*/
		/* (void) dup2(0, 2);	*/
#endif LWSRV_LPR_LOG
		chown(tname, requid, reqgid);
		setuid(requid);
		setgid(reqgid);
	    }
#endif LWSRV_AUFS_SECURITY
#ifdef LWSRV_LPR_LOG
	/*
	 * log all lpr invocations for troubleshooting
	 * printing problems
	 *
	 */
#ifdef TIMESTAMP
	    fprintf(stderr, "%s: %s: Invoking lpr as user %s using execv: %s",
	        myname, timestamp(), (requname) ? requname : "<unknown>",
		lprcmd);
#else TIMESTAMP
	    fprintf(stderr, "%s: Invoking lpr as user %s using execv: %s",
	        myname, (requname) ? requname : "<unknown>", lprcmd);
#endif TIMESTAMP
	    for ( i = 1 ; i < argc ; i++) 
	      fprintf(stderr," %s",childargv[i]);
	    fprintf(stderr, "\n");
#endif LWSRV_LPR_LOG
	    if (execv(lprcmd,childargv)) {
#ifdef TIMESTAMP
		fprintf(stderr,"%s: %s: exec of %s failed\n",
		 myname, timestamp(), lprcmd);
#else TIMESTAMP
		fprintf(stderr,"%s: exec of %s failed\n", myname, lprcmd);
#endif TIMESTAMP
		perror("exec");
		exit(-1);
	    }
	    break;
	case -1:
#ifdef TIMESTAMP
	    fprintf(stderr,"%s: %s: fork failed trying to run %s\n",
	     myname,timestamp(),lprcmd);
#else TIMESTAMP
	    fprintf(stderr,"%s: fork failed trying to run %s\n",myname,lprcmd);
#endif TIMESTAMP
	    perror("fork");
	    break;
	default:

	/*
	 * This code is important, since lwsrv relies upon lpr to remove
	 * the temporary files, we must remove them if lpr fails for any
	 * reason (unless rflag TRUE).
	 *
	 */

	    /* wait for the lpr to do its thing */

	    if ((waitret = wait(&waitstatus)) == -1) {
#ifdef UTS
		waitstatus = 0; /* wait can return early */
#else  UTS
		perror("wait");
		exit(-1);
#endif UTS
	    }

#if defined(hpux) || defined(SOLARIS)
	    if (!rflag)
	        unlink(tname);
#else  /* hpux || SOLARIS */
	    if (WIFEXITED(waitstatus)) {
		if (W_RETCODE(waitstatus) != 0) {
#ifdef TIMESTAMP
		  fprintf(stderr,
		    "%s: %s: lpr exited with status %d, %sremoving %s\n",
		    myname, timestamp(), W_RETCODE(waitstatus),
		    (rflag) ? "not " : "", tname);
#else TIMESTAMP
		  fprintf(stderr,
		    "%s: lpr exited with status %d, %sremoving %s\n", myname,
		    W_RETCODE(waitstatus), (rflag) ? "not " : "", tname);
#endif TIMESTAMP
		  if (!rflag)
		    unlink(tname);
		}
	    }
#endif /* hpux || SOLARIS */
	    break;
	}
  }
  p_cls(pf);				/* close out the pap connection */
}

#else  /* LWSRV8 */

export
childjob(pf)
PFILE *pf;
{
  long t;
  int argc, i;
  FILE *outfile;
  char tname[256],status[256];
  char pbuf[256],rhbuf[16],jbuf[1024];
  char *childargv[64];
#ifdef LWSRV_AUFS_SECURITY
  char bbuf[256];
#endif LWSRV_AUFS_SECURITY
#ifdef NeXT
  char dpistring[6];
#endif NeXT
#ifdef RUN_AS_USER
  int uid;
#endif RUN_AS_USER

  int waitret;
  WSTATUS waitstatus;

#ifdef AUTHENTICATE
  register PAPSOCKET *ps;
  register unsigned net;
#ifdef ISO_TRANSLATION
  void cMac2ISO(), cISO2Mac();
#endif ISO_TRANSLATION

  if((ps = cnotopapskt(pf->p_cno)) == NULL || !authenticate(net =
   ntohs(ps->addr.net), ps->addr.node)) {
    p_cls(pf);				/* close out the pap connection */
    (void)time(&t);
    fprintf(stderr,"lwsrv: Authentication failed: net %u.%u node %u; on %s",
     ((unsigned)net >> 8), (unsigned)(net & 0xff), (unsigned)ps->addr.node,
     ctime(&t));
    return;
  }
#endif AUTHENTICATE
  
  jobname[0] = username[0] = '\0';

  if (tracefile != NULL)		/* is this a trace? */
    strcpy(tname,tracefile);		/* yes... then output is tracefile */
  else {				/* otherwise use a temp file */
    if (tmpfiledir != NULL) {
      strcpy(tname,tmpfiledir);
      strcat(tname,"/lwsrvXXXXXX");
    } else
      strcat(tname,TEMPFILE);
    mktemp(tname);
  }
  
  if ((outfile = fopen(tname, "w+")) == NULL)
    perror(tname);

  chmod(tname, 0600);

  if (singlefork)
    NewStatus("initializing");

#ifdef ISO_TRANSLATE
  cMac2ISO(prtname);
  sprintf(status,"receiving job for %s", (char *)prtname);
  cISO2Mac(prtname);
#else  ISO_TRANSLATE
  sprintf(status,"receiving job for %s", (char *)prtname);
#endif ISO_TRANSLATE

  scandicts(dictdir);

#ifdef PAGECOUNT
  pagecount = -1;
  pcopies = 0;
#endif PAGECOUNT

  while (getjob(pf,outfile)) {	/* while still open... */
    if (singlefork)
      NewStatus(status);
    /* don't send out real eof - causes real problems */
    /* given that we are prepending procsets */
    fprintf(outfile,"%% *EOF*\n");
  }

  fclose(outfile);

  (void) time(&t);

#ifdef RUN_AS_USER
#ifdef LWSRV_AUFS_SECURITY
  if (((uid = requid) != 0)
   || (uid = unixuid(username)))
#else  LWSRV_AUFS_SECURITY
  if ((uid = unixuid(username)))
#endif LWSRV_AUFS_SECURITY
    chown(tname, uid, -1);
#ifdef USER_REQUIRED
  else {
    int n;
    FILE *infile;
    char buffer[BUFSIZ];

    fprintf(stderr, "lwsrv: Job refused for macuser %s\n", username);
    /* NewStatus ("Unknown user, job refused"); */
    unlink(tname);
    if ((outfile = fopen(tname, "w+")) != NULL) {
      fprintf(outfile, "\n\nMacintosh user name: %s\nJob: %s\n",
          username, jobname);
      fprintf(outfile, "\nJob refused. ");
      fprintf(outfile,
        "Can't map Macintosh user name to a Unix user name\n\n");
      if ((infile = fopen(REFUSE_MESSAGE, "r")) != NULL) {
        while ((n = fread(buffer, 1, BUFSIZ, infile)) > 0)
          fwrite(buffer, 1, n, outfile);
        fclose(infile);
      } else
        fprintf(outfile, "No detailed message available\n");
      fclose(outfile);
    }
  }
#endif USER_REQUIRED
#endif RUN_AS_USER

  if (tracefile != NULL)
    fprintf(stderr,"lwsrv: Tracing to file: %s; job %s; user %s; on %s\n",
	    tracefile,jobname,username,ctime(&t));
  else {
    if (rflag)
      fprintf(stderr,"lwsrv: Preserving file in %s\n",tname);

/*
 * this way lies madness ...
 */

#ifdef RUN_AS_USER
    if (uid)
#endif RUN_AS_USER
      fprintf(stderr,"lwsrv: Printing job: %s; user %s; on %s\n",
	    jobname,username,ctime(&t));
#ifdef RUN_AS_USER
    else
      fprintf(stderr,"lwsrv: Printing notification: %s; user %s; on %s\n",
	    jobname,username,ctime(&t));
#endif RUN_AS_USER

	argc = 0;
#ifdef USESYSVLP
	childargv[argc++]="lp";
#else  USESYSVLP
	childargv[argc++]="lpr";
#endif USESYSVLP
#ifdef MELBOURNE
	childargv[argc++]="-v";
#endif MELBOURNE
#ifdef VUW
	childargv[argc++]="-l";
#endif VUW
#ifdef xenix5
	childargv[argc++]="-og";
	sprintf(pbuf,"-d%s",unixpname); /* name of the printer */
	childargv[argc++]=pbuf;
#else xenix5
#ifdef USESYSVLP
	sprintf(pbuf,"-d%s",unixpname); /* name of the printer */
#else  USESYSVLP
	sprintf(pbuf,"-P%s",unixpname); /* name of the printer */
#endif USESYSVLP
	childargv[argc++]=pbuf;
	if (hflag) {			/* want a burst page */
#ifdef USESYSVLP
#ifdef DOCNAME
#ifdef __hpux
	  for (i = 0; i < MAXJOBNAME-1; i++)
	    if (isspace(jobname[i]))
	      jobtitle[i] = '_';
	    else
	      jobtitle[i] = jobname[i];
	  jobtitle[MAXJOBNAME-1] = '\0';
	  sprintf(jbuf,"-tMacJobName:%s",jobtitle);
#else  __hpux
	  sprintf(jbuf,"-tMacUser: %s Job: %s",username,jobname); /* job name */
#endif __hpux
#else  DOCNAME
	  sprintf(jbuf,"-tMacUser: %s",username); /* job name */
#endif DOCNAME
#else  USESYSVLP
	  childargv[argc++]="-J";
#ifdef DOCNAME
	  sprintf(jbuf,"MacUser: %s Job: %s",username,jobname); /* job name */
#else  DOCNAME
	  sprintf(jbuf,"MacUser: %s",username); /* job name */
#endif DOCNAME
#endif USESYSVLP
#ifdef PAGECOUNT
	  if (pagecount >= 0) {
	    if (pcopies <= 0)
	      pcopies = 1;
#ifdef __hpux
	    sprintf(&jbuf[strlen(jbuf)], "_Pages:_%04d", pcopies*pagecount);
#else  __hpux
	    sprintf(&jbuf[strlen(jbuf)], " Pages: %d", pcopies*pagecount);
#endif __hpux
	  }
#endif PAGECOUNT
	  childargv[argc++]=jbuf;
        }
#ifndef hpux
	else
#ifdef USESYSVLP
# ifdef SOLARIS
	  childargv[argc++]="-onobanner";	/* suppress burst page */
# else  SOLARIS
	  childargv[argc++]="-o-h";		/* suppress burst page */
# endif SOLARIS
#else  USESYSVLP
	  childargv[argc++]="-h";		/* suppress burst page */
#endif USESYSVLP
#endif hpux
#endif xenix5
#ifdef LWSRV_AUFS_SECURITY
	if (aufsdb && bin != NULL) {
#ifdef RUTGERS
	  sprintf(bbuf, "-B%s", bin);
#else  RUTGERS
	  sprintf(bbuf, "-C%s", bin);
#endif RUTGERS
	  childargv[argc++]=bbuf;
	}
#endif LWSRV_AUFS_SECURITY
#ifdef NeXT
        if (nextdpi) {
	  sprintf(dpistring, "-R%s", nextdpi);
	  childargv[argc++]=dpistring;
	}
#endif NeXT
        rhbuf[0] = rhbuf[1] = '\0';
#ifdef xenix5
	/* will this work ... ? */
	sprintf(rhbuf,"-%s%s",rflag ? "" : "r",hflag ? "" : "ob");
#else  xenix5
#if defined(__hpux) || defined(SOLARIS)
	sprintf(rhbuf,"-c");
#else  /* __hpux || SOLARIS */
	sprintf(rhbuf,"-%s",rflag ? "" : "r");
#endif /* __hpux || SOLARIS */
#endif xenix5
	if (rhbuf[1] != '\0') 
	  childargv[argc++]=rhbuf;	/* include h and/or r flags */
#ifdef USELPRSYM
#ifndef USESYSVLP
	childargv[argc++]="-s";		/* better for > 1M files */
#endif USESYSVLP
#endif USELPRSYM
#ifdef LPRARGS
	while(*lprargs)
	  childargv[argc++] = *lprargs++;
	lprargs=lprargsbuf;
#endif LPRARGS
        childargv[argc++]=tname;	/* our temporary file name */
        childargv[argc]=(char *) 0;	/* end of argument list */

	switch (fork()) {
	case 0:
#ifdef RUN_AS_USER
#ifndef LWSRV_AUFS_SECURITY
	    setuid(uid);
#endif  LWSRV_AUFS_SECURITY
#endif RUN_AS_USER
#ifdef LWSRV_AUFS_SECURITY
	    if (aufsdb) {
	/*
	 * dissassociate from any tty to make sure
	 * lpr uses our requid instead of getlogin()
	 *
	 */
#ifndef LWSRV_LPR_LOG
		for(i = 0 ; i < 10 ; i++)
	          (void) close(i);
	 	(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
#else LWSRV_LPR_LOG
		for(i = 3 ; i < 10 ; i++)
		  (void) close(i);
	/* 
	 * stderr is the logfile; stdin and stdout are already 
	 * using /dev/null, therefore disassociated from any tty;
	 * The Rutgers hack requires stdout to be the logfile if
	 * one exists.
	 *
	 */
		/* (void) open("/", 0);	*/
		/* (void) dup2(0, 1);	*/
		/* (void) dup2(0, 2);	*/
#endif LWSRV_LPR_LOG
		chown(tname, requid, reqgid);
		setuid(requid);
		setgid(reqgid);
	    }
#endif LWSRV_AUFS_SECURITY
#ifdef LWSRV_LPR_LOG
	/*
	 * log all lpr invocations for troubleshooting
	 * printing problems
	 *
	 */
	    fprintf(stderr, "Invoking lpr as user %s using execv: %s",
	        (requname) ? requname : "<unknown>", lprcmd);
	    for ( i = 1 ; i < argc ; i++) 
	      fprintf(stderr," %s",childargv[i]);
	    fprintf(stderr, "\n");
#endif LWSRV_LPR_LOG
	    if (execv(lprcmd,childargv)) {
		fprintf(stderr,"exec of %s failed\n", lprcmd);
		perror("exec");
		exit(-1);
	    }
	    break;
	case -1:
	    fprintf(stderr,"fork failed trying to run %s\n", lprcmd);
	    perror("fork");
	    break;
	default:

	/*
	 * This code is important, since lwsrv relies upon lpr to remove
	 * the temporary files, we must remove them if lpr fails for any
	 * reason (unless rflag TRUE).
	 *
	 */

	    /* wait for the lpr to do its thing */

	    if ((waitret = wait(&waitstatus)) == -1 ) {
#ifdef UTS
		waitstatus = 0; /* wait can return early */
#else  UTS
		perror("wait");
		exit(-1);
#endif UTS
	    }

#if defined(hpux) || defined(SOLARIS)
	    if (!rflag)
	        unlink(tname);
#else  /* hpux || SOLARIS */
	    if (WIFEXITED(waitstatus)) {
		if (W_RETCODE(waitstatus) != 0 ) {
		  fprintf(stderr,"lpr exited with status %d, %sremoving %s\n",
		    W_RETCODE(waitstatus), (rflag) ? "not " : "", tname);
		  if (!rflag)
		    unlink(tname);
		}
	    }
#endif /* hpux || SOLARIS */
	    break;
	}
  }
  p_cls(pf);				/* close out the pap connection */
}
#endif /* LWSRV8 */

export void
setjobname(ts)
register char *ts;
{
#ifndef LWSRV8
  strncpy(jobname, ts, sizeof(jobname));
#else /* LWSRV8 */
  strncpy(jobname, ts, sizeof(jobname) - 1);
  jobname[sizeof(jobname) - 1] = 0;
#ifdef JOBNOPAREN
  /*
   * replace parenthesis so that jobname doesn't screw up banner pages 
   * for some print spoolers
   */
  while (ts = index(jobname, '('))
    *ts = '[';
  while (ts = index(jobname, ')'))
    *ts = ']';
#endif JOBNOPAREN
#endif /* LWSRV8 */
}

setusername(ts)
register char *ts;
{
#ifdef RUN_AS_USER
  if (*username != '\0')
	return;
#endif RUN_AS_USER
  strcpy(username, ts);
}

export
NewStatus(status)
char *status;
{
  char tmp[1024];

  if (*username != '\0')
#ifndef LWSRV8
    sprintf(tmp,"job: %s for %s; status: %s", jobname,username,status);
#else /* LWSRV8 */
    sprintf(tmp,"%s; document: %s: status: %s", username, jobname, status);
#endif /* LWSRV8 */
  else
    sprintf(tmp,"status: %s",status);
#ifndef LWSRV8
  cpyc2pstr(statbuffp->StatusStr, tmp);
#else /* LWSRV8 */
  if (singlefork)
    cpyc2pstr(prtlist[prtno].statbuf.StatusStr, tmp);
  else
    writestatus(myslot, tmp);
#endif /* LWSRV8 */
  abSleep(0,TRUE);			/* make sure protocol runs */
}

#ifdef LWSRV_AUFS_SECURITY
/**************** budd... ****************/
/* duplicated in aufs.c and lwsrv.c sigh */
make_userlogin(buf, dir, addrb)
    char *buf, *dir;
#ifndef LWSRV8
    AddrBlock addrb;
#else /* LWSRV8 */
    AddrBlock *addrb;
#endif /* LWSRV8 */
{
    sprintf( buf, "%s/net%d.%dnode%d",
	    dir,
#ifndef LWSRV8
	    nkipnetnumber(addrb.net), nkipsubnetnumber(addrb.net),
	    addrb.node);
#else /* LWSRV8 */
	    nkipnetnumber(addrb->net), nkipsubnetnumber(addrb->net),
	    addrb->node);
#endif /* LWSRV8 */
}
#endif LWSRV_AUFS_SECURITY

#ifdef LWSRV8
#ifdef TIMESTAMP
private char *
timestamp()
{
  char *cp;
  time_t t;

  time(&t);
  cp = ctime(&t);
  cp[24] = 0;	/* kill newline */
  return(cp);
}
#endif TIMESTAMP
#endif /* LWSRV8 */
