/*
 * $Author: djh $ $Date: 1996/09/10 13:47:34 $
 * $Header: /mac/src/cap60/lib/cap/RCS/absched.c,v 2.8 1996/09/10 13:47:34 djh Rel djh $
 * $Revision: 2.8 $
*/

/*
 * absched.c - Simple Protocol Scheduling Facility
 *
 * Provides a very simple non-preemptive "scheduling" facility.
 * It allows one to:
 *     (a) set timeouts with routines called back on timeouts
 *     (b) call listeners if input is available
 * Base interface is through "abSleep" (bad name).  abSleep
 *  will run for a the time passed returning only if the time has
 *  passed or if one of the above events has occurred
 *  and you asked it to return on any event occurring.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 14, 1986    Schilit	Created.
 *  June 18, 1986    CCKim      Chuck's handler runs protocol
 *
 */

/* needs to do printfs on other than lap_debug */

/* good place to stick the copyright so it shows up in object files */
char Columbia_Copyright[] = "Copyright (c) 1986,1987,1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include <netat/compat.h>	/* to cover difference between bsd systems */

/*
 * this is a hack that
 * needs more elegance
 *
 */

#ifdef __bsdi__
#define MULTI_BPF_PKT
#endif /* __bsdi__ */

#ifdef __NetBSD__
#define MULTI_BPF_PKT
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#define MULTI_BPF_PKT
#endif /* __FreeBSD__ */

#ifdef NeXT
#define MULTI_BPF_PKT
#endif /* NeXT */

/*
 * NOTE: it is not important to get the TZ information.  It is simply
 * included because some systems complain if there isn't a valid pointer
 * there.
 *
*/

/*
 * define localtime_gtod if your system's get time of day call
 * calls _gettimeofday to get the system time and uses disk based
 * information to get the time zone information EVERY TIME.
 *
 * (e.g. gettimeofday is based on pd_localtime, but does it incorrectly) 
 *
 * For AUX version 1.0.
 *
*/
#ifdef LOCALTIME_GTOD
# define gettimeofday(tv, tz) _gettimeofday((tv))
#endif

/*
 * Configuration defines
 *  NOFFS - no ffs function defined, use our own
 *
 * Note: need not be ffs as per vax.  It may be any routine
 * that returns bits assuming 1 == 1, 2==2, 4==3, 8==4, etc.
 * and 0 is none
*/

/*
 * structure defs
 *
*/

/* timer support */
typedef struct TimerEntry {
  struct TimerEntry *t_next;	     /* pointer to next */
  struct timeval t_time;	     /* timeout internal form */
  ProcPtr t_fun;		     /* function to call */
  caddr_t t_arg;		     /* argument for function */
} TimerEntry;

#define NILTIMER (TimerEntry *) 0    /* a nil pointer */

/* select on fds */

typedef struct {
  boolean fl_valid;		/* is this item valid? (true/false) */
  int (*fl_listener)();		/* listener */
  caddr_t fl_addrarg;		/* any arguments */
  int fl_intarg;
} FDLISTENER;

/* local defines */
#define MICRO 1000000		/* micro is one million'th, 1mil per sec*/
#define UNITT  250000		/* 1/4 second is tick unit: 250k usec */

/* variables */
private gfd_set fdbitmask;	/* file descriptors open... */
private gfd_set *fdbits;
private int fdnotinited = 1;	/* 0 is not inited */
private int fdmaxdesc = -1;	/* mark not inited */


/* list of fd and listeners */
/* NOFILE is the number of file descriptors.  If it isn't set */
/* then we use FD_SETSIZE which must be set one way or the other */
/* prefer NOFILE since it can be much smaller */
#ifndef NOFILE
# define NOFILE FD_SETSIZE
#endif
FDLISTENER fdlist[NOFILE];

