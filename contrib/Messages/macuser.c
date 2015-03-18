/*
 * $Date: 1993/08/05 15:54:23 $
 * $Header: /mac/src/cap60/contrib/Messages/RCS/macuser.c,v 2.3 1993/08/05 15:54:23 djh Rel djh $
 * $Log: macuser.c,v $
 * Revision 2.3  1993/08/05  15:54:23  djh
 * change wait handling
 *
 * Revision 2.2  91/03/14  14:20:25  djh
 * Fall back to log(), don't write non-ascii on a terminal,
 * handle Sys V utmp files.
 * 
 * Revision 1.2  91/03/04  18:20:11  djh
 * Add USER_PROCESS test to fix SysV style utmp,
 * make sure we don't write non-ascii characters to the terminal.
 * 		John Sellens <jmsellen@watmath.uwaterloo.ca>
 * 
 * Revision 1.1  91/01/10  01:10:48  djh
 * Initial revision
 * 
 *
 * djh@munnari.OZ.AU, 05/05/90
 * Copyright (c) 1991, The University of Melbourne
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of Melbourne makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 *
 * NBP Register each of the argument list as "user@host:macUser@*",
 * listen for messages sent from Mac users & forward to UNIX users
 * (write to terminal if logged in, send email if not).
 *
 * 02/12/90 Fix unregister/biff bugs, add some security, make debug work.
 */

#include "notify.h"

#include <utmp.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define USERNAMELEN	34
#define DEV_PATH	"/dev/"
#define DEV_TTY		"/dev/tty"
#define UTMP		"/etc/utmp"
#define MAIL		"/usr/ucb/mail"
#define LOGF		"/usr/tmp/macuserLog"

struct userlist {			/* map socket to username	*/
	int skt;
	EntityName en;
	char user[USERNAMELEN];
};

int mailing;
int numusers;
struct userlist userlist[MAXUSERS];

char host[MAXHOSTNAMELEN];
char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

main(argc, argv)
int argc;
char *argv[];
{
	int f;
	int debug;
	int cleanup();
	char *s, **dbgargv;
	char logf[MAXPATHLEN];
	char realuser[MAXHOSTNAMELEN];
	char *cp, *index(), *getlogin();

	signal(SIGHUP, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGTERM, cleanup);

	debug = 0;
	if(argc > 1) {
		dbgargv = argv;
		s = *++dbgargv;
		if(s[0] == '-' && s[1] == 'd' && s[2] != '\0') {
			debug = 1;
			++argv; --argc;
			dbugarg(&s[2]);
		}
	}

	if(argc >= 2 && getuid() != 0) {
		fprintf(stderr, "No permission to register name.\n");
		exit(1);
	}

	strcpy(logf, LOGF);
	strcpy(realuser, getlogin());

	if(!debug) {
		if(fork())
			exit(0);
		for(f = 0; f < 32; f++)
			(void) close(f);
		(void) open("/dev/null", 0);
		(void) dup2(0, 1);
		if((f = open(DEV_TTY, 2)) >= 0) {
			ioctl(f, TIOCNOTTY, (char *)0);
			(void) close(f);
		}
	
		if((f = open(logf, O_WRONLY|O_APPEND|O_CREAT, 0644)) >= 0) {
			if(f != 2) {
				(void) dup2(f, 2);
				(void) close(f);
			}
		}
	}

	abInit(TRUE);
	nbpInit();

	gethostname(host, MAXHOSTNAMELEN);
	if((cp = index(host, '.')) != NULL)
		*cp = '\0';

	numusers = 0;
	while(--argc > 0 && numusers < MAXUSERS)
		makeuser(*++argv);

	if(numusers == 0)
		makeuser(realuser);

	if(numusers != 0)
		for( ; ; )
			abSleep(400, TRUE);	/* 100 seconds */
}

int
makeuser(name)
char *name;
{
	EntityName *en;
	nbpProto nbpr;
	int i, listener();
	NBPTEntry nbpt[NUMNBPENTRY];
	char user[MAXHOSTNAMELEN*2];

	userlist[numusers].skt = 0;	/* dynamic */
	if(DDPOpenSocket(&userlist[numusers].skt, listener) != noErr) {
		log("Couldn't get dynamic DDP socket\n");
		return(0);
	}

	en = &userlist[numusers].en;
	strncpy(user, name, sizeof(user));
	strncpy(userlist[numusers].user, user, USERNAMELEN);
	strcat(user, "@");
	strcat(user, host);
	strncpy(en->objStr.s, user, (i = sizeof(en->objStr.s)));
	en->objStr.s[i-1] = '\0'; /* just in case of long name */
	strncpy(en->typeStr.s, MACUSER, sizeof(en->typeStr.s));
	strncpy(en->zoneStr.s, OURZONE, sizeof(en->zoneStr.s));

	nbpr.nbpEntityPtr = en;
	nbpr.nbpBufPtr = nbpt;
	nbpr.nbpBufSize = sizeof(nbpt);
	nbpr.nbpAddress.skt = userlist[numusers].skt;
	nbpr.nbpRetransmitInfo.retransInterval = 3;
	nbpr.nbpRetransmitInfo.retransCount = 2;

	NBPRemove(en);

	if(NBPRegister(&nbpr, FALSE) != noErr) {
		log("Failed to register: %s\n", user);
		DDPCloseSocket(userlist[numusers].skt);
		return(0);
	}
	numusers++;
}

