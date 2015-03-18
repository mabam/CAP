static char rcsid[] = "$Author: djh $ $Date: 1992/07/30 09:52:59 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/uab.c,v 2.3 1992/07/30 09:52:59 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * UAB - Unix AppleTalk Bridge
 *
 *  AppleTalk Bridge For Unix
 *
 *  written by Charlie C. Kim
 *     Academic Computing and Communications Group
 *     Center For Computing Activities
 *     Columbia University
 *   August 1988
 *
 *
 * Copyright (c) 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Permission is granted to any individual or institution to use,
 * copy, or redistribute this software so long as it is not sold for
 * profit, provided that this notice and the original copyright
 * notices are retained.  Columbia University nor the author make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * Edit History:
 *
 *  April 3, 1988	CCKim			Created
 *  December 2, 1990	djh@munnari.OZ.AU	add async appletalk support
 *
*/

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>
#endif
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>

#include <netat/appletalk.h>
#include <netat/compat.h>
#include "mpxddp.h"
#include "gw.h"
#include "if_desc.h"
#include "log.h"

/* bridge description file */
#ifndef BRIDGE_DESC
# define BRIDGE_DESC "bridge_desc"
#endif
private char *bd_file = BRIDGE_DESC; /* default bridge description */

private int debug = 0;
private int have_log = 0;

/* import  link access protocol descriptions */
extern struct lap_description ethertalk_lap_description;
extern struct lap_description asyncatalk_lap_description;

/* IMPORT: multiplex ddp descriptions */
/* import mpx ddp module udpcap */
extern struct mpxddp_module mkip_mpx; /* modified kip udp port range */
extern struct mpxddp_module kip_mpx; /* no rebroadcaster allowed, */

private void disassociate();
private int mark();		/* mark alive */
private void usage();
private char *doargs();
export void dumpether();

/* these are currently only used internally */
export boolean match_string();
export char *gettoken();
export char *getdelimitedstring();
export char *mgets();

/* guts of start */
private boolean parse_bd();
private void pbd_err();
private int parse_net();

private boolean connect_port();

/* control module */
private void setup_signals();	/* MODULE:CONTROL */
private void listsigactions();	/* MODULE:CONTROL */
private int handlesigaction();	/* MODULE:CONTROL */
private int runsignalactions();	/* MODULE:CONTROL */
private void set_uab_pid();	/* MODULE:CONTROL */
private int get_uab_pid();	/* MODULE:CONTROL */

/*
 * give away terminal
 *
*/
private void
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
#else
  (void)dup2(0,1);
  (void)dup2(0,2);
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

private void
usage()
{
  fprintf(stderr,"Usage: uab -D <debug level> -f <bridge description file>\n");
  listsigactions();
  exit(1);
}

private char *
doargs(argc, argv)
int argc;
char **argv;
{
  int c, pid;
  extern char *optarg;
  extern int optind;

  while ((c = getopt(argc, argv, "D:df:l:")) != EOF) {
    switch (c) {
    case 'f':
      bd_file = optarg;
      break;
    case 'D':
      debug = atoi(optarg);
      break;
    case 'd':
      debug++;
      break;
    case 'l':
      have_log = 1;
      logitfileis(optarg, "w");
      break;
    }
  }

  if (optind == argc)
    return;
  
  if ((pid = get_uab_pid()) < 0) 
    logit(L_EXIT|LOG_LOG,
	"Couldn't get Unix AppleTalk Bridge pid, is it running?");

  for (; optind < argc ; optind++)
    if (handlesigaction(argv[optind], pid) < 0)
      usage();
  exit(0);
}

main(argc, argv)
char **argv;
int argc;
{
  int mark();

  doargs(argc, argv);
  set_debug_level(debug);
  if (!debug) {
    disassociate();
    if (!have_log)
      nologitfile();		/* clear stderr as log file */
  }
  setup_signals();
  ddp_route_init();		/* initialize */
  if (!parse_bd(bd_file))
    exit(1);

  ddp_route_start();
  set_uab_pid();		/* remember */
  /* mark every 30 minutes */
  Timeout(mark, (caddr_t)0, sectotick(60*30));
  do {
    abSleep(sectotick(30), TRUE);
  } while (runsignalactions());
}