/*
 * Tappan:
 * optimization - holds the current gettimeofday() so lower level routines
 * can avoid system call overhead - updated when abSleep() is entered and
 * after every sleep
 *
 * cck: optimization for Timeout and other time related support
 *
*/
private struct timeval tv_now;
/* should be able to pass null for tz if no tz information wanted.  */
/* careful: some systems may want structure and may blow up */
#define NO_TZ ((struct timezone *)0)

/* routine declarations */

private TimerEntry *tm_alloc();
private void tm_free();
private int i_doTimeout();
#ifdef NOFFS
private int ffs();
#endif
private int FD_GETBIT();
private void abstoreltime();
private void reltoabstime();
private void apptoabstime();

export int init_fdlistening();
export int fdlistener();
export int fdunlisten();
export int fdlistenread();
export int fdlistensuspend();
export int fdlistenresume();
export void Timeout();
export void AppleTimeout();
export void relTimeout();
export void absTimeout();
export int remTimeout();
export int doTimeout();
export boolean minTimeout();
export int abSleep();
int abSelect();			/* haven't decided whether this should */
				/* be exported or not */
/*
 * FD LISTENER SUPPORT
 *
*/

int
init_fdlistening()
{
  int i;

  if (!fdnotinited)		/* already inited */
    return(NOFILE);
  fdbits = &fdbitmask;
  FD_ZERO(fdbits);		/* make sure these are cleared */
  fdmaxdesc = -1;		/* mark init, but not in use */
  fdnotinited = FALSE;		/* initted */
  for (i = 0; i < NOFILE; i++)
    fdlist[i].fl_valid = FALSE;	/* not in use */
  return(NOFILE);
}

/*
 * install a listener "listener" on file descriptor "fd"
 *
 * the listener is called with the file descriptor when INPUT is ready
 * on the fd.
 *
 * returns: 0 okay, -1 problem (fd too large, too small, etc)
*/
export int
fdlistener(fd, listener, addrarg, intarg)
int fd;
int (*listener)();
caddr_t addrarg;
int intarg;
{
  if (fd < 0 || fd >= NOFILE || fdnotinited)
    return(-1);
  FD_SET(fd, fdbits);
  fdmaxdesc = max(fd, fdmaxdesc);
  fdlist[fd].fl_listener = listener;
  fdlist[fd].fl_valid = TRUE;
  fdlist[fd].fl_addrarg = addrarg;
  fdlist[fd].fl_intarg = intarg;
  return(0);
}

/*
 * close off a listener on file descriptor "fd"
 *
*/
export int
fdunlisten(fd)
int fd;
{
  FDLISTENER *fdi;

  if (fd < 0 || fd >= NOFILE || fdnotinited)
    return(-1);

  fdi = &fdlist[fd];		/* mark end */
  if (fdi->fl_valid) {		/* this was a valid item */
    FD_CLR(fd, fdbits);
    fdi->fl_valid = FALSE;	/* mark off list */
    if (fd >= fdmaxdesc) {	/* was it at the tail of the list */
      /* use >= instead of = above is paranoia */
      /* select out boundary case and no fdmaxdesc case (paranoia) */
      if (fd == 0 || fdmaxdesc < 0) {
	fdmaxdesc = -1;
	return(0);
      }
      /* step back one and start from there */
      for ( fdmaxdesc--, fdi = &fdlist[fdmaxdesc]; fdmaxdesc >= 0;
	   fdmaxdesc--, fdi--) {
	if (fdi->fl_valid)
	  break;
      }
      /* if none valid, then we come out with fdmaxdesc = -1 */
    }
  }
  return(0);
}

/*
 * run the listener on the file descriptor fd
 *
 * returns value returned by listener if there was one
 *   -1 if no listener or fd not valid
 *
*/
export int
fdlistenread(fd)
{
  FDLISTENER *fdi;

  if (dbug.db_skd)
    fprintf(stderr,"fdlistenread call\n");
  /* paranoia */
  if (fd < 0 || fd >= NOFILE || fdnotinited)
    return(-1);

  if (fdlist[fd].fl_valid) {
    fdi = &fdlist[fd];
    if (fdi->fl_listener != NULL)
      return((*(fdi->fl_listener))(fd, fdi->fl_addrarg, fdi->fl_intarg));
  }
  return(-1);
}

