static char rcsid[] = "$Author: djh $ $Date: 1996/04/28 13:22:16 $";
static char rcsident[] = "$Header: /mac/src/cap60/samples/RCS/getzones.c,v 2.9 1996/04/28 13:22:16 djh Rel djh $";
static char revision[] = "$Revision: 2.9 $";

/*
 * getzones - retrieves the zone list from our bridge
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia
 * University in the City of New York.
 *
 * Edit History:
 *
 *  March 1988, CCKim, Created
 *  March 1993, John Huntley, added debug argument processing
 *
 */

char copyright[] = "Copyright (c) 1988 by The Trustees of Columbia \
University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>			/* so htons() works for non-vax */
#include <netat/appletalk.h>		/* include appletalk definitions */

#define NUMZONES 500

main(argc, argv)
int argc;
char **argv;
{
  int i,cnt;
  OSErr err;
  byte function;
  char *zones[NUMZONES];	/* room for pointers to zone names */
  u_char *GetMyZone();
  u_char *myzone;
  int verbose = 0;
  int sortit = 0;
  int mzone = 0;
  extern int opterr;
  extern char *optarg;
  int zonecmp();

  opterr = 0;
  function = zip_GetZoneList;

  while ((i = getopt(argc, argv, "d:D:lmsv")) != EOF) {
    switch (i) {
      case 'd':
      case 'D':
	dbugarg(optarg);
	break;
      case 'l':
	function = zip_GetLocZones;
	break;
      case 'm':
	mzone = 1;
	break;
      case 's':
	sortit = 1;
	break;
      case 'v':
	verbose++;
	break;
    }
  }

  abInit(verbose ? TRUE : FALSE);

  myzone = GetMyZone();

#ifdef ISO_TRANSLATE
  cMac2ISO(myzone);
#endif ISO_TRANSLATE

  if (mzone) {
    printf("%s\n", (char *)myzone);
    exit(0);
  }

  if ((err = GetZoneList(function, zones, NUMZONES, &cnt)) != noErr) {
    fprintf(stderr, "error %d getting zone list\n", err);
    exit(1);
  }

  if (cnt > NUMZONES) {
    printf("only asked for %d zones when there were actually %d\n",
	   NUMZONES,cnt);
    cnt = NUMZONES;
  }

  /*
   * Sort the Zone list, if required
   *
   */
  if (sortit)
    qsort((char *)zones, cnt, sizeof(char *), zonecmp);

  if (verbose)
    printf("Count is %d\n", cnt);

  for (i = 0; i < cnt ; i++) {
#ifdef ISO_TRANSLATE
    cMac2ISO(zones[i]);
#endif ISO_TRANSLATE
    if (verbose)
      printf("ZONE %s", zones[i]);
    else
      printf("%s", zones[i]);
    if (verbose && strcmp(zones[i], (char *)myzone) == 0)
      putchar('*');
    putchar('\n');
  }

  FreeZoneList(zones, cnt);
}

int 
zonecmp(s1, s2)
void *s1, *s2;
{
	return(strcmp(*(char **)s1, *(char **)s2));
}
