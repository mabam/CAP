/*
 * $Author: djh $ $Date: 1996/04/25 01:06:27 $
 * $Header: /mac/src/cap60/netat/RCS/afp.h,v 2.5 1996/04/25 01:06:27 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * afp.h - header file for AppleTalk Filing Protocol
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March, 1987	Schilit    Created
 *
 */

#ifndef _MACFILE
#include <netat/macfile.h>
#endif  _MACFILE

/* AFP Errors.  The little "ae" prevents conflicts with other codes */

#define aeAccessDenied		-5000
#define aeAuthContinue		-5001	/* Authorization not yet complete */
#define aeBadUAM		-5002	/* Unknown User Auth Method */
#define aeBadVersNum		-5003	/* Server cannot speak AFP version */
#define aeBitMapErr		-5004
#define aeCantMove		-5005
#define aeDenyConflict		-5006
#define aeDirNotEmpty		-5007
#define aeDiskFull		-5008
#define aeEOFErr		-5009
#define aeFileBusy		-5010
#define aeFlatVol		-5011
#define aeItemNotFound		-5012
#define aeLockErr		-5013
#define aeMiscErr		-5014
#define aeNoMoreLocks		-5015
#define aeNoServer		-5016	/* Server not responding */
#define aeObjectExists		-5017
#define aeObjectNotFound 	-5018
#define aeParamErr		-5019
#define aeRangeNotLocked 	-5020
#define aeRangeOverlap		-5021
#define aeSessClosed		-5022	/* Sessions was closed, no response */
#define aeUserNotAuth		-5023	/* User authorization failure */
#define aeCallNotSupported 	-5024
#define aeObjectTypeErr		-5025
#define aeTooManyFilesOpen 	-5026
#define aeServerGoingDown  	-5027
#define aeCantRename		-5028
#define aeDirNotFound		-5029
#define aeIconTypeError		-5030
#define aeVolumeLocked		-5031 /* AFP2.0 */
#define aeObjectLocked		-5032 /* AFP2.0 */
#define aeIDNotFound		-5034 /* AFP2.1 */
#define aeIDExists		-5035 /* AFP2.1 */
#define aeCatalogChanged	-5037 /* AFP2.1 */
#define aeSameObjectErr		-5038 /* AFP2.1 */
#define aeBadIDErr		-5039 /* AFP2.1 */
#define aePwdSameErr		-5040 /* AFP2.1 */
#define aePwdTooShort		-5041 /* AFP2.1 */
#define aePwdExpired		-5042 /* AFP2.1 */
#define aeInsideSharedErr	-5043 /* AFP2.1 */
#define aeInsideTrashErr	-5044 /* AFP2.1 */

/* AFP Commands Definitions */

#define AFPByteRangeLock 1	/* Lock a range of bytes in a file  */
#define AFPCloseVol	2	/* Close a volume */
#define AFPCloseDir	3	/* Close a directory */
#define AFPCloseFork	4	/* Close a fork */
#define AFPCopyFile	5	/* Copy a file */
#define AFPCreateDir	6	/* Create a directory */
#define AFPCreateFile	7	/* Create a file */
#define AFPDelete	8	/* Delete a file or directory */
#define AFPEnumerate	9	/* Enumerate directory entries */
#define AFPFlush	10	/* Flush a volume */
#define AFPFlushFork	11	/* Flush a fork */
#define AFPGetForkParms 14	/* Get fork parameters */
#define AFPGetSrvrInfo	15	/* Get server info */
#define AFPGetSrvrParms 16	/* Get server parameters */
#define AFPGetVolParms	17	/* Get volume parameters */
#define AFPLogin	18	/* Login to the server */
#define AFPLoginCont	19	/* Continue a login sequence */
#define AFPLogout	20	/* Logout (FPLogout) */
#define AFPMapID	21	/* Map a protection ID to: */
#define   MapID_C 1		/*   creater name */
#define   MapID_G 2		/*   group name */
#define AFPMapName	22	/* Map a protection name to: */
#define   MapName_C 3		/*   creator ID (uid) */
#define   MapName_G 4		/*   group ID (gid) */
#define AFPMove		23	/* Move a file */
#define AFPOpenVol      24	/* Open a volume */
#define AFPOpenDir	25	/* Open a directory */
#define AFPOpenFork	26	/* Open a fork */
#define AFPRead		27	/* Read from a fork */
#define AFPRename	28	/* Rename a file or directory */
#define AFPSetDirParms	29	/* Set directory parameters */
#define AFPSetFileParms	30	/* Set file parameters */
#define AFPSetForkParms	31	/* Set fork parameters */
#define AFPSetVolParms	32	/* Set volume parameters */
#define AFPWrite	33	/* Write to a fork */
#define AFPGetFileDirParms 34	/* Get params for a file or directory */
#define AFPSetFileDirParms 35	/* Set params for a file or directory */
#define AFPChgPasswd	36	/* AFP2.0: Change Password */
#define AFPGetUserInfo	37	/* AFP2.0: Get User Information */
#define AFPGetSrvrMsg	38	/* AFP2.1: Get Server Message */
#define AFPCreateID	39	/* AFP2.1: Create a File ID */
#define AFPDeleteID	40	/* AFP2.1: Invalidate a File ID */
#define AFPResolveID	41	/* AFP2.1: Get params for a file by File ID */
#define AFPExchangeFiles   42	/* AFP2.1: Exchange 2 files Data/Resource */
#define AFPCatSearch	43	/* AFP2.1: Search volume for files */
#define AFPOpenDT	48	/* Open the volume's desktop database */
#define AFPCloseDT	49	/* Close the volume's desktop database */
#define AFPGetIcon	51	/* Get an icon from the dt database */
#define AFPGetIconInfo	52	/* Get icon info from the dt database */
#define AFPAddAPPL	53	/* Find an application from the dt database */
#define AFPRmvAPPL	54	/* Remove an application from the dt ... */
#define AFPGetAPPL	55	/* Get an application ... */
#define AFPAddComment	56	/* Add a comment to the dt */
#define AFPRmvComment	57	/* Remove a comment from the dt */
#define AFPGetComment	58	/* Get a comment from the dt */
#define AFPAddIcon	192	/* Add an icon to the dt */
#define AFPMaxCmd	AFPAddIcon
#define AFPShutDown	0xffff	/* Shutdown server - unlikely command? */