/* primarily useful outside libraries where we are not event based, but */
/* are more poll based.  for instance, you might want the listener for */
/* a terminal to just return that data is available */

/* listensuspend and resume both return 0 on okay, -1 on error */
/* error; bad fd or invalid fd */
/* listensuspend temporarily supsends listening (selecting) on a fd */
export int
fdlistensuspend(fd)
{
  if (fd < 0 || fd >= NOFILE || fdnotinited)
    return(-1);
  if (fdlist[fd].fl_valid) {
    FD_CLR(fd, fdbits);		/* don't bother updating max */
    return(0);
  }
  return(-1);
}

/* listensupend resumes listening (selecting) on a fd */
export int
fdlistenresume(fd)
{
  if (fd < 0 || fd >= NOFILE || fdnotinited)
    return(-1);
  if (fdlist[fd].fl_valid) {
    FD_SET(fd, fdbits);		/* don't bother updating max */
    return(0);
  }
  return(-1);
}


/*
 * TIMER SUPPORT
 *
 * SUPPORTS a timer facility.  The clock resolution is in 1/4 second
 * units (TICKS).
 *
 * Timeouts are given as a <routine, routine argument (ra), timeout>
 *  triple.
 * The combination <routine, ra> should be unique or else remTimeout
 *  must be called for the "unknown" number of times the combination
 *  occurs.
*/
private TimerEntry TimoutQ = {NILTIMER,{0,0},NILPROC,0};  
private TimerEntry FreeTimoutQ = {NILTIMER, {0,0}, NILPROC, 0};
/* these are used primarily for debugging  */
int mcalloced = 0;
int mcfreesize = 0;

/* allocate and dealloc functions for timer entries */
/* keeps chain of allocated pointers */

/* allocate a timer entry with specified fun, arg */
private TimerEntry *
tm_alloc(fun,arg)
ProcPtr fun;
caddr_t arg;
{
  TimerEntry *tu;

  if (FreeTimoutQ.t_next == NILTIMER) {
    tu = (TimerEntry *) malloc(sizeof(TimerEntry)); /* new entry */
    mcalloced++;
  } else {
    mcfreesize--;
    tu = FreeTimoutQ.t_next;	/* get first free */
    FreeTimoutQ.t_next = tu->t_next; /* and unlink it */
  }
  tu->t_fun = fun;		/* set function to call */
  tu->t_arg = arg;		/*  and arg to pass... */
  tu->t_next = NILTIMER;	/* make sure it doesn't go anywhere */
  return(tu);
}

/*
 * link timer entry into the free list
 *
*/
private void
tm_free(tu)
TimerEntry *tu;
{
  mcfreesize++;
  tu->t_next = FreeTimoutQ.t_next;
  FreeTimoutQ.t_next = tu;
}

/*
 * void
 * Timeout(ProcPtr fun,caddr_t arg,int t)
 *
 * Call "fun" with "arg" after elapsed time "t" has expired.  "t"
 * is in internal tick units of 1/4 seconds.
 *
 * This unit conforms to LAP and PAP timeout units.
 *
 * FastTimeout is the same as Timeout except it is designed to be
 *  called from called "Timeout" routines.  (Basic difference, it
 *  doesn't update the time of day).
 *
 * GENERAL WARNING: YOU MUST NOT DELAY FOR A LONG TIME IN THE TIMEOUT
 * ROUTINES!  DOING SO MAY CAUSE PROTOCOL ERRORS
 * 
*/

export void
Timeout(fun,arg,tim)
ProcPtr fun;
caddr_t arg;
int tim;
{
  AppleTimeout(fun, arg, tim, TRUE);
}

export void
AppleTimeout(fun, arg, tim, doupdate)
ProcPtr fun;
caddr_t arg;
int tim;
boolean doupdate;
{
  struct timeval tv;

  apptoabstime(&tv, tim, doupdate);
  absTimeout(fun, arg, &tv);
}