int
listener(skt, type, pkt, len, addr)
u_char skt;
u_char type;
char *pkt;
int len;
AddrBlock *addr;
{
	char *q;
	int i, fd;
	long secs;
	char timestr[32];
	struct tm *tm, *gmtime();

	if(type != ddpECHO) {		/* drop it		*/
		log("Bogus packet type: %d\n", type);
		return(0);
	}

	for(i = 0 ; i < numusers ; i++)
		if(userlist[i].skt == skt)
			break;

	if(i == numusers) {
		log("Couldn't find a user for socket %d\n", skt);
		return(0);
	}

	if((fd = findUser(userlist[i].user)) >= 0) {
		bcopy(pkt+1, &secs, 4);
		secs = ntohl(secs);
		secs -= TIME_OFFSET;
		tm = gmtime(&secs);
		sprintf(timestr, " at %02d:%02d %s ...%s\t",
			tm->tm_hour, tm->tm_min,
			days[tm->tm_wday],
			(mailing) ? "\n\n" : "\r\n\r\n");
		if(!mailing)
			write(fd, "\007\007\r", 3);
		write(fd, "\n", 1);
		q = pkt+5;
		i = strlen(q);
		write(fd, q, i - 4);	/* delete original " ..." */
		write(fd, timestr, strlen(timestr));
		q += i + 1;
		while(*q != '\0') {
			if(*q == '\n' || *q == '\r') {
				q++;
				if(!mailing)
					write(fd, "\r", 1);
				write(fd, "\n\t", 2);
				continue;
			}
			if(!isascii(*q)) {
				*q = toascii(*q);
				write(fd, "M-", 2);
				if(!iscntrl(*q)) {
					write(fd, q++, 1);
					continue;
				}
			}
			if(iscntrl(*q)) {
				*q = *q ^ 0100;
				write(fd, "^", 1);
				write(fd, q++, 1);
				continue;
			}
			write(fd, q++, 1);
		}
		if(!mailing)
			write(fd, "\r", 1);
		close(fd);
	}
}

/*
 * Search utmp for user details.
 *
 */

int
findUser(name)
char *name;
{
	struct utmp buf;
	FILE *fp, *fopen();
	struct stat sbuf;
	long now, idle, minmidle;
	int idling, off, fd, doOpenTTY();
	char tname[MAXPATHLEN], mostidle[MAXPATHLEN];

	idling = 0;
	mailing = 0;

	if((fp = fopen(UTMP, "r")) == NULL) {
		log("Couldn't open utmp file: %s\n", UTMP);
		return(-1);
	}

	minmidle = now = time(0);
	off = strlen(DEV_PATH);
	strncpy(tname, DEV_PATH, sizeof(tname));
	while(fread((char *) &buf, sizeof(buf), 1, fp) == 1) {
		if(*buf.ut_name == '\0')
			continue;
#ifdef USER_PROCESS
		if(buf.ut_type != USER_PROCESS)
			continue;
#endif USER_PROCESS
		if(strncmp(buf.ut_name, name, sizeof(buf.ut_name)) == 0) {
			strncpy(tname+off, buf.ut_line, sizeof(tname)-off);
			if(stat(tname, &sbuf) == 0) {
				if(!(sbuf.st_mode & S_IFCHR)) /* not char */
					continue;
				if(!(sbuf.st_mode & 0020)) /* no grp wrt */
					continue;
				idle = now - sbuf.st_atime;
				if(idle <= minmidle) {
					minmidle = idle;
					strcpy(mostidle, tname);
				}
				idling++;
				continue;
			}
		}
	}
	fclose(fp);

	if(idling) {
		if((fd = doOpenTTY(mostidle)) >= 0)
			return(fd);
	} else {
		if((fd = doOpenMail(name)) >= 0)
			return(fd);
	}

	return(-1);
}

int
doOpenTTY(tname)
char *tname;
{
	int i, fd, dummy();

	signal(SIGALRM, dummy);
	alarm(2);
	if((fd = open(tname, O_WRONLY, 0644)) >= 0) {
		if((i = open(DEV_TTY, O_RDWR)) >= 0) { /* disassociate */
			ioctl(i, TIOCNOTTY, 0);
			(void) close(i);
		}
	}
	alarm(0);
	return(fd);
}

int
dummy()
{
	/* a dummy */
}

int
doOpenMail(name)
char *name;
{
	int funeral();
	int p[2], pid;

	if(pipe(p) < 0)
		return(-1);

	signal(SIGCHLD, funeral);

	if((pid = fork()) == 0) { /* in child */
		close(p[1]);
		DUP2(p[0], 0);

		execl(MAIL, "mail", "-s", "Macintosh Message", name, 0);
		exit(1);	/* o, oh */
	}

	/* parent */

	if(pid == -1)
		return(-1);
	
	close(p[0]);

	mailing = 1;

	return(p[1]);
}

int
DUP2(a, b)
int a, b;
{
	close(b);
	dup(a);
	close(a);
}

int
funeral()
{
	WSTATUS junk;
	
#ifdef POSIX
	while(waitpid(-1,&junk, WNOHANG) > 0)
	  ;
#else  POSIX
#ifndef NOWAIT3
	while(wait3(&junk, WNOHANG, 0) > 0)
	  ;
#else   NOWAIT3
	wait(&junk); /* assume there is at least one process to wait for */
#endif  NOWAIT3
#endif POSIX
}

/*
 * Log an error message.
 */

log(fmt, a1, a2, a3, a4, a5, a6)
char *fmt, *a1, *a2, *a3, *a4, *a5, *a6;
{
	long now;
	char *ctime();
	
	(void) time(&now);
	fprintf(stderr, "%.*s\t", 24-5, ctime(&now));
	fprintf(stderr, fmt, a1, a2, a3, a4, a5, a6);
	fflush(stderr);	/* especially for SUN's	*/
}

int
cleanup()
{
	int i, j;

	for(i = 0 ; i < numusers ; i++)
		NBPRemove(&userlist[i].en);

	exit(0);
}
