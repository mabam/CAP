/*
 * lwrename - daemon to hide printers on AppleTalk network by resetting 
 * 	   AppleTalk type value.
 * 
 * Syntax:	lwrename [-t min ] [-s] [-r] [lwrenamefile]
 * 
 * Options:
 * 	-t min	Specify how many integer minutes to sleep in between each 
 * 		sweep of the network looking for the monitored printers.
 * 		Default=2 minutes
 * 	
 * 	-s	Make a single sweep only looking for monitored printers;
 * 		wait the sleep time; reset printers if "-r" option also
 * 		specified; and exit.  Use for testing.
 * 
 * 	-r	Reset printers back to original types before exiting.
 * 		Only has effect if used in combination with "-s" flag.
 * 		To force normal daemon to reset printers and exit,
 * 		send it the HUP signal.
 * 
 * 	lwrenamefile	Pathname of the file containing the list of
 * 		printers to be monitored (see format below).  
 * 		Default=LWRENAMEFILE compile-time flag or
 * 		"/etc/lwrename.list" if no compile-time flag.
 * 
 * Copyright (c) 1990, The Regents of the University of California.
 * Edward Moy, Workstation Software Support Group, Workstation Support
 * Services, Information Systems and Technology.
 * 
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of California makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * Revised Dec 15, 1993 by P. Farrell, Stanford Univ. Earth Sciences.
 * 	Add "-s" and "-r" options for testing, and ability to specify
 * 	printer list filename on command line.  Add optional 3rd field
 * 	to printer list records to specify new AppleTalk type for each
 * 	printer, or default to new compile-time flag LWRENAMETYPE
 * 	(value must be in quotes, defaults to "LaserShared").  Add
 * 	documentation and lwrename.8 man page.  General cleanup.
 * 
 * Use lwrename to "capture" LaserWriter or equivalent PostScript printers
 * on an AppleTalk network by renaming their AppleTalk type to a value
 * known only to the CAP host, which can then run a lwsrv process as 
 * a spooler for that printer.  Because many printers store AppleTalk type
 * changes in normal RAM, they return to default value "LaserWriter"
 * when power-cycled.  Lwrename automates the process of watching for
 * such printers and forcing the type change whenever they are found
 * back with their old type.
 * 
 * The list of printers to be monitored is stored in the file specified by
 * the compile-time option LWRENAMEFILE (default value /etc/lwrename.list).
 * Comment lines are allowed in this file; start them with the # character.
 * Include one line per printer with three tab-separated fields in this format:
 * 	password<tab>printer_NBP_name<tab>newtype
 * Password is an integer value, either the factory default of 0, or a
 * value you have previously set by another means.  Printer_NBP_name is
 * the full name needed to find it on the network, with its original
 * (default) type, in this format:
 * 	name:type@zone
 * Use * for current or default zone.  Newtype is an optional third field
 * specifying the new AppleTalk type to be used when renaming the
 * printer.  WARNING:  any trailing blanks on the line after "newtype"
 * will be interpreted as part of the type name!  If you omit "newtype"
 * (be sure to leave off the <tab> before it as well) for a particular
 * printer, it reverts to the compile time option LWRENAMETYPE, or to
 * "LaserShared" if that option was not specified at compile time.
 * 
 * Lwrename sleeps a user-specified number of minutes between each network
 * "sweep".  It runs forever until killed, unless you specify the "-s"
 * flag to run one sweep only for testing.  If you send it the HUP signal,
 * or use the "-r" flag with the "-s" flag, it will first restore the
 * original type to all of the printers it is monitoring before exiting.
 * Any other kill signal just kills it without resetting any printer
 * types.
 * 
 * Lwrename does not need to run from the root account, although you may
 * want to so restrict it to prevent private users from battling to
 * control printers on the network.  If your AppleTalk access method
 * requires special privilege (e.g., the packetfilter under Ultrix), make
 * sure lwrename executes under the appropriate user or group.  Generally,
 * you would start lwrename from your start-cap-servers file.
 * 
 * There is no way to dynamically update the list of printers to be
 * monitored by lwrename.  To change the printer list, kill lwrename with
 * the -HUP signal (to restore the current list of printers to original
 * type); edit the list; and then restart lwrename.
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <netat/appletalk.h>
#include <netat/compat.h>
#ifdef USESTRINGDOTH
#include <string.h>
#else  USESTRINGDOTH
#include <strings.h>
#endif USESTRINGDOTH

#define ATPRESPONSETIMEOUT sectotick(60*2)
#define	MINUTES		* 60
#define	R_BUFMAX	PAPSegSize*atpMaxNum
#define LWRENAMEBUFSIZ	1024

/*
 * LWRENAMEFILE is location of configuration file specifying 
 * LaserWriters to be renamed.  File has one line per printer, in
 * this format:
 * 	password<tab>printer_NBP_name<tab>newtype
 * Password is usually the number 0 unless you have reset it.
 * Printer_NBP_name is the full name needed to find it on the
 * network, in this format:
 * 	name:type@zone
 * (use * for current or default zone).
 * Newtype is the optional new AppleTalk type to use when renaming this
 * printer; if not specified, defaults to LWRENAMETYPE (see below).
 * Comment lines are allowed, start with # character.
 *
 */

