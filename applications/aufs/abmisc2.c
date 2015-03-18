/*
 * $Author: djh $ $Date: 91/02/15 21:05:37 $
 * $Header: abmisc2.c,v 2.1 91/02/15 21:05:37 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * abmisc2.c - miscellaneous, but nevertheless useful routines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *
 */

#include <stdio.h>
#include <netat/appletalk.h>
#include <ctype.h>
#include <sys/types.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>


/* Should be put in abnbp.c */
/* cck - is server dependent - don't want in libraries */

/*
 * OSErr SrvrRegister(int skt, char *name, char *type, char *zone)
 *
 * Register the name and socket using NBP.
 *
 * If type is NULL then 'name' is a compound name in the form "name:type@zone"
 * otherwise 'name', 'type' and 'zone' point to the respective parts of the
 * name.
 *
 */

#define SRV_RTI (sectotick(1))		/* retransmit interval is 4 seconds */
#define SRV_RTC 5			/* retransmit count */


OSErr
SrvrRegister(skt,name,type,zone, en)
int skt;
char *name,*type,*zone;
EntityName *en;
{
  nbpProto nbpr;
  NBPTEntry nbpt[1];
  int err;
  
  if (type == NULL) 			/* if no type then compound name */
    create_entity(name,en); /* create entity */
  else {				/* otherwise already split */
    strcpy(en->objStr.s,name);
    strcpy(en->typeStr.s,type);
    strcpy(en->zoneStr.s,zone);
  }

  nbpr.nbpAddress.skt = skt;
  nbpr.nbpRetransmitInfo.retransInterval = SRV_RTI;
  nbpr.nbpRetransmitInfo.retransCount = SRV_RTC;
  nbpr.nbpBufPtr = nbpt;
  nbpr.nbpBufSize = sizeof(nbpt);
  nbpr.nbpDataField = 1;
  nbpr.nbpEntityPtr = en;		/* entity */
  return(NBPRegister(&nbpr,FALSE));	/* register the name */
}

OSErr
SrvrShutdown(en)
EntityName *en;
{
  return(NBPRemove(en));
}


