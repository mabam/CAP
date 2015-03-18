/*
 * Copyright (c) 1988, The Regents of the University of California.
 * Edward Moy, Workstation Software Support Group, Workstation Support Serices,
 * Information Systems and Technology.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of California makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#ifndef lint
static char *SCCSid = "@(#)macps.c	2.2 10/25/89";
#ifdef ADOBE
static char *SCCSid2 = "Modified 6/27/89 by jaffe@rutgers to include support for Transcript";
#endif ADOBE
#endif lint

#include <ctype.h>
#include <stdio.h>
#ifdef SYSV
#include <string.h>
#else SYSV
#include <strings.h>
#endif SYSV
#include <sys/types.h>
#include <sys/file.h>
#include "str.h"
#include "ucbwhich.h"
#ifdef ADOBE
#include <sys/time.h>
#endif ADOBE

#define	CONFIG		"macps.config"
#ifdef SYSV
#define	index		strchr
#define	rindex		strrchr
#endif SYSV

#ifdef ADOBE
#define debugp(x) {fprintf x ; (void) fflush(stderr);}
#endif ADOBE
#ifdef SAVE
char *finale = "clear countdictstack 2 sub{end}repeat macps restore\n";
char intro[] = "\
%%! *** Created by macps: %s\
/macps save def\n\
";
#else SAVE
char intro[] = "\
%%! *** Created by macps: %s\
";
#endif SAVE
char *myname;
int ncopies = 0;
#ifdef CONFIGDIR
char ucblib[UCBMAXPATHLEN] = CONFIGDIR;
#else CONFIGDIR
int ucbalternate;
char ucbpath[UCBMAXPATHLEN];
char ucblib[UCBMAXPATHLEN];
#endif CONFIGDIR

#ifdef ADOBE
extern char *envget();
extern char *getenv();
FILE  *jobout;  /* for debugging and logging */
int   Debugging = 0;      /* debugging  boolean */
int   verboselog = 0;     /* do we want messages dumped to a log */
int   havejobout = 0;
long  starttime;          /* for time in messages to log */
char           *outname;
#endif ADOBE

