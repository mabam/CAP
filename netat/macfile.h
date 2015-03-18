/*
 * $Author: djh $ $Date: 1991/06/13 11:14:52 $
 * $Header: /mac/src/cap60/netat/RCS/macfile.h,v 2.2 1991/06/13 11:14:52 djh Rel djh $
 * $Revision: 2.2 $
*/

/*
 * macfile.h - header file with Macintosh file definitions
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Sept 1987	Created by Charlie
 *
 */

#ifndef _MACFILE

#define MAXCLEN 199		/* max size of a comment string */
#define FINFOLEN 32		/* Finder info is 32 bytes */
#define MAXMACFLEN 31		/* max Mac file name length */

/* finder info defintions */
typedef struct {
  byte fi_fndr[FINFOLEN];	/* finder info */
  word fi_attr;			/* attributes */
  byte fi_comln;		/* length of comment */
  byte fi_comnt[MAXCLEN+1];	/* comment string */
} OldFileInfo;
#define FI_BASELENGTH (FINFOLEN+3)

typedef struct {
  byte fi_fndr[FINFOLEN];	/* finder info */
  word fi_attr;			/* attributes */
#define FI_MAGIC1 255
  byte fi_magic1;		/* was: length of comment */
#define FI_VERSION 0x10		/* version major 1, minor 0 */
				/* if we have more than 8 versions wer're */
				/* doiong something wrong anyway */
  byte fi_version;		/* version number */
#define FI_MAGIC 0xda
  byte fi_magic;		/* magic word check */
  byte fi_bitmap;		/* bitmap of included info */
#define FI_BM_SHORTFILENAME 0x1	/* is this included? */
#define FI_BM_MACINTOSHFILENAME 0x2 /* is this included? */
  byte fi_shortfilename[12+1];	/* possible short file name */
  byte fi_macfilename[32+1];	/* possible macintosh file name */
  byte fi_comln;		/* comment length */
  byte fi_comnt[MAXCLEN+1];	/* comment string */
#ifdef USE_MAC_DATES
  byte fi_datemagic;		/* sanity check */
#define FI_MDATE 0x01		/* mtime & utime are valid */
#define FI_CDATE 0x02		/* ctime is valid */
  byte fi_datevalid;		/* validity flags */
  byte fi_ctime[4];		/* mac file create time */
  byte fi_mtime[4];		/* mac file modify time */
  byte fi_utime[4];		/* (real) time mtime was set */
#endif USE_MAC_DATES
} FileInfo;

/* Atribute flags */
#define FI_ATTR_SETCLEAR 0x8000 /* set-clear attributes */
#define FI_ATTR_READONLY 0x20	/* file is read-only */
#define FI_ATTR_ROPEN 0x10	/* resource fork in use */
#define FI_ATTR_DOPEN 0x80	/* data fork in use */
#define FI_ATTR_MUSER 0x2	/* multi-user */
#define FI_ATTR_INVISIBLE 0x1	/* invisible */

/**** MAC STUFF *****/

/* Flags */
#define FNDR_fOnDesk 0x1
#define FNDR_fHasCustomIcon 0x0400
#define FNDR_fHasBundle 0x2000
#define FNDR_fInvisible 0x4000
/* locations */
#define FNDR_fTrash -3	/* File in Trash */
#define FNDR_fDesktop -2	/* File on desktop */
#define FNDR_fDisk 0	/* File in disk window */

/* finder info structure */
/* Note: technically, the fileFinderInfo and the dirFinderInfo should be */
/* seperated into FileInfo, XFileInfo, and DirINfo, XDirInfo */
typedef struct {
  /* base finder information */
  byte fdType[4];		/* File type [4]*/
  byte fdCreator[4];		/* File creator [8]*/
  word fdFlags;			/* Finder flags [10]*/
  word fdLocation[2];		/* File's location [14] */
  word fdFldr;			/* File's window [16] */
  /* extended finder information */
  word fdIconID;		/* Icon ID [18] */
  word fdUnused[4];		/* Unused [26] */
  word fdComment;		/* Comment ID [28] */
  dword fdPutAway;		/* Home directory ID [32] */
} fileFinderInfo;

typedef struct {
  word frRect[4];		/* [rect] Folder'r rectange [8] */
  word frFlags;			/* Folder's flags [10] */
  word frLocation[2];		/* Folder's location [14] */
  word frView;			/* Folder's view [16] */
  /* extended finder information */
  word frScroll[2];		/* (Point) Scroll position [20] */
  dword frOpenChain;		/* dir id chain of open folders [24] */
  byte frScript;		/* script flag and code [26] */
  byte frXFlags;		/* reserved [27] */
  word frComment;		/* Comment id [28] */
  dword frPutAway;		/* home directory id [32] */
} dirFinderInfo;

typedef union {
  dirFinderInfo dir;
  fileFinderInfo file;
} FinderInfo;

#define _MACFILE
#endif /* MACFILE */
