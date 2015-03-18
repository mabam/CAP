static char rcsid[] = "$Author: djh $ $Date: 1996/06/18 10:51:10 $";
static char rcsident[] = "$Header: /mac/src/cap60/samples/RCS/lwpr.c,v 2.5 1996/06/18 10:51:10 djh Rel djh $";
static char revision[] = "$Revision: 2.5 $";

/*
 * lwpr - UNIX AppleTalk test program: print a ps file to appletalk LaserWriter
 *  or Appletalk ImageWriter (depending if IMAGEWRITER is defined).
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia
 *   University in the City of New York.
 *
 * Edit History:
 *
 *  June 29, 1986    Schilit&CCKim	Created.
 *  July  5, 1986    CCKim		Clean up
 *  Feb.     1987    CCKim              Handle ImageWriter.
 *
 */

char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the City of New York";


#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>

#include <netat/appletalk.h>		/* include appletalk definitions */
#ifdef USESTRINGDOTH
# include <string.h>
#else
# include <strings.h>
#endif
#if defined(xenix5) || defined(SOLARIS)
# include <unistd.h>
#endif /* xenix5 || SOLARIS */
#ifdef linux
# include <unistd.h>
#endif /* linux */

#ifndef CAPPRINTERS
# define CAPPRINTERS "/etc/cap.printers"
#endif

char *fname ;
int cno;
int pid;
#ifdef IMAGEWRITER
# define RFLOWQ 1
#else  IMAGEWRITER
# define RFLOWQ 8
#endif IMAGEWRITER
#define BUFMAX 512*RFLOWQ
#ifndef SFLOWQ
#ifdef IMAGEWRITER
# define SFLOWQ 1
#else  IMAGEWRITER
# define SFLOWQ 8
#endif IMAGEWRITER
#endif
#define SBUFMAX 512*SFLOWQ
char buf[SBUFMAX+10];
int xdebug = TRUE;

usage(pgm)
char *pgm;
{
  fprintf(stderr,"%s [-d<debug args>] [-p printer] file [file]*\n",pgm);
  fprintf(stderr, "\tnote: uses PRINTER environment var if printer name\n");
  fprintf(stderr, "\tnot given (requires %s)\n",CAPPRINTERS);
  exit(1);
}

main(argc,argv)
int argc;
char **argv;
{
  char *s;
  void stopall();
  int pstatus();
  PAPStatusRec statusbuff;
  int c;
  char *LWNAME;
  char *getlwname();
  AddrBlock addr;
  extern char *optarg;
  extern int optind;

  pid = -1;
  abInit(xdebug);		/* initialize appletalk driver */
  nbpInit();
  PAPInit();			/* init PAP printer routines */

  LWNAME = NULL;
  while ((c = getopt(argc, argv, "d:p:")) != EOF) {
    switch (c) {
    case 'd':
      dbugarg(optarg);
      break;
    case 'p':
      LWNAME = optarg;
      break;
    case '?':
      usage(argv[0]);
      break;
    }
  }

  if (optind == argc)		/* no file name given */
    usage(argv[0]);

  if (LWNAME == NULL || strlen(LWNAME) == 0) {
    LWNAME = getlwname((char *)getenv("PRINTER"));
  }
  if (LWNAME == NULL)
    usage(argv[0]);
  addr.net = 0;			/* tell papstatus we don't know */

  PAPStatus(LWNAME, &statusbuff, &addr);
  printf("Status: ");
  dumppstr(statusbuff.StatusStr);

  signal(SIGHUP, stopall);
  signal(SIGINT, stopall);

  cno = openlw(LWNAME);

  for (; optind < argc; optind++ ) {
    s = argv[optind];
    if (access(s, R_OK) == 0)
      sendfile(s);
    else
      perror(s);
  }
  PAPClose(cno);
}


/*
 * open laserwriter connection
 * log errors every 5 minutes to stderr
 *
*/
int
openlw(lwname)
char *lwname;
{
  int i, cno, ocomp, err;
  PAPStatusRec status;

  i = 0;
  /* Keep trying to open */
  while ((err = PAPOpen(&cno, lwname, RFLOWQ, &status, &ocomp) ) != noErr) {
    if (err != -1)		/* should be can't find lw.. */
      fprintf(stderr,"PAPOpen returns %d\n",err);
    else {
      if ((i % 12) == 0) {	/* waited 1 minute? */
	fprintf(stderr, "Problems finding %s\n",lwname);
	i = 1;
      } else i++;
    }
    sleep(5);			/* wait N seconds */
  }
  do {
    abSleep(16, TRUE);
    dumppstr(status.StatusStr);
  } while (ocomp  > 0);
  return(cno);
}

