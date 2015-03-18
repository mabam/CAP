/*
 * $Date: 91/03/14 14:24:09 $
 * $Header: notify.c,v 2.2 91/03/14 14:24:09 djh Exp $
 * $Log:	notify.c,v $
 * Revision 2.2  91/03/14  14:24:09  djh
 * Revision for CAP.
 * 
 * Revision 1.1  91/01/10  01:11:09  djh
 * Initial revision
 * 
 *
 * djh@munnari.OZ.AU, 03/05/90
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
 * Low level message delivery to a Macintosh running "Messages"
 * (handle lookups for 'macwho' as a special case).
 *
 */

#include "notify.h"

NBPTEntry nbpt[NUMNBPENTRY];	/* return lookup storage	 	*/
ABusRecord ddpr;		/* outgoing packet		 	*/

#define LINEWIDTH	226	/* pixel width of Messages window	*/

int charWidth[128] = {		/* char widths for Messages 'applFont'	*/

	0,  8,  0,  0,  0,  0,  8,  8,  8,  6,  8,  8,  8,  0,  8,  8,  
	8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  
	3,  4,  5,  9,  7, 10,  8,  3,  6,  6,  7,  8,  4,  7,  3,  6,  
	7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  3,  4,  5,  7,  5,  7,  
	9,  6,  7,  6,  7,  6,  6,  6,  7,  4,  6,  7,  6,  9,  7,  6,  
	7,  6,  7,  6,  6,  7,  6, 10,  6,  6,  6,  5,  5,  5,  5,  7,  
	4,  5,  6,  5,  5,  5,  4,  5,  6,  4,  5,  6,  4,  9,  6,  5,  
	6,  5,  6,  5,  4,  6,  6,  8,  6,  6,  5,  5,  4,  5,  7,  0 
};

int
notify(msgtype, msg, from, user, zone, iconfile, lkupCount, lkupIntvl)
int msgtype;
char *msg, *from, *user, *zone, *iconfile;
int lkupCount, lkupIntvl;
{
	int msgFormat();
	int i, j, err, fd;
	char *cp, *index();
	long timenow, calctime();

	EntityName en;			/* obj/type/zone to lookup	*/
	nbpProto nbpr;			/* NBP record			*/
	AddrBlock addr;			/* Address Block storage	*/
	char ddpt[DDPPKTSIZE];		/* outgoing message		*/
	char host[MAXHOSTNAMELEN];	/* local host name		*/
	char header[256];		/* "Dispatch from ..." etc	*/

	abInit(FALSE);			/* Initialise CAP routines	*/
	nbpInit();			/* Initialise Name Binding Prot	*/

	strncpy(en.objStr.s,  user, sizeof(en.objStr.s));
	strncpy(en.typeStr.s, MACUSER, sizeof(en.typeStr.s));
	strncpy(en.zoneStr.s, zone, sizeof(en.zoneStr.s));

	nbpr.nbpEntityPtr = &en;
	nbpr.nbpBufPtr = nbpt;
	nbpr.nbpBufSize = sizeof(nbpt);
	nbpr.nbpDataField = NUMNBPENTRY;
	nbpr.nbpRetransmitInfo.retransInterval = lkupIntvl;
	nbpr.nbpRetransmitInfo.retransCount = lkupCount;

	if((err = NBPLookup(&nbpr, FALSE)) != noErr)
		return(err);

	if(nbpr.nbpDataField == 0)
		return(-1028);

	gethostname(host, sizeof(host));
	if((cp = index(host, '.')) != 0)	/* discard domains	*/
		*cp = '\0';

	switch (msgtype) {
		case 0:			/* special case for macwho	*/
			return(nbpr.nbpDataField);
			break;
		case MSGTO:
			sprintf(header, "Dispatch from %s@%s ...", from, host);
			break;
		case MSGWALL:
			sprintf(header, "Broadcast Message from %s@%s ...",
				from, host);
			break;
		case MSGMAIL:
			sprintf(header, "You have new mail on %s ...", host);
			break;
		default:
			return(-1);
			break;
	}
	
	/* fill packet	*/

	i = 0;
	ddpt[i] = (char) msgtype;			i++;
	timenow = calctime();
	bcopy(&timenow, ddpt+i, 4);			i += 4;
	ddpt[i] = '\0';

	j = strlen(header)+1;		/* include null			*/
	if(i + j > DDPPKTSIZE-1)
		j = DDPPKTSIZE-i-1;
	bcopy(header, ddpt+i, j);			i += j;

	if(i == DDPPKTSIZE-1) {
		fprintf(stderr, "Bogus header length!\n");
		exit(1);
	}

	j = msgFormat(msg)+1;		/* include null			*/
	if(i + j > DDPPKTSIZE-1)
		j = DDPPKTSIZE-i-1;
	bcopy(msg, ddpt+i, j);				i += j;

	if(i == DDPPKTSIZE-1) {		/* ensure null terminated	*/
		ddpt[i++] = '\0';
		fprintf(stderr, "Outgoing messages too long, truncated!\n");
	}

	if(iconfile != 0 && (fd = open(iconfile, O_RDONLY, 0644)) >= 0) {
		lseek(fd, ICONOFFSET, 0);		/* MUCHO MAGIC	*/
		if(i + ICONSIZE < DDPPKTSIZE) {
			ddpt[0] |= ICONFLAG;
			read(fd, ddpt+i, ICONSIZE);	i += ICONSIZE;
		}
		close(fd);
	}

	for(j = 1 ; j <= nbpr.nbpDataField ; j++)
		if((err=NBPExtract(nbpt,nbpr.nbpDataField,j,&en,&addr))==noErr)
			err = sendMessage(ddpt, i, &addr);

	return(err);
}

