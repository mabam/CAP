/*
 * $Date: 91/03/14 14:22:25 $
 * $Header: messages.c,v 2.2 91/03/14 14:22:25 djh Exp $
 * $Log:	messages.c,v $
 * Revision 2.2  91/03/14  14:22:25  djh
 * Revision for CAP.
 * 
 * Revision 1.1  91/01/10  01:11:03  djh
 * Initial revision
 * 
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
 * Send a message to a Macintosh running "Messages"
 * or a UNIX user registered with 'macuser'.
 * 
 */

#include "notify.h"

main(argc, argv)
int argc;
char *argv[];
{
	int i, j;
	int msgtype;
	char zone[34];
	char user[256];
	char *progname;
	char icon[MAXPATHLEN];
	char *userlist[MAXUSERS];
	char message[DDPPKTSIZE];
	char *cp, *index(), *rindex(), *getlogin();
	struct passwd *pp, *getpwuid(), *getpwnam();;

	progname = argv[0];
	if((cp = rindex(argv[0], '/')) != NULL)
		progname = ++cp;

	if(strcmp(progname, "macto") == 0)
		msgtype = MSGTOARG;
	else if(strcmp(progname, "macwrite") == 0)
		msgtype = MSGTO;
	else if(strcmp(progname, "macwall") == 0)
		msgtype = MSGWALL;
	else if(strcmp(progname, "macmail") == 0)
		msgtype = MSGMAIL;
	else {
		fprintf(stderr, "%s: Unknown Message Protocol\n", progname);
		exit(1);
	}

	if(msgtype == MSGTOARG && argc < 3) {
		fprintf(stderr, "usage: %s user[@zone] message\n", progname);
		exit(1);
	}

	if(msgtype != MSGWALL && argc < 2) {
		fprintf(stderr, "usage: %s user[@zone] ...\n", progname);
		exit(1);
	}

	if((cp = getlogin()) == NULL) {
		if((pp = getpwuid(getuid())) == NULL) {
			fprintf(stderr, "%s: Who are you ??\n", progname);
			exit(1);
		} else
			cp = pp->pw_name;
	}
	strncpy(user, cp, sizeof(user));

	icon[0] = '\0';
	if((pp = getpwnam(user)) != NULL)
		strncpy(icon, pp->pw_dir, sizeof(icon));
	strcat(icon, "/");
	strcat(icon, ICONFILE);

	userlist[0] = OURZONE;
	userlist[1] = NULL;
	for(j = 0 ; j < MAXUSERS-1 && --argc > 0 ; j++) {
		userlist[j] = *++argv;
		userlist[j+1] = NULL;
		if(msgtype == MSGTOARG)	 /* just the first */
			break;
	}

	switch (msgtype) {
	case MSGMAIL:	/* read stdin and present in 'biff' format	*/
		processMail(message);
		break;

	case MSGTOARG:	/* get the message from the argument list	*/
		i = 0;
		while(i < DDPPKTSIZE-1 && --argc > 0) {
			strncat(message, *++argv, DDPPKTSIZE-i-1);
			i = strlen(message);
			strncat(message, " ", DDPPKTSIZE-i-1);
			i = strlen(message);
		}
		strncat(message, "\n", DDPPKTSIZE-i-1);
		msgtype = MSGTO;
		break;

	default:	/* get the message from stdin			*/
		i = 0;
		while(i < DDPPKTSIZE-1
		    && fgets(message+i, DDPPKTSIZE-i, stdin) != 0)
			i = strlen(message);
		break;
	}

	msgFormat(message);	/* format for Mac display	*/

	j = 0;
	while(j < MAXUSERS && userlist[j] != NULL) {
		if((cp = rindex(userlist[j], '@')) != NULL) {
			*cp++ = '\0';
			strncpy(zone, cp, sizeof(zone));
		} else
			strncpy(zone, OURZONE, sizeof(zone));

		if((cp = rindex(userlist[j], '%')) != NULL)
			*cp = '@';

		if(notify(msgtype, message, user, 
		    (msgtype==MSGWALL) ? "=":userlist[j],zone,icon,1,3)!=noErr)
			fprintf(stderr, "Couldn't deliver to %s\n",userlist[j]);

		j++;
	}
	return(0);
}

/*
 * act just like biff/comsat
 *
 */

int
processMail(buf)
char *buf;
{
	register char *cp;
	register int linecnt, charcnt;
	char line[256], *index();
	int inheader, cnt, statr, len;

	/* 
	 * Get the first 7 lines or 500 characters of the new mail
	 * (whichever comes first).  Skip header crap other than
	 * From, Subject, To, and Date.
	 *
	 */

	linecnt = 7;
	charcnt = 500;
	inheader = 1;

	while(fgets(line, sizeof(line), stdin) != NULL) {
		if(linecnt <= 0 || charcnt <= 0) {  
			len = strlen(buf);
			strncat(buf, "...more...\n", DDPPKTSIZE-len);
			return(len);
		}
		if(strncmp(line, "From ", 5) == 0)
			continue;

		if(inheader && (line[0] == ' ' || line[0] == '\t'))
			continue;

		cp = index(line, ':');
		if(cp == 0 || (index(line, ' ') && index(line, ' ') < cp))
			inheader = 0;
		else
			cnt = cp - line;

		if(inheader &&
		    strncmp(line, "Date", cnt) &&
		    strncmp(line, "From", cnt) &&
		    strncmp(line, "Subject", cnt) &&
		    strncmp(line, "To", cnt))
			continue;

		if((cp = index(line, '\n')) != NULL)
			*cp = '\0';

		strcat(line, "\n");
		len = strlen(buf);
		strncat(buf, line, DDPPKTSIZE-len);
		charcnt -= strlen(line);
		linecnt--;
	}
	return(len);
}
