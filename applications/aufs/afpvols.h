/*
 * $Author: djh $ $Date: 1994/02/16 05:09:02 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpvols.h,v 2.3 1994/02/16 05:09:02 djh Rel djh $
 * $Revision: 2.3 $
*/

/*
 * afpvols.h - Appletalk Filing Protocol Volume definitions
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *
 */


typedef struct {		/* Volume Entry */
  IDirP v_rootd;		/* root directory (mount point) */
  word v_bitmap;		/* for packing purposes only */
  char v_path[MAXDLEN];		/* path for this volume */
  word v_attr;			/* (lsb) read-only flag */
#define V_RONLY			V_READONLY /* backward compat */
#define V_READONLY		0x0001	/* read only flag */
#define V_HASVOLUMEPASSWORD	0x0002	/* has a volume password */
#define V_SUPPORTSFILEIDS	0x0004	/* supports file IDs */
#define V_SUPPORTSCATSEARCH	0x0008	/* supports cat search */
#define V_SUPPORTSBLNKACCESS	0x0010	/* supports blank access privs */
  char v_name[MAXVLEN];		/* advertised name */
  char v_pwd[MAXPLEN];		/* volume password (unused) */
  boolean v_mounted;		/* mounted flag */
  word v_sig;			/* volume signature */
  word v_volid;			/* volume ID */
  sdword v_cdate;		/* volume creation date */
  sdword v_mdate;		/* volume modification date */
  sdword v_bdate;		/* volume backup date */
  dword v_size;			/* size of volume in bytes */
  dword v_free;			/* free bytes on volume */
  byte v_esize[8];		/* size of volume in bytes */
  byte v_efree[8];		/* free bytes on volume */
} VolEntry, *VolPtr;		/* user volume table */

#define NILVOL ((VolPtr) 0)

