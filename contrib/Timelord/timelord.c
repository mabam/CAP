/*
 *	timelord - UNIX Macintosh Time Server 
 *
 *	Provides:
 *		Time Server
 *
 *	written	1.0	May '89	   djh@munnari.oz
 *	revised	1.1	28/05/89   djh@munnari.oz	Add Boot/Chooser log
 *	revised 1.2	16/07/91   djh@munnari.OZ.AU	Fix argument handling
 *
 * Copyright (c) 1990, the University of Melbourne
 *
 *	You may use and distribute (but NOT SELL!) 
 *	this freely providing that...
 *		+ any improvements/bug fixes return to the author
 *		+ this notice remains intact.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <netat/appletalk.h>
#include "timelord.h"

#define forever() for(;;)

extern int errno;

time_t now;
int skt, debug = 0;
int doRequest(), cleanup();
char logf[150], obj[33], type[33], *index();

main(argc, argv)
int argc;
char *argv[];
{
	int f, err;
	char *s;			/* argument parsing		*/
	char *cp;			/* general char pointer		*/
	char requestBuffer[atpMaxData];	/* A/TALK packet buffer		*/
	AddrBlock addr;
	ABusRecord abr;
	atpProto *atPtr;		/* pointer to ATP record	*/
	
	signal(SIGQUIT, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGHUP, cleanup);
	
	strncpy(logf, LOGFILE, sizeof(logf));
	strncpy(type, MACSERVER, sizeof(type));
	gethostname(obj, sizeof(obj));
	if((cp = index(obj, '.')) != NULL)	/* strip domain names */
		*cp = 0;

	while(--argc > 0 && (*++argv)[0] == '-')
		for(s = argv[0]+1 ; *s != '\0' ; s++)
			switch (*s) {
				case 'd':
				case 'D':
					debug++;
					if (--argc > 0) {
					  if ((*++argv)[0] != '-')
					    dbugarg(*argv);
					  else
					    argv--,argc++;
					}
					break;
				case 'l':
				case 'L':
					if (--argc <= 0)
					  usage();
					strncpy(logf, *++argv, sizeof(logf));
					break;
				case 'n':
				case 'N':
					if (--argc <= 0)
					  usage();
					strncpy(obj, *++argv, sizeof(obj));
					break;
				case 't':
				case 'T':
					if (--argc <= 0)
					  usage();
					strncpy(type, *++argv, sizeof(type));
					break;
				default:
					usage();
					break;
			}
	
	if(!debug) {
		if(fork())
			exit(0);
		for(f = 0; f < 32; f++)
			(void) close(f);
		(void) open("/dev/null", 0);
		(void) dup2(0, 1);
		if((f = open("/dev/tty", 2)) >= 0) {
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
	
	(void) time(&now);
	log("timelord server starting\n");
	
	abInit(debug);
	nbpInit();
	
	addr.net = addr.node = addr.skt = 0; /* accept from anywhere */
	skt = 0;	/* Get Dynamic ATP Socket */
	
	if ((err = ATPOpenSocket(&addr, &skt)) < 0) {
		log("ATPopen() failed, code %d\n", err);
		exit(1);
	}
	doNBPRemove(obj, type, ZONE);	/* if one already running */
	if ((err = doNBPRegister(obj, type, ZONE, skt)) != noErr) {
		log("NBP register error: %d\n", err);
		exit(1);
	}
	
	atPtr = &abr.proto.atp;
	forever() {
		atPtr->atpDataPtr = requestBuffer;
		atPtr->atpReqCount = atpMaxData;
		atPtr->atpSocket = skt;
		if((err = ATPGetRequest(&abr, FALSE)) != noErr)
			log("ATPGetRequest error: %d\n",err);
		doRequest(&abr);
	}
}

/*
 * process a new request
 */

doRequest(abr)
ABusRecord *abr;
{
	char *q;
	int request;
	unsigned long mactime, mact;
	register long diff;
	struct tm gmt, local, *localtime(), *gmtime();
	atpProto *atPtr = &abr->proto.atp;
	
	(void) time(&now);
	
	/* request data depends on the user data field */
	abr->proto.atp.atpUserData = ntohl(abr->proto.atp.atpUserData);
	request = abr->proto.atp.atpUserData;
	
	if (debug)
		fprintf(stderr, "Request %d from %d/%d:%x\n", request,
		    htons(abr->proto.atp.atpAddress.net),
		    abr->proto.atp.atpAddress.node,
		    abr->proto.atp.atpAddress.skt);
	
	switch(request) {
	case GETTIME:
		q = (char *) abr->proto.atp.atpDataPtr;
		/*
		 * Do this by determining what the given time
		 * is when converted to local time, and when
		 * converted to GMT and taking the difference.
		 * This works correctly regardless of whether
		 * local time is Daylight Savings Time or not.
		 *
		 *	- courtesy kre@munnari.oz
		 */
		gmt = *gmtime((time_t *)&now);
#define	isleap(yr) ((yr) % 4 == 0 && ((yr) % 100 != 0 || (yr) % 400 == 0))
		local = *localtime((time_t *)&now);
		diff = gmt.tm_year - local.tm_year;
		diff *= 365;
		if(gmt.tm_year > local.tm_year) {
			if(isleap(local.tm_year))
				diff++;
		} else
			if(local.tm_year > gmt.tm_year) {
				if(isleap(gmt.tm_year))
					diff--;
			}
		diff += gmt.tm_yday - local.tm_yday;
		diff *= 24;
		diff += gmt.tm_hour - local.tm_hour;
		diff *= 60;
		diff += gmt.tm_min - local.tm_min;
		diff *= 60;
		diff += gmt.tm_sec - local.tm_sec;
		now -= diff;
#undef	isleap
		mactime = now + TIME_OFFSET;
		if(abr->proto.atp.atpActCount > 0) {
			bcopy(q+1, &mact, sizeof(long));
			mact = ntohl(mact);
			log("Get Time:  %-16s %6d secs (%s)\n", 
				(*(q+5) == '\0') ? "<nobody>" : q+5,
				mact - mactime,
				(*q == 0) ? "Boot" : "Chooser");
		} else
			log("Get Time\n");
		mactime = htonl(mactime);
		sendReply(abr, ENONE, &mactime, sizeof(mactime));
		break;
	default:
		sendMsg(abr, ENOPERM, "Bad Request");
		log("Bad Request [%x]\n", request);
		break;
	}
}

/*
 * send back an (error) message.
 */

sendMsg(abr, code, msg)
ABusRecord *abr;
int code;
char *msg;
{
	char buffer[atpMaxData];	/* room for translated message */
	
	buffer[0] = (char) strlen(msg);
	bcopy(msg, &buffer[1], buffer[0]);
	sendReply(abr, code, buffer, buffer[0]+1);
}
	
/*
 * send a reply back (fill in the BDS junk and then send it)
 *
 */

ABusRecord res_abr;
sendReply(abr, userdata, data, length)
ABusRecord *abr;
int userdata;
char *data;
int length;
{
	BDS bds[1];
	
	userdata = htonl(userdata);
	bds[0].userData = userdata;
	bds[0].buffPtr = data;
	bds[0].buffSize = length;
	return(sendReplyBDS(abr, bds, 1));
}
	
/*
 *  send the BDS block
 *
 */

sendReplyBDS(abr, bds, bdslen)
ABusRecord *abr;
BDS bds[];
int bdslen;
{
	register atpProto *atPtr;
	
	atPtr = &res_abr.proto.atp;
	atPtr->atpAddress = abr->proto.atp.atpAddress;
	atPtr->atpTransID = abr->proto.atp.atpTransID;
	atPtr->atpSocket = abr->proto.atp.atpSocket;
	atPtr->atpRspBDSPtr = bds;
	atPtr->atpNumBufs = bdslen;
	atPtr->atpBDSSize = bdslen;
	atPtr->fatpEOM = 1;
	return(ATPSndRsp(&res_abr, FALSE));
}
	
/*
 * Log an error message.
 */

log(fmt, a1, a2, a3, a4, a5, a6)
char *fmt, *a1, *a2, *a3, *a4, *a5, *a6;
{
	char *ctime();
	
	(void) time(&now);
	fprintf(stderr, "%.*s\t", 24-5, ctime(&now));
	fprintf(stderr, fmt, a1, a2, a3, a4, a5, a6);
	fflush(stderr);	/* especially for SUN's	*/
}

cleanup()
{
  	doNBPRemove(obj, type, ZONE); /* if one already running */
	nbpShutdown();
	ATPCloseSocket(skt);
	exit(0);
}

doNBPRemove(sobj, stype, szone)
char *sobj, *stype, *szone;
{
	EntityName en;
	int err;

  	strncpy(en.objStr.s, sobj, sizeof(en.objStr.s));
  	strncpy(en.typeStr.s, stype, sizeof(en.typeStr.s));
  	strncpy(en.zoneStr.s, szone, sizeof(en.zoneStr.s));
	return(NBPRemove(&en));
}

doNBPRegister(sobj, stype, szone, skt)
char *sobj, *stype, *szone;
int skt;
{
	EntityName en;
	nbpProto nbpr;			/* nbp proto		*/
	NBPTEntry nbpt[1];		/* table of entity names */
	
	strncpy(en.objStr.s, sobj, sizeof(en.objStr.s));
	strncpy(en.typeStr.s, stype, sizeof(en.typeStr.s));
	strncpy(en.zoneStr.s, szone, sizeof(en.zoneStr.s));
	
	nbpr.nbpAddress.skt = skt;
	nbpr.nbpRetransmitInfo.retransInterval = 10;
	nbpr.nbpRetransmitInfo.retransCount = 3;
	nbpr.nbpBufPtr = nbpt;
	nbpr.nbpBufSize = sizeof(nbpt);
	nbpr.nbpDataField = 1;	/* max entries */
	nbpr.nbpEntityPtr = &en;
	
	return((int) NBPRegister(&nbpr,FALSE));	/* try synchronous */
}

usage()
{
	fprintf(stderr,
	  "usage: timelord [-d [CAPDebugFlags]] [-l logfile] [-n name]\n");
	exit(1);
}