private int
mark()
{
  logit(LOG_LOG, "uab running");
}

export void
dumpether(lvl,msg, ea)
int lvl;
char *msg;
byte *ea;
{
  logit(lvl, "%s: %x %x %x %x %x %x",msg,
	 ea[0], ea[1], ea[2],
	 ea[3], ea[4], ea[5]);
}

/*
 * start of guts
 *   parse the file
 *
*/

/* list of known allowable lap descriptions */
private struct lap_description *ld_list[] = {
  &ethertalk_lap_description,
  &asyncatalk_lap_description,
  NULL
};

/* list of valid interfaces */
private struct interface_description *id_list;

private struct mpxddp_module *mdm_list[] = {
  &kip_mpx,
  &mkip_mpx,
  NULL
};

private struct mpxddp_module *check_local_delivery();
private struct lap_description *check_lap_type();

/*
 * check out local delivery methods (couldn't get the pointers right,
 * so just hard coded the names :-)
 *
*/
private struct mpxddp_module *
check_local_delivery(local, invalid)
char *local;
int *invalid;
{
  struct mpxddp_module **m;

  *invalid = FALSE;
  if (strcmpci(local, "none") == 0)
    return(NULL);
  
  for (m = mdm_list; *m ; m++) {
    if (strcmpci((*m)->mpx_key, local) == 0)
      return(*m);
  }
  *invalid = TRUE;
  return(NULL);
}

/*
 * check for a valid lap type
 *
*/
private struct lap_description *
check_lap_type(name)
char *name;
{
  struct lap_description **ld;
  char **keys;

  for (ld = ld_list; *ld ; ld++) {
    for (keys = (*ld)->ld_key; *keys; keys++)
      if (strcmpci(*keys, name) == 0)
	return(*ld);
  }
  return(NULL);
}

private void
interface_dump_table(fd)
FILE *fd;
{
  IDESC_TYPE *id = id_list;

  while (id) {
    if (id->id_ld && id->id_ld->ld_dump_routine)
      (*id->id_ld->ld_dump_routine)(fd, id);
    id = id->id_next;		/* move to next in our list */
  }
}

private void
interface_dump_stats(fd)
FILE *fd;
{
  IDESC_TYPE *id = id_list;

  while (id) {
    if (id->id_ld && id->id_ld->ld_dump_routine)
      (*id->id_ld->ld_stats_routine)(fd, id);
    id = id->id_next;		/* move to next in our list */
  }
}

/*
 * match base against pattern
 * special chars are
 * allow: % to match any char
 *        * (at start) to match anything (anything following * is ignored)
 *        * at end to match previous then anything
 *
*/
export boolean
match_string(base, pattern)
char *base;
char *pattern;
{
  char pc, bc;

  while ((pc = *pattern++)) {
    if ((bc = *base) == '\0')
      return(FALSE);
    switch (pc) {
    case '%':
      break;
    case '*':
      return(TRUE);
    default:
      if (bc != pc)
	return(FALSE);
      break;
    }
    base++;
  }
  if (*base != '\0')		/* end of string */
    return(FALSE);		/* no, and pattern has ended! */
  return(TRUE);
}

