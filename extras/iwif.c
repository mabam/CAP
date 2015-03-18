static char rcsid[] = "$Author: djh $ $Date: 91/02/15 22:30:48 $";
static char rcsident[] = "$Header: iwif.c,v 2.1 91/02/15 22:30:48 djh Rel $";
static char revision[] = "$Revision: 2.1 $";
/*
 *
 * iwif.c -- ImageWriter Input Spooler - for BSD 4.2 LPD
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Jan.  1986    CCKim		Created - ln03 input filer.
 *  Feb.  1987    CCKim		Modified for ImageWriter
 *
 */

char copyright[] = "Copyright (c) 1986, 1987 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <signal.h>

char printer[32];		/* printer name */
char user[32];			/* user name */
char host[32];			/* printing host name */

/* this guy sets up the printer -- don't do anything fancy now */
reset_imagewriter()
{
  putchar('\033');		/* reset printer */
  putchar('c');
}

/* quit gets called when we've been killed by lprm via SIGINT */
quit()
{
  putchar('\033');		/* reset printer */
  putchar('c');
  exit(0);			/* bye bye */
}  

main(argc, argv)
int argc;
char *argv[];
{
  int i,c,c2;

  signal(SIGINT,quit);		/* this is what lprm sends us */

  printer[0]=0;

  for (i=0; i<argc; i++) {
    if (strcmp(argv[i],"-p") == 0)
      strcpy(printer,argv[i+1]);
    if (strcmp(argv[i],"-n") == 0)
      strcpy(user,argv[i+1]);
    if (strcmp(argv[i],"-h") == 0)
      strcpy(host,argv[i+1]);
  }

#ifdef notdef
  if ((bannerfd = open(".banner", 0)) >= 0) {
    sendbanner(ptr, bannerfd); /* don't care if we can't send it */
    close(bannerfd);
  }
#endif

  reset_imagewriter();		/* get things set up */

  do {
    if ((c = getchar()) == EOF)
      break;
#ifdef IMAGEWRITER
    /* These codes are ImageWriter II only */
    if (c == '\033') {
      if ((c2 = getchar()) == EOF) {
	putchar(c);
	break;
      }
      switch (c2) {
      case 'K':
	getchar();		/* eat it */
	break;
      case 'H':
	getchar(); getchar(); getchar(); getchar(); /* eat it */
	break;
      default:
	putchar(c);
	putchar(c2);
	break;
      }
      continue;
    }
#endif
    putchar(c);
  } while (1);

  /* How do we figure out how many pages we printed? */
  fflush(stdout);		/* flush print buffer */
  quit();			/* and go away */
}
