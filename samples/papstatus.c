static char rcsid[] = "$Author: djh $ $Date: 1995/05/31 13:03:31 $";
static char rcsident[] = "$Header: /mac/src/cap60/samples/RCS/papstatus.c,v 2.5 1995/05/31 13:03:31 djh Rel djh $";
static char revision[] = "$Revision: 2.5 $";

/*
 * papstatus - UNIX AppleTalk program: simple status display
 *  for LaserWriter
 *
 * Based on papif
 *
 * Usage:
 * papstatus -n nbp      status from NBP entry (name:type@zone)
 * papstatus printer..   status from printer
 * papstatus -a          status from all printers in /etc/cap.printers
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *	July	 1993	 MJC	Created from papif
 *
 */


char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <sys/param.h>
#ifndef _TYPES
#include <sys/types.h>		/* in case param doesn't */
#endif  _TYPES
#include <signal.h>

#include <netat/appletalk.h>		/* include appletalk definitions */
#include <netat/compat.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH
#ifdef USEVPRINTF
# include <varargs.h>
#endif USEVPRINTF
#ifdef xenix5
# include <unistd.h>
#endif xenix5

/* Configuration options */

#ifndef CAPPRINTERS
# define CAPPRINTERS "/etc/cap.printers"
#endif


#ifndef ATPRESPONSETIMEOUT
# define ATPRESPONSETIMEOUT sectotick(60*2)
#endif


/*
 * GLOBAL VARIABLES
*/
/* don't know where I got 30 from */
char printer[30];		/* printer name */
int  verbose = 0;

char *lwname = NULL;			/* entity name */
int  doall = 0;



char *capprinters = CAPPRINTERS; /* location of cap.printers */
u_long atpresponsetimeout = ATPRESPONSETIMEOUT;	/* atp resp. cache timeout */


/* Definitions */


/* logging levels */
#define log_i dolog /* information */
#define log_w dolog /* warning */
#define log_e dolog /* error */
#define log_r dolog /* return from remote */
#define log_d dolog /* log debugging */



main(argc,argv)
int argc;
char **argv;
{
  int pstatus();
  char *getlwname();
  extern boolean dochecksum;
  char	*c, *getenv();
  char	*arg;

  if ((c = getenv("CAPPRINTERS")) != NULL)
    capprinters = c;

  argv++; argc--;
  while (argc > 0 && **argv == '-') {
    arg = *argv++; argc--; arg++;
    while (*arg) {
      switch (*arg++) {
      case 'a':
	doall++;
	break;
      case 'c':
	capprinters = *argv++; argc--;
	break;
      case 'd':
	dbugarg(*argv++); argc--;
	break;
      case 'k':			/* no DDP checksum */
	dochecksum = 0;
	break;
      case 'n':
	lwname = *argv++; argc--;
	break;
      case 'p':			/* printer name */
      case 'P':			/* printer name */
	lwname = getlwname(*argv++); argc--;
	if (lwname == NULL) {
	  log_e("papstatus: Cannot map name %s to LaserWriter name\n",argv[-1]);
	  exit(1);
	}
	break;
      case 'v':
	verbose++;
	break;
      default:
	log_e("papstatus: Unknown argument %c\n",*arg);
      }
    }
  }

  /* init cap */
  abInit(FALSE);		/* don't printout -- messes up with <stdin> */
  nbpInit();
  PAPInit();			/* init PAP printer routines */
  ATPSetResponseTimeout(atpresponsetimeout); /* set to 2 minutes */

  if (doall)
    status_all ();
  else if (lwname)
    getstatus (lwname);
  else {
    while (argc-- > 0) {
      lwname = getlwname(*argv);
      if (lwname == NULL) {
	log_e("papstatus: Cannot map name %s to LaserWriter name\n",*argv);
	continue;
      }
      printf ("%s (%s):\n", *argv, lwname);
      getstatus (lwname);
      argv++;
    }
  }


}


