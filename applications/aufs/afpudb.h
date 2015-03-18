/*
 * $Author: djh $ $Date: 1993/08/04 15:23:33 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpudb.h,v 2.3 1993/08/04 15:23:33 djh Rel djh $
 * $Revision: 2.3 $
*/

/*
 * afpudb.c - Appletalk Filing Protocol Unix Finder Information
 *  database.  Header file.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  August 1987     CCKim	Created.
 *
 */

struct ufinderdb {
  byte ufd_creat[4];
  byte ufd_ftype[4];
  char *ufd_comment;
  int ufd_commentlen;
  byte *ufd_icon;
  int ufd_iconsize;
};

int os_getunixtype();

#ifdef USR_FILE_TYPES
struct uft {
  byte uft_suffix[16];		/* unix filename suffix, ie: .tar */
  short uft_suffixlen;
  byte *uft_xlate;		/* translation to perform ie: raw */
  byte uft_creat[4];		/* creator to appear as ie: 'TAR ' */
  byte uft_ftype[4];		/* macintosh file type ie: 'TEXT' */
  char *uft_comment;		/* "This is a UNIX tar file" */
  short uft_commentlen;
};
#define NUMUFT 50
#define FNDR_UFT 100
#endif USR_FILE_TYPES
