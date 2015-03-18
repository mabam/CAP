/* "$Author: djh $ $Date: 1995/08/28 11:10:14 $" */
/* "$Header: /mac/src/cap60/applications/lwsrv/RCS/procset.h,v 2.3 1995/08/28 11:10:14 djh Rel djh $" */
/* "$Revision: 2.3 $" */

/*
 * procset - UNIX AppleTalk spooling program: act as a laserwriter
 *   handles simple procset list - assumes procsets in a directory
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986,1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Created sept 5, 1987 by cck from lwsrv
 *
 */

typedef struct Dict_List {		/* known dictionary list */
#ifndef LWSRV8
  struct Dict_List *ad_next;		/* pointer to next */
  char *ad_ver;				/* the version number */
#endif /* not LWSRV8 */
  char *ad_fn;				/* the file name */
  int ad_sent;				/* downloaded during this job? */
} DictList;

DictList *GetProcSet();		/* DictList *GetProcSet(char *) */
void ClearProcSetsSent();
void newdictionary();		/* void newdictionary(DictList *) */
void ListProcSet();

#ifdef LWSRV8
DictList *CreateDict();
KVTree **scandicts();
void set_dict_list();
#endif /* LWSRV8 */
