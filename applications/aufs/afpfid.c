/*
 * $Author: djh $ $Date: 1996/04/27 12:03:04 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpfid.c,v 2.2 1996/04/27 12:03:04 djh Rel djh $
 * $Revision: 2.2 $
 *
 */

/*
 * afpfid.c - File ID routines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  September 1995 scooter	Created
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#ifndef _TYPES
 /* assume included by param.h */
# include <sys/types.h>
#endif  _TYPES
#include <sys/stat.h>
#include <netat/appletalk.h>
#include <netat/afp.h>
#include "afps.h"

#ifdef FIXED_DIRIDS
#include "../../lib/afp/afpidaufs.h"
#endif /* FIXED_DIRIDS */

/*
 * FIdmove(IDirP fpdir, char *from, IDirP tpdir, char *to)
 *
 * Maintains consistency in database structures when a file
 * is renamed or moved.
 *
 * The file "from" in parent fpdir is being renamed to be
 * "to" in the parent tpdir.  Because file ids may not
 * change we must modify the tree instead of recreating nodes.
 *
 */ 

#ifndef FIXED_DIRIDS
void
FIdmove(fpdir,from,tpdir,to)
IDirP fpdir,tpdir;
char *from,*to;
{
	return;
}
#else	/* FIXED_DIRIDS */
void
FIdmove(fpdir,from,tpdir,to)
IDirP fpdir,tpdir;
char *from,*to;
{
  IDirP fdir;
  sdword toEid, fileID;
  char path[MAXUFLEN];

#ifdef SHORT_NAMES
  if (DBFIL && fpdir != NULL)
   printf("FIdmove fpdir->name %s, tpdir->name %s, from %s, to %s\n",fpdir->name, tpdir->name, from, to);
#endif SHORT_NAMES

  if (DBFIL) {
    printf("FIdmove: changing path=%s, file=%s",pathstr(fpdir),from);
    if (tpdir == fpdir)
      printf(" to new name %s\n",to);
    else
      printf(" to path=%s, file=%s\n",pathstr(tpdir),to);
  }

  sprintf(path, "%s/%s", pathstr(fpdir), from);

  /* Look up from to see if we have a FileID */
  if ((fileID = aufsExtFindId(path)) < 0) {
    if (DBFIL)
       printf("FIdmove: no ID for %s/%s",pathstr(fpdir),from);
    return;		/* Nope, we're done */
  }

  /* We've got one, now rename it! */

  if (fpdir != tpdir) {			/* if different parents then... */
    toEid = Edirid(tpdir);
    aufsExtMoveIds(fileID, toEid, to);
  } else { /* effectively a rename */
    aufsExtRenameId(fileID, to);
  }
}
#endif FIXED_DIRIDS