main(argc, argv)
int argc;
char **argv;
{
	register STR* str;
	register char *cp, *pp;
	register FILE *fp;
	register int i, fd;
	char line[BUFSIZ];
	char path[UCBMAXPATHLEN];
	long ltime;
	char *ctime();
#ifdef ADOBE
	char           *Debug;
	char           *Verbose;
#endif ADOBE

#ifndef CONFIGDIR
	ucbwhich(*argv);
#endif CONFIGDIR
	if(myname = rindex(*argv, '/'))
		myname++;
	else
		myname = *argv;
	cp = NULL;

#ifdef ADOBE
	/* set up for debugging */
	Debug = envget("DEBUG");
	if (Debug != NULL)
	  Debugging = atoi(Debug);

	/* set up stderr to go to the right place ala transcript */

	setjobout();

	/* identify myself like most of the postscript filters do */

	Verbose = envget("VERBOSELOG");
	if (Verbose != NULL)
	  verboselog = atoi(Verbose);

	if (verboselog)
	  {
	    (void) time(&starttime);
	    if (havejobout)
	      {
		fprintf(jobout, "%s: Started at %s\n", myname, ctime(&starttime));
		(void) fflush(jobout);
	      }
	    else
	      {
		fprintf(stderr, "%s: Started at %s\n", myname, ctime(&starttime));
		(void) fflush(stderr);
	      }
	  }
#endif ADOBE

	for(argc--, argv++ ; argc > 0 && **argv == '-' ; argc--, argv++) {
		switch((*argv)[1]) {
		 case 'c':	/* multiple copies */
			if((*argv)[2])
				ncopies = atoi(&(*argv[2]));
			else {
				if(argc < 2)
					Usage();	/* never returns */
				argc--;
				ncopies = atoi(*++argv);
			}
			if(ncopies <= 0)
				Usage();	/* never returns */
			break;
		 case 'd':	/* alternate directory for config file */
			if((*argv)[2])
				cp = &(*argv[2]);
			else {
				if(argc < 2)
					Usage();	/* never returns */
				argc--;
				cp = *++argv;
			}
			strcpy(ucblib, cp);
			break;
		 case 'r':	/* raw mode */
			rawmode++;
			break;
		 default:
			Usage();	/* never returns */
		}
	}
	if(argc > 1)
		Usage();	/* never returns */
	if(argc == 1 && freopen(*argv, "r", stdin) == NULL) {
#ifdef ADOBE
		debugp((jobout, "%s: can't open %s\n", myname, *argv));
#else  ADOBE
		fprintf(stderr, "%s: can't open %s\n", myname, *argv);
#endif ADOBE
		exit(2);
	}
	str = STRalloc();
	if(!STRgets(str, stdin)) {
#ifdef ADOBE
		debugp((jobout, "%s: Null input\n", myname));
#else  ADOBE
		fprintf(stderr, "%s: Null input\n", myname);
#endif ADOBE
		exit(2);
	}
	strcat(ucblib, "/");
	strcpy(path, ucblib);
	strcat(path, CONFIG);
	if((fp = fopen(path, "r")) == NULL) {
#ifdef ADOBE
		debugp((jobout, "%s: Can't open %s\n", myname, path));
#else  ADOBE
		fprintf(stderr, "%s: Can't open %s\n", myname, path);
#endif ADOBE
		exit(2);
	}
	time(&ltime);
	printf(intro, ctime(&ltime));
	do {
		if(ncopies != 0 && STRheadcompare(str, "userdict /#copies ")
		 == 0)
			continue;
		if(STRcompare(str, "%%EOF\n") == 0) {
#ifdef SAVE
			if(finale) {
				fputs(finale, stdout);
				finale = NULL;
			}
#endif SAVE
			STRputs(str, stdout);
			continue;
		}
		if(STRheadcompare(str, "%%IncludeProcSet:") == 0) {
			for(cp = (char *)&str->bufptr[17] ; ; cp++) {
				if(!*cp) {
#ifdef ADOBE
					debugp((jobout,
				 "%s: Syntax error on IncludeProcSet line\n",
				   myname));
#else  ADOBE
					fprintf(stderr,
				 "%s: Syntax error on IncludeProcSet line\n",
					 myname);
#endif ADOBE
					exit(2);
				}
				if(!isascii(*cp) || !isspace(*cp))
					break;
			}
			pp = (char *)str->curendptr;
			while(--pp >= cp) {
				if(!isascii(*pp) || !isspace(*pp))
					break;
				*pp = 0;
			}
			str->curendptr = (unsigned char *)(pp + 1);
			fseek(fp, 0L, 0);
			for( ; ; ) {
				if(!fgets(line, BUFSIZ, fp)) {
#ifdef ADOBE
					debugp((jobout,
					 "%s: Unknown IncludeProcSet %s\n",
					 myname, cp));
#else  ADOBE
					fprintf(stderr,
					 "%s: Unknown IncludeProcSet %s\n",
					 myname, cp);
#endif ADOBE
					exit(2);
				}
				if(*line == '#')
					continue;
				if(pp = index(line, '\n')) {
					if(pp == line)
						continue;
					*pp = 0;
				}
				if(!(pp = index(line, '\t'))) {
#ifdef ADOBE
					debugp((jobout,
					 "%s: Syntax error in macps.config\n",
					 myname));
#else  ADOBE
					fprintf(stderr,
					 "%s: Syntax error in macps.config\n",
					 myname);
#endif ADOBE
					exit(2);
				}
				*pp++ = 0;
				if(STRcompareptr(str, cp, line) == 0)
					break;
			}
			if(*pp == '/')
				strcpy(path, pp);
			else {
				strcpy(path, ucblib);
				strcat(path, pp);
			}
			fflush(stdout);
			if((fd = open(path, O_RDONLY, 0)) < 0) {
#ifdef ADOBE
				debugp((jobout, "%s: Can't open %s\n", myname,
				 path));
#else  ADOBE
				fprintf(stderr, "%s: Can't open %s\n", myname,
				 path);
#endif ADOBE
				exit(2);
			}
			while((i = read(fd, line, BUFSIZ)) > 0)
				write(1, line, i);
			close(fd);
			continue;
		}
		STRputs(str, stdout);
		if(ncopies > 1 && isascii(*str->bufptr) &&
		 isdigit(*str->bufptr)) {
			cp = (char *)str->bufptr;
			while(cp < (char *)str->curendptr && isascii(*cp)
			 && isdigit(*cp))
				cp++;
			if((char *)str->curendptr - cp == 4 &&
			 STRcompareptr(str, cp, " mf\n") == 0) {
				printf("userdict /#copies %d put\n", ncopies);
				ncopies = -1;
			}
		}
	} while(STRgets(str, stdin));
#ifdef SAVE
	if(finale)
		fputs(finale, stdout);
#endif SAVE
	exit(0);
}

Usage()
{
	fputs("Usage: macps [-c #] [-d directory] [-r] [file]\n", stderr);
	exit(2);
}

#ifdef ADOBE
setjobout()
{
/* So  that all messages will go to someplace that is permanent.  This is to
   defeat Sun 4.0's lpd that uses a temporary file for error messages
   (stderr) that disappears as soon as the printjob stops */

  /* get jobout from environment if there, otherwise use stderr */
  if (((outname = envget("JOBOUTPUT")) == NULL)
      || ((jobout = fopen(outname,"a")) == NULL)) {
    debugp((stderr,"%s: Failure opening log file (%s)\n", myname, outname));
    jobout = stderr;
  }
  else havejobout = 1;
}


/* envget is a getenv
 * 	if the variable is not present in the environment or
 *	it has the null string as value envget returns NULL
 *	otherwise it returns the value from the environment
 */

char *envget(var)
char *var;
{
    register char *val;
    if (((val = getenv(var)) == NULL) || (*val == '\0'))
    	return ((char *) NULL);
    else return (val);
}
#endif ADOBE