#ifndef LWRENAMEFILE
#define LWRENAMEFILE "/etc/lwrename.list"
#endif  LWRENAMEFILE

/*
 * LWRENAMETYPE macro is the default new AppleTalk type name to be used
 * to hide the LaserWriters.  Specific types for each printer can be
 * specified in the LWRENAMEFILE list.
 *
 */

#ifndef LWRENAMETYPE
#define LWRENAMETYPE	"LaserShared"
#endif  LWRENAMETYPE

/*
 * Structure used to create linked list of printers to monitor.
 *
 */

struct lws {
	struct lws *next;
	char *passwd;
	char *name;
	char *newtype;
} *lwhead;

u_long atpresponsetimeout = ATPRESPONSETIMEOUT;
char lwfile[256] = LWRENAMEFILE;
char *myname;
char renamestr[] = "\
currentfile\n\
statusdict begin product (LaserWriter IIg) eq version (2010.113) eq and end not\n\
{save exch 291 string readstring pop pop restore} if\n\
/ASCIIHexDecode filter /SystemPatch statusdict /emulate get exec\n\
85f6ba98b8147bdb3c41fc154e390200521caba043febd65f48e008d42590001cd0f62e4c9f2b841c6c1c85660f30002ba262234d72494f203c119951000000376b481858e01bff2db172cf2ecfe000446e2f3ddca7b1fb2d27814e1c22e000598f64cae7bb9897afb760a5d81ac0106>\n\
serverdict begin %s exitserver\n\
statusdict begin\n\
(%s) (%s) currentdict /appletalktype known\n\
{/appletalktype}{/product}ifelse exch def setprintername\n\
end\n\
";
char resetstr[] = "\
currentfile\n\
statusdict begin product (LaserWriter IIg) eq version (2010.113) eq and end not\n\
{save exch 291 string readstring pop pop restore} if\n\
/ASCIIHexDecode filter /SystemPatch statusdict /emulate get exec\n\
85f6ba98b8147bdb3c41fc154e390200521caba043febd65f48e008d42590001cd0f62e4c9f2b841c6c1c85660f30002ba262234d72494f203c119951000000376b481858e01bff2db172cf2ecfe000446e2f3ddca7b1fb2d27814e1c22e000598f64cae7bb9897afb760a5d81ac0106>\n\
serverdict begin %s exitserver\n\
statusdict begin\n\
(%s) (%s) currentdict /appletalktype known\n\
{/appletalktype}{/product}ifelse exch def setprintername\n\
end\n\
";
int s_time = 2 * 60;

char *newpsstring();
char *newstring();
void reset();

