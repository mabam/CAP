/*
 * Copyright (c) 1990, The Regents of the University of California.
 * Edward Moy, Workstation Software Support Group, Workstation Support Services,
 * Information Systems and Technology.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of California makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#include <netat/appletalk.h>
#ifdef NEEDFCNTLDOTH
#include <fcntl.h>
#endif NEEDFCNTLDOTH
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>		/* sometimes needed for sys/file.h */
#endif _TYPES
#include <sys/file.h>
#ifdef USEDIRENT
#include <dirent.h>
#else  USEDIRENT
#include <sys/dir.h>
#endif USEDIRENT
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <signal.h>
#ifdef USESTRINGDOTH
#include <string.h>
#else  USESTRINGDOTH
#include <strings.h>
#endif USESTRINGDOTH
#include <ctype.h>
#include <stdio.h>

#include <netinet/in.h>

#define	ASYNC		1
#define	BDSSIZE		512
#define	BITMASK		((1 << NBITSPERFIELD) - 1)
#define	HOLDTIME	60
#define	MACID		4
#define	MORESIZE	(sizeof(moremsg) - 1)
#define	NBDS		8
#define	NBITSPERFIELD	(NUSERBITS / (NFIELDS - 1))
#define	NFIELDS		6
#define	NUSERBITS	32
#define	SYNC		0

struct Queue {
	time_t	q_time;			/* modification time */
	char	q_name[MAXNAMLEN+1];	/* control file name */
};

AddrBlock addr;
ABusRecord abrecord;
BDS	bds[NBDS];
char	bdsbuffer[NBDS * BDSSIZE]; /* bds buffer */
int	bdsfull;	/* bds buffer full */
char	*bdsptr;	/* current position in bds buffer */
int	colwidth[NFIELDS]; /* column widths */
char	current[40];	/* current file being printed */
char	empty[] = "";	/* empty string */
EntityName entity;	/* Entity name of register naming */
int	holdtime = HOLDTIME; /* minimum time between looking at queue */
long	lasttime;	/* last time queue looked at */
char	line[BUFSIZ];	/* line buffer */
char	*lock = "lock";	/* lock file name */
char	moremsg[] = "   (More ...)\r";
char	*myname;	/* name of program */
char	*printer;	/* name of LW spooler */
int	rank;		/* order to be printed (-1=none, 0=active) */
struct stat statbuf;
char	*status = "status"; /* status file name */
char	temp[512];	/* temp buffer for bds */
char	type[] = "Printer Queue"; /* AppleTalk type */
char	zone[] = "*";	/* AppleTalk zone */

char	*pgetstr();
char	*malloc();