export void
relTimeout(fun,arg,tv,doupdate)
ProcPtr fun;
caddr_t arg;
struct timeval *tv;
boolean doupdate;
{
  struct timeval tv_abs;

  reltoabstime(tv, &tv_abs, doupdate);
  absTimeout(fun, arg, &tv_abs); /* send down timeout */
}

export void
absTimeout(fun,arg,tv)
ProcPtr fun;
caddr_t arg;
struct timeval *tv;
{
  TimerEntry *tu,*tn,*t;

  tu = tm_alloc(fun, arg);
  tu->t_time = *tv;		/* copy absolute time time */
#ifdef DEBUG
  if (dbug.db_skd)
    fprintf(stderr,"TIMEOUT: %x at %d/%d %d\n",
	    fun, tim, tu->t_time.tv_sec, tu->t_time.tv_usec);
#endif
  for (t=(&TimoutQ), tn=TimoutQ.t_next;
       tn != NILTIMER && (cmptime(&(tu->t_time),&(tn->t_time)) > 0);
       t = tn, tn=t->t_next)
    /* NULL */;
  tu->t_next = t->t_next;	/* make sure new entry knows where it is */
  t->t_next = tu;		/* and link it in the list */
}  

/*
 * int
 * remTimeout(ProcPtr fun, caddr_t arg);
 *
 * Given the function and function arg of a pending timeout
 * remove that timeout from the q.
 *
 * Question: should we remove all instances?  Should timeout enforce
 * a single (fun, arg) pair?
 *
 * Resolved above by allowing multiple instances.  remTimeout will
 * remove the first.  remTimeout is now modified to return TRUE if it
 * removed something, so just call until false if you need to worry about
 * it.
 *
*/
export int
remTimeout(fun,arg)
ProcPtr fun;
caddr_t arg;
{
  TimerEntry *t,*tn;

  /* t acts as prev, tn is curr */
  for (t=(&TimoutQ), tn=TimoutQ.t_next;
       (tn != NILTIMER) &&	/* if current is valid and */
       /* either fun or arg mismatches */
       ( (tn->t_fun != fun) || (tn->t_arg != arg));
       t = tn, tn = t->t_next)
    /* NULL */;
  if (tn == NILTIMER)		/* find anything? */
    return(FALSE);		/* no, missing entry! */
  t->t_next = tn->t_next;	/* unlink ourselves */
  tm_free(tn);			/* and release timer block */
  return(TRUE);
}

/*
 * void
 * doTimeout()
 *
 * doTimeout expires entries on the timeout queue. The timeout
 * function is called with the function argument, the timeout
 * entry is unlinked from the q and memory is returned.
 *
 * doTimeout can be called at any time, if the q is empty, or
 * no timers have expired then no action is taken.
 *
 * doTimeout updates the "current" time because it is designed
 * be called by external parties.  doUnforcedTimeout is the guts
 * and is only meant be called from within this module.
 *
*/
export int
doTimeout()
{
  gettimeofday(&tv_now, NO_TZ); /* update, 'cause could be at any point */
  return(i_doTimeout());
}

/* update time before calling doTimeOut */
private int
i_doTimeout()
{
  TimerEntry *t,*tn;
  int ntimeout = 0;

  t = &TimoutQ, tn = TimoutQ.t_next;
  while (tn != NILTIMER && (cmptime(&tn->t_time, &tv_now) <= 0)) {
    t->t_next = tn->t_next;	/* unlink tn */
    if (tn->t_fun != NULL)
      (*tn->t_fun)(tn->t_arg); /* call timeout function */      
    if (dbug.db_skd)
      fprintf(stderr, " timeout occurred ");
    ntimeout++;
    tm_free(tn);
    tn = t->t_next;
    /* t should stay constant since head of list is min time! */
  }
  return(ntimeout);
}

