static char rcsid[] = "$Author: djh $ $Date: 1994/10/11 07:28:06 $";
static char rcsident[] = "$Header: /mac/src/cap60/contrib/AppManager/RCS/aufsmon.c,v 2.4 1994/10/11 07:28:06 djh Rel djh $";
static char revision[] = "$Revision: 2.4 $";

/*
 * aufsmon [-fpv] [-s N] <listname>
 *
 * Display the number of uses (eg: copies of an application running)
 * for each file named in the <listname> file. The running aufs server
 * must have had 'APPLICATION_MANAGER' defined and have been started with
 * the -A <listname> option.
 *
 * Options:
 *	-v	verbose, print a '1' if byte in range is locked, else '0'
 *	-p	verbose, print the process ids of the locking process
 *	-f	formfeed, print a ^L at the start of each group of files
 *	-s N	sleep for N seconds between printouts, default 10 seconds
 *
 * Copyright (c) 1991, The University of Melbourne
 * djh@munnari.OZ.AU
 * September 1991
 * August 1993
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef sun
#ifdef __svr4__
#define SOLARIS
#endif __svr4__
#endif sun

#include <netat/sysvcompat.h>

#ifdef apollo
#include <netat/fcntldomv.h>
#endif apollo

struct flist {
	char *filename;
	int protected;
	short incarnations;
	struct flist *next;
};

#define PERIOD	10

struct flist *head, *newp;		/* file list		*/
int verboseflag = 0;			/* more detail		*/
int processid = 0;			/* show process ids	*/
int formfeed = 0;			/* show a ^L		*/
int period = PERIOD;
char *progname;


main(argc, argv)
int argc;
char *argv[];
{
	char buf[MAXPATHLEN*2];
	FILE *fp, *fopen();
	void checklocks();
	int num, protect;
	char *cp;

	head = NULL;
	progname = *argv;

	while(--argc > 0 && (*++argv)[0] == '-') {
		for(cp = argv[0]+1 ; *cp != '\0' ; cp++) {
			switch (*cp) {
				case 'f':
					formfeed++;
					break;
				case 'p':
					processid++;
					/* fall thro' */
				case 'v':
					verboseflag++;
					break;
				case 's':
					if (--argc > 0) 
					  period = atoi(*++argv);
					if (period == 0)
					  period = PERIOD;
					break;
				default:
					usage();
					break;
			}
		}
	}

	if (argc != 1)
		usage();

	if ((fp = fopen(*argv, "r")) == NULL) {
	  perror(progname);
	  exit(1);
	}

	/* read the file contents */

	while (fgets(buf, sizeof(buf), fp) != NULL) {
	  if (buf[0] == '#')	/* comment, ignore */
	    continue;
	  if ((cp = (char *)rindex(buf,':')) == NULL) {
	    printf("%s: bad format in %s\n", progname, *argv);
	    exit(1);
	  }
	  *cp++ = '\0';
	  if ((num = atoi(cp)) <= 0) {
	    printf("%s: illegal count in %s (%d)\n", progname, *argv, num);
	    exit(1);
	  }
	  protect = ((char *)index(cp, 'P') == NULL) ? 0 : 1;
	  if ((newp = (struct flist *) malloc(sizeof(struct flist))) == NULL) {
	    perror(progname);
	    exit(1);
	  }
	  if ((newp->filename = (char *) malloc(strlen(buf)+12)) == NULL) {
	    perror(progname);
	    exit(1);
	  }
	  if ((cp = (char *)rindex(buf,'/')) == NULL)
	    continue; /* just ignore it */
	  *cp++ = '\0';
	  strcpy(newp->filename, buf);
	  strcat(newp->filename, "/.resource/");
	  strcat(newp->filename, cp);
	  newp->incarnations = num;
	  newp->protected = protect;
	  newp->next = head;
	  head = newp;
	}
	fclose(fp);

	for ( ; ; ) {
	  if (formfeed)
	    putchar(014);
	  for (num = 0, newp = head ; newp != NULL ; newp = newp->next) {
	    if ((cp = (char *)rindex(newp->filename, '/')) == NULL)
	      cp = newp->filename;
	    else
	      cp++;
	    printf("%-16s\t%4d\t", cp, newp->incarnations);
	    checklocks(newp->filename, newp->incarnations);
	    if (newp->protected)
	      printf(" (no copy)");
	    printf("\n");
	    num++;
	  }
	  if (num > 1)
	    putchar('\n');
	  fflush(stdout);
	  sleep(period);
	}
}

/*
 * check the file 'name' for a shared advisory lock
 * on a single byte in the range 1 - maxm
 *
 */

void
checklocks(name, maxm)
char *name;
int maxm;
{
	int i, fd, qty;
	struct flock flck;

	if ((fd = open(name, O_RDONLY, 0644)) < 0) {
	  perror(name);
	  return;
	}
	for (i = 1, qty = 0 ; i <= maxm ; i++) {
	  flck.l_type = F_WRLCK;
	  flck.l_whence = SEEK_SET;
	  flck.l_start = i+4;
	  flck.l_len = 1;
	  if (fcntl(fd, F_GETLK, &flck) != -1) {
	    if (flck.l_type == F_UNLCK) {
	      if (verboseflag)
		if(!processid)
	          printf("0");
	    } else {
	      if (verboseflag)
	        if (processid)
		  printf("%d ", flck.l_pid);
	        else
	          printf("1");
	      qty++;
	    }
	  } else
	    printf("F");
	}
	close(fd);
	printf(" [%d open]", qty);
}

usage()
{
  printf("usage: %s [-fpv] [-s N] <listname>\n", progname);
  exit(1);
}