/*
 * get a token.
 *  returns a pointer to a copy of the token (must be saved if you
 *  wish to keep across invocations of gettoken).
 *
 * We depend upon isspace to define white space (should be space, tab,
 * carriage return, line feed and form feed
 *
 * "/" may be used to quote the characters.
 *
 * "#" at the start of a line (possibly with white space in front)
 * is consider a comment.
 *
*/
private char tmpbuf[BUFSIZ];	/* temp for gettoken and getdel...string */
#define TMPBUFENDP (tmpbuf+BUFSIZ-2) /* room for null */
char *
gettoken(pp)
char **pp;
{
  char *p = *pp;
  char *dp = tmpbuf;
  char c;
  boolean sawquote;

  /* no string or at the end */
  if (p == NULL || (c = *p) == '\0')
    return(NULL);

  while ((c = *p) && isascii(c) && isspace(c)) /* skip over any spaces */
    p++;

  if (*p == '#') {		/* is a comment */
    *pp = p;			/* repoint */
    return(NULL);
  }

  for (sawquote=FALSE, c = *p ; c != '\0' && dp < TMPBUFENDP; c = *p) {
    if (sawquote) {		/* in quote mode? */
      *dp++ = c;		/* yes, move char in */
      sawquote = FALSE;		/* and turn off quote mode */
    } else if (c == '\\')	/* else, is the char a quote? */
      sawquote = TRUE;		/* yes, turn quote flag on */
    else if (isascii(c) && isspace(c)) /* or is it a space? */
      break;			/* yes, so stop tokenizing */
    else *dp++ = c;		/* wasn't in quote, wasn't a quote */
				/* char and wasn't a space, so part of */
				/* token */
    p++;			/* move past char */
  }
  *dp = '\0';			/* tie off string */
  *pp = p;			/* update pointer */
  if (tmpbuf == dp)		/* nothing in string? */
    return(NULL);
  return(tmpbuf);			/* return pointer to token */
}

/*
 * get a string deliminted by the characters start and end
 *
*/
export char *
getdelimited_string(pp, start, end)
char **pp;
char start;
char end;
{
  char *p = *pp;
  char *dp = tmpbuf;
  char c;
  boolean sawquote;

  if (start)
    while ((c = *p) && c != start) /* skip to start */
      p++;
  *pp = p;
  if (c == '\0')
    return(NULL);
  p++;				/* skip begin char */
  for (sawquote=FALSE,c = *p; c != '\0' || dp < TMPBUFENDP; c = *p) {
    if (sawquote) {		/* in quote mode? */
      *dp++ = c;		/* yes, move char in */
      sawquote = FALSE;		/* and turn off quote mode */
    } else if (c == '\\')	/* else, is the char a quote? */
      sawquote = TRUE;		/* yes, turn quote flag on */
    else if (c == end) {	/* or is it our end char */
      p++;			/* yes, push past char */
      break;			/* and stop tokenizing */
    }
    else *dp++ = c;		/* wasn't in quote, wasn't a quote */
				/* char and wasn't a end char, so part */
				/* of token */
    p++;			/* move past char */
  }
  *dp = '\0';			/* tie off string */
  *pp = p;			/* update pointer */
  if (tmpbuf == dp)		/* nothing in string? */
    return(NULL);
  return(tmpbuf);			/* return pointer to token */
}


/*
 * like gets, but accepts fd, will always null terminate,
 * accepts "\" at the end of a line as a line continuation.
 *
*/
export char *
mgets(buf, size, fd)
char *buf;
int size;
FILE *fd;
{
  int c;
  char lc = 0;
  char *p = buf;

  if (size)			/* make room for the null */
    size--;
  while ((c = getc(fd)) != EOF && size) {
    if (c == '\n' || c == '\r') {
      if (lc == '\\') {
	p--;			/* backup pointer (toss the "\" */
	lc = 0;			/* and clear lc */
	continue;
      }
      break;
    }
    *p++ = c;
    size--;
    lc = c;
  }
  *p = '\0';			/* tie off string */
  return(c == EOF ? NULL : buf);
}