/*
 * boolean
 * minTimeout(struct timeval *mt)
 *
 * minTimeout returns the minimum timeout of all entries on the
 * timeout q.  The timer records are ordered earliest first so
 * this routine only needs to check on the first one.
 *
*/ 
export boolean
minTimeout(mt)
struct timeval *mt;
{
  TimerEntry *tt;

  if ((tt = TimoutQ.t_next) == NILTIMER) /* anything on queue? */
    return(FALSE);		/* nothing... */
  *mt = tt->t_time;		/* else pass along min time */
  return(TRUE);
}


/*
 *
 * external interface to "protocol scheduler"
 *
 *
*/

/* 
 * abSleep(int t, boolean nowait) 
 *
 * this is the main "protocol event scheduler loop".  It waits for
 *  incoming packets or timeout events.
 * keeps running until timeout "t" if nowait is not set, returns after
 * the first timeout or incoming packet if nowait is set.
 * (e.g. returns after first "protocol event scheduled" if nowait is set)
 *
*/
export int
abSleep(appt,nowait)
int appt,nowait;
{
  struct timeval sleept;
  int eventhappened = 0;		/* no events happened */

  apptoabstime(&sleept,appt,TRUE); /* find absolute time for sleep */
  do {
    if (abSelect(&sleept, nowait) > 0) {
      eventhappened++;
      if (nowait)
	break;
    }
    /* abSelect will have updated tv_now */
  } while (cmptime(&sleept,&tv_now) > 0); /* past wait time? */
  return(eventhappened);
}