#define MAXSNAM 31		/* max server name string */
#define MAXUAME 16		/* max size for each UAM string */
#define MAXVERE 16		/* max size for each version string */

/* some base definitions */
#define MAXPSTR 255		/* max size of a pascal string */
#define MAXVLEN 27		/* max size of volume name */
#define MAXVNAME MAXVLEN
#define MAXPLEN 8		/* max size of volume password */
#define MAXPASSWD 8
#define MAXDLEN 1024		/* max length of a path */
#ifndef MAXPATH
#define MAXPATH MAXDLEN
#endif  MAXPATH
#ifdef  AIX
#undef  MAXPATH
#define MAXPATH MAXDLEN
#endif  AIX
#define MAXLFLEN 31		/* max length of long file name */
#define MAXSFLEN 12		/* max length of short file name */
#define MAXUFLEN ((3*MAXLFLEN)+1) /* max length of unix expanded name */

typedef struct {		/* Directory Only Parms */
  sdword dp_dirid;		/* directory id */
  word dp_nchild;		/* number of offspring */
  sdword dp_ownerid;		/* owner id */
  sdword dp_groupid;		/* group id */
  dword dp_accright;		/* access rights */
} DirParm;

typedef struct {		/* File Only Parms */
  sdword fp_fileno;		/* file number */
  sdword fp_rflen;		/* resource fork length */
  sdword fp_dflen;		/* data fork length */
} FileParm;

#define FDP_DIRFLG 0x80		/* directory flag */
#define FDP_ISDIR(flg) (((flg) & FDP_DIRFLG) != 0)

typedef struct {		/* FileDirParms */
  byte fdp_flg;			/* Directory flag */
  byte fdp_zero;		/* zero byte for packing */
  word fdp_attr;		/* attribute flags */
  sdword fdp_pdirid;		/* parent directory ID */
  sdword fdp_cdate;		/* creation date */
  sdword fdp_mdate;		/* modification date */
  sdword fdp_bdate;		/* backup date */
  byte fdp_finfo[FINFOLEN];	/* Finder info */
  char fdp_lname[MAXLFLEN];	/* long name */
  char fdp_sname[MAXSFLEN];	/* short name */
  word fdp_fbitmap;		/* file bitmap for packing */
  word fdp_dbitmap;		/* directory bitmap for packing */
  union {			/* union for file/directory only parms */
    DirParm dp_parms;		/* directory only parms */
    FileParm fp_parms;		/* file only parms */
  } fdp_parms;			/*  these are called fdp_parms */
  word fdp_prodos_ft;		/* prodos file type information */
  dword fdp_prodos_aux;		/* prodos aux file type info */
} FileDirParm, *FDParmPtr;

/* Volume Params */

#define VP_ATTR  00001		/* attributes */
#define VP_SIG   00002		/* signature byte */
#define VP_CDATE 00004		/* creation date */
#define VP_MDATE 00010		/* modification date */
#define VP_BDATE 00020		/* backup date */
#define VP_VOLID 00040		/* volume id */
#define VP_FREE  00100		/* free bytes */
#define VP_SIZE  00200		/* size in bytes */
#define VP_NAME  00400		/* volume name */
#define VP_EFREE 01000		/* AFP2.2: extended free bytes */
#define VP_ESIZE 02000		/* AFP2.2: extended total bytes */
#define VP_ALLOC 04000		/* AFP2.2: allocation block size */
#define VP_ALL  (07777)