/*
 * parse a bridge description file
 *
*/
private boolean
parse_bd(file)
char *file;
{
  FILE *fd;
  char hostname[BUFSIZ];	/* room for host name */
  char buf[BUFSIZ];		/* room for a lot */
  char *p, *bp, *t;
  char *savedpat = NULL;
  int i;
  int lineno = 0;		/* start line numbers */
  int connected_ports = 0;
  int anymatches = TRUE;
  LDESC_TYPE *ld;
  IDESC_TYPE *id;
  
  if ((id = (IDESC_TYPE *)malloc(sizeof(IDESC_TYPE))) == NULL)
    logit(L_EXIT|L_UERR|LOG_LOG, "out of memory in parse_pd");

  if (gethostname(hostname, sizeof(hostname)) < 0) {
    logit(L_UERR|LOG_LOG, "Can't get hostname: will only accept wildcards");
    hostname[0] = '\0';
  }

  if ((fd = fopen(file, "r")) == NULL)
    logit(L_EXIT|L_UERR|LOG_LOG, "Can't open file %s", file);

  while (mgets(buf, sizeof(buf),fd)) {
    lineno++;
    p = buf;
    /* get the tokens */
    if ((bp = gettoken(&p)) == NULL) /* comment or blank */
      continue;
    if (savedpat && strcmp(bp, savedpat) == 0);
    if (!savedpat) {
      /* no saved pattern */
      if (!match_string(hostname, bp)) /* none of our concern */
	continue;
      savedpat = (char *)strdup(bp);	/* save the pattern */
    } else {
      /* saved pattern */
      if (savedpat && strcmp(bp, savedpat) != 0)
	continue;
    }
    logit(LOG_PRIMARY, "Port description match on %s", bp);
    anymatches = TRUE;
    if ((bp = getdelimited_string(&p, '[', ']')) == NULL) {
      pbd_err(lineno, buf, "missing lap specification");
      continue;
    }
    if ((t = (char *)index(bp, ','))) {
      *t = '\0';		/* divide */
      t++;			/* and conquer*/
    }
    if ((ld = check_lap_type(bp)) == NULL) {
      pbd_err(lineno,buf, "invalid lap type");
      continue;
    }
    id->id_ld = ld;		/* remember for later */
    id->id_intf = NULL;
    id->id_intfno = 0;
    bp = t;			/* move to device dependent */
    if (bp) {
      if ((t = (char *)index(bp, ':'))) {
	id->id_intfno = atoi(t+1);	/* convert to int */
	if (id->id_intfno == 0 && t[1] != '0') {
	  pbd_err(lineno,buf,
	      "interface specification, expected number after colon");
	  continue;
	}
	*t = '\0';		/* kill of interface # */
      }
      id->id_intf = (char *)strdup(bp); /* copy interface name */
    } else {
      if (ld->ld_wants_data) {
	pbd_err(lineno, buf, "this lap type requires additional data");
	continue;
      }
    }
    if ((bp = gettoken(&p)) == NULL) {
      pbd_err(lineno, buf, "missing local delivery");
      if (id->id_intf)
	free(id->id_intf);
      continue;
    }
    id->id_local = check_local_delivery(bp, &i);
    if (i) {
      pbd_err(lineno, buf, "invalid local delivery");
      if (id->id_intf)
	free(id->id_intf);
      continue;
    }
    id->id_zone = NULL;
    if ((bp = getdelimited_string(&p, '[', ']')) != NULL) {
      byte *z;

      id->id_isabridge = TRUE;
      id->id_network = parse_net(bp, NULL);	/* get network number */
      z = (byte *)index(bp, ',');
      if (z) {
	z = (byte *)strdup(z);	/* duplicate string */
	*z = strlen(z+1);	/* make pstr */
	id->id_zone = z;
      } else id->id_zone = NULL;
      id->id_zone = z ? ((byte *)strdup(z)) : NULL;
    } else id->id_isabridge = FALSE;
    logit(LOG_PRIMARY, "interface %s%d", id->id_intf, id->id_intfno);
    if (id->id_local)
      logit(LOG_PRIMARY, "\tlocal delivery is %s", id->id_local->mpx_name);
    else
      logit(LOG_PRIMARY, "\tno local delivery");
    if (id->id_isabridge) {
      if (id->id_network)
	logit(LOG_PRIMARY, "\tnetwork %d.%d", nkipnetnumber(id->id_network),
	       nkipsubnetnumber(id->id_network));
      else
	logit(LOG_PRIMARY, "\tnetwork number from network");
      if (id->id_zone)
	logit(LOG_PRIMARY, "\tzone %d-'%s'", *id->id_zone, id->id_zone+1);
    } else
      logit(LOG_PRIMARY, "\tnot routing on this interface");
    if ((*ld->ld_init_routine)(id, TRUE)) {
      connected_ports++;
      id->id_next = id_list;	/* link into list */
      id_list = id;		/* of active ld descriptions */
      /* create a new id */
      if ((id = (IDESC_TYPE *)malloc(sizeof(IDESC_TYPE))) == NULL)
	logit(L_EXIT|L_UERR|LOG_LOG, "out of memory in parse_pd");
    } else {
      logit(LOG_PRIMARY, "Couldn't establish the port");
    }
    if (id->id_intf)
      free(id->id_intf);
    if (id->id_zone)
      free(id->id_zone);
  }
  if (id)
    free(id);
  if (savedpat)
    free(savedpat);
  fclose(fd);
  if (connected_ports == 0) {
    logit(LOG_PRIMARY, "NO CONNECTED PORTS, ABORTING");
    return(FALSE);
  }
  return(TRUE);
}