/*
 * abSelect(struct timeval *awt, nowait)
 *
 * Call with absolute time.  Might return before that... 
 * Returns > 0 if event occured.
 *
*/
int
abSelect(awt,nowait)
struct timeval *awt;
int nowait;
{
  struct timeval rwt,mt;
  register int rdy;
  int fd;
  gfd_set rdybits;
#ifdef ABRPC
  register int i, temp;
  extern gfd_set svc_fdset;
  gfd_set svcbits;
  int svcrdy;
#endif ABRPC

  int nevent;
  /*
   * Tappan:
   * optimization: if a timeout is less than the argument sleep time
   * then we don't have to return after the select() times out. If 
   * a timeout is not less then we don't have to scan the timeout lists.
   * Note: we must return if a timeout is less than the arg. sleep time
   * because this may have been an event we were waiting for!!!!!
   */
  /* cck: the above is correct, except we need to know if they are */
  /* waiting to drop out after the first event. */
  int timeoutless;		/* one of the following states: */
#define TL_FALSE FALSE		/* timeout is not less than sleep */
#define TL_TRUE	 1		/* timeout is less than sleep time */
#define TL_USED	2		/* timeout was run */


  if (dbug.db_skd)
    fprintf(stderr,"abSelect enter... ");
 
  nevent = 0;
  do {
    timeoutless = TL_FALSE;

    /* if min timeout on functions and smaller than requested */
    if (minTimeout(&mt) && cmptime(&mt,awt) < 0) {
      abstoreltime(&mt,&rwt, FALSE); /* yes use it */
      timeoutless = TL_TRUE;	/* and mark it */
    } else
      abstoreltime(awt,&rwt, FALSE); /* use requested */
#ifdef DEBUG
    if (dbug.db_skd)
      fprintf(stderr, "%d %d ",rwt.tv_sec, rwt.tv_usec);
#endif
    /* rwt.tv_sec less than 0 means (a) that we are past our welcome here */
    /* or that (b) a timeout event is in our past and we should take care */
    /* of it before any read calls */
    /* in case (a) timeoutless is zero and we drop out almost immediately */
    /* in case (b) timeoutless is non-zero and we call doTimeout */
    /* to special case (a) here would need about 5 extra lines of code */
    if ((long)rwt.tv_sec < 0) {
      if (dbug.db_skd)
	fprintf(stderr," negative ");
      rdy = 0;
    } else {
#ifndef ABRPC
      /* should really sleep if fdmaxdesc == -1 or else we might */
      /* go into a tight loop here!  Will comprimise by getting */
      /* 1 second accuracy with sleep (shouldn't be happening */
      /* anyway.  If sleep turns out not be widespread, then */
      /* ....  don't know what to do */
      if (fdmaxdesc == -1) {
	rdy = rwt.tv_sec + (rwt.tv_usec/(2*UNITT))/2;
	sleep(rdy == 0 ? 1 : rdy);
	rdy = 0;
      } else {
	rdybits = *fdbits;	/* update before call to select */
	rdy = select(fdmaxdesc+1,&rdybits,0,0,&rwt); /* perform wait... */
      }
#else ABRPC
      rdybits = *fdbits;	/* update before call to select */
      for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	rdybits.fds_bits[i] |= svc_fdset.fds_bits[i];
      rdy = select(FD_SETSIZE,&rdybits,0,0,&rwt); /* perform wait... */
      if (rdy > 0) {
	svcrdy = 0;
	for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
	  if (temp = rdybits.fds_bits[i] & svc_fdset.fds_bits[i]) {
	    svcbits.fds_bits[i] = temp;
	    rdybits.fds_bits[i] &= ~ svc_fdset.fds_bits[i];
	    while (temp) {
	      if (temp & 1) {
		svcrdy++;
		rdy--;
	      }
	      temp >>= 1;
	    }
	  }
	  else svcbits.fds_bits[i] = 0;
	if (svcrdy > 0)
	  svc_getreqset(&svcbits);
      }
#endif ABRPC
    }
    if (dbug.db_skd)
      fprintf(stderr,"%d ", rdy);
    if (rdy < 0)
      return(rdy);
    if (rdy > 0) {
      /* rdy should be # of set file descriptors in the masks */
      /* since we only pass it the "read" bits, this loop */
      /* should take care of all */
      while (rdy--) {
	if ((fd = FD_GETBIT(&rdybits, fdmaxdesc)) < 0)
	  break;			/* paranoia */
	FD_CLR(fd, &rdybits);
#ifdef MULTI_BPF_PKT
	/*
	 * BPF can return multiple packets
	 *
	 */
        { extern short lap_proto;
	  if (lap_proto == LAP_ETALK) {
	    extern int read_buf_len;
	    do {
	      fdlistenread(fd);
	    } while (read_buf_len > 0);
	  } else
	    fdlistenread(fd);
	}
#else /* MULTI_BPF_PKT */
	fdlistenread(fd);
#endif /* MULTI_BPF_PKT */
	nevent++;
      }
    } else {
      /*
       * Tappan:
       * Optimization: If a packet was waiting don't check the timeout lists -
       * odds are that the timeout hasn't expired yet, and we'll catch it the
       * next time through anyway
       * cck: this should be okay though and things have been
       * running fine with this, but there are a lot of "nowait"
       * calls to abSleep...  If we modify, take out the "else" on rdy>0
       * and check for: timeoutless && (rdy <= 0 || nowait)
       */
      if (timeoutless) {
	if (dbug.db_skd)
	  fprintf(stderr, "doTimeout...");
	/* An internal timeout occured before the user timeout, */
	/* and there were no packets available */
	gettimeofday(&tv_now, NO_TZ); /* do this to reduce subroutine */
	i_doTimeout();		/* call overhead */
	nevent++;
	timeoutless = TL_USED;	/* mark we ran the timeout */
      }
    }
    if (nevent && nowait) {
      if (dbug.db_skd)
	fprintf(stderr, "exit to abSleep\n");
      return(nevent);
    }
    /* cck: we don't want to update the time of day if */
    /* the minimum time out was less than the sleep time and doTimeout */
    /* was run because the time of day was updated by running doTimeout */
    /* timeout routines are supposed to be fairly quick and often update */
    /* the time of day themselves */
    if (timeoutless != TL_USED)
      gettimeofday(&tv_now, NO_TZ);
  } while (timeoutless);
  if (dbug.db_skd)
    fprintf(stderr,"exit to abSleep\n");
  return(nevent);		/* return "event" count */
}