#define VOL_VAR_DIRID 0x03	/* volume has variable dirids */
#define VOL_FIXED_DIRID 0x02	/* volume has fixed dirids */
#define VOL_FLAT 0x01		/* volume is flat file systems */

/* DirParms - Directory Parameters Bitmap */
/* Bit on signifies item is present in packed parameters block */

#define DP_ATTR	 0x0001		/* (LSB) attributes */
#define DP_PDIR	 0x0002		/* parent directory id */
#define DP_CDATE 0x0004		/* creation date */
#define DP_MDATE 0x0008		/* modify date */
#define DP_BDATE 0x0010		/* backup date */
#define DP_FINFO 0x0020		/* finder info */
#define DP_LNAME 0x0040		/* long name flag */
#define DP_SNAME 0x0080		/* short name flag */
#define DP_DIRID 0x0100		/* directory id */
#define DP_CHILD 0x0200		/* number of directory offspring */
#define DP_CRTID 0x0400		/* creator id */
#define DP_GRPID 0x0800		/* group id */
#define DP_ACCES 0x1000		/* access bits */
#define DP_PDOS  0x2000		/* AFP2.0: prodos file type */

/* list of all bitmap items aufs can fill in or set */

#ifdef SHORT_NAMES
#define DP_AUFS_VALID (DP_ATTR|DP_PDIR|DP_CDATE|DP_MDATE|DP_BDATE|DP_FINFO|\
		       DP_SNAME|DP_LNAME|DP_DIRID|DP_CHILD|DP_CRTID|DP_GRPID|\
	               DP_ACCES|DP_PDOS)
#else SHORT_NAMES
#define DP_AUFS_VALID (DP_ATTR|DP_PDIR|DP_CDATE|DP_MDATE|DP_BDATE|DP_FINFO|\
		DP_LNAME|DP_DIRID|DP_CHILD|DP_CRTID|DP_GRPID|DP_ACCES|DP_PDOS)
#endif SHORT_NAMES

#define DP_ALL  (0x3777)	/* all bits */

/* File Params */

#define FP_ATTR  0x0001		/* attributes: */
#define  FPA_INV  0x001		/*  invisible */
#define  FPA_MUS  0x002		/*  multi-user */
#define  FPA_SYS  0x004		/*  AFP2.0: System */
#define  FPA_DAO  0x008		/*  DAlreadyOpen */
#define  FPA_RAO  0x010		/*  RAlreadyOpen */
#define  FPA_WRI  0x020		/*  Write Inhibit */
#define  FPA_BKUP 0x040		/*  AFP2.0: backup needed */
#define  FPA_RNI  0x080		/*  AFP2.0: rename inhibit */
#define  FPA_DEI  0x100		/*  AFP2.0: delete inhibit */
#define  FPA_CPR  0x400		/*  AFP2.0: copy protect */
#define  FPA_SCL  0x8000	/*  set/clear  */
#define  FPA_MASK1	(FPA_INV|FPA_MUS|FPA_DAO|FPA_RAO|FPA_WRI) /* AFP 1.1 */
#define FP_PDIR  0x0002		/* parent directory id */
#define FP_CDATE 0x0004		/* creation date */
#define FP_MDATE 0x0008		/* modification date */
#define FP_BDATE 0x0010		/* backup date */
#define FP_FINFO 0x0020		/* finder info */
#define FP_LNAME 0x0040		/* long name */
#define FP_SNAME 0x0080		/* short name */
#define FP_FILNO 0x0100		/* file number */
#define FP_DFLEN 0x0200		/* data fork length */
#define FP_RFLEN 0x0400		/* resource fork length */
#define FP_PDOS  0x2000		/* AFP2.0: prodos file type */
#ifdef SHORT_NAMES
#define FP_AUFS_VALID (FP_ATTR|FP_PDIR|FP_CDATE|FP_MDATE|FP_BDATE|FP_FINFO|\
		       FP_SNAME|FP_LNAME|FP_FILNO|FP_DFLEN|FP_RFLEN|FP_PDOS)
#else SHORT_NAMES
#define FP_AUFS_VALID (FP_ATTR|FP_PDIR|FP_CDATE|FP_MDATE|FP_BDATE|FP_FINFO|\
		       FP_LNAME|FP_FILNO|FP_DFLEN|FP_RFLEN|FP_PDOS)
#endif SHORT_NAMES

/* Get User Info bitmap items */

#define UIP_USERID 0x1		/* user id (dword) */
#define UIP_PRIMARY_GID 0x2	/* primary group (dword) */

#define AFSTYPE "AFPServer"	/* NBP type for AFS */

#define ASIP_PORT 548		/* AppleShare over TCP/IP well-known port */

char *afperr();			/* in afperr.c */