main(argc,argv)
int argc;
char **argv;
{
  register char *cp, *tp;
  register FILE *fp;
  register struct lws *lp, *ln;
  register int i;
  int sflag = 0;
  int rflag = 0;
  int cno, ocomp, wcomp;
  /*
   * buf is used to read list lines
   * and set up PostScript to send
   *
   */
  char buf[LWRENAMEBUFSIZ];
  PAPStatusRec status;
  char *malloc();
  long atol();

  if (myname = rindex(*argv, '/'))
    myname++;
  else
    myname = *argv;

  /*
   * Parse the arguments 
   *
   */
  for (argc--, argv++ ; argc > 0 ; argc--, argv++) {
    switch ((*argv)[0]) {  /* option or filename? */
      case '-':
        switch ((*argv)[1]) {  /* see which option is given */
          case 't':
            if ((*argv)[2])
              s_time = atoi(&(*argv)[2]);
            else if (argc < 2)
              Usage();	/* never returns */
            else {
              argc--;
              s_time = atoi(*++argv);
            }
            if (s_time <= 0)
              Usage();
            s_time *= 60;
            break;
          case 's':
            sflag = 1;
	    break;
          case 'r':
            rflag = 1;
	    break;
          default:
            Usage();	/* never returns */
        }  /* end see which option is given */
        break;
      default:    /* doesn't start with -, not an option */
        strcpy(lwfile,*argv);
    }  /* end option or filename? */
  }  /* end parse arguments */

  if (argc > 0)  /* leftover unexpected arguments */
    Usage();	/* never returns */

  /*
   * Open the monitored printer list file.
   *
   */
  if ((fp = fopen(lwfile, "r")) == NULL) {
    fprintf(stderr, "%s: can't open %s\n", myname, lwfile);
    exit(1);
  }

  /*
   * Read the file with list of printers to rename and store information
   * in linked list.
   *
   */
  ln = NULL;
  i = 0;
  while (fgets(buf, LWRENAMEBUFSIZ, fp)) {
    i++;
    if (*buf == '#')	/* allow comments */
      continue;
    if (cp = index(buf, '\n'))
      *cp = '\0';  /* change newline to string terminator */

    /*
     * Find the three tab-separated sections of the line:
     * 	password<tab>printer_NBP_name<tab>newtype
     * Last field (newtype) is optional - will set to LWRENAMETYPE if 
     * not specified.  Use cp & tp pointers to mark off the sections
     * so can then copy them into new variables.
     *
     */
    if ((cp = index(buf, '\t')) == NULL) {  /* no tab after password */
      fprintf(stderr, "%s: Syntax error in %s, line %d\n", myname, lwfile, i);
      exit(1);
    }
    *cp++ = '\0';  /* change tab to string terminator & advance cp */

    /*
     * At least first 2 fields exist (minimum required), so allocate
     * memory for next element in linked list.
     *
     */
    if ((lp = (struct lws *)malloc(sizeof(struct lws))) == NULL) {
      fprintf(stderr, "%s: Out of memory\n", myname);
      exit(1);
    }

    /*
     * Fill in fields for this element of linked list of printers.
     *
     */
    lp->passwd = is_it_a_number(buf) ? newstring(buf) : newpsstring(buf);

    /*
     * Look for third optional new type field, and make sure it is not null.
     *
     */
    if ((tp = index(cp, '\t')) == NULL) {  /* no 3rd field, all 2nd field */
      lp->name = newstring(cp);
      lp->newtype = newstring(LWRENAMETYPE);
    } else {  /* 3rd field found, parse between 2nd & 3rd fields */
      *tp++ = '\0';  /* change tab to string term. & advance tp */
      if (*tp == '\0') {   /* null 3rd field */
        fprintf(stderr, "%s: Syntax error in %s, line %d\n", myname, lwfile, i);
        fprintf(stderr, "\t\ttrailing tab character at end of line\n");
        exit(1);
      }  /* quit if trailing tab at end */
      lp->name = newstring(cp);
      lp->newtype = newstring(tp);
    }

    /*
     * Create the links between this element of list and others.
     *
     */
    if (ln)
      ln->next = lp;
    else
      lwhead = lp;
    ln = lp;
  }  /* end reading list of printers */

  if (lwhead == NULL) {
    fprintf(stderr, "%s: No entries in %s\n", myname, lwfile);
    exit(1);
  }
  ln->next = NULL;
  fclose(fp);

  /*
   * Become a daemon, unless single sweep flag was specified
   *
   */
  if (!sflag)
    disassociate();

  /*
   * Set signal so that if HUP is received, calls "reset" to first put the
   * printer types back to their original values before exiting.
   *
   */
  signal(SIGHUP, reset);

  /* init cap */
  abInit(FALSE);		/* don't printout -- messes up with <stdin> */
  nbpInit();
  PAPInit();			/* init PAP printer routines */
  ATPSetResponseTimeout(atpresponsetimeout); /* set to 2 minutes */

  /*
   * Main loop tries to find each printer in input file list.
   * If found, renames AppleTalk type to specified or default new type.
   * Then sleeps as specified in argument before starting over.
   * Goes on forever until program is killed, unless single sweep flag
   * specified.
   *
   */
  do {
    lp = lwhead;
    /*
     * Loop through each printer in the linked list.
     *
     */
    do {
      /*
       * Open connection to printer.  If found on net with original type,
       * reset to new type.
       *
       */
      if (PAPOpen(&cno, lp->name, atpMaxNum, &status, &ocomp) == noErr) {
	do {
	  abSleep(16, TRUE);
	} while (ocomp > 0);
        /*
         * Need name only (not type or zone) to substitute into rename string.
         * Setting the ":" after name to zero and back again does the trick.
         *
	 */
	cp = index(lp->name, ':');
	*cp = '\0';
	sprintf(buf ,renamestr ,lp->passwd ,lp->name ,lp->newtype);
	*cp = ':';
        writeit(cno, buf);
	PAPClose(cno);
      }  /* end if printer found on net */
    } while (lp = lp->next);   /* end of do-while loop */
    sleep(s_time);
  } while (!sflag);   /* end of loop */
  if (rflag)
    reset();	/* only get here if single sweep flag was set */
  exit(0);
}  /* end main */