getstatus (name)
char	*name;
{
  AddrBlock addr;
  PAPStatusRec status;
  int	nostat;

  addr.net = 0;
  addr.node = 0;
  addr.skt = 0;

  if (verbose)
    log_i("papstatus: Getting status on printer %s\n",name);

  do {
    PAPStatus (name, &status, &addr);
    
    nostat = strcmp ((char *)(status.StatusStr + 1), "%no status");
    if (verbose || nostat != 0)
      pstatus(status.StatusStr);
  } while (nostat == 0);
}


/*
 * output status message to stdout.
 * Note: input string is a pascal string
 *
 */

pstatus(s)
char *s;
{
  printf ("%*.*s\n\n", *s, *s, s+1);
}


status_all ()
{
#ifdef NOCAPDOTPRINTERS
  fprintf(stderr, "Sorry, CAP was configured without cap.printers support\n");
#else  NOCAPDOTPRINTERS
  FILE *fd;
  static char buf[1024];
  char *ep;

  if ((fd = fopen(capprinters,"r")) == NULL) {
    perror(capprinters);
    return;
  }
  do {
    if (fgets(buf, 256, fd) == NULL)
      break;
    buf[strlen(buf)-1] = '\0';	/* get rid of the lf */
    if (buf[0] == '#' || buf[0] == '\0')
      continue;
    if ((ep=index(buf,'=')) == NULL) /* find first = */
      continue;			/* no = in string */
    *ep = '\0';			/* set = to null now */
    if (strlen(ep+1) == 0)	/* no name */
      continue;
    lwname = ep+1;
    printf ("%s (%s):\n", buf, lwname);
    getstatus (lwname);
  } while (1);
  fclose(fd);
#endif NOCAPDOTPRINTERS
}

/*
 * get the laserwriter name of the unix spooled printer
 *
 */

char *
getlwname(printer)
char *printer;
{
  FILE *fd;
  static char buf[1024];
  char *ep;

#ifdef NOCAPDOTPRINTERS
  sprintf(buf, "/etc/lp/printers/%s/comment", printer);
  if ((fd = fopen(buf, "r")) == NULL) {
    perror(buf);
    return(NULL);
  }
  if (fgets(buf, sizeof(buf), fd) == NULL)
    return(NULL);
  buf[strlen(buf)-1] = '\0';  /* get rid of the lf */
  return(buf);
#else  NOCAPDOTPRINTERS
  if ((fd = fopen(capprinters,"r")) == NULL) {
    perror(capprinters);
    return(NULL);
  }
  do {
    if (fgets(buf, 256, fd) == NULL)
      break;
    buf[strlen(buf)-1] = '\0';	/* get rid of the lf */
    if (buf[0] == '#' || buf[0] == '\0')
      continue;
    if ((ep=index(buf,'=')) == NULL) /* find first = */
      continue;			/* no = in string */
    *ep = '\0';			/* set = to null now */
    if (strcmp(buf,printer) == 0) {
      if (strlen(ep+1) == 0)	/* no name */
	continue;
      fclose(fd);
      return(ep+1);		/* return pointer to value */
    }
  } while (1);
  fclose(fd);
  return(NULL);
#endif NOCAPDOTPRINTERS
}


/*
 * Setup this so we can be smarter about errors in future
 * logging level are setup as: i - information, w - warning
 * e - error, r - return from laserwriter, and d - for debugging
 *
*/

private FILE *jobout;

#ifndef USEVPRINTF
/* Bletch - gotta do it because pyramids don't work the other way */
/* (using _doprnt and &args) and don't have vprintf */
/* of course, there will be something that is just one arg larger :-) */
/* VARARGS1 */
dolog(fmt, a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af)
char *fmt;
#else
dolog(va_alist)
va_dcl
#endif
{
#ifdef USEVPRINTF
  register char *fmt;
  va_list args;

  va_start(args);
  fmt = va_arg(args, char *);
  if (jobout)
    vfprintf(jobout, fmt, args);
  vfprintf(stderr, fmt, args);
  va_end(args);
#else
  /*
   * Keep buffers flushed to avoid double-output after fork();
   */
  if (jobout) {
    fprintf(jobout, fmt, a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af);
    fflush(jobout);
  }
  fprintf(stderr, fmt, a1,a2,a3,a4,a5,a6,a7,a8,a9,aa,ab,ac,ad,ae,af);
  fflush(stderr);
#endif
}

/* END MODULE: log */
