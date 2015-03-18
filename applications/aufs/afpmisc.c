/*
 * $Author: djh $ $Date: 91/02/15 21:07:34 $
 * $Header: afpmisc.c,v 2.1 91/02/15 21:07:34 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * afpmisc.c - miscellaneous, but nevertheless useful routines
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
#include <sys/types.h>
#include <netat/appletalk.h>
#include <netat/afp.h>
#include "afps.h"

int afp_dbug;

/*
 * int SetDBLevel(char *argv)
 *
 * Process arguments for tracing of AFP levels: fork, file, directory,
 * etc.  The options are:
 *
 */

struct {
  int db_flg;
  char *db_nam;
} dbtab[] = {
  {DBG_ALL,"All"},
  {DBG_DEBG|DBG_DESK,"DeskTop"},
  {DBG_DEBG|DBG_DIRS,"Directory"},
  {DBG_DEBG|DBG_ENUM,"Enumerate"},
  {DBG_DEBG|DBG_FILE,"File"},
  {DBG_DEBG|DBG_FORK,"Fork"},
  {DBG_DEBG|DBG_OSIN,"OS"},
  {DBG_DEBG|DBG_SRVR,"Server"},
  {DBG_DEBG|DBG_UNIX,"Unix"},
  {DBG_DEBG|DBG_VOLS,"Volume"},
  {DBG_DEBG,"debug"}
};

#define DBTABN 11		/* size of debug table */

char *DBLevelOpts()
{
  int i;
  static char dbopts[100];

  *dbopts = '\0';
  for (i=0; i < DBTABN; i++) {
    strcat(dbopts,dbtab[i].db_nam);
    strcat(dbopts," "); 
  }
  return(dbopts);
}

int SetDBLevel(s)
char *s;
{
  char dbuf[30],*cp;
  int i,idx,len;

  while (*s != '\0') {			/* until runnout */
    while(*s == ' ' || *s == '\t')	/* skip spaces */
      s++;
    if (*s == '\0')
      break;
    for (len=0, cp = dbuf; *s != ' ' && *s != '\t' && *s != '\0' && len < 29;
	 cp++, s++, len++)
      *cp = *s;
    *cp++ = '\0';
    /* length should be correct */
    /* len = strlen(dbuf); */			/* find length of command */
    idx = -1;
    for (i=0; i < DBTABN; i++) {
      if (strncmpci(dbuf,dbtab[i].db_nam,len) == 0)
	if (idx > 0) {
	  printf("SetDBLevel: ambiguous debug '%s' (%s and %s)\n",
		 dbuf,dbtab[idx].db_nam,dbtab[i].db_nam);
	  return(FALSE);
	} else
	  idx = i;
    }      
    if (idx < 0) {
      printf("SetDBLevel: unknown debug level %s\n",dbuf);
      return(FALSE);
    }
    printf("SetDBLevel: Debugging %s\n",dbtab[idx].db_nam);
    afp_dbug |= dbtab[idx].db_flg;	/* add the flag */
  }
  return(TRUE);
}