#ifdef NOFFS

/* Find the First Set bit and return its number.  Numbering starts */
/* at one */
private int
ffs(pat)
register int pat;
{
  register int j;
  register int i;

  for (i = 0; i < 8*sizeof(int); i+=8) { /* each byte */
    if (pat & 0xff) {		/* if byte has bits */
      /* do a linear scan */
      for (j = 0; j < 8; j++) {
	if (pat & 0x1)
	  return(i+j+1);
	pat >>= 1;
      }
    }
    pat >>= 8;			/* go to the next byte */
  }
  return(0);			/* none */
}

#endif

/*
 * Find the first active file descriptor
 *
 * really doesn't belong here
 *
*/
private int
FD_GETBIT(p, maxfd)
gfd_set *p;
int maxfd;
{
  register int i;
  register int w;
  register gfd_mask *fm;
  int top;

  top = howmany(maxfd, NFDBITS); /* find length of array */
  fm = (gfd_mask *)p->fds_bits;	/* point to start of array */
  for (w=0,i=0; i < top; i++,fm++) {
    if ((w=ffs(*fm)) > 0)
      break;
  }
  if (w < 1)
    return(-1);
  w += (i*NFDBITS)-1;		/* find bit no */
  return((w > FD_SETSIZE) ? -1 : w);
}

/*
 * abstoreltime(struct timeval *t)
 *
 * convert absolute time in t to relative time by subtracting the
 * current time as returned by gettimeofday.
 *
 * novalidtod means that we can't be sure the tod clock is set properly
 * so update it
 *
*/
private void
abstoreltime(at,rt, novalidtod)
register struct timeval *at;
register struct timeval *rt;
boolean novalidtod;
{
  if (novalidtod)
    gettimeofday(&tv_now, NO_TZ); /* update current time */
  if (rt != at)
    *rt = *at;			/* copy relative times */
  rt->tv_sec -= tv_now.tv_sec;	/* add in user elapsed time */
  if ((rt->tv_usec -= tv_now.tv_usec) < 0) {	/*  both seconds and usecs */
    --rt->tv_sec;		/* yes, so one less second */
    rt->tv_usec += MICRO;	/* and fix up usecs */
  }
}

/*
 * reltoabstime(struct timeval *rt, *at)
 *
*/
private void
reltoabstime(rt, at, novalidtod)
register struct timeval *at;
register struct timeval *rt;
boolean novalidtod;
{
  if (novalidtod)
    gettimeofday(&tv_now, NO_TZ); /* update current time */

  if (at != rt)
    *at = *rt;			/* copy relative to absolute */ 
  at->tv_sec += tv_now.tv_sec;	/* add in user elapsed time */
  /* do micro seconds */
  if ((at->tv_usec += tv_now.tv_usec) >= MICRO) {
    ++at->tv_sec;		/* yes, so one more second */
    at->tv_usec -= MICRO;	/* and fix up usecs */
  }
}


/*
 * void
 * apptoabstime(struct timval *tv,int t)
 * 
 * Construct actual time of timeout given unit tick 1/4 of a second.
 *
 * novalidtod means that we can't be sure the tod clock is set properly
 * so update it
*/
private void
apptoabstime(tv,t,novalidtod)
register struct timeval *tv;
register int t;
int novalidtod;
{
  if (novalidtod)
    gettimeofday(&tv_now, NO_TZ); /* update current time */
  *tv = tv_now;
  tv->tv_sec += ticktosec(t);	/* seconds till timeout */
  tv->tv_usec += (t%4)*UNITT;           /* micro seconds... */
  if (tv->tv_usec >= MICRO) {	/* second or more? */
    tv->tv_sec++;
    tv->tv_usec -= MICRO;	/* adjust */
  }
}

/*
 * user access to scheduler clock
 *
*/

getschedulerclock(tv)
struct timeval **tv;
{
  *tv = &tv_now;			/* return clock */
}

updateschedulerclock()
{
  gettimeofday(&tv_now, NO_TZ);
}