/*
 * Reset function used to put things back before exiting.
 * Will find the printers on the network that have
 * been set to the new AppleTalk type and return them
 * to their original types, as specified in LWRENAMEFILE list.
 * Then exits the program.
 *
 */

void
reset()
{
  register char *tp, *cp;
  register struct lws *lp;
  register int i;
  int cno, ocomp;
  char buf[LWRENAMEBUFSIZ];
  char NBPname[256];
  char origname[256];
  char origtype[256];
  PAPStatusRec status;

  signal(SIGHUP, SIG_IGN);
  lp = lwhead;
  /*
   * Run once through all printers in the linked list.
   *
   */
  do {
    /*
     * Create NBP name to search for on network.  This has the new type
     * that has been previously set by this program.
     * 
     * Pick up name section only from lp->name by trick of finding colon
     * and temporarily resetting it to 0.  Also save name part alone into
     * a variable to substitute into the reset string.
     *
     */
    tp = index(lp->name, ':');
    *tp = '\0';
    strcpy(origname, lp->name);
    strcpy(NBPname, lp->name);
    *tp = ':';

    /*
     * Now add new type as the type.
     *
     */
    strcat(NBPname, ":");
    strcat(NBPname, lp->newtype);

    /*
     * Save off the original AppleTalk type (from the linked list of 
     * printers) to use in the reset string.
     *
     */
    tp++;   /* go past colon */
    cp = index(tp, '@');
    *cp = '\0';
    strcpy(origtype, tp);  /* everything past colon up to @ sign */
    *cp = '@';

    /*
     * Finally, get the zone.
     *
     */
    strcat(NBPname, cp);  /* everything from @ sign to end */

    /*
     * Open connection to printer.  If found, send reset string to put 
     * back to original type.
     *
     */
    if (PAPOpen(&cno, NBPname, atpMaxNum, &status, &ocomp) == noErr) {
      do {
        abSleep(16, TRUE);
      } while (ocomp > 0);
      sprintf(buf, resetstr, lp->passwd, origname, origtype);
      writeit(cno, buf);
      PAPClose(cno);
    }
  } while (lp = lp->next);   /* end of do-while loop */
  exit(0);  /* done resetting back to original, exit the program */
}  /* end reset function */

