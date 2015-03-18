/*
 * $Date: 91/03/14 14:27:01 $
 * $Header: asyncad.c,v 2.2 91/03/14 14:27:01 djh Exp $
 * $Log:	asyncad.c,v $
 * Revision 2.2  91/03/14  14:27:01  djh
 * Fall back to log().
 * 
 * Revision 2.1  91/02/15  21:21:03  djh
 * CAP 6.0 Initial Revision
 * 
 * Revision 1.1  90/03/17  22:05:51  djh
 * Initial revision
 * 
 *
 *	asyncad.c
 *
 *	Asynchronous Appletalk broadcast redirector
 *	
 *	Asynchronous AppleTalk for CAP UNIX boxes.
 *	David Hornsby, djh@munnari.oz, August 1988
 *	Department of Computer Science.
 *	The University of Melbourne, Parkville, 3052
 *	Victoria, Australia.
 *
 * Copyright 1988, The University of Melbourne.
 *
 * You may use, modify and redistribute BUT NOT SELL this package provided
 * that all changes/bug fixes are returned to the author for inclusion.
 *
 *	Ref: Async Appletalk, Dr. Dobbs Journal, October 1987, p18.
 *
 *	Listen on aaBroad for:
 *		1. DDP broadcasts and redirect them to each async node
 *		2. announcements of node arrivals and departures
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include "async.h"

#define asyncadlog "/usr/tmp/asyncadlog"

int debug;
short node_is_up[MAXNODE];
struct sockaddr_in sin, fsin;
struct ap ap;

main(argc, argv)
char *argv[];
{
	char *t;
	int i, s, n;
	int fsinlen;
	char hostname[64];
	struct hostent *host, *gethostbyname();

	while(--argc > 0 && (*++argv)[0] == '-')
		for(t = argv[0]+1 ; *t != '\0' ; t++)
			switch (*t) {
			case 'd':
				debug = 1;
				break;
			}

	for( i = 0 ; i < MAXNODE ; i++)
		node_is_up[i] = 0;

	if(debug == 0) {

		int t, f;

		if(fork())
			exit(0);

		for(f = 0; f < 10; f++)
			(void) close(f);
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		if((t = open("/dev/tty", 2)) >= 0) {
			ioctl(t, TIOCNOTTY, (char *)0);
			(void) close(t);
		}
	}

	while ((s = socket(AF_INET, SOCK_DGRAM, 0, 0)) < 0) {
		log("socket call failed");
		sleep(10);
	}

	sin.sin_port = htons(aaBroad);
	if(bind(s, (caddr_t) &sin, sizeof (sin), 0) < 0) {
		log("bind call failed");
		exit(1);
	}

	for( ; ; ) {
		fsinlen = sizeof (fsin);
		if((n = recvfrom(s, (caddr_t) &ap, sizeof(ap), 
				0, (caddr_t) &fsin, &fsinlen)) < 0) {
			log("recv failed");
			exit(1);
		}

		if(ap.ldst == 0xC0 && ap.lsrc == 0xDE) {
			/* an async node is registering with us 	*/
			i = ap.srcskt & MAXNODE;	/* who from	*/
			node_is_up[i] = ap.ltype;	/* up or down	*/
			if(debug)
				fprintf(stderr,
				"node %d is %s",i,(ap.ltype)?"up":"down");
			continue;
		}

		if(ap.ldst != 0xFA || ap.lsrc != 0xCE || ap.ltype != 2) {
			log("found a dud packet");
			continue;	/* not valid lap header */
		}

		/* here we have a valid broadcast, redirect it to nodes	*/

		if(debug)
			fprintf(stderr, "found a broadcast packet");

		gethostname(hostname, sizeof(hostname));
		host = gethostbyname(hostname);
		bcopy(host->h_addr, &fsin.sin_addr.s_addr,
				sizeof(fsin.sin_addr.s_addr));

		for(i = 1 ; i < MAXNODE ; i++) { /* 0 is invalid node #	*/
			if(node_is_up[i]) {
				if(debug)
					fprintf(stderr, "redirect to %d", i);
				fsin.sin_port = htons((short) i + PortOffset);
				if(sendto(s, (caddr_t) &ap, n, 0, &fsin, 
						sizeof(fsin)) != n) {
					perror("sendto");
					log("sendto failed");
					exit(1);
				}
			}
		}
	}
}


/*
 * log an error message 
 */
log(msg)
char *msg;
{
	FILE *fp;
	long time(), tloc;
	struct tm *tm, *localtime();

	if(debug)
		fp = stderr;
	else
		if((fp = fopen(asyncadlog, "a+")) == NULL)
			return;

	time(&tloc);
	tm = localtime(&tloc);
	fprintf(fp, "%d/%d %02d:%02d ", tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min);
	fprintf(fp, msg);
	putc('\n', fp);

	if(!debug)
		fclose(fp);
	else
		fflush(fp); /* for SUN's and other junk	*/
}
