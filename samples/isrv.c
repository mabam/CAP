static char rcsid[] = "$Author: djh $ $Date: 1993/08/05 15:53:17 $";
static char rcsident[] = "$Header: /mac/src/cap60/samples/RCS/isrv.c,v 2.4 1993/08/05 15:53:17 djh Rel djh $";
static char revision[] = "$Revision: 2.4 $";

/*
 * isrv - UNIX AppleTalk test program: act as a laserwriter
 *
 *  very simple test of printer server capabilities for the ImageWriter.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Feb.  1987    CCKim		Created.
 *
 */

char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <netat/appletalk.h>		/* include appletalk definitions */
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

int cno;
#define RFLOWQ 8
#define BUFMAX 512*RFLOWQ
#ifndef SFLOWQ
# define SFLOWQ 8
#endif  SFLOWQ
#define SBUFMAX 512*SFLOWQ
char rbuf[BUFMAX+10];
int xdebug = TRUE;

usage() 
{
  fprintf(stderr,"usage: lsrv -P printer [-T Type] [-d flags]\n");
  fprintf(stderr,"usage: Printer in printcap, type is generic type\n");
  fprintf(stderr,"usage: eg. lsrv -Pxx -TImageWriter\n");
  exit(0);
}

char *prtname = NULL,
     *prttype = "ImageWriter",
     *prtmodel = "";

main(argc,argv)
int argc;
char **argv;
{
  char *s,*arg,buf[100];
  int err;
  PAPStatusRec statusbuff;
  int rcomp, wcomp, paperr;
  int srefnum,i;
  void childdone();


  while (argc > 1 && argv[1][0] == '-') {
    argc--; 
    arg = *++argv;

    switch(arg[1]) {
    case 'D': case 'd':
      if (arg[2] != '\0')
	dbugarg(&arg[2]);
      else if (argc > 1) {
	argc--;
	dbugarg(*++argv);
      }
      break;

    case 'P': case 'p':
      if (arg[2] != '\0')
	prtname = &arg[2];
      else if (argc > 1) {
	argc--;
	prtname = *++argv;
      }
      break;

    case 'T':
      if (arg[2] != '\0')
	prttype = &arg[2];
      else if (argc > 1) {
	argc--;
	prttype = *++argv;
      }
      break;
    }
  }

  if (prtname == NULL)
    usage();

  if (!dbug.db_flgs) {
    /* disassociate */
    if (fork())
      exit(0);			/* kill parent */
    {
      int i;
      for (i=0; i < 20; i++) close(i); /* kill */
      (void)open("/",0);
#ifdef NODUP2
      (void)dup(0);		/* slot 1 */
      (void)dup(0);		/* slot 2 */
#else
      (void)dup2(0,1);
      (void)dup2(0,2);
#endif
#ifdef TIOCNOTTY
      if ((i = open("/dev/tty",2)) > 0) {
	ioctl(i, TIOCNOTTY, 0);
	close(i);
      }
#endif TIOCNOTTY
#ifdef POSIX
      (void) setsid();
#endif POSIX
    }
  }

  abInit(xdebug);		/* initialize appletalk driver */
  nbpInit();
  PAPInit();			/* init PAP printer routines */
  cpyc2pstr(statusbuff.StatusStr, "Status: initializing");

  printf("Spooler starting for %s.  Type is %s, model %s\n",
	 prtname,prttype, prtmodel);

  sprintf(buf,"%s:%s@*",prtname,prttype);
  err = SLInit(&srefnum, buf, 8, &statusbuff);
  if ( err < 0) {
    fprintf(stderr,"Errror = %d\n", err);
    exit(8);
  }

  abSleep(60, TRUE);

  signal(SIGCHLD, childdone);
  do {
    statusbuff.StatusStr[0] = '\02'; /* some fake out??? */
    statusbuff.StatusStr[1] = 0x00;
    statusbuff.StatusStr[2] = 0x80;
    err = GetNextJob(srefnum, &cno, &rcomp);
    if (err < 0) {
      fprintf(stderr, "Open failed with %d\n",err);
      exit(1);
    }
    do { abSleep(4*20, TRUE); } while (rcomp  > 0);

/*    strcpy(statusbuff.StatusStr, "Status: busy, processing job"); */
#ifndef DEBUG
    if (fork() == 0) {
#else
    {
#endif
      char tname[100];
      char buf[256];

      strcpy(tname, "/tmp/lsrvXXXXXX");
      mktemp(tname);
      if (freopen(tname, "w+", stdout) != NULL) {
#ifndef DEBUG
	SLClose(srefnum);		/* close down server for child */
#endif
	getjob(cno);
	/* end eof */
	paperr = PAPWrite(cno, NULL, 0, TRUE, &wcomp);
	if (paperr != noErr)
	  fprintf(stderr,"PAPWrite error %d\n",paperr);
	else
	  do { abSleep(4, TRUE); } while (wcomp > 0);
	PAPClose(cno, TRUE);
	fclose(stdout);
	sprintf(buf,"/usr/ucb/lpr -P%s -r -T 'from AppleTalk' %s\n",
		prtname,tname);
#ifdef DEBUG
	fprintf(stderr,"Starting: %s\n",buf);
#endif
	system(buf);
#ifndef DEBUG
	unlink(tname);
#endif
      } else perror("freopen");
#ifndef DEBUG
      exit(0);
#endif
    }
#ifndef DEBUG
    PAPShutdown(cno);		/* get rid of it */
#endif
  } while (1);
  exit(0);			/* exit okay */
}

void
childdone()
{
  WSTATUS status;

  (void)wait(&status);
  signal(SIGCHLD, childdone);
}

/*
 * handle the incoming job
 *
*/
getjob(cno)
int cno;
{
  int rcomp, rlen, eof, err;
  eof = 0;
  while (!eof) {
    err = PAPRead(cno, rbuf, &rlen, &eof, &rcomp);
    if (err < 0) {
      return(0);		/* ? */
    }
    do {
      if (rcomp <= 0) {
	if (rcomp != noErr) {
	  fprintf(stderr,"PAPRead error %d\n",rcomp);
	  return(0);
	}
	break;
      }
      abSleep(4, TRUE);
    } while (1);
    if (rlen > 0)
      write(fileno(stdout), rbuf, rlen);
  }
  return(0);
}