/*
 * handle the papread
 *  return: -1 paperr
 *           0 ok
 *           1 eof
*/
handleread(cno)
{
  static int rcomp = noErr;
  static int rlen = 0;
  static char rbuf[BUFMAX+10];
  static int eof = 0;
  int paperr, eofgotten;

  if (rcomp > 0)
    return(0);
  switch (rcomp) {
  case noErr:
    break;
  default:
    fprintf(stderr, "PAPRead error %d\n", rcomp);
    return(-1);
  }
  if (rlen)
    write(fileno(stdout), rbuf, rlen);
  eofgotten = eof;
  eof = 0;
  paperr = PAPRead(cno, rbuf, &rlen, &eof, &rcomp);
  switch (paperr) {
  case noErr:
    break;
  default:
    fprintf(stderr,"PAPRead error\n");
    return(-1);
  }
  return(eofgotten);
}

/*
 * Send a file
 *  return TRUE on error on pap connection
 *  false ow.
*/
sendfile(fname)
char *fname;
{
  char *getusername();
  int fd, err;
  int eof, wcomp, paperr;

  printf("Sending %s\n",fname);
  fd = open(fname,0);
  if (fd < 0) {
    perror(fname);
    return(FALSE);
  } 
  wcomp = 0;
#ifndef IMAGEWRITER
  strcpy(buf, "/statusdict where {pop statusdict /jobname (");
  strcat(buf, getusername());
  strcat(buf, "; document: ");
  strcat(buf, fname);
  strcat(buf, ") put} if\n");
  if ((paperr=PAPWrite(cno, buf,strlen(buf), FALSE, &wcomp)) < 0) {
    printf("Error in first line\n");
    return(TRUE);
  }
#endif
  err = SBUFMAX;			/* good inital value */
  do {
    if ((eof = handleread(cno)))
      break;
    if (wcomp <= 0) {
      if (wcomp != noErr) {
	fprintf(stderr,"PAPWrite completion error %d\n",&wcomp);
	break;
      } else {
	err = read(fd, buf, SBUFMAX);
	if (err >= 0) 
	  if ((paperr = PAPWrite(cno, buf, err,
				 err == SBUFMAX ? FALSE : TRUE, &wcomp)) < 0)
	    break;
      }
    }
    abSleep(4, TRUE);		/* wait a bit */
  } while (err == SBUFMAX );

  if (paperr != noErr)
    fprintf(stderr,"PAPWrite call error %d\n",paperr);
  if (err < 0)
    perror("read");
  while (!eof || wcomp > 0) {
    if (!eof)
      eof = handleread(cno);
    abSleep(4, TRUE);
  }
  close(fd);
  return(((eof<0) || (paperr != noErr) || (wcomp != noErr))?TRUE:FALSE);
}


void
stopall()
{
  PAPClose(cno, FALSE);
  if (pid != -1) kill(pid, SIGHUP);
  exit(1);
}

/*
 * get the laserwriter name of the unix spooled printer
 *
 * returns NULL if nothing found
 * returns 'LaserWriter Plus' if printer is null
*/
char *
getlwname(printer)
char *printer;
{
  FILE *fd;
  static char buf[256];
  char *ep;

#ifdef IMAGEWRITER
  if (printer == NULL || printer[0] == '\0')
    return("AppleTalk ImageWriter:ImageWriter@*");
#else
  if (printer == NULL || printer[0] == '\0')
    return("LaserWriter Plus:LaserWriter@*");
#endif
  if ((fd = fopen(CAPPRINTERS,"r")) == NULL) {
    perror("fopen");
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
}

/*
 * Dump a PAP status message
 *
*/
dumppstr(pstr)
unsigned char *pstr;
{
  int len = (int)(pstr[0]);
  unsigned char *s = &pstr[1];

  while (len--) {
    if (isprint(*s))
      putchar(*s);
    else
      printf("\\%o",*s&0xff);
    s++;
  }
  putchar('\n');
}

#include <pwd.h>
char *
getusername()
{
  struct passwd *pw;
  static char buf[256+20];	/* enough for host + user */
  if (gethostname(buf, 255) < 0)
    strcpy(buf, "unknown host");
  strcat(buf, ":");
  if ((pw = getpwuid(getuid())) == NULL)
    strcat(buf, "unknown user");
  else
    strcat(buf, pw->pw_name);
  return(buf);
}