main(argc, argv)
int argc;
char **argv;
{
	register ABusRecord *abr = &abrecord;
	register OSErr err;
	register int i, j, k;
	int sock;
	long l;
	void cleanup();

	if(myname = rindex(*argv, '/'))
		myname++;
	else
		myname = *argv;
	for(argc--, argv++ ; argc > 0 && **argv == '-' ; argc--, argv++) {
		switch((*argv)[1]) {
		 case 'l':	/* alternate lock file */
			if((*argv)[2])
				lock = &(*argv)[2];
			else if(argc < 2)
				Usage();	/* never returns */
			else {
				argc--;
				lock = *++argv;
			}
			break;
		 case 's':	/* alternate status file */
			if((*argv)[2])
				status = &(*argv)[2];
			else if(argc < 2)
				Usage();	/* never returns */
			else {
				argc--;
				status = *++argv;
			}
			break;
		 case 't':	/* new hold time */
			if((*argv)[2])
				holdtime = atoi(&(*argv)[2]);
			else if(argc < 2)
				Usage();	/* never returns */
			else {
				argc--;
				holdtime = atoi(*++argv);
			}
			if(holdtime < 0)
				Usage();
			break;
		 default:
			Usage();	/* never returns */
		}
	}
	if(argc < 2)
		Usage();	/* never returns */
	printer = *argv++;
	if (chdir(*argv) < 0)
		fatal("cannot chdir to spooling directory");
	disassociate();
	for(i = 0 ; i < NBDS ; i++)
		bds[i].buffPtr = &bdsbuffer[i * BDSSIZE];
	signal(SIGHUP, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	abInit(TRUE);
	nbpInit();
	sock = 0;
	if((err = ATPOpenSocket(&addr, &sock)) != noErr)
		fatal("Error opening ATP socket (%d)\n", err);
	if((err = nbp_register(printer, type, zone, sock)) != noErr)
		fatal("Error registering name (%d)\n", err);
	for( ; ; ) {
		abr->proto.atp.atpSocket = sock;
		abr->proto.atp.atpReqCount = BDSSIZE;
		abr->proto.atp.atpDataPtr = line;
		if((err = ATPGetRequest(abr, SYNC)) != noErr)
			fatal("Error setting up ATPGetRequest (%d)\n", err);
		if(abr->abResult != 1) {
			time(&l);
			if(l - lasttime >= holdtime) {
				lasttime = l;
				bdsptr = bdsbuffer;
				bdsfull = FALSE;
				tobds("\"%s\" Printer Queue  %s",
				 printer, ctime(&lasttime));
				displayq();
				*bdsptr = 0;
				k = bdsptr - bdsbuffer;
				abr->proto.atp.atpNumBufs =
				 abr->proto.atp.atpBDSSize = j =
				 (k + (BDSSIZE - 1)) / BDSSIZE;
				for(i = 0 ; i < j ; i++) {
					bds[i].buffSize = k > BDSSIZE ?
					 BDSSIZE : k;
					k -= BDSSIZE;
				}
				l = 0;
				for(i = (NFIELDS - 2) ; i >= 0 ; i--)
					l = (l << NBITSPERFIELD) |
					 (colwidth[i] > BITMASK ? BITMASK :
					 colwidth[i]);
				/*
				 * We shouldn't have to do this, but we do!
				 */
				bds[0].userBytes = htonl(l);
			}
			abr->proto.atp.fatpEOM = TRUE;
			abr->proto.atp.atpRspBDSPtr = bds;
			if((err = ATPSndRsp(abr, SYNC)) != noErr)
				fprintf(stderr,
				 "%s: Error sending response (%d)\n",
				 myname, err);
		}
	}
}

disassociate()
{
	int i;

	if (fork())
		_exit(0);			/* kill parent */
	for (i=0; i < 3; i++) close(i); /* kill */
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

void
cleanup()
{
	NBPRemove(&entity);
	exit(1);
}

Usage()
{
	fprintf(stderr,
	"Usage: %s [-l lockfile] [-s statusfile] [-t holdtime] name spooldir\n",
	myname);
	exit(1);
}

/*
 * Put unformated text into BDS buffers.
 */
writebds(buf, size)
char *buf;
{
	bcopy(buf, bdsptr, size);
	bdsptr += size;
}

/*
 * Put formatted text into BDS buffers.
 */
tobds(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
char *fmt;
{
	register int len, bdslen;

	if(bdsfull)
		return;
	sprintf(temp, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7,
	 arg8, arg9);
	if((bdslen = bdsptr - bdsbuffer) + (len = strlen(temp)) >
	 NBDS * BDSSIZE) {
		bdsfull = TRUE;
		while(NBDS * BDSSIZE - bdslen < MORESIZE) {
			if(*--bdsptr != '\r')
				bdsptr++;
			while(*--bdsptr != '\r')
				{}
			bdsptr++;
			bdslen = bdsptr - bdsbuffer;
		}
		strcpy(bdsptr, moremsg);
		bdsptr += MORESIZE;
		return;
	}
	strcpy(bdsptr, temp);
	bdsptr += len;
}

/*
 * Display the current state of the queue. Format = 1 if long format.
 */
displayq()
{
	register struct Queue *q;
	register int i, nitems, fd;
	struct Queue **Queue;
	FILE *fp;

	rank = -1;
	for(i = 0 ; i < (NFIELDS - 1) ; i++)
		colwidth[i] = 0;
	if ((nitems = Getq(&Queue)) < 0)
		fatal("cannot examine spooling area\n");
	if (stat(lock, &statbuf) >= 0) {
		if (statbuf.st_mode & 0100) {
			tobds("Warning: \"%s\" is down: ", printer);
			fd = open(status, O_RDONLY);
			if (fd >= 0) {
				OSLockFileforRead(fd);
				while ((i = read(fd, line, BUFSIZ)) > 0)
					writebds(line, i);
				OSUnlockFile(fd);
				(void) close(fd);
			} else
				writebds("\n", 1);
		}
		if (statbuf.st_mode & 010)
			tobds("Warning: \"%s\" queue is turned off\n", printer);
	}

	if (nitems) {
		fp = fopen(lock, "r");
		if (fp == NULL)
			warn();
		else {
			register char *cp;

			/* get daemon pid */
			cp = current;
			while ((*cp = getc(fp)) != EOF && *cp != '\n')
				cp++;
			*cp = '\0';
			i = atoi(current);
			if (i <= 0 || kill(i, 0) < 0)
				warn();
			else {
				/* read current file name */
				cp = current;
				while ((*cp = getc(fp)) != EOF && *cp != '\n')
					cp++;
				*cp = '\0';
				/*
				 * Print the status file.
				 */
				fd = open(status, O_RDONLY);
				if (fd >= 0) {
					OSLockFileforRead(fd);
					while ((i = read(fd, line, BUFSIZ)) > 0)
						writebds(line, i);
					OSUnlockFile(fd);
					(void) close(fd);
				} else
					writebds("\n", 1);
			}
			(void) fclose(fp);
		}
		/*
		 * Now, examine the control files and print out the jobs to
		 * be done for each user.
		 */
		for (i = 0; i < nitems; i++) {
			q = Queue[i];
			inform(q);
			free(q);
		}
		free(Queue);
	} else if (nitems == 0)
		tobds("no entries\n");
}

/*
 * Print a warning message if there is no daemon present.
 */
warn()
{
	tobds("Warning: print spooler not running\n");
	current[0] = '\0';
}

inform(q)
register struct Queue *q;
{
	register int copies, pc;
	register char *cp;
	register long size;
	int mac;
	FILE *cfp;
	char *alias;
	char *class;
	char cstr[128];		/* 'H' */
	char fstr[128];		/* [a-z] */
	char jstr[128];		/* 'J' */
	char nstr[128];		/* 'N' */
	char ustr[128];		/* 'P' */
	char *job;
	char *user;

	/*
	 * There's a chance the control file has gone away
	 * in the meantime; if this is the case just keep going
	 */
	if ((cfp = fopen(q->q_name, "r")) == NULL)
		return;

	alias = class = user = job = empty;
	*cstr = *fstr = *jstr = *nstr = *ustr = '\0';
	pc = -1;
	size = -1L;
	mac = FALSE;
	if (rank < 0)
		rank = 0;
	if (strcmp(q->q_name, current) != 0)
		rank++;
	copies = 0;
	while (getline(cfp)) {
		switch (line[0]) {
		case 'H':
			strcpy(cstr, line + 1);
			continue;
		case 'J':
			strcpy(jstr, line + 1);
			continue;
		case 'P':
			strcpy(ustr, line + 1);
			continue;
		default:
			if(line[0] < 'a' || line[0] > 'z')
				continue;
			if(copies == 0)
				strcpy(fstr, line + 1);
			copies++;
			continue;
		case 'N':
			if(*fstr && stat(fstr, &statbuf) >= 0)
				size = copies * statbuf.st_size;
			strcpy(nstr, line + 1);
			continue;
		}
	}
	fclose(cfp);
	if(strcmp(ustr, "root") == 0 && strncmp(jstr, "MacUser: ", 9) == 0) {
		mac = TRUE;
		cp = jstr + 9;
		for( ; ; ) {
			if((cp = index(cp, ' ')) == NULL) {
				job = "Untitled";
				pc = pagecount(jstr + 9);
				break;
			}
			if(strncmp(cp, " Job: ", 6) == 0) {
				*cp = 0;
				job = cp + 6;
				pc = pagecount(job);
				break;
			}
			cp++;
		}
		strcpy(ustr, jstr + 9);
		if((cp = rindex(ustr, '!')) && strlen(cp) == (MACID + 1)) {
			*cp++ = 0;
			class = cp;
			if(cp = rindex(ustr, ' ')) {
				*cp++ = 0;
				alias = ustr;
				user = cp;
			} else
				user = ustr;
		} else {
			user = ustr;
			class = "Macintosh";
		}
	} else {
		if(*jstr)
			job = jstr;
		else if(strcmp(nstr, " ") == 0)
			job = "(standard input)";
		else
			job = nstr;
		user = ustr;
		class = cstr;
		if((copies = strlen(class)) >= 13 &&
		 strcmp(cp = &class[copies - 13], ".berkeley.edu") == 0)
			*cp = 0;
	}
	cp = bdsptr;
	prank(rank);
	if(pc >= 0)
		tobds("%s\t%s\t%s\t%s\t%d Page%s\n", alias, user, job, class,
		 pc, pc == 1 ? "" : "s");
	else if(size >= 0L)
		tobds("%s\t%s\t%s\t%s\t%ld byte%s\n", alias, user, job, class,
		 size, size == 1 ? "" : "s");
	else
		tobds("%s\t%s\t%s\t%s\t?? bytes\n", alias, user, job, class);
	*bdsptr = 0;
	measurecols(cp);
}

measurecols(cp)
register char *cp;
{
	register char *tp;
	register int i, j;

	for(i = 0 ; i < (NFIELDS - 1) ; i++) {
		if((tp = index(cp, '\t')) == NULL) {
			if((j = strlen(cp)) > colwidth[i])
				colwidth[i] = j;
			break;
		}
		if((j = tp - cp) > colwidth[i])
			colwidth[i] = j;
		cp = tp + 1;
	}
}

pagecount(str)
register char *str;
{
	for( ; ; ) {
		if((str = index(str, ' ')) == NULL)
			return(-1);
		if(strncmp(str, " Pages: ", 8) == 0) {
			*str = 0;
			str += 8;
			return(atoi(str));
		}
		str++;
	}
}

/*
 * Print the job's rank in the queue,
 *   update col for screen management
 */
prank(n)
{
	char line[100];
	static char *r[] = {
		"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
	};

	if (n == 0) {
		tobds("active\t");
		return;
	}
	if ((n/10) == 1)
		tobds("%dth\t", n);
	else
		tobds("%d%s\t", n, r[n%10]);
}

/*
 * fatal error
 */
fatal(fmt, arg1, arg2, arg3)
char *fmt;
{
	fprintf(stderr, "%s: ", myname);
	fprintf(stderr, fmt, arg1, arg2, arg3);
	exit(1);
}

/*
 * register the specified entity
 *
*/
nbp_register(sobj, stype, szone, skt)
char *sobj, *stype, *szone;
int skt;
{
  nbpProto nbpr;		/* nbp proto */
  NBPTEntry nbpt[1];		/* table of entity names */
  int err;

  strcpy((char *)entity.objStr.s, sobj);
  strcpy((char *)entity.typeStr.s, stype);
  strcpy((char *)entity.zoneStr.s, szone);


  nbpr.nbpAddress.skt = skt;
  nbpr.nbpRetransmitInfo.retransInterval = 4;
  nbpr.nbpRetransmitInfo.retransCount = 3;
  nbpr.nbpBufPtr = nbpt;
  nbpr.nbpBufSize = sizeof(nbpt);
  nbpr.nbpDataField = 1;	/* max entries */
  nbpr.nbpEntityPtr = &entity;

  err = NBPRegister(&nbpr,FALSE);	/* try synchronous */
  return(err);
}

getline(cfp)
	FILE *cfp;
{
	register int linel = 0;
	register char *lp = line;
	register c;

	while ((c = getc(cfp)) != '\n') {
		if (c == EOF)
			return(0);
		if (c == '\t') {
			do {
				*lp++ = ' ';
				linel++;
			} while ((linel & 07) != 0);
			continue;
		}
		*lp++ = c;
		linel++;
	}
	*lp++ = '\0';
	return(linel);
}

/*
 * Scan the current directory and make a list of daemon files sorted by
 * creation time.
 * Return the number of entries and a pointer to the list.
 */
Getq(namelist)
	struct Queue *(*namelist[]);
{
#ifdef USEDIRENT
	register struct dirent *d;
#else  USEDIRENT
	register struct direct *d;
#endif USEDIRENT
	register struct Queue *q, **Queue;
	register int nitems;
	int arraysz, compar();
	DIR *dirp;

	if ((dirp = opendir(".")) == NULL)
		return(-1);
	if (fstat(dirp->dd_fd, &statbuf) < 0)
		goto errdone;

	/*
	 * Estimate the array size by taking the size of the directory file
	 * and dividing it by a multiple of the minimum size entry. 
	 */
	arraysz = (statbuf.st_size / 24);
	Queue = (struct Queue **)malloc(arraysz * sizeof(struct Queue *));
	if (Queue == NULL)
		goto errdone;

	nitems = 0;
	while ((d = readdir(dirp)) != NULL) {
#ifdef __hpux
		if (d->d_name[0] != 'c' || d->d_name[1] != 'A')
#else  /* __hpux */
		if (d->d_name[0] != 'c' || d->d_name[1] != 'f')
#endif /* __hpux */
			continue;	/* daemon control files only */
		if (stat(d->d_name, &statbuf) < 0)
			continue;	/* Doesn't exist */
		q = (struct Queue *)malloc(sizeof(time_t)+strlen(d->d_name)+1);
		if (q == NULL)
			goto errdone;
		q->q_time = statbuf.st_mtime;
		strcpy(q->q_name, d->d_name);
		/*
		 * Check to make sure the array has space left and
		 * realloc the maximum size.
		 */
		if (++nitems > arraysz) {
			Queue = (struct Queue **)realloc((char *)Queue,
				(statbuf.st_size/12) * sizeof(struct Queue *));
			if (Queue == NULL)
				goto errdone;
		}
		Queue[nitems-1] = q;
	}
	closedir(dirp);
	if (nitems)
		qsort(Queue, nitems, sizeof(struct Queue *), compar);
	*namelist = Queue;
	return(nitems);

errdone:
	closedir(dirp);
	return(-1);
}

/*
 * Compare modification times.
 */
compar(p1, p2)
	register struct Queue **p1, **p2;
{
	if ((*p1)->q_time < (*p2)->q_time)
		return(-1);
	if ((*p1)->q_time > (*p2)->q_time)
		return(1);
	return(0);
}