private void
pbd_err(lineno, line, msg)
{
  logit(LOG_LOG, "error in line %d - %s",lineno, msg);
  logit(LOG_LOG, "line: %s", line);
}

/*
 * parse a network number in the format:
 *  number
 *  number.number (means high byte, low byte)
 * where the number can be hex (0x or 0X preceeding) or octal (leading 0) 
 *  (handled by strtol)
 *
 * set *rptr to point to last part of "s" used
 *
*/
private int
parse_net(s, rptr)
char *s;
char **rptr;
{
  char *e;
  int p1, p2;
  
  p1 = strtol(s, &e, 0);	/* convert */
  if (rptr)
    *rptr = e;
  if (*e != '.')
    return(htons(p1));
  p2 = strtol(e+1, rptr, 0);
  return(htons( ((p1&0xff) << 8) | (p2&0xff)));
}


/* MODULE: CONTROL */

/* pid file */
#ifndef UAB_PIDFILE
# define UAB_PIDFILE "/etc/uab.pid"
#endif

/* logging file */
#ifndef UAB_RUNFILE
# define UAB_RUNFILE "/usr/tmp/uab.run"
#endif

#ifndef UAB_STATSFILE
# define UAB_STATSFILE "/usr/tmp/uab.stats"
#endif

#ifndef UAB_DUMPFILE
# define UAB_DUMPFILE "/usr/tmp/uab.dump"
#endif

private char *uab_runfile = NULL;
private char *uab_pidfile = UAB_PIDFILE;
private char *uab_statsfile = UAB_STATSFILE;
private char *uab_dumpfile = UAB_DUMPFILE;

private int uab_end();
private int uab_undebug();
private int uab_debuginc();
private int table_dump();
private int stats_dump();

/*
 * do nice stuff
 *
*/

private void
set_uab_pid()
{
  FILE *fd;

  if ((fd = fopen(uab_pidfile, "w")) != NULL) {
    fprintf(fd, "%d\n",getpid());
    fclose(fd);
  }
}

private
get_uab_pid()
{
  FILE *fp;
  int pid;

  if ((fp = fopen(uab_pidfile, "r")) == NULL) {
    logit(L_UERR|LOG_LOG, "No pid file - maybe the daemon wasn't running?");
    return(-1);
  }
  if (fscanf(fp, "%d\n", &pid) != 1) {
    logit(LOG_LOG, "pid file was bad");
    return(-1);
  }
  return(pid);
}


#define NSIGACT 6

struct sigtab {
  char *s_name;
  int s_signal;
  char *s_action;
};

private struct sigtab sigtab[NSIGACT] = {
#define SIG_TDUMP SIGUSR1
  "tdump", SIG_TDUMP, "dump tables",
#define SIG_DEBUG SIGIOT
  "debug", SIG_DEBUG, "increment debug level",
#define SIG_NODEBUG SIGEMT
  "nodebug", SIG_NODEBUG, "clear debugging",
#define SIG_STAT SIGUSR2
  "statistics", SIG_STAT, "dump statistics",
  "stat", SIG_STAT, "dump statistics",
#define SIG_EXIT SIGTERM
  "exit", SIG_EXIT, "stop running uab"
};

private int signalactions; /* no signal actions to handle */
#define SA_TABLE_DUMP 0x1
#define SA_STATISTICS 0x2
#define SA_UNDEBUG 0x4		/* turn off debugging */
#define SA_DEBUG 0x8		/* turn on debugging */
private void
setup_signals()
{
  signal(SIG_STAT, stats_dump);
  signal(SIG_TDUMP, table_dump);
  signal(SIG_EXIT, uab_end);
  signal(SIG_DEBUG, uab_debuginc);
  signal(SIG_NODEBUG, uab_undebug);
  signalactions = 0; /* no signal actions to handle */
}

