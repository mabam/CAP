static char rcsid[] = "$Author: djh $ $Date: 1991/05/18 10:16:09 $";
static char rcsident[] = "$Header: /mac/src/cap60/samples/RCS/atistest.c,v 2.2 1991/05/18 10:16:09 djh Exp djh $";
static char revision[] = "$Revision: 2.2 $";

/*
 * atistest.c - simple test program to ensure that atis is functioning
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Edit History:
 *
 *  July 27, 1987    CCKim		Created.
 *
*/
#include <stdio.h>
#include <sys/types.h>

#include <netat/appletalk.h>

main(argc, argv)
int argc;
char *argv[];
{
  AddrBlock useaddr;
  int skt, err, remove;
  struct cap_version *cv;

  if (argc > 1 && strcmp(argv[1], "-r") == 0)
    remove = 1;
  else
    remove = 0;

  cv = what_cap_version();
  printf("%s distribution %d.%02d using %s, %s %s\n",
	 cv->cv_name, cv->cv_version, cv->cv_subversion,
	 cv->cv_type, cv->cv_rmonth, cv->cv_ryear);
  printf("%s\n\n", cv->cv_copyright);

  abInit(TRUE);
  nbpInit();
  dbugarg("n");
  useaddr.net = useaddr.node = useaddr.skt = 0; /* accept from anywhere */
  skt = 0;			/* dynamically allocate skt please */
  if ((err = ATPOpenSocket(&useaddr, &skt)) < 0) {
    perror("ATP Open Socket");
    aerror("ATP Open Socket",err);
    exit(1);
  }

  printf("Registering \"atis test:testing@*\"\n");
  err = nbp_register("atis test", "testing", "*", skt);
  if (err != noErr)
    aerror("nbp register",err);
  else
    printf("Okay\n");

  if (remove) {
    err = nbp_remove("atis test", "testing", "*");
    if (err != noErr)
      aerror("nbp remove",err);
  }
}

/*
 * register the specified entity
 *
*/
nbp_register(sobj, stype, szone, skt)
char *sobj, *stype, *szone;
int skt;
{
  EntityName en;
  nbpProto nbpr;		/* nbp proto */
  NBPTEntry nbpt[1];		/* table of entity names */
  int err;

  strcpy(en.objStr.s, sobj);
  strcpy(en.typeStr.s, stype);
  strcpy(en.zoneStr.s, szone);


  nbpr.nbpAddress.skt = skt;
  nbpr.nbpRetransmitInfo.retransInterval = 4;
  nbpr.nbpRetransmitInfo.retransCount = 3;
  nbpr.nbpBufPtr = nbpt;
  nbpr.nbpBufSize = sizeof(nbpt);
  nbpr.nbpDataField = 1;	/* max entries */
  nbpr.nbpEntityPtr = &en;

  err = NBPRegister(&nbpr,FALSE);	/* try synchronous */
  return(err);
}

/*
 * remove the specified entry
 *
 */

nbp_remove(sobj, stype, szone)
char *sobj, *stype, *szone;
{
  EntityName en;
  strcpy(en.objStr.s, sobj);
  strcpy(en.typeStr.s, stype);
  strcpy(en.zoneStr.s, szone);

  return(NBPRemove(&en));
}

aerror(msg, err)
char *msg;
int err;
{
  printf("%s error because: ",msg);
  switch (err) {
  case tooManySkts:
    printf("too many sockets open already\n");
    break;
  case noDataArea:
    printf("internal data area corruption - no room to create socket\n");
    break;
  case nbpDuplicate:
    printf("name already registered\n");
    break;
  case nbpNoConfirm:
    printf("couldn't register name - is atis running?\n");
    break;
  case nbpBuffOvr:
    printf("couldn't register name - too many names already registered\n");
    break;
  default:
    printf("error: %d\n",err);
    break;
  }
}
