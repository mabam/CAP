/*
 * $Author: djh $ $Date: 1994/01/31 23:51:14 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abzip.c,v 2.4 1994/01/31 23:51:14 djh Rel djh $
 * $Revision: 2.4 $
 *
 */

/*
 * abzip.c - AppleTalk Zone Information Protocol
 *
 * Only support for GetZoneList & GetLocalZones.  GetMyZone is faked.
 *
 * MUST BE RUNNING KIP 1/88 or later to work properly (KIP 9/87 was the first
 * revision with zones, but the code didn't return properly)
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1988, CCKim Create
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>			/* so htons() works for non-vax */
#include <netat/appletalk.h>		/* include appletalk definitions */

/* move these defines to appletalk.h */
#ifndef zip_GetMyZone		/* in case defined */
/* define atp zip commands */
#define zip_GetMyZone 7		/* get my zone command */
#define zip_GetZoneList 8	/* get zone list command */
#define zip_GetLocZones 9	/* get local zone list */

/* atp user bytes for zip commands */
/* no reply struct since return is packed array of pascal strings */

typedef struct zipUserBytes {
  byte zip_cmd;			/* zip command (LastFlag on return) */
  byte zip_zero;		/* always zero */
  word zip_index;		/* zip index (count on return) */
} zipUserBytes;

#endif zip_GetMyZOne

extern u_char this_zone[];
extern u_char async_zone[];

/*
 * send a get zone list command to our bridge
 * arguments:
 *   start_index: zone start index (first is 1)
 *   buffer to return zone list reply in - must be atpMaxData
 *   zipuserbytes: on noErr return: set to user bytes returned by bridge
 *   function: zip_GetZoneList or zip_GetLocZones
 *
 */

OSErr
atpgetzonelist(start_index, buf, zipuserbytes, function)
int start_index;
char *buf;
zipUserBytes *zipuserbytes;
byte function;
{
  AddrBlock addr;
  BDS bds[1];			/* 1 for zip */
  zipUserBytes *zub;
  atpProto *ap;
  ABusRecord abr;
  OSErr err, GetBridgeAddress();

#ifndef PHASE2
  if (function == zip_GetLocZones) {
    fprintf(stderr, "ZIP: local zones request undefined for Phase 1\n");
    exit(1);
  }
#endif  PHASE2

  if (function != zip_GetZoneList && function != zip_GetLocZones)
    return(-1);

  ap = &abr.proto.atp;

  ap->atpUserData = 0L;
  zub = (zipUserBytes *)&ap->atpUserData;
  zub->zip_cmd = function;
  zub->zip_zero = 0;
  zub->zip_index = htons(start_index); /* start at 1 */

  GetBridgeAddress(&addr);
  addr.skt = ddpZIP;

  if (addr.node == 0)
    fprintf(stderr, "warning: no appletalk router found on network\n");

  ap->atpAddress.net = addr.net;
  ap->atpAddress.node = addr.node;
  ap->atpAddress.skt = ddpZIP;

  ap->atpSocket = 0;
  ap->atpReqCount = 0;
  ap->atpDataPtr = 0;
  ap->atpNumBufs = 1;
  bds[0].buffPtr = buf;		/* expect single reply from kip */
  bds[0].buffSize = atpMaxData;
  bds[0].userData = 0L;
  ap->atpRspBDSPtr = bds;
  ap->fatpXO = 0;
  ap->atpTimeOut = 4;
  ap->atpRetries = 3;

  /* send off request */
  err = ATPSndRequest(&abr, FALSE);

  if (err == noErr)
    bcopy(&bds[0].userData, zipuserbytes, sizeof(bds[0].userData));

  return(err);
}

/*
 * Given an array of char pointers
 *  return: array filled with pointers to zone names (malloc'ed)
 *   up the the size of the array
 *  return: the real count in realcnt
 *  return: any errors
 *
 */

OSErr
GetZoneList(function, zones, numzones, realcnt)
byte function;
byte *zones[];
int numzones;
int *realcnt;
{
  OSErr err;
  byte *bp;
  byte *p;
  byte buf[atpMaxData+1];
  int gotcnt, zc, zi, i;
  zipUserBytes zub;

  /* setup GetZoneList command */

  zc = 0;
  zi = 1;
  do {
    if ((err=atpgetzonelist(zi, buf, &zub, function)) != noErr)
      return(err);
    gotcnt = ntohs(zub.zip_index); /* get count */
    if (gotcnt && numzones) {
      for (bp = buf, i = 0; i < gotcnt; i++) {
	if (numzones) {
	  p = (byte *)malloc(*bp+1); /* room for string */
	  cpyp2cstr(p, bp);	/* copy out */
	  zones[zc++] = p;	/* mark it */
	  numzones--;
	}
	bp += (*bp + 1);
      }
    }
    zi += gotcnt;		/* new index */
  } while (zub.zip_cmd == 0);
  *realcnt = zi - 1;

  return(noErr);
}

/*
 * Don't ask bridge because it may be in a different zone
 *
 */

u_char *
GetMyZone()
{
  return((u_char *)this_zone);
}

/*
 * return zone for Async Appletalk
 *
 */

u_char *
GetAsyncZone()
{
  return((u_char *)async_zone);
}

/*
 * Get rid of memory malloc'ed by GetZoneList
 *
 */

FreeZoneList(zones, cnt)
char **zones;
int cnt;
{
  while (cnt--) {
    if (zones[cnt])
      free(zones[cnt]);
    zones[cnt] = NULL;
  }
}