int
sendMessage(q, len, addr)
char *q;
int len;
AddrBlock *addr;
{
	int skt, err;

	skt = 0;	/* dynamic */
	if((err = DDPOpenSocket(&skt, 0)) != noErr)
		return(err);

	ddpr.abResult = 0;
	ddpr.proto.ddp.ddpAddress = *addr;
	ddpr.proto.ddp.ddpSocket = skt;
	ddpr.proto.ddp.ddpType = ddpECHO;
	ddpr.proto.ddp.ddpDataPtr = (u_char *) q;
	ddpr.proto.ddp.ddpReqCount = len;
	DDPWrite(&ddpr, FALSE);

	DDPCloseSocket(skt);
	
	return(noErr);
}

/*
 * Calulate the time in Macintosh Format
 *
 * Epochs:
 *	UNIX:	time in seconds since Thu Jan 1 10:00:00 1970
 *	MAC:	time in seconds since Fri Jan 1 11:00:00 1904
 *
 */

long
calctime()
{
	time_t now;
	register long diff, mactime;
	struct tm gmt, local, *gmtime(), *localtime();

	/*
	 * Do this by determining what the given time
	 * is when converted to local time, and when
	 * converted to GMT and taking the difference.
	 * This works correctly regardless of whether
	 * local time is Daylight Savings Time or not.
	 *
	 *	- courtesy kre@munnari.OZ.AU
	 */

#define	isleap(yr) ((yr) % 4 == 0 && ((yr) % 100 != 0 || (yr) % 400 == 0))

	(void) time(&now);
	gmt = *gmtime((time_t *) &now);
	local = *localtime((time_t *) &now);
	diff = gmt.tm_year - local.tm_year;
	diff *= 365;
	if(gmt.tm_year > local.tm_year) {
		if(isleap(local.tm_year))
			diff++;
	} else {
		if(local.tm_year > gmt.tm_year)
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
	return(htonl(mactime));
}

/*
 * format the message to fit into the Macintosh Dialog box.
 *
 */

int
msgFormat(msg)
char *msg;
{
	char *q;
	int i, j, wordWidth();

	q = msg;
	i = j = 0;
	while(*q != '\0') {
		if(*q == '\t') *q = ' ';
		if(*q == ' ' && (j + wordWidth(q+1)) >= LINEWIDTH) {
			*q = '\n';
			j = 0;
		} else
			if(*q == '\n' || *q == '\r')
				j = 0;
			else
				j += charWidth[*q & 0x7f];
		i++;
		q++;
	}
	return(i);
}

int
wordWidth(q)
char *q;
{
	int i;

	i = 0;
	while(*q != '\0') {
		if(*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
			break;
		i += charWidth[*q & 0x7f];
		q++;
	}
	return(i);
}
