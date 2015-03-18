/*
 * $Author: djh $ $Date: 1996/06/19 04:04:29 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afps.h,v 2.11 1996/06/19 04:04:29 djh Rel djh $
 * $Revision: 2.11 $
 *
 */

/*
 * afps.h - Appletalk Filing Protocol Common Server Definitions
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986,1987,1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  April 1987     Schilit	Created.
 *
 */

extern int statflg;		/* external stats flag */
extern int afp_dbug;		/* the debug flags */

#define uSEC 1000000		/* microseconds in a second */
typedef OSErr (*PFE)();		/* function returning OSErr */
/* volume bit map - limited to long by macros below */
typedef long VolBitMap;		/* longer than long is a bad idea?*/   

/* All File System Calls */

OSErr FPByteRangeLock(), FPCloseVol(), FPCloseDir(), FPCloseFork();
OSErr FPCopyFile(), FPCreateDir(), FPCreateFile(), FPDelete();
OSErr FPEnumerate(), FPFlush(), FPFlushFork(), FPGetForkParms();
OSErr FPGetSrvrInfo(), FPGetSrvrParms(), FPGetVolParms(), FPLogin();
OSErr FPLoginCont(), FPLogout(), FPMapID(), FPMapName();
OSErr FPMove(), FPOpenVol(), FPOpenDir(), FPOpenFork();
OSErr FPRead(), FPRename(), FPSetDirParms(), FPSetFileParms();
OSErr FPSetForkParms(), FPSetVolParms(), FPWrite(), FPGetFileDirParms();
OSErr FPSetFileDirParms(), FPOpenDT(), FPCloseDT(), FPGetIcon();
OSErr FPGetIconInfo(), FPAddAPPL(), FPRmvAPPL(), FPGetAPPL();
OSErr FPAddComment(), FPRmvComment(), FPGetComment(), FPAddIcon();
OSErr FPNop();
/* afp2.0 */
OSErr FPChgPasswd(), FPGetUserInfo();
/* afp2.1 */
OSErr FPExchangeFiles(), FPGetSrvrMsg();
OSErr FPCreateID(), FPDeleteID(), FPResolveID();

/* abafpserver.c */

char *SrvrInfo();
OSErr SrvrRegister();

#define F_RSRC 01		/* resource file */
#define F_DATA 02		/* data file */
#define F_FNDR 03		/* finder info file */

#define DBG_FILE 00001		/* debug file routines */
#define DBG_FORK 00002		/* debug fork routines */
#define DBG_SRVR 00004		/* debug server routines */
#define DBG_VOLS 00010		/* debug volume routines */
#define DBG_OSIN 00020		/* debug os interface routines */
#define DBG_DIRS 00040		/* debug directory routine */
#define DBG_DESK 00100		/* debug desktop routines */
#define DBG_DEBG 00200		/* in debug */
#define DBG_UNIX 00400		/* debug unix routines */
#define DBG_ENUM 01000		/* debug directory enumerations */

#define DBG_ALL (DBG_FILE|DBG_FORK|DBG_SRVR|DBG_VOLS| \
		 DBG_OSIN|DBG_DIRS|DBG_DESK|DBG_DEBG| \
		 DBG_UNIX|DBG_ENUM)


#define DBOSI (afp_dbug & DBG_OSIN)
#define DBFRK (afp_dbug & DBG_FORK)
#define DBSRV (afp_dbug & DBG_SRVR)
#define DBDIR (afp_dbug & DBG_DIRS)
#define DBVOL (afp_dbug & DBG_VOLS)
#define DBFIL (afp_dbug & DBG_FILE)
#define DBDSK (afp_dbug & DBG_DESK)
#define DBDEB (afp_dbug & DBG_DEBG)
#define DBUNX (afp_dbug & DBG_UNIX)
#define DBENU (afp_dbug & DBG_ENUM)

/* afpdid.c */

typedef struct idir {		/* local directory info (internal) */
  char *name;			/* the directory name */
  long hash;			/* hash of the directory name */
  struct idir *next;		/* ptr to next at same level */
  struct idir *subs;		/* ptr to children */
  struct idir *pdir;		/* ptr to parent */
  unsigned int modified;	/* count of times modified */
  VolBitMap volbm;		/* vols that contain this dir */
  sdword edirid;		/* external dirid */
  int eceidx;			/* index into enum cache */
  int flags;			/* any flags */
#define DID_RESOURCE 0x1	/* remember there is resource subdir */
#define DID_FINDERINFO 0x2	/* remember there is finderinfo subdir */
#define DID_VALID 0x4		/* flags are valid? (always) */
#define DID_DATA 0x8		/* data - means dir. exists :-) */
#define DID_SYMLINKS 0x30	/* number of symbolic links in list */
				/* allow up to 3 (0 means none, 1-3 in */
				/* 3 bit field */
#define DID_MAXSYMLINKS 3
#define DID_SYMLINKS_SHIFT 4	/* shift from right */
} IDir, *IDirP;
 
#define NILDIR ((IDirP) 0)	/* a null directory pointer */

#define EROOTD	02		/* external ID of volumes root directory */
#define EPROOTD 01		/* external ID of parent of root */

