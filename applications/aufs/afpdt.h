/*
 * $Author: djh $ $Date: 1994/10/10 09:02:04 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpdt.h,v 2.2 1994/10/10 09:02:04 djh Rel djh $
 * $Revision: 2.2 $
 *
 */

/*
 * afpdt.h - Appletalk Filing Protocol Desktop definitions
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Mar 30, 1987     Schilit	Created.
 *
 *
 */

#include "afpavl.h"		/* relies strickly on avl structs */
  
#define FF_ICON 00
#define FF_APPL 01
#define FF_COMM 02

#define FCreatorSize 4
#define FTypeSize 4
#define ITagSize 4

typedef struct {		/* Icon Information */
  sdword i_bmsize;		/* 4: size of the icon bitmap */
  byte i_FCreator[FCreatorSize]; /* 4[8]: file's creator type */
  byte i_FType[FTypeSize];	/* 4[12] file's type */
  byte i_IType;			/* 1[13] icon type */
  byte i_pad1;			/* 1[14] */
  byte i_ITag[ITagSize];	/* 4[18] user bytes */
  byte i_pad2[2];		/* 2[20] pad out to double word boundry */
} IconInfo;

typedef struct {		/* APPL information */
  byte a_FCreator[4];		/* creator of application */
  byte a_ATag[4];		/* user bytes */  
} APPLInfo;

/* never use zero or 0x1741 as the major version */
#define AFR_MAGIC 0x00010002	/* version 1.2 (don't use 1.1, 2.2, etc) */
/* version 1.0 (version 0x1741.0000/0x1741) */


typedef struct {		/* File Format APPL record */
  dword afr_magic;		/* magic number for check */
  APPLInfo afr_info;		/* the appl info */
  sdword afr_pdirlen;		/* length of (relative) parent directory */
  sdword afr_fnamlen;		/* length of application name */
				/*  name follows */
} APPLFileRecord;
  
/* never use zero or 0x2136 as the major version */
#define IFR_MAGIC 0x00010002	/* Version 1.2, skip 1.1, 2.2, etc. */
/* version 1.0: 0x2136.0000/0x2136 */

typedef struct {		/* File Format ICON record */
  dword ifr_magic;		/* the magic check */
  IconInfo ifr_info;		/* the icon info */
				/* bitmap follows this */
} IconFileRecord;

struct IconNodeStruct {		/* Internal format Icon record */
  IconInfo in_info;		/* the icon info */
  off_t in_iloc;		/* file location */
  int in_mloc;			/* cache index */
  byte *in_uloc;		/* if set, then pointer to unix icon */
  struct IconNodeStruct *in_next; /* pointer to next in chain */
};
typedef struct IconNodeStruct IconNode;


struct APPLNodeStruct {		/* Internal format APPL record */
  struct APPLNodeStruct *an_next;
  APPLInfo an_info;		/* Appl info */
  IDirP an_ipdir;		/* parent directory */
  IDirP an_ippdir;		/* parent of parent */
  off_t an_iloc;		/* location in .ADeskTop */
  int an_flags;			/* flags */
#define AN_DEL 0x1		/* entry is deleted - really need AVLDelete */
#define AN_MOD 0x2		/* entry is "new" - modified or added */
				/* after readadesktop */
  char *an_fnam;		/* file name pointer */
};

typedef struct APPLNodeStruct  APPLNode;


typedef struct {
  int dt_ifd;			/* handle on desktop file */
  int dt_afd;			/* handle on desktop file */
  int dt_ivol;			/* desktop belongs to this volume */
  word dt_evol;			/* external value of above */
  AVLNode *dt_avlroot;		/* root of avl tree mapping file creators */
  byte dt_oFCreator[FCreatorSize]; /* key for last cache entry of above */
  AVLNode *dt_avllast;		/* node pointed to by key */
  IDirP dt_rootd;		/* volumes root dir */
  int dt_opened;		/* true if desktop is open */
} DeskTop, *DeskTopP;

#define IFRLEN (sizeof(IconFileRecord))
#define AFRLEN (sizeof(APPLFileRecord))

#define NILDT ((DeskTop *) 0)

private DeskTop *VolGetDesk();
private void VolSetDesk();
private void VolClrDesk();

typedef struct {
  IconNode *ib_node;		/* pointer to owner node */
  byte *ib_data;		/* pointer to the icon data */
  int ib_size;			/* size of icon data */
} IconCacheEntry, *ICEP;

/* 
 * ICSize is the size of the icon cache which is used to store icon
 * bitmaps in memory.  The size of the cache is dependant on the
 * following:
 *
 *  Memory:
 *   size of each entry is about 256 bytes (icon data size for b&w icons) +
 *   Sizeof(IconCacheEntry).
 *
 *  Work:
 *   Reference to an icon bitmap in memory is constant time since the 
 *   cache index is stored in the IconNode.
 *
 *   Reference to an icon bitmap in a file causes a scan of the entire
 *   cache in order to find the minimum entry for replacement.
 *
 *  Performance:
 *   A good cache size would try to contain all the icons on the desktop 
 *   (considering the above constaints) since the finder will try to read
 *   the icon bitmaps when returning to the desktop.
 *
 */

#define ICSize 100			/* number of icon cache entries */

