/*
 * $Date: 91/03/14 14:23:37 $
 * $Header: macwho.c,v 2.2 91/03/14 14:23:37 djh Exp $
 * $Log:	macwho.c,v $
 * Revision 2.2  91/03/14  14:23:37  djh
 * Revision for CAP.
 * 
 * Revision 1.1  91/01/10  01:10:57  djh
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
 * List Chooser Names of Macintoshes running "Messages"
 *
 */

#include "notify.h"

extern NBPTEntry nbpt[];

main(argc, argv)
int argc;
char *argv[];
{
	int notify();
	int compare();
	int num, i, j, k;
	EntityName en;
	AddrBlock addr;
	char *zonelist[MAXZONES];

	zonelist[0] = OURZONE;
	zonelist[1] = NULL;

	for(i = 0 ; --argc > 0 && i < MAXZONES-1 ; i++) {
		zonelist[i] = *++argv;
		zonelist[i+1] = NULL;
	}

	j = 0;
	k = 1;
	while(zonelist[j] != NULL) {
	    if((num = notify(0, "", "", "=", zonelist[j], 0, 1, 6)) > 0) {
		qsort((char *) nbpt, num, sizeof(NBPTEntry), compare);
		for(i = 1; i <= num ; i++)
		    if(NBPExtract(nbpt, num, i, &en, &addr) == noErr) {
			printf("%3d  %-34s[Net %3d.%-3d Node %3d%s%s]\n",
			    k++,
			    en.objStr.s,
			    ntohs(addr.net) / 256,
			    ntohs(addr.net) % 256,
			    addr.node,
			    (*zonelist[j]=='*') ? "" : " ",
			    (*zonelist[j]=='*') ? "" : zonelist[j]);
			    /* Sigh: much nicer if we could use en.zoneStr */
			}
	    }
	    j++;
	}
}

int
compare(a, b)
NBPTEntry *a, *b;
{
	return(strcmp(a->ent.objStr.s, b->ent.objStr.s));
}