disassociate()
{
  int i;

  if (fork())
    _exit(0);			/* kill parent */
  for (i=0; i < 3; i++)
    close(i); /* kill */
  (void)open("/",0);
  (void)dup2(0,1);
  (void)dup2(0,2);
#ifndef POSIX
#ifdef TIOCNOTTY
  if ((i = open("/dev/tty",2)) > 0) {
    (void)ioctl(i, TIOCNOTTY, (caddr_t)0);
    (void)close(i);
  }
#endif TIOCNOTTY
#else  POSIX
  (void) setsid();
#endif POSIX
}

writeit(cno, str)
int cno;
char *str;
{
  int eof, wcomp, paperr, err, doeof = FALSE;

  wcomp = 0;
  if ((paperr=PAPWrite(cno, str, strlen(str), FALSE, &wcomp)) < 0) {
    return;
  }
  /* post initial read from LW */
  err = 1;
  /* this is the main read/write loop */
  inithandleread();		/* initialze handleread */
  do {
    if ((eof = handleread(cno)))
      break;
    if (wcomp <= 0) {
      if (wcomp != noErr) {
	return;
      } else {
	err = 0;
	doeof = TRUE;
	if (err || doeof) {
	  if ((paperr=PAPWrite(cno, NULL, 0, doeof, &wcomp)) < 0)
	    break;
	} else err = 1;
      }
    }
    abSleep(4, TRUE);		/* wait a bit */
  } while (err > 0 );

  if (paperr != noErr) {
    wcomp = 0;
  }
  while (!eof || wcomp > 0) {		/* wait for completion */
    abSleep(4,TRUE);
    if (!eof)
      eof = handleread(cno);
  }
}

private int hr_rcomp = noErr;
private int hr_rlen = 0;
private char hr_rbuf[R_BUFMAX+10];
private int hr_eof = 0;

inithandleread()
{
  hr_rcomp = noErr;
  hr_rlen = 0;
  hr_eof = 0;
}

/*
 * handle the papread
 *  return: -1 paperr
 *           0 ok
 *           1 eof
 *
 */

handleread(cno)
{
  int paperr;

  if (hr_rcomp > 0)
    return(0);
  switch (hr_rcomp) {
  case noErr:
    break;
  default:
    return(-1);
  }
  hr_rbuf[hr_rlen] = '\0';
  if (hr_eof) {
    return(1);
  }
  paperr = PAPRead(cno, hr_rbuf, &hr_rlen, &hr_eof, &hr_rcomp);
  switch (paperr) {
  case noErr:
    break;
  default:
    return(-1);
  }
  return(0);
}

char *
newstring(str)
char *str;
{
  register char *cp;
  char *malloc();

  if ((cp = malloc(strlen(str) + 1)) == NULL) {
    fprintf(stderr, "%s: newstring: Out of memory\n", myname);
    exit(1);
  }
  strcpy(cp, str);
  return(cp);
}

char *
newpsstring(str)
char *str;
{
  register char *fp, *tp;
  register int len;
  char buf[128];

  tp = buf;
  *tp++ = '(';
  for (len = 1, fp = str ; *fp ; ) {
    if (++len >= (sizeof(buf) - 1)) {
      fprintf(stderr, "%s: newpsstring: String too long\n", myname);
      exit(1);
    }
    switch (*fp) {
     case '(':
     case ')':
     case '\\':
      if (++len >= (sizeof(buf) - 1)) {
        fprintf(stderr, "%s: newpsstring: String too long\n", myname);
        exit(1);
      }
      *tp++ = '\\';
    }
    *tp++ = *fp++;
  }
  *tp++ = ')';
  *tp = 0;
  return(newstring(buf));
}

is_it_a_number(str)
register char *str;
{
  while (*str) {
    if (!isdigit(*str))
      return(0);
    str++;
  }
  return(1);
}

Usage()
{
  fprintf(stderr, "Usage: %s [-t minutes] [-s] [printer_list_file]\n", myname);
  exit(1);
}