IDirP EtoIdirid();		/* external to internal translation */
sdword ItoEdirid();		/* internal to external translation */
IDirP Idirid();			/* return internal dirid given path */
IDirP Idndirid();		/* return internal dirid given dirid, name */
IDirP Ipdirid();		/* return parent of dirid given dirid */
char *pathstr();		/* return path given dirid */
void InitDID();			/* initialize directory id mechanism */
OSErr EtoIfile();		/* convert file names */
void Idmove();			/* rename a directory */
byte *ItoEName();		/* unix to mac name */
OSErr EtoIName();		/* mac to unix name */
int ENameLen();			/* length of mac name (arg is unix name) */
void EModified();		/* mark an entry as having been modified */
 
/* afpvols.c */


#define VOLFILE "afpvols"	/* name of the file containing volume info */
#define VOLFILE1 ".afpvols"	/* alternate afpvols file */
#define MAXLLEN 200		/* max line length in VOLFILE file */

/*
 * We use a bitmap to record information about groupings of volumes
 * The way we did things we are limited to sizeof(long)*8 bits in size
 * (well, if you have a scalar type larger than long then you are
 * limited to the number of bits in that type).
 *
 * Note however, that the logic behind things is such that you should
 * be able to recode the macros to arbritrary sizes in the future
 * without any problem, but for now just say we got lazy (actually the
 * real issue was that the code gets a little long the other way...)
 *
*/
#define MAXVOLS (sizeof(VolBitMap)*8)	/* max number of volumes per user */

#define V_BITSET(item,bit) (item) |= (1<<(bit))
#define V_BITTST(item,bit) ((item) & (1<<(bit)))
#define V_BITCLR(item,bit) (item) &= ~(1<<(bit))
#define V_BITOR(res, i1, i2) (res) = ((i1) | (i2))
#define V_ZERO(item) bzero(&(item), sizeof(VolBitMap))
#define V_COPY(d,s) (d) = (s)

void VInit();			/* Initialize user volume structure */
IDirP VolRootD();		/* return root dirid for a volume */
word ItoEVolid();
int EtoIVolid();

/* afpdt.c */

sdword CurTime();

/* afpvols.c */

void VolModified();

/* afpcmd.c */

void PackWord();	
void PackDWord();

/* afpdt.c */

void InitIconCache();

/* afposenum.c */

#define NOECIDX (-1)

#define NECSIZE 100		/* size of the enum cache */

void ECacheInit();
char *OSEnumGet();
int  OSEnumInit();
void OSEnumDone();
#ifdef NOCASEMATCH
void noCaseFind();
void noCaseMatch();
#endif NOCASEMATCH

/* Portable library functions */

#if (!(defined(AIX) || defined(hpux) || defined(SOLARIS)))
char *malloc(),*strcpy(),*strcat(),*realloc();
#endif  /* AIX || hpux || SOLARIS */

/* defined here so that enumerate can skip these entries when reading dirs */

#define DESKTOP_ICON ".IDeskTop"
#define DESKTOP_APPL ".ADeskTop"

#define RFDIRFN ".resource"		/* subdir holding resource forks */
#define FIDIRFN ".finderinfo"		/* subdir holding finder info */

#define RFDIR "/.resource"
#define FIDIR "/.finderinfo"

#define DIRRF ".resource/"
#define DIRFI ".finderinfo/"
#define DIRRFLEN 10
#define DIRFILEN 12

/* Finder info bits */
#define DEFCMNT "This is a Unix\252 created file."
#define DEFCMNTZ (sizeof(DEFCMNT)-1)
#ifdef notdef
#ifndef DEFFNDR
# define DEFFNDR "TEXTunix"	/* type, creator */
#endif
#define DEFFNDRZ (sizeof(DEFFNDR)-1) /* size of above */
#endif

#define DEFATTR (0x00)		/* get rid of "locked" bit */
#define DEFFCREATOR "unix"
#define DEFFTYPE "TEXT"

#define ENEWLINE '\r'			/* external (mac) new line char */
#define INEWLINE '\n'			/* internal (unix) new line char */

#ifdef USR_FILE_TYPES
#define ELDQUOTE 0xd2			/* external (mac) left double quote */
#define ERDQUOTE 0xd3			/* external (mac) right double quote */
#define ELSQUOTE 0xd4			/* external (mac) left single quote */
#define ERSQUOTE 0xd5			/* external (mac) right single quote */
#define IDQUOTE '\"'			/* internal (unix) double quote */
#define ISQUOTE '\''			/* internal (unix) single quote */
#define TYPFILE "afpfile"
#define TYPFILE1 ".afpfile"
#endif USR_FILE_TYPES

/* used internally */
#define AFPVersionUnknown 0
#define AFPVersion1DOT0 100
#define AFPVersion1DOT1 110
#define AFPVersion2DOT0 200
#define AFPVersion2DOT1 210
#define AFPVersion2DOT2 220

#ifdef APPLICATION_MANAGER
struct flist {
  char *filename;			/* full pathname to resource fork */
  int protected;			/* we don't want it to be copied */
  short incarnations;			/* can be opened this many times */
  struct flist *next;			/* this is a sorted linked list */
};
#endif APPLICATION_MANAGER
