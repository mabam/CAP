/*
 * $Author: djh $ $Date: 1996/05/18 10:51:21 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abversion.c,v 2.98 1996/05/18 10:51:21 djh Rel djh $
 * $Revision: 2.98 $
 *
 */

/*
 * abversion.c - Return version information
 *
 * Copyright (c) 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  March 1988    CCKim	Created.
 *
 */

#include <netat/appletalk.h>

private struct cap_version myversion;

struct cap_version *
what_cap_version()
{
  extern char Columbia_Copyright[];
  extern short lap_proto;

  myversion.cv_copyright = Columbia_Copyright;
  myversion.cv_name = "CAP";
  myversion.cv_version = 6;
  myversion.cv_subversion = 0;
  myversion.cv_patchlevel = 198;
  myversion.cv_rmonth = "June";
  myversion.cv_ryear = "1996";
  switch (lap_proto) {
  case LAP_KIP:
 	myversion.cv_type = "UDP encapsulation";
	break;
  case LAP_MKIP:
 	myversion.cv_type = "Modified UDP encapsulation";
	break;
  case LAP_ETALK:
#ifdef PHASE2
 	myversion.cv_type = "EtherTalk Phase 2 encapsulation";
#else  PHASE2
 	myversion.cv_type = "EtherTalk encapsulation";
#endif PHASE2
	break;
  case LAP_KERNEL:
	myversion.cv_type = "Kernel Based EtherTalk encapsulation";
	break;
  }
  return(&myversion);
}