private void
listsigactions()
{
  int i;
  struct sigtab *st;

  fprintf(stderr, " commands:\n");
  for (st = sigtab, i = 0; i < NSIGACT; i++, st++)
    fprintf(stderr,"\t%s\t%s\n", st->s_name, st->s_action);
}

private int
handlesigaction(s, pid)
char *s;
{
  int i;
  struct sigtab *st;

  for (st = sigtab, i = 0; i < NSIGACT; i++, st++) {
    if (strcmpci(s, st->s_name) != 0)
      continue;
    if (kill(pid, st->s_signal) < 0)
      logit(LOG_LOG, "Couldn't send %s signal to daemon[%d] - is it running?",
	  st->s_action, pid);
    else
      logit(LOG_LOG, "Sent %s signal to daemon[%d]",st->s_action,pid);
    return(0);
  }
  return(-1);
}


/* safe enough */
private int
uab_end()
{
  logit(LOG_LOG, "Exiting - stopped by remote");
  unlink(uab_pidfile);
  exit(0);
}

/* nothing yet */
/* unsafe (defer until later) */
private int
table_dump()
{
  signalactions |= SA_TABLE_DUMP;
  signal(SIG_TDUMP, table_dump);
}

private int
stats_dump()
{
  signalactions |= SA_STATISTICS;
  signal(SIG_STAT, stats_dump);
}

private int
uab_undebug()
{
  debug = 0;
  signalactions |= SA_UNDEBUG;
  signal(SIG_NODEBUG, uab_undebug);
}

private int
uab_debuginc()
{
  signalactions |= SA_DEBUG;
  debug++;
  signal(SIG_DEBUG, uab_debuginc);
}

/*
 * run deferred actions
 *
 * returns FALSE if everythings should stop
*/
private int
runsignalactions()
{
  int done = 0;			/* init to zero */

  while (signalactions) {
    /* always allow undebug and debug to run */
    if ((SA_UNDEBUG & signalactions) && debug == 0) {
      set_debug_level(debug);
      logit(LOG_LOG, "DEBUGGING OFF");
      if (uab_runfile) {
	nologitfile();
	uab_runfile = NULL;		/* reset this */
      }
      signalactions &= ~SA_UNDEBUG;
    }
    if ((SA_DEBUG & signalactions) && debug != 0) {
      if (!islogitfile()) {
	uab_runfile = UAB_RUNFILE;
	logitfileis(uab_runfile, "w+");
      }
      set_debug_level(debug);
      logit(LOG_LOG, "Debugging level is now %d", debug);
      signalactions &= ~SA_DEBUG;
    }
    /* only run one of these at a time */
    if (SA_TABLE_DUMP & signalactions) {
      FILE *fd;
      long t;

      done = SA_TABLE_DUMP;
      if ((fd = fopen(uab_dumpfile, "a+")) == NULL) {
	logit(LOG_LOG|L_UERR, "can't open dump file %s", uab_dumpfile);
      } else {
	chmod(uab_dumpfile, 0774);
	time(&t);
	fprintf(fd, "Tables dump at %s", ctime(&t));
	interface_dump_table(fd);
	rtmp_dump_table(fd);
	fclose(fd);
      }
    } else if (SA_STATISTICS & signalactions) {
      FILE *fd;
      long t;
      
      done = SA_STATISTICS;
      if ((fd = fopen(uab_statsfile, "a+")) == NULL) {
	logit(LOG_LOG|L_UERR, "can't open statistics file %s", uab_statsfile);
      } else {
	chmod(uab_statsfile, 0774);
	time(&t);
	fprintf(fd, "Statistics dump at %s", ctime(&t));
	ddp_dump_stats(fd);
	rtmp_dump_stats(fd);
	interface_dump_stats(fd);
	fclose(fd);
      }
    }
    signalactions &= ~done;	/* turn off run signals */
  }
  return(TRUE);
}

