/*
 * $Author: djh $ $Date: 1996/09/10 14:30:03 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpos.c,v 2.80 1996/09/10 14:30:03 djh Rel djh $
 * $Revision: 2.80 $
 *
 */

/*
 * afpos.c - Appletalk Filing Protocol OS Interface. 
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *  March 1988     CCKIM	cleanup
 *  December 1990  djh 		tidy up for AFP 2.0
 *  May   1991     ber,jlm	accept a part of the gcos or login name
 *                              (PERMISSIVE_USER_NAME)
 *  March 1993     jbh/rns	Add support for Shadow Password file
 *  July  1993     pabe/jbh	Tidy up Shadow Password support; fix
 *				combination of same with PERMISSIVE_USER_NAME
 *
 */

/*
 * POSSIBLE DEFINES:
 *
 *  NONLXLATE
 *   Define to turn off translation of \n to \r on line at a time
 *    reads
 *  USECHOWN
 *   System allows user to use chown to give away file ownership
 * Following are used to get volume information
 *  USEGETMNT - DECs getmnt call.  Works for either Ultrix 1.2, 2.0 or
 *   2.2 (to be verified).  Call differs from 2.0 to 2.2
 *  USEQUOTA - base volume information on quota for user on file
 *   system if it exists (messed up by symlinks off the structure).
 *   This is for the "Melbourne" quota system as usually found in BSD
 *   systems.
 *  USESUNQUOTA - running with sun quota system.  Basically, just turns on 
 *   emulation of the Melbourne quota call
 *  USEBSDQUOTA - "new" BSD quota which uses the quotactl call and
 *   provides quotas for both users and groups
 *  USEUSTAT - not recommended.  returns information about file, but
 *   information doesn't tell us how much space is there -- only
 *   how much is free and we need both
 *  USESTATFS - statfs is the Sun NFS solution to volume information.
 *   (has modified call arguments and structure elements under SGI IRIX).
 *
 *  PERMISSIVE_USER_NAME - let the Chooser name be from the gcos field
 *  SHADOW_PASSWD - enable support of shadow password files
 *
 *  aux has a couple of "ifdefs".
 *   - one to set full BSD compatibility
 *   - one to protect against rename("file1","file1"): it will
 *     incorrectly unlink "file1" (period - nothing left afterwards)
 *
 *  OTHER: GGTYPE
 *   Some versions of unix return a gid_t array in getgroups instead of
 *    an int array.  For those, define GGTYPE to gid_t.  In particular,
 *    this is a problem with (at least some version of) "MORE/BSD"
 *    from Mt. Xinu. 
 *
 * System V defines
 *  NOLSTAT - no lstat call - don't try to figure out things with symlinks
 *  USERAND - use sysv rand call not bsd random
 *
 */

#if (defined(__386BSD__) || defined(__FreeBSD__))
#define __BSD_4_4__
#endif /* __386BSD__ */

#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>		/* assume included by param.h */
#endif  _TYPES
#include <sys/file.h>
#include <sys/stat.h>
#include <netat/appletalk.h>
#include <netinet/in.h>
#include <sys/time.h>

#ifdef aux
# include <compat.h>
#endif aux

#ifdef USEDIRENT
#include <dirent.h>
#else  USEDIRENT
# ifdef xenix5
# include <sys/ndir.h>
# else xenix5
#  ifndef drsnx
#   include <sys/dir.h>
#  endif drsnx
# endif xenix5
#endif USEDIRENT

#ifdef SHADOW_PASSWD
#include <shadow.h>
#endif SHADOW_PASSWD

#ifdef gould
# define USESUNQUOTA
#endif gould

#ifdef USESUNQUOTA
# ifndef USEQUOTA
#  define USEQUOTA
# endif  USEQUOTA
#endif USESUNQUOTA

#if defined (APPLICATION_MANAGER) || defined (DENYREADWRITE)
# define NEEDFCNTLDOTH
#endif APPLICATION_MANAGER|DENYREADWRITE

#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
# ifdef apollo
#  include <netat/fcntldomv.h>
# endif apollo
#endif NEEDFCNTLDOTH

#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef USEUSTAT
# include <ustat.h>
#endif USEUSTAT

#ifdef USESTATFS
# ifdef SOLARIS
#  include <sys/statvfs.h>
# else  /* SOLARIS */
#  if defined(__BSD_4_4__) || defined(__osf__)
#   include <sys/param.h>
#   include <sys/mount.h>
#  else /* __BSD_4_4__ || __osf__ */
#   if defined(sgi) || defined(apollo)
#    include <sys/statfs.h>
#   else  /* sgi || apollo */
#    include <sys/vfs.h>
#   endif /* sgi || apollo */
#  endif /* __BSD_4_4__ || __osf__ */
# endif /* SOLARIS */
#endif /* USESTATFS */

#ifdef AIX
#include <sys/statfs.h>
#endif AIX

#ifdef __BSD_4_4__
#include <unistd.h>
#endif /* __BSD_4_4__ */

#ifdef linux
#include <unistd.h>
#endif /* linux */

#ifdef NeXT
#undef USEQUOTA
#undef USESUNQUOTA
#endif NeXT

#ifdef USEBSDQUOTA
# include <fstab.h>
# include <ufs/quota.h>
#endif USEBSDQUOTA

#ifdef USEQUOTA
# ifdef SOLARIS
#  undef USESUNQUOTA
#  include <sys/fs/ufs_quota.h>
# else SOLARIS
#  ifndef USESUNQUOTA
#   include <sys/quota.h>
/*
 * NOTE: If there is not sys/quota.h and there is a ufs/quota.h
 * then you should probably define SUN_QUOTA -- especially if your
 * NFS is based on the sun model
 *
 */
#  else USESUNQUOTA
#   include <mntent.h>
#   include <ufs/quota.h>
#  endif USESUNQUOTA
# endif SOLARIS
# ifndef Q_GETDLIM
#  ifdef Q_GETQUOTA
#   define Q_GETDLIM Q_GETQUOTA
#  else  Q_GETQUOTA
    /*
     * Error: You have turned on quotas and aren't using the
     * bsd or sun quota system.
     *
     */
#  endif Q_GETQUOTA
# endif  Q_GETDLIM
#endif USEQUOTA

/* assumes that ultrix 1.1 doesn't have getmnt or if it does, then it */
/* has limits.h and uname */
#ifdef USEGETMNT
# include <sys/mount.h>
/* the following assumes ultrix 1.2 or above */
/* well, do you really think dec would license their code to another */
/* vendor :-) */
# include <limits.h>
# include <sys/utsname.h>
#endif USEGETMNT

#ifdef drsnx
# ifdef USESTATFS
#  undef USESTATFS	/* ICL DRS/NX statfs() is a little different */
# endif USESTATFS
#endif drsnx

#ifdef SOLARIS
# include <unistd.h>
# define NGROUPS NGROUPS_MAX_DEFAULT
#endif SOLARIS

#include <netat/afp.h>
#include <netat/afpcmd.h>		/* flags should be in misc  */
#include "afps.h"			/* common includes */
#include "afpvols.h"
#include "afppasswd.h"			/* in case we are using privates */
#include "afposncs.h"
#include "afpgc.h"

#ifdef SIZESERVER
#include <setjmp.h>
#include <signal.h>
#include <sys/socket.h>
#include "sizeserver.h"
#endif SIZESERVER

#ifdef PERMISSIVE_USER_NAME
#include <ctype.h>
#endif PERMISSIVE_USER_NAME

#ifdef ULTRIX_SECURITY
#include <sys/svcinfo.h>
#include <auth.h>
#endif ULTRIX_SECURITY

#ifdef DIGITAL_UNIX_SECURITY
#include <sys/types.h>
#include <sys/security.h>
#include <prot.h>
#endif DIGITAL_UNIX_SECURITY

#ifdef LOG_WTMP
# if defined(sgi) || defined(SOLARIS)
#  define LOG_WTMPX
# endif /* sgi || SOLARIS */
# ifdef LOG_WTMPX
#  include <utmpx.h>
# else  /* LOG_WTMPX */
#  include <utmp.h>
# endif /* LOG_WTMPX */
#endif /* LOG_WTMP */

#ifdef DISTRIB_PASSWDS
#include <netat/afppass.h>
#endif /* DISTRIB_PASSWDS */

#ifdef MAXBSIZE
# define IOBSIZE MAXBSIZE	/* set to max buf entry size by if there */
#else  MAXBSIZE
# ifdef BLKDEV_IOSIZE
#  define IOBSIZE BLKDEV_IOSIZE	/* set to std block device read size */
# else  BLKDEV_IOSIZE
#  define IOBSIZE BUFSIZ	/* use stdio bufsiz */
# endif BLKDEV_IOSIZE
#endif MAXBSIZE

#define NILPWD ((struct passwd *) 0)

#ifdef SHADOW_PASSWD
#define NILSPWD ((struct spwd *) 0)
#endif SHADOW_PASSWD

/* macro to test for directory file */

#ifndef S_ISDIR
#define	S_ISDIR(mode)	(((mode) & S_IFMT) == S_IFDIR)
#endif  S_ISDIR

#define E_IOWN 0x80000000	/* owner of file */

#define I_SETUID 04000		/* internal set user ID */
#define I_SETGID 02000		/* internal set group ID */

#define I_RD 04			/* Internal (Unix) bits for read */
#define I_WR 02			/*  write */
#define I_EX 01			/*  execute (search) */

#define E_RD 02			/* External bits for read */
#define E_WR 04			/*  write */
#define E_SR 01			/*  search */

#define IS_OWNER 6		/* internal owner shift amount */
#define IS_GROUP 3		/* internal group shift amount */
#define IS_WORLD 0		/* internal world shift amount */

#define ES_USER 24		/* external user shift amount */
				/*  which is a calculated access based on: */
#define ES_WORLD 16		/* external world shift amount */
#define ES_GROUP 8		/* external group shift amount */
#define ES_OWNER 0		/* external owner shift amount */

/* Mask search (is unix execute) bits for world, group and owner */

#define IS_EX_WGO ((I_EX << IS_OWNER) | \
		   (I_EX << IS_GROUP) | \
		   (I_EX << IS_WORLD))

private char *usrnam,*usrdir;
private int usruid;
private int usrgid;
private int ngroups;

#ifndef GGTYPE
# ifdef SOLARIS
#  define GGTYPE gid_t
# else  SOLARIS
#  define GGTYPE int
# endif SOLARIS
#endif  GGTYPE

private GGTYPE groups[NGROUPS+1];

#ifdef USEGETMNT
# ifndef NOSTAT_ONE
/* no big deal if this changes, it just means you can't compile under */
/* ultrix 2.0/1.2 for ultrix 2.2 (you probably can't for 1.2 anyway) */
/* WARNING: not 100% sure that there aren't other problems preventing */
/* a compile from ultrix 2.0 from working on 2.2 */
#  define NOSTAT_ONE 4		/* as of ultrix 2.2 */
#  define ISOLDGETMNT 1
# else  NOSTAT_ONE
#  define ISOLDGETMNT 0
# endif NOSTAT_ONE
private int oldgetmnt = ISOLDGETMNT; /* use new or old format getmnt */
				/* default is based on compiling system */
# ifndef NUMGETMNTBUF
#  define NUMGETMNTBUF 8	/* 8 before, let's not be conservative */
				/* because on systems with many file */
				/* systems we will get killed with 8 */
				/* Back off!!!!! struct fs_data is huge */
# endif NUMGETMNTBUF
#endif USEGETMNT

import int errno;

/*
 * AFPOS functions
 *
 *  Generally, a name like: OSXxxxx means that it is a "primary" function
 *   that accomplishes a afp command (not always though).
 *
*/

export int OSEnable();
export void tellaboutos();
export OSErr OSMapID();
export OSErr OSMapName();
export OSErr OSDelete();
private OSErr os_rmdir();
private OSErr os_delete();
export OSErr OSRename();
export OSErr OSMove();
private OSErr os_move();
export OSErr OSFlush();
export OSErr OSFlushFork();
export OSErr OSClose();
export OSErr OSRead();
export OSErr OSWrite();
export OSErr OSCreateDir();
private OSErr os_mkdir();
export OSErr OSCreateDir();
export OSErr OSFileDirInfo();
export OSErr OSDirInfo();	/* from old spec */
export OSErr OSFileInfo();	/* from old spec */
export void OSValidateDIDDirInfo();
export OSErr OSFileExists();
export OSErr OSSetFileDirParms();
export OSErr OSSetFileParms();
export OSErr OSSetDirParms();
export OSErr OSSetForklen();
export OSErr OSGetSrvrMsg();
private OSErr os_chmod();
private void os_change_all();
private u_short EtoIPriv();
private int OSInGroup();
private u_short EtoIAccess();
private dword ItoEPriv();
private dword ItoEAccess();
export OSErr OSAccess();
export OSErr OSVolInfo();
#ifdef USEGETMNT
private OSErr ultrix_volinfo();
#endif USEGETMNT
export OSErr OSCopyFile();
private OSErr os_copy();
export OSErr OSCreateFile();
export OSErr OSOpenFile();
export boolean setguestid();
export boolean setpasswdfile();
export OSErr OSLoginRand();
export OSErr OSLogin();
export word OSRandom();
export sdword CurTime();	/* current time in internal form */
export char *tilde();		/* tilde expansion */
export char *logdir();		/* user login directory */
export int guestlogin;		/* session is a guest login */
private OSErr unix_rmdir();
private OSErr unix_unlink();
private OSErr unix_rename();
#ifdef XDEV_RENAME
private OSErr xdev_rename();	/* rename across devices */
#endif XDEV_RENAME
private OSErr unix_open();
private OSErr unix_close();
private OSErr unix_mkdir();
private OSErr unix_create();
private OSErr unix_createo();
private OSErr unix_chown();
private OSErr unix_chmod();
private OSErr unix_stat();
private OSErr ItoEErr();
private int filemode();
private char *syserr();

#ifdef SIZESERVER
private void getvolsize();
#endif SIZESERVER

#ifdef LWSRV_AUFS_SECURITY
char *bin, *tempbin ;
#endif LWSRV_AUFS_SECURITY

#ifdef SHADOW_PASSWD
int shadow_flag;
#endif SHADOW_PASSWD

#ifdef DENYREADWRITE
struct accessMode {
  struct accessMode *next;
  char path[MAXPATHLEN];
  int mode;
  int fd;
};
struct accessMode *accessMQueue = (struct accessMode *)NULL;
#endif DENYREADWRITE

/*
 * Enable OS Dependent functions
 *
 * For now: under AUX, set full BSD compatibility
 * under sun quota systems, build a mount table for use in quota call
 *
*/
export int
OSEnable()
{
#ifdef USEGETMNT
  struct utsname unames;
#endif USEGETMNT

#ifdef USEGETMNT
  if (uname(&unames) >= 0) {
    /* don't think getmnt was available in ultrix 1.1.  If it was */
    /* then we assume it had uname too */
    if (strcmp(unames.sysname, "ULTRIX-32") == 0) {
      if (strcmp(unames.release, "2.0") == 0 ||
	  strcmp(unames.release, "1.2") == 0 ||
	  strcmp(unames.release, "1.1") == 0) {
	oldgetmnt = TRUE;
      } else oldgetmnt = FALSE;
    }
  }
#endif USEGETMNT
#ifdef aux
/* ensure reliable signals, etc */
#define BSDCOMPAT COMPAT_BSDNBIO|COMPAT_BSDPROT|COMPAT_BSDSIGNALS|\
COMPAT_BSDTTY|COMPAT_EXEC|COMPAT_SYSCALLS
  setcompat(BSDCOMPAT);
#endif aux
#ifdef USESUNQUOTA
  build_mount_table();
#endif USESUNQUOTA
#ifdef SHADOW_PASSWD
  shadow_flag = (access(SHADOW, F_OK) == 0);
#endif SHADOW_PASSWD
}

void
tellaboutos()
{
  int haveflock;
  int havelockf;

  logit(0,"CONFIGURATION");
  getlockinfo(&haveflock, &havelockf);
  if (haveflock)
    logit(0," Configured: FLOCK");
  if (havelockf)
    logit(0," Configured: LOCKF");
#ifdef USEUSTAT
  logit(0," Configured: Volume space information: ustat");
#endif USEUSTAT
#ifdef USESTATFS
  logit(0," Configured: Volume space information: stat[v]fs");
#endif USESTATFS
#ifdef USEGETMNT
  logit(0," Configured: Volume space information: getmnt");
  if (oldgetmnt)
    logit(0,"   using old style (Ultrix 1.2, 2.0) getmnt");
#endif USEGETMNT

#ifdef USEQUOTA
# if defined(USESUNQUOTA) || defined(SOLARIS)
  logit(0," Configured: SUN quota system");
# else  /* USESUNQUOTA || SOLARIS */
#  ifdef USEBSDQUOTA
  logit(0," Configured: New BSD quota system");
#  else  /* USEBSDQUOTA */
  logit(0," Configured: Melbourne (BSD) quota system");
#  endif /* USEBSDQUOTA */
# endif /* USESUNQUOTA || SOLARIS */
#endif /* USEQUOTA */

#ifdef USECHOWN
  logit(0," Configured: chown: system allows one to give away ownership of files");
#endif USECHOWN
  if (os_issmartunixfi())
    logit(0," Configured: reading unknown unix files to get finder information");
  logit(0," Configured translation tables are:");
  ncs_table_dump();
#ifdef DISTRIB_PASSWDS
  logit(0," Configured: Using Distributed Passwords for authentication");
#endif DISTRIB_PASSWDS
}

/*
 * OwnerID = 0 means that the folder is "unowned" or owned by
 * <any user>.  The owner bit of the User Rights summary is always 
 * set for such a folder.
 *
 * GroupID = 0 means that the folder has no group affiliation; hence
 * the group's access privileges (R, W, S) are ignored for such a
 * folder.
 *
*/

#define NOID (-1)	/* internal group/user id for <any user> */

/*
 * Make external and internal ids differ by one.  Then 1 (administrator)
 * maps to 0 (root), and 0 (any user or group) maps to -1.  Thus the
 * AFP client thinks user ids are 1 higher than what the server (and the
 * unix machine) use internally.
 *
 */

#define ItoEID(iid) ((sdword) (iid + 1))
#define EtoIID(eid) ((int) (eid - 1))

/*
 *
 * OSErr OSMapID(byte fcn,char *name,dword id)
 *
 *
 * This function is used to map a creator ID to a creator name, or
 * a group ID to a group name.
 *
 * The creator name/id is identical to the user name and UID under
 * Unix.
 *
 * Inputs:
 *   fcn	MapID_C for creator, MapID_G for group.
 *   id		The id to be mapped, either group ID or creator ID.	
 * 
 * Outputs:
 *  OSErr	Function result. 
 *  name	name corresponding to input ID.
 *
 *
 */

export OSErr
OSMapID(fcn,name,idd)
byte fcn;
char *name;
sdword idd;				/* 4 bytes */
{
  struct passwd *pwd;
  struct group *grp;
  int id = EtoIID(idd);			/* convert to internal id */

  if (DBOSI)
    printf("OSMapID fcn=%s id=%d\n",
	   ((fcn == MapID_C) ? "Creator" : "Group"),id);

  if (idd == 0) {
    name[0] = '\0';
    return(noErr);
  }

  switch (fcn) {
  case MapID_C:
    pwd = getpwuid(id);
    if (pwd == NULL) 
      return(aeItemNotFound);
    strcpy(name,pwd->pw_name);
    break;
	  
  case MapID_G:
    grp = getgrgid(id);
    if (grp == NULL)
      return(aeItemNotFound);
    strcpy(name,grp->gr_name);
    break;
  default:
    return(aeParamErr);
  }
  return(noErr);
}

/*
 * OSErr OSMapName(byte fcn,char *name,sdword *id);
 *
 * This call is used to map a creator name to a creator ID or
 * a group name to a group id.
 *
 * The creator name/id is identical to the user name and UID under
 * Unix.
 *
 * Inputs:
 *  fcn		MapName_C for creator, MapName_G for group.
 *  name	Item to be mapped, either creator name or group name.
 *
 * Outputs:
 *  OSErr	Function result.
 *  id		ID corresponding to input name.
 *
 * 
 */

OSErr
OSMapName(fcn,name,eid)
byte fcn;
char *name;
sdword *eid;				/* 4 bytes */
{
  struct passwd *pwd;
  struct group *grp;
  int id;

  if (DBOSI)
    printf("OSMapName fcn=%s name=%s\n",
	   (fcn == MapName_C) ? "Creator" : "Group",name);

  if (name[0] == '\0') {
    *eid = 0;
    return(noErr);
  }

  switch (fcn) {
  case MapName_C:
    pwd = getpwnam(name); 
    if (pwd == NULL) 
      return(aeItemNotFound);
    id = pwd->pw_uid;			/* return user ID */
    break;

  case MapName_G:
    grp = getgrnam(name);		/* get group entry */
    if (grp == NULL)
      return(aeItemNotFound);
    id = grp->gr_gid;			/* return group ID */
    break;
  }
  *eid = ItoEID(id);			/* convert to external */
  return(noErr);
}

/*
 * return information about a particular user id.
 * if doself is true then return information about logged in user
 * AFP2.0
 *
 */
OSGetUserInfo(doself, userid, guirp)
boolean doself;
dword userid;
GetUserInfoReplyPkt *guirp;
{
  int bm = guirp->guir_bitmap;
  int usr = usruid;
  int grp = usrgid;
  struct passwd *pwd;

  if (!doself) {
    if ((pwd = (struct passwd *)getpwuid(EtoIID(userid))) == NULL)
      return(aeItemNotFound);
    usr = pwd->pw_uid;
    grp = pwd->pw_gid;
  }
  /* Convert to external form */
  guirp->guir_userid = ItoEID(usr);
  guirp->guir_pgroup = ItoEID(grp);
  return(noErr);
}

/*
 * get the server or login message from the specified file
 * AFP2.1
 *
 */

#define LOGINMSG	0
#define SERVERMSG	1

OSErr
OSGetSrvrMsg(typ, msg)
word typ;
byte *msg;
{
  int fd, i, len;
  char *msgfile;
  extern char *motdfile;
  extern char *messagefile;

  msg[0] = '\0';

  if (typ != LOGINMSG && typ != SERVERMSG)
    return(noErr);

  msgfile = (typ == LOGINMSG) ? motdfile : messagefile;

  if (msgfile == NULL)
    return(noErr);

  if ((fd = open(msgfile, O_RDONLY, 0644)) >= 0) {
    if ((len = read(fd, (char *)msg, SRVRMSGLEN-1)) >= 0) {
      if (len == SRVRMSGLEN-1)
	msg[len-1] = 0xc9; /* ... */
      msg[len] = '\0';
      for (i = 0; i < len; i++)
	if (msg[i] == '\n')
	  msg[i] = '\r';
    }
    close(fd);
    return(noErr);
  }

  sprintf(msg, "<no path to message file>");

  return(noErr);
}

/*
 * OSErr OSDelete(IDirP ipdir, idir ,char *file)
 *
 * OSDelete is used to delete a file or an empty directory.
 *
 * Inputs:
 *    parent directory
 *    directory id of directory (null if file)
 *    file name in parent directory
 *
 * Outputs, OSErr Function result:
 * 
 *  ParamErr		Bad path.  
 *  ObjectNotFound	Path does not point to an existing file or directory.
 *  DirNotEmpty		The directory is not empty.
 *  FileBusy		The file is open.
 *  AccessDenied	User does not have the rights (specified in AFP spec).
 *  ObjectLocked	AFP2.0: file or dir marked DeleteInhibit
 *  VolLocked		AFP2.0: the volume is ReadOnly
 *
 */
OSErr
OSDelete(ipdir,idir,file)
IDirP ipdir, idir;
char *file;
{
  int err;
  word attr;
  extern int sessvers;
  char path[MAXPATHLEN];

  OSfname(path,ipdir,file,F_DATA);	/* create data fork name */
#ifdef NOCASEMATCH
  noCaseMatch(path);
#endif NOCASEMATCH

  /* new for AFP2.0 */
  OSGetAttr(ipdir,file,&attr);
  if (attr & FPA_DEI)
    return((sessvers == AFPVersion1DOT1) ? aeAccessDenied : aeObjectLocked);

  if (DBOSI)
    printf("OSDelete file=%s\n",path);

  if (idir) {
    err = os_rmdir(path,F_FNDR);	/* remove finder dir  */
    if (err != noErr)
      return(err);
    err = os_rmdir(path,F_RSRC);	/* delete resource directory */
    if (err != noErr)
      return(err);
    err = unix_rmdir(path);		/* delete the data file */
    if (err != noErr)
      return(err);
    (void) os_delete(ipdir,file,F_FNDR);	/* delete finder fork */
    /* remove the dirid */
    FModified(ipdir, file);
    Idrdirid(ipdir, idir);		/* idir is invalid after this point */
    return(noErr);			/* and return result */
  }

  err = unix_unlink(path);		/* rid the data file */
  if (err != noErr)
    return(err);
  (void) os_delete(ipdir,file,F_RSRC);	/* delete resource fork */
  (void) os_delete(ipdir,file,F_FNDR);	/* delete finder fork */
  FModified(ipdir, file);
  return(noErr);
}

/*
 * OSErr os_rmdir(char *dir, int typ)
 *
 * Delete a finder/resource directory as specified by type which
 * is either F_FNDR or F_RSRC.
 *
 * If a simple unix_rmdir fails because the directory is not empty
 * then scan the directory for "junk" files and remove those.
 *
 * Junk files are leftovers which exist in our finder/resource
 * directory but do not exist in the data directory.  They don't
 * usually occur under normal operation but cause a headache when
 * they do since the folder can stay locked after a delete error.
 *
 */
private OSErr
os_rmdir(dir,typ)
char *dir;
int typ;
{
  char path[MAXPATHLEN];		/* resource/finder path */
  char dpath[MAXPATHLEN];		/* data file path */
  DIR *dirp;
#ifdef USEDIRENT
  struct dirent *dp, *readdir();
#else  USEDIRENT
  struct direct *dp, *readdir();
#endif USEDIRENT
  struct stat stb;
  int pl,dpl,err;

  /* create the directory path for this rmdir... either fndr or rsrc dir */

  strcpy(path,dir);			/* local copy of dir name */
  if (typ == F_RSRC)
    strcat(path,RFDIR);			/* build resource directory name */
  else
    strcat(path,FIDIR);			/* build finder directory name */

  /* try deleting the directory */

  err = unix_rmdir(path);		/* try rmdir */
  if (err == aeObjectNotFound)		/* does not exist error? */
    err = noErr;			/* then ok by use */
  if (err == noErr ||			/* deleted ok? */
      err != aeDirNotEmpty)		/* or unknown cause? */
    return(err);			/* then return now */
  
  /* directory could not be deleted because it was not empty */
  /* delete dir entries which are not in the data directory and try again */

  strcpy(dpath,dir);			/* local copy of data dir name */
  dpl = strlen(dpath);			/* find length */
  dpath[dpl++] = '/';			/*  append slash */

  dirp = opendir(path);			/* open the fndr/rsrc dir... */
  if (dirp == NULL)			/* does not exist etc? */
    return(noErr);			/* then no directory */

  pl = strlen(path);			/* set length of fndr/rsrc dir */
  path[pl++] = '/';			/* add slash for file concats */

  for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {

    if ((dp->d_name[0] == '.' && dp->d_name[1] == '\0') ||
	(dp->d_name[0] == '.' && dp->d_name[1] == '.' && dp->d_name[2] == '\0'))
      continue;				/* skip dot and dot dot */

    strcpy(dpath+dpl,dp->d_name);	/* compose name in data dir */
    if (stat(dpath,&stb) == 0) {	/* to see if it exists there */
      closedir(dirp);
      return(aeDirNotEmpty);		/* if so... dir really not empty */
    }
    
    strcpy(path+pl,dp->d_name);		/* otherwise use fndr/rsrc file name */
    if (DBOSI)
      printf("os_rmdir: cleaning %s\n",path);
    err = unix_unlink(path);		/*  and remove */
    if (err != noErr) {			/* if that failed... */
      closedir(dirp);
      return(err);			/*  then return now */
    }
  }
  closedir(dirp);			/* finished with directory  */
  path[pl-1] = '\0';			/* back to fndr/rsrc name */
  return(unix_rmdir(path));		/* try deleting now */
}

private OSErr
os_delete(pdir,file,typ)
IDirP pdir;
char *file;
int typ;
{
  char path[MAXPATHLEN];
#ifdef NOCASEMATCH
  register int i;
#endif NOCASEMATCH

  OSfname(path,pdir,file,typ);		/* build unix file name */
#ifdef NOCASEMATCH
  if((i = unix_unlink(path)) != noErr) {
    noCaseMatch(path);
    i = unix_unlink(path);
  }
  return(i);				/* do the work... */
#else NOCASEMATCH
  return(unix_unlink(path));		/* do the work... */
#endif NOCASEMATCH
}


/*
 * OSErr OSRename(IDirP pdir, char *from, char *to);
 *
 * OSRename is used to rename a file or directory.
 *
 * Inputs:
 *  pdir	parent directory id.
 *  from	name of file or directory to rename.
 *  to		new name for file or directory.
 *
 * Outputs:
 *  OSErr	Function result.
 *
 * Pass the work along to os_move, which handles the general case.
 *
 */

OSErr
OSRename(pdir,from,to)
IDirP pdir;				/* parent directory id */
char *from,*to;				/* from and to file names */
{
  return(os_move(pdir,from,pdir,to));	/* do the work */
}

/*
 * OSErr OSMove(IDirP fpdir, char *from, IDirP tpdir, char *to)
 *
 * OSMove is used to move a directory or file to another location on
 * a single volume (source and destination must be on the same volume).
 * The destination of the move is specified by provinding a pathname
 * that indicates the object's new parent directory.
 *
 * Inputs:
 *  fpdir	parent directory id of from.
 *  from	name of file or directory to rename.
 *  tpdir	parent directory id of to.
 *  to		new directory for file or directory.
 *
 * Outputs:
 *  OSErr	Function result.
 *
 * Create an internal directory id for the new directory name by
 * combining the parent dir id (tpdir), and directory name of the
 * destination (to) and call the general routine os_move().  Note
 * that the new name is the same as the old name and to specifies 
 * the destination directory.
 *
 */

export OSErr
OSMove(fpdir,from,tpdir,to)
IDirP fpdir,tpdir;			/* from and to parent dirs */
char *from;				/* from file name */
char *to;				/*  to file name is dest directory */
{
  return(os_move(fpdir, from, tpdir, to));
}

/*
 * OSErr os_move(IDirP fpdir, char *from, IDirP tpdir, char *to)
 *
 * Move the file.
 *
 */
private OSErr
os_move(fpdir,from,tpdir,to)
IDirP fpdir,tpdir;			/* from and to parent dirs  */
char *from,*to;				/* from and to file names */
{
  char f_path[MAXPATHLEN];
  char t_path[MAXPATHLEN];
  char fpath[MAXPATHLEN];
  char tpath[MAXPATHLEN];
  extern int sessvers;
  struct stat stb;
  word attr;
  int err,cerr;
  int mo;
#ifdef NOCASEMATCH
  register char *pp;
#endif NOCASEMATCH

  /* new for AFP2.0 */
  OSGetAttr(fpdir,from,&attr);
  if (attr & FPA_RNI)
    return((sessvers == AFPVersion1DOT1) ? aeAccessDenied : aeObjectLocked);

  OSfname(f_path,fpdir,from,F_DATA);	/* build data file name */
  OSfname(t_path,tpdir,to,F_DATA);	/* for from and to files */

  if (DBOSI)
    printf("OSRename from=%s, to=%s\n",f_path,t_path);

  if ((fpdir->flags & DID_FINDERINFO) && (tpdir->flags & DID_FINDERINFO) == 0)
    return(aeCantMove);
  if ((fpdir->flags & DID_RESOURCE) && (tpdir->flags & DID_RESOURCE) == 0)
    return(aeCantMove);

  /* must be able to stat destination */
#ifdef NOCASEMATCH
  if ((err = unix_stat(pp = pathstr(tpdir), &stb)) != noErr) {
    noCaseFind(pp);
    if ((err = unix_stat(pp, &stb)) != noErr)
      return(err);
  }
#else NOCASEMATCH
  if ((err = unix_stat(pathstr(tpdir), &stb)) != noErr)
    return(err);
#endif NOCASEMATCH
  mo = filemode(stb.st_mode, stb.st_uid, stb.st_gid);

#ifdef NOCASEMATCH
  if ((err = unix_stat(f_path,&stb)) != noErr) {
    noCaseFind(f_path);
    if ((err = unix_stat(f_path,&stb)) != noErr)
      return(err);
  }
  noCaseMatch(t_path);
#else NOCASEMATCH
  if ((err = unix_stat(f_path,&stb)) != noErr)
    return(err);
#endif NOCASEMATCH

  err = unix_rename(f_path,t_path);	/* give unix the args */
  if (err != noErr)			/* if an error on data files */
    return(err);			/*  then give up */

#ifdef DENYREADWRITE
  {
    struct accessMode *p = accessMQueue;

    while (p != (struct accessMode *)NULL) {
      if (strcmp(f_path, p->path) == 0) {
	strcpy(p->path, t_path);
	break;
      }
      p = p->next;
    }
  }
#endif DENYREADWRITE

  if (!S_ISDIR(stb.st_mode)) {		/* directories have no rsrc fork */
    unix_chmod(t_path, mo);		/* file: try to reset protection */
    if (fpdir->flags & DID_RESOURCE) {
      strcpy(fpath, f_path);
      strcpy(tpath, t_path);
      toResFork(fpath,from);		/* build resource file names */
      toResFork(tpath,to);
      err = unix_rename(fpath,tpath);	/* give unix a shot at it */
      /* allow non-existant resource */
      if (err != noErr && err != aeObjectNotFound) { /* error on rename? */
	if (DBOSI)
	  printf("os_rename: failed %s for %s -> %s\n",
		 afperr(err),fpath,tpath);
	cerr = unix_rename(t_path,f_path);	/* rename back to orignal */
	if (cerr != noErr && DBOSI)
	  printf("os_rename: cleanup failed\n");
	unix_chmod(t_path, stb.st_mode&0777); /* file:try to reset protection */
	return(err);
      }
    }
  }  

  if (fpdir->flags & DID_FINDERINFO) {
    strcpy(fpath, f_path);
    strcpy(tpath, t_path);
    toFinderInfo(fpath,from);		/* build finder info file names */
    toFinderInfo(tpath,to);
    err = unix_rename(fpath,tpath);	/* give unix a shot at it */
    if (err != noErr && DBOSI) {
      printf("os_rename: failed %s for %s -> %s\n", afperr(err),fpath,tpath);
    } else {
      if (!S_ISDIR(stb.st_mode))
	unix_chmod(tpath, mo);		/* file: try to reset protection */
      OSSetMacFileName(tpdir, to);
    }
  } else
    if (DBOSI)
      printf("os_rename: no finder info to rename\n");

  if (S_ISDIR(stb.st_mode))		/* if a directory then need to */
    Idmove(fpdir,from,tpdir,to);	/*  change internal structure */
  else
    FIdmove(fpdir,from,tpdir,to);

  FModified(fpdir, from);	/* does an emodified */
  /* EModified(fpdir); */
  /* don't need to mark dest file as modified since mac won't let this */
  /* happen */
  EModified(tpdir);
  return(noErr);
}

    
/*
 * OSErr OSFlush(int vid)
 *
 * OSFlush is used to flush to disk any data relating to the specified
 * volume that has been modified by the user.
 *
 * The Unix system call sync is used.  We should probably be dumping out
 * internal volume buffers instead.
 *
 * Inputs:
 *  vid		Volume ID.
 *
 * Outputs:
 *  OSErr	Function Result.	 
 *
 */

/*ARGSUSED*/
export OSErr
OSFlush(vid)
int vid;
{
  import GCHandle *fich;	/* get FinderInfo cache */

  GCFlush(fich, NILDIR);	/* flush the finderinfo of bad data */
  FlushDeskTop(vid);
/*  sync();			/* this is probably a waste */
  /* above is a waste and slows down system unnecessarily */
  return(noErr);
}

/*
 * OSErr OSFlushFork(int fd)
 *
 * Forces the write to disk of any pending file activity.
 * 
 * Really intended for buffered OSWrites.
 *
 * Inputs:
 *  fd		File descriptor.
 *  
 * Outputs:
 *  OSErr	Function Result.
 *
 */

OSErr
OSFlushFork(fd)
int fd;
{
  if (DBOSI)
    printf("OSFlushFork fd=%d\n",fd);

  return(fsync(fd) == 0 ? noErr : aeMiscErr);
}


export OSErr
OSClose(fd)
int fd;
{
  return(unix_close(fd));		/* return OSErr */
}

/*
 * From the open file referenced by fd
 * at the offset offs
 * read "reqcnt" bytes
 * using the new line mask nlmsk
 * and the newline character nlchr
 * into the buffer r for at most rl bytes
 * keeping the file position in fpos
 * translating from lf to cr if unixtomac is nonzero
 *
*/
export OSErr
OSRead(fd,offs,reqcnt,nlmsk,nlchr,r,rl,fpos,trans_table_index)
int fd;
sdword offs,reqcnt;
byte nlmsk,nlchr;
byte *r;
int *rl;
sdword *fpos;
int trans_table_index;
{
  register char c;
  int cnt,i;

  if (DBOSI)
    printf("OSRead: fd=%d, pos=%d, off=%d, req=%d\n", fd, *fpos, offs, reqcnt);

#ifdef APPLICATION_MANAGER
  {
    extern int fdplist[NOFILE];
    extern struct flist *applist;

    if (applist != NULL && fdplist[fd] == 1) {
      /* we want Finder copy protection */
      if (offs == 0 && reqcnt > 128)
	return(aeAccessDenied);
    }
  }
#endif APPLICATION_MANAGER

  /* want to probe for eof -- probably there */
  /* back off this.  If the request count is zero, then */
  /* don't tell them about EOF because zero length files */
  /* will not get xfered properly */
  if (reqcnt == 0) {
    *rl = 0;
    return(noErr);
  }

  if (offs != *fpos)
    *fpos = lseek(fd,(off_t)offs,L_SET);

#ifdef APPLICATION_MANAGER
  /*
   * we have to resort to fcntl() lock tests
   * because lockf() as used by OSTestLock()
   * returns "permission denied" if more than
   * one single byte read lock exists on fd.
   * This is probably a bug, but since we are
   * only interested in any write lock in the
   * range it doesn't matter ...
   *
   */
  {
    struct flock flck;
    extern struct flist *applist;

    if (applist != NULL) {
      flck.l_type = F_RDLCK;
      flck.l_whence = 1; /* SEEK_CUR */
#ifdef DENYREADWRITE
      flck.l_start = 4;
#else  DENYREADWRITE
      flck.l_start = 0;
#endif DENYREADWRITE
      flck.l_len = reqcnt;
      if (fcntl(fd, F_GETLK, &flck) != -1) {
        if (flck.l_type == F_WRLCK)
          return(aeLockErr);
      }
    } else {
      if (OSTestLock(fd, reqcnt) != noErr)
          return(aeLockErr);
    }
  }
#else  APPLICATION_MANAGER
  if (OSTestLock(fd, reqcnt) != noErr) {
      return(aeLockErr);
  }
#endif APPLICATION_MANAGER

  cnt = read(fd,r,reqcnt);
  if (cnt < 0) {
    printf("OSRead: Error from read %s\n",syserr());
    return(aeParamErr);
  }

  if (cnt == 0)
    return(aeEOFErr);

  *fpos += cnt;

  /* under appleshare prior to version 2.0, nlmask was either 0 or 0xff */
  /* so no anding needed to be done (either use or not).  shouldn't hurt */
  /* to do it for previous versions though */
  if (nlmsk != 0) {
#ifndef NONLXLATE
    if (nlchr == ENEWLINE) {
      for (i=0; i < cnt; i++) {
	c = r[i] & nlmsk;
	if (c == ENEWLINE || c == INEWLINE)
	  break;
      }
      if (r[i] == INEWLINE)		/* if ended on internal newline */
	r[i] = ENEWLINE;		/*  then convert to external */
    } else
      for (i=0; i < cnt && (r[i]&nlmsk) != nlchr; i++)
	/* NULL */;
#else    
    for (i=0; i < cnt && (r[i]&nlmsk) != nlchr; i++)
      /* NULL */;
#endif NONLXLATE

    if (i < cnt)			/* found it? */
      cnt = i+1;			/* yes count is position plus 1 */
  }
  if (trans_table_index >= 0) {
    if (DBOSI)
      printf("FPRead: translating to macintosh according to: %s\n",
	     ncs_tt_name(trans_table_index));
    ncs_translate(NCS_TRANS_UNIXTOMAC, trans_table_index, r, cnt);
  }

  *rl = cnt;				/* store count of bytes read */
  if (cnt < reqcnt && nlmsk == 0)	/* less than request and no nlchr */
    return(aeEOFErr);			/*  means we found eof */
  return(noErr);			/* else no error.... */
}

/*
 * Write to the open file referenced by fd
 *   the write buffer is wbuf of length wlen
 *   do the write at offset offs
 *   keeping file position in fpos
 * flg - marks whether offs relative to beginning or end of file
 * unixtomax - if non-zero translate cr to lf on writes
 *
*/
OSWrite(fd,wbuf,wlen,offs,flg,fpos,trans_table_index)
int fd;
byte *wbuf;
sdword wlen,offs;
byte flg;
sdword *fpos;
int trans_table_index;
{
  int cnt;

  if (wlen == 0)			/* zero byte request? */
    return(noErr);			/*  yes... no work */

  if (flg == 0) {
    if (offs != *fpos)
      *fpos = lseek(fd,(off_t)offs,L_SET);
  } else
   *fpos = lseek(fd,(off_t)offs,L_XTND);

  if (OSTestLock(fd, wlen) != noErr) {
    return(aeLockErr);
  }
  if (trans_table_index >= 0) {
    if (DBOSI)
      printf("FPWrite: translating from macintosh according to: %s\n",
	     ncs_tt_name(trans_table_index));
    ncs_translate(NCS_TRANS_MACTOUNIX, trans_table_index, wbuf, wlen);
  }

  cnt = write(fd,wbuf,wlen);
  *fpos += cnt;
  if (cnt == wlen)
    return(noErr);
  if (DBOSI)
    printf("OSWrite: Error from write %s\n",syserr());
  return(ItoEErr(errno));
}

/*
 * OSErr OSCreateDir(char *path,dword *dirid)
 *
 * This function is used to create a new directory.
 *
 * Inputs:
 *  path	The directory to create.
 *
 * Outputs:
 *  OSErr	Function result.
 *  dirid	Directory ID of newly created directory.
 *
 */

private OSErr
os_mkdir(pdir,name,mode,typ)
IDirP pdir;				/* parent directory */
char *name;
int typ,mode;
{
  char path[MAXPATHLEN];
  OSErr err;
					/* want path/name/.resource */
					/* want path/name/.finderinfo */
					/* want path/name */

  OSfname(path, pdir, name, F_DATA);	/* get path/name */
#ifdef NOCASEMATCH
  noCaseMatch(path);
#endif NOCASEMATCH
  switch (typ) {
  case F_RSRC:
    strcat(path, RFDIR);
    break;
  case F_FNDR:
    strcat(path, FIDIR);
    break;
  }
  if (DBOSI)
    printf("os_mkdir: creating mode=o%o dir=%s\n",mode,path);

  err = unix_mkdir(path,mode);		/* let unix do the work */
  if (err != noErr)
    return(err);
  EModified(pdir);
  return(noErr);
}  


OSErr
OSCreateDir(pdir,name, newdirid)
IDirP pdir;				/* parent directory id */
char *name;				/* name of new directory */
IDirP *newdirid;
{
  char path[MAXPATHLEN];
  int err,mo;
  struct stat stb;

  if (DBOSI)
    printf("OSCreateDir: creating %s\n",name);

  err = unix_stat(pathstr(pdir),&stb);	/* get current protections */
  mo = stb.st_mode & 0777;

  err = os_mkdir(pdir,name,mo,F_DATA);	/* create the data directory */
  if (err != noErr)			/* if that failed... then */
    return(err);			/*  return the error */

  if (pdir->flags  & DID_FINDERINFO) {
    OSfname(path,pdir,name,F_FNDR);	/* create finderinfo for folder */
#ifdef NOCASEMATCH
    noCaseMatch(path);
#endif NOCASEMATCH
    err = unix_create(path,TRUE,mo);	/*  do delete if exists... */
    os_mkdir(pdir,name,mo,F_FNDR);	/* create the finderinfo directory */
  }
  if (pdir->flags & DID_RESOURCE)
    os_mkdir(pdir,name,mo,F_RSRC);	/* create the resource directory */
  *newdirid = Idndirid(pdir,name);	/* now install the directory */
  return(noErr);
}


/*
 * OSErr OSFileDirInfo(IDirP ipdir, char *fn,
 *			FileDirParms *fdp, int volid);
 *
 * Return information for file fn, existing in directory ipdir into
 * the fdp structure.
 *
 * fdp->fdp_dbitmap and fdp->fdp_fbitmap are set with the request
 * bitmaps so we don't necessarily need to fetch unwanted (costly)
 * items.
 *
 */
export OSErr 
OSFileDirInfo(ipdir,idir,fn,fdp,volid)
IDirP ipdir;				/* parent directory */
IDirP idir;				/* directory id if directory */
char *fn;				/* file name */
FileDirParm *fdp;			/* returned parms */
int volid;				/* volume */
{
  char path[MAXPATHLEN];
  struct stat buf;
  time_t sometime;

  OSfname(path,ipdir,fn,F_DATA);

  if (DBOSI)
    printf("OSFileDirInfo on %s, idir = %02x,%02x\n",path,idir,VolRootD(volid));

#ifndef STAT_CACHE
  if (stat(path,&buf) != 0) {		/* file exists? */
#else STAT_CACHE
  if (OSstat(path,&buf) != 0) {		/* file exists? */
#endif STAT_CACHE
#ifdef NOCASEMATCH
    noCaseFind(path);			/* case-insensitive */
#ifndef STAT_CACHE
    if (stat(path,&buf) != 0) {		/* file exists? */
#else STAT_CACHE
    if (OSstat(path,&buf) != 0) {	/* file exists? */
#endif STAT_CACHE
      if (idir)				/* was in directory tree? */
        Idrdirid(ipdir, idir);		/* invalidate the entry then */
      return(aeObjectNotFound);		/* no... */
    }
#else NOCASEMATCH
    if (idir)				/* was in directory tree? */
      Idrdirid(ipdir, idir);		/* invalidate the entry then */
    return(aeObjectNotFound);		/* no... */
#endif NOCASEMATCH
  }

  /* pick out the earliest date for mac creation time */
  sometime = (buf.st_mtime > buf.st_ctime) ? buf.st_ctime : buf.st_mtime;
  fdp->fdp_cdate = (sometime > buf.st_atime) ? buf.st_atime : sometime;
  /* pick the later of status change and modification for mac modified */
  fdp->fdp_mdate = (buf.st_mtime < buf.st_ctime) ? buf.st_ctime : buf.st_mtime;

#ifdef USE_MAC_DATES
  { time_t when;
    OSGetCDate(ipdir,fn,&fdp->fdp_cdate);
    if (OSGetMDate(ipdir,fn,&sometime,&when) == noErr)
	if (fdp->fdp_mdate < (when+5)) /* fuzz factor */
	  fdp->fdp_mdate = sometime;
  }
#endif USE_MAC_DATES

  fdp->fdp_bdate = 0;
  fdp->fdp_zero = 0;			/* zero the zero byte (?) */
#ifdef SHORT_NAMES
  if (idir == VolRootD(volid)) {
    if ((fdp->fdp_dbitmap & DP_SNAME) || (fdp->fdp_fbitmap & FP_SNAME)) {
    /* change this for volume names */
        strcpy(fdp->fdp_sname,(char *) VolSName(volid));
        if (DBOSI && fdp != NULL)
           printf("dirParms sname %s\n",fdp->fdp_sname);
    }
    if ((fdp->fdp_dbitmap & DP_LNAME) || (fdp->fdp_fbitmap & FP_LNAME)) {
        strcpy(fdp->fdp_lname, (char *) VolName(volid));
        if (DBOSI && fdp != NULL)
           printf("dirParms lname %s\n",fdp->fdp_lname);
    }
  } else {
    if ((fdp->fdp_dbitmap & DP_SNAME) || (fdp->fdp_fbitmap & FP_SNAME)) 
        ItoEName_Short(ipdir,fn,fdp->fdp_sname);
    if ((fdp->fdp_dbitmap & DP_LNAME) || (fdp->fdp_fbitmap & FP_LNAME)) 
        ItoEName(fn,fdp->fdp_lname);
  }
#else SHORT_NAMES
  if (idir == VolRootD(volid))
    strcpy(fdp->fdp_lname, (char *) VolName(volid));
  else
    ItoEName(fn,fdp->fdp_lname);
#endif SHORT_NAMES

  if (S_ISDIR(buf.st_mode)) {		/* is this a directory? */
    fdp->fdp_flg = FDP_DIRFLG;		/* yes... */
    return(OSDirInfo(ipdir,fn,fdp,&buf,volid));	/* fill in */
  }
  fdp->fdp_flg = 0;			/* otherwise a file */
#ifdef SHORT_NAMES
  /* The PC asks for this information and wasn't implemented */
  /* should we be doing this for directories as well as files ? */
  if (fdp->fdp_fbitmap & FP_PDIR) 
    fdp->fdp_pdirid = ItoEdirid(ipdir,volid);
#endif SHORT_NAMES
  return(OSFileInfo(ipdir,fn,fdp,&buf,path)); /* fill in */
}

  
export OSErr
OSDirInfo(ipdir,fn,fdp,buf,volid)
IDirP ipdir; 
char *fn;
FileDirParm *fdp;
struct stat *buf;
int volid;
{
  IDirP idirid;
  dword ItoEAccess();
  char path[MAXPATHLEN];
  dirFinderInfo *dfi;
  word bm;
  int nchild;
  extern int sessvers;

  fdp->fdp_dbitmap &= DP_AUFS_VALID; /* truncate to findable */
  if (sessvers == AFPVersion1DOT1)   /* AFP1.1 ignores PRODOS */
    fdp->fdp_dbitmap &= ~DP_PDOS;
  bm = fdp->fdp_dbitmap;

  if (DBDIR)
    printf("OSDirInfo on %s bm=%x\n",fn,bm);

  if (bm & DP_ATTR)			/* skip attr if not requested */
    OSGetAttr(ipdir,fn,&fdp->fdp_attr);

  if (bm & (DP_FINFO|DP_PDOS)) {	/* skip finfo if not requested */
    OSGetFNDR(ipdir,fn,fdp->fdp_finfo);
    dfi = (dirFinderInfo *)fdp->fdp_finfo;
    OSfname(path, ipdir, fn, F_DATA);
    strcat(path, "/Icon:0d");
    if (access(path, R_OK) == 0)
      dfi->frFlags |= htons(FNDR_fHasCustomIcon); /* has custom ICON */
    else
      dfi->frFlags &= htons(~FNDR_fHasCustomIcon); /* no custom ICON */
  }

  if (bm & DP_PDOS)			/* generate some ProDOS info. */
    mapFNDR2PDOS(fdp);

  fdp->fdp_parms.dp_parms.dp_ownerid = ItoEID(buf->st_uid);
  fdp->fdp_parms.dp_parms.dp_groupid = ItoEID(buf->st_gid);
  fdp->fdp_parms.dp_parms.dp_accright =
    ItoEAccess(buf->st_mode,buf->st_uid,buf->st_gid);

  idirid = Idndirid(ipdir,fn);	/* must validate the entry now */
  if (idirid == NILDIR)
    return(aeAccessDenied);
  InitDIDVol(idirid, volid);
#ifdef notdef
  /* should be done already - so don't do it here! */
  if ((idirid->flags & DID_VALID) == 0) /* info valid? */
    OSValidateDIDDirInfo(idirid); /* valid it then */
#endif
  if (bm & DP_CHILD) {
    nchild = OSEnumCount(idirid);
    if (nchild < 0)			/* error if less than zero */
      nchild = 0;			/*  probably no read... set to 0 */
    fdp->fdp_parms.dp_parms.dp_nchild = nchild;
  }
  fdp->fdp_parms.dp_parms.dp_dirid = ItoEdirid(idirid,volid);
  return(noErr);
}

/* 
 * validates a did by making sure:
 *  - idirid points to a directory
 *  - limits number of symbolic links we will follow in any given path!
 *    o note: overlapping volumes may cause us to follow too many
 *      symbolic links, but things should eventually catch up.
 * also checks if the .resource and .finderinfo directories exists
 *  (must not be symbolic links)
 * double check that parent exists
*/
export void
OSValidateDIDDirInfo(idirid)
IDirP idirid;
{
  char path[MAXPATHLEN];
  char p_ath[MAXPATHLEN];
  struct stat stb;
  IDirP pdir;
  int i;

  OSfname(p_ath,idirid,"",F_DATA);
#ifndef STAT_CACHE
  i = stat(p_ath, &stb);
#else STAT_CACHE
  i = OSstat(p_ath, &stb);
#endif STAT_CACHE
#ifdef NOCASEMATCH
  if(i != 0) {
    noCaseFind(p_ath);
#ifndef STAT_CACHE
    i = stat(p_ath, &stb);
#else STAT_CACHE
    i = OSstat(p_ath, &stb);
#endif STAT_CACHE
  }
#endif NOCASEMATCH
  if (i == 0) {
    if (S_ISDIR(stb.st_mode))
      idirid->flags |= DID_DATA;
#ifndef NOLSTAT
#ifndef STAT_CACHE
    if (lstat(p_ath, &stb) != 0) { /* shouldn't fail! */
      idirid->flags = DID_VALID;
      return;
    }
    if ((pdir = Ipdirid(idirid)) != NILDIR) {	/* get parent */
      /* get count of symlinks of parent */
      i = ((pdir->flags & DID_SYMLINKS) >> DID_SYMLINKS_SHIFT);
      if ((stb.st_mode & S_IFMT) == S_IFLNK) {
	i++;			/* bump up count */
	if (i > DID_MAXSYMLINKS) {
	  idirid->flags = DID_VALID; /* toss it, too many links down */
	  return;
	}
      }
      /* really shouldn't need to mask it - means we are overinc'ed */
      idirid->flags |= ((i << DID_SYMLINKS_SHIFT) & DID_SYMLINKS);
    }
#endif STAT_CACHE
    /* don't follow symbolic links here! */
#ifdef STAT_CACHE
   if (idirid->flags & DID_DATA) {  /* Dan */
#endif STAT_CACHE
    strcpy(path,p_ath);
    toFinderInfo(path,"");
#ifndef STAT_CACHE
    if (lstat(path, &stb) == 0)
      if (S_ISDIR(stb.st_mode))
#else STAT_CACHE
    if (OSfinderinfo(p_ath))
#endif STAT_CACHE
	idirid->flags |= DID_FINDERINFO;
    strcpy(path,p_ath);
    toResFork(path,"");
#ifndef STAT_CACHE
    if (lstat(path, &stb) == 0)
      if (S_ISDIR(stb.st_mode))
#else STAT_CACHE
    if (OSresourcedir(p_ath))
#endif STAT_CACHE
	idirid->flags |= DID_RESOURCE;
#ifdef STAT_CACHE
   }
#endif STAT_CACHE
#else NOLSTAT
    /* no symolic links then */
    strcpy(path,p_ath);
    toFinderInfo(path,"");
#ifndef STAT_CACHE
    if (stat(path, &stb) == 0)
      if (S_ISDIR(stb.st_mode))
#else STAT_CACHE
    if (OSfinderinfo(p_ath)
#endif STAT_CACHE
	idirid->flags |= DID_FINDERINFO;
    strcpy(path,p_ath);
    toResFork(path,"");
#ifndef STAT_CACHE
    if (stat(path, &stb) == 0)
      if (S_ISDIR(stb.st_mode))
#else STAT_CACHE
    if (OSresourcedir(p_ath)
#endif STAT_CACHE
	idirid->flags |= DID_RESOURCE;
#endif NOLSTAT
  }
  idirid->flags |= DID_VALID;
}

export OSErr
OSFileInfo(ipdir,fn,fdp,buf,rpath)
IDirP ipdir;
char *fn,*rpath;
FileDirParm *fdp;
struct stat *buf;
{
  struct stat stb;
  word bm;
  extern int sessvers;

  fdp->fdp_fbitmap &= FP_AUFS_VALID; /* truncate to aufs supported */
  if (sessvers == AFPVersion1DOT1)   /* AFP1.1 ignores PRODOS */
    fdp->fdp_dbitmap &= ~FP_PDOS;
  bm = fdp->fdp_fbitmap;		/* fetch file bitmap */

  if (DBDIR)
    printf("OSFileInfo on %s bm=%x\n",fn,bm);

  if (bm & FP_ATTR)			/* skip attr if not requested */
    OSGetAttr(ipdir,fn,&fdp->fdp_attr);

  if (bm & (FP_FINFO|FP_PDOS))		/* skip finfo if not requested */
    OSGetFNDR(ipdir,fn,fdp->fdp_finfo);

  if (bm & FP_PDOS)			/* generate some ProDOS info. */
    mapFNDR2PDOS(fdp);

/* don't have volid available here ...
#ifdef SHORT_NAMES
  if (fdp->fdp_fbitmap & FP_PDIR) 
    fdp->fdp_pdirid = ItoEdirid(ipdir,volid);
#endif SHORT_NAMES
*/

  fdp->fdp_parms.fp_parms.fp_fileno = buf->st_ino;
  fdp->fdp_parms.fp_parms.fp_dflen = buf->st_size;
  fdp->fdp_parms.fp_parms.fp_rflen = 0;

#ifndef STAT_CACHE
  if (bm & FP_RFLEN) {
#else STAT_CACHE
  if ((bm & FP_RFLEN) && ipdir != NULL && (ipdir->flags&DID_RESOURCE)) {/*Dan*/
#endif STAT_CACHE
    toResFork(rpath,fn);		/* convert to rsrc name */
    if (stat(rpath,&stb) != 0)		/* to figure size of resource fork */
      return(noErr);
    if (DBFIL)
      printf("OSFileInfo: %s size is %d\n", rpath, (int)stb.st_size);
    fdp->fdp_parms.fp_parms.fp_rflen = stb.st_size;
  }
  return(noErr);
}

/*
 * simply check to see if a particular file exists
 *
*/
export OSErr
OSFileExists(ipdir, fn, type)
IDirP ipdir;
char *fn;
int type;
{
  char path[MAXPATHLEN];  
  struct stat stb;
#ifdef NOCASEMATCH
  register int i;
#endif NOCASEMATCH

  OSfname(path, ipdir, fn, type);
#ifdef NOCASEMATCH
  if((i = unix_stat(path, &stb)) != noErr) {
    noCaseFind(path);
    i = unix_stat(path, &stb);
  }
  return(i);
#else NOCASEMATCH
  return(unix_stat(path, &stb));
#endif NOCASEMATCH
}

export OSErr
OSSetFileDirParms(ipdir,idir,fn,fdp)
IDirP ipdir;
IDirP idir;				/* directory id if dir */
char *fn;
FileDirParm *fdp;
{
  char path[MAXPATHLEN];
  struct stat stb;
  int err;

  OSfname(path,ipdir,fn,F_DATA);	/* unix file name */
  if ((err = unix_stat(path,&stb)) != noErr) {
#ifdef NOCASEMATCH
    noCaseFind(path);
    if ((err = unix_stat(path,&stb)) != noErr) {
#endif NOCASEMATCH
      /* can't find it !!! */
      if (idir)
        Idrdirid(ipdir, idir);		/* remove from tree */
      return(err);
#ifdef NOCASEMATCH
    }
#endif NOCASEMATCH
  }

  if (S_ISDIR(stb.st_mode))		/* if a directory */
    return(OSSetDirParms(ipdir,fn,fdp)); /* then set dir parms... */
  else					/* else set file parms */
    return(OSSetFileParms(ipdir,fn,fdp));
}

export OSErr
OSSetFileParms(ipdir,fn,fdp)
IDirP ipdir;
char *fn;
FileDirParm *fdp;
{
  word bm = fdp->fdp_fbitmap;

#ifdef USE_MAC_DATES
  if (bm & (FP_FINFO|FP_ATTR|FP_PDOS|FP_CDATE|FP_MDATE))
#else  USE_MAC_DATES
  if (bm & (FP_FINFO|FP_ATTR|FP_PDOS))
#endif USE_MAC_DATES
    OSSetFA(ipdir,fn,fdp->fdp_fbitmap,fdp);

  return(noErr);
}

/*
 * OSSetDirParms
 * 
 */

OSErr
OSSetDirParms(ipdir,fn,fdp)
IDirP ipdir;
char *fn;
FileDirParm *fdp;
{
  u_short EtoIAccess();
  char p_ath[MAXPATHLEN];
  char path[MAXPATHLEN];
  int flags, err;
  int own, grp;	/* owner and group ids */
  DirParm *dp = &fdp->fdp_parms.dp_parms;
#ifdef NETWORKTRASH
  struct stat buf;
#endif NETWORKTRASH

#ifdef USE_MAC_DATES
  if (fdp->fdp_dbitmap & (DP_FINFO|DP_ATTR|DP_PDOS|DP_CDATE|DP_MDATE))
#else  USE_MAC_DATES
  if (fdp->fdp_dbitmap & (DP_FINFO|DP_ATTR|DP_PDOS))
#endif USE_MAC_DATES
    OSSetFA(ipdir,fn,fdp->fdp_dbitmap,fdp);

  grp = own = -1;
  if (fdp->fdp_dbitmap & DP_CRTID)
    own = EtoIID(dp->dp_ownerid);    
  if (fdp->fdp_dbitmap & DP_GRPID)
    grp = EtoIID(dp->dp_groupid);

  flags = ipdir->flags;		/* should use to prevent overworking */
  if (own != -1 || grp != -1) {
    /* error recovery? do all just in case */
    OSfname(p_ath,ipdir,fn,F_DATA);
    /* change owner/group for fn */
    if ((err = unix_chown(p_ath,own,grp)) != noErr) {
#ifdef NOCASEMATCH
      noCaseFind(p_ath);
      if ((err = unix_chown(p_ath,own,grp)) != noErr)
#endif NOCASEMATCH
        return(err);
    }
    /* change owner/group for fn/.resource */
    strcpy(path,p_ath); strcat(path,RFDIR);
    if ((err = unix_chown(path,own,grp)) != noErr && err != aeObjectNotFound)
      return(err);

    /* change owner/group for fn/.finderinfo */
    strcpy(path,p_ath); strcat(path,FIDIR);
    if ((err = unix_chown(path,own,grp)) != noErr && err != aeObjectNotFound)
      return(err);

    /* change owner/group for ../.resource/fn */
    OSfname(path,ipdir,fn,F_RSRC);
    if ((err = unix_chown(path,own,grp)) != noErr && err != aeObjectNotFound) {
#ifdef NOCASEMATCH
      noCaseFind(path);
      if ((err = unix_chown(path,own,grp)) != noErr && err != aeObjectNotFound)
#endif NOCASEMATCH
        return(err);
    }

    /* change owner/group for ../.finderinfo/fn */
    OSfname(path,ipdir,fn,F_FNDR);
    if ((err = unix_chown(path,own,grp)) != noErr && err != aeObjectNotFound) {
#ifdef NOCASEMATCH
      noCaseFind(path);
      if ((err = unix_chown(path,own,grp)) != noErr && err != aeObjectNotFound)
#endif NOCASEMATCH
        return(err);
    }
    EModified(ipdir);
  }
    
  if (fdp->fdp_dbitmap & DP_ACCES) {
    u_short acc, accd; /* file, directory mode	*/
    acc = accd = EtoIAccess(dp->dp_accright);
#ifdef NETWORKTRASH
    /* make the Network Trash Folder the same	*/
    /* access mode as the parent directory	*/
    if (*fn == 'N') { /* if first letter is 'N' */
      if (strcmp(fn, "Network Trash Folder") == 0
#ifndef STAT_CACHE
        && stat(pathstr(ipdir),&buf) == 0	/* and stat() OK */
#else   STAT_CACHE
        && OSstat(pathstr(ipdir),&buf) == 0	/* and stat() OK */
#endif  STAT_CACHE
      )
	/* parent directory mode */
	acc = accd = buf.st_mode;
    }
#endif NETWORKTRASH
#ifdef USEDIRSETGID
    if (grp != usrgid)
      accd |= I_SETGID;
#endif USEDIRSETGID

    /* change mode for fn */
    if ((err = os_chmod(ipdir,fn,accd,F_DATA)) != noErr)
      return(err);

    /* change all file protections, owner & group in fn */
    os_change_all(ipdir, fn, acc, own, grp, F_DATA);
    os_change_all(ipdir, fn, acc, own, grp, F_RSRC);
    os_change_all(ipdir, fn, acc, own, grp, F_FNDR);

    OSfname(p_ath,ipdir,fn,F_DATA);
    /* change mode for fn/.resource */
    strcpy(path,p_ath); strcat(path,RFDIR);
    if (( err = unix_chmod(path,accd)) != noErr && err != aeObjectNotFound) {
#ifdef NOCASEMATCH
      noCaseFind(path);
      if (( err = unix_chmod(path,accd)) != noErr && err != aeObjectNotFound)
#endif NOCASEMATCH
	return(err);
    }

    /* change mode for fn/.finderinfo */
    strcpy(path,p_ath); strcat(path,FIDIR);
    if (( err = unix_chmod(path,accd)) != noErr && err != aeObjectNotFound) {
#ifdef NOCASEMATCH
      noCaseFind(path);
      if (( err = unix_chmod(path,accd)) != noErr && err != aeObjectNotFound)
#endif NOCASEMATCH
	return(err);
    }

    /* change mode for ../.resource/fn */
    if ((err = os_chmod(ipdir,fn,acc,F_RSRC)) != noErr &&
     err != aeObjectNotFound)
      return(err);

    /* change mode for ../.finderinfo/fn */
    if ((err = os_chmod(ipdir,fn,acc,F_FNDR)) != noErr &&
     err != aeObjectNotFound)
      return(err);
  }
  return(noErr);
}


/*
 * Set fork length specified the file handle
 * - careful about len - should be at least a signed double word
 *   on mac (4 bytes)
*/
export OSErr
OSSetForklen(fd, len)
int fd;
int len;
{
  int err;
  if (DBOSI)
    printf("OSSetForklen: truncating file on file descriptor %d to %d\n",
	   fd,len);
  if (ftruncate(fd, len) < 0) {
    err = errno;
    if (DBOSI)
      printf("OSSetForklen: error on truncate %s\n",syserr());
    return(ItoEErr(err));
  }
  return(noErr);
}

/*
 * OSErr os_chmod(IDirP idir, u_short mode, int typ)
 *
 * Directory id (idir), and type (typ) specify a directory name.
 * Internal access bits are mode.
 *
 * Change the protection of the directory to eacc.  
 *
 */
private OSErr
os_chmod(idir,fn,mode,typ)
IDirP idir;
char *fn;
u_short mode;
int typ;
{
  char path[MAXPATHLEN];
  int err;

  OSfname(path,idir,fn,typ);		/* convert unix name */
  if (DBOSI)
    printf("os_chmod: setting %o for %s\n",mode,path);

  err = unix_chmod(path,mode);		/* and set for the directory */
#ifdef NOCASEMATCH
  if (err != noErr) {
    noCaseFind(path);
    if((err = unix_chmod(path,mode)) != noErr)
      return(err);
  }
#else NOCASEMATCH
  if (err != noErr)
    return(err);
#endif NOCASEMATCH

  EModified(idir);
  return(noErr);
}

/*
 * Change file protection, owner & group for all files in directory
 *
 * Have to do because:
 * unix has file protection and AFP does not, change the protection
 * of each file in the directory as well.
 *
 * Do not change the protection of directories contained within
 * the directory... 
 *
*/
private void
os_change_all(idir,fn,mode,own,grp,typ)
IDirP idir;
char *fn;
u_short mode;
int own, grp, typ;
{
  char path[MAXPATHLEN];
  char p_ath[MAXPATHLEN];
  struct stat stb;
#ifdef USEDIRENT
  struct dirent *dp, *readdir();
#else  USEDIRENT
  struct direct *dp, *readdir();
#endif USEDIRENT
  DIR *dirp;
  int pl,err;

  OSfname(path,idir,fn,F_DATA);		/* convert unix name */
  pl = strlen(path);
  switch (typ) {
  case F_DATA:
    break;
  case F_RSRC:
    strcpy(path+pl, RFDIR);
    break;
  case F_FNDR:
    strcpy(path+pl, FIDIR);
    break;
  }
  if (DBOSI)
    printf("os_change_all: setting %o for %s\n",mode,path);

  dirp = opendir(path);
  if (dirp == NULL) {
#ifdef NOCASEMATCH
    noCaseFind(path);
    if((dirp = opendir(path)) == NULL) {
#endif NOCASEMATCH
      if (DBOSI)
        printf("os_change_all: opendir failed on %s\n",path);
      return;
#ifdef NOCASEMATCH
    }
#endif NOCASEMATCH
  }

  pl = strlen(path);			/* length of the path */
  path[pl++] = '/';			/* add component terminator */
  for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
    strcpy(path+pl,dp->d_name);		/* create file name */
    if (stat(path,&stb) != 0) {		/* get the file status  */
      if (DBOSI)
	printf("os_change_all: stat failed for %s\n",path);
      continue;				/*  some error... */
    }
    if (S_ISDIR(stb.st_mode))
	continue;
    if (typ == F_FNDR) {
      OSfname(p_ath,idir,fn,F_DATA);
      strcat(p_ath,"/"); strcat(p_ath,dp->d_name);
      if (stat(p_ath,&stb) == 0) {
        if (S_ISDIR(stb.st_mode))
	  continue;
      }
    }
    /* ignore errors */
    unix_chmod(path,mode);		/* set the mode for this file */
    unix_chown(path,own,grp);		/* set owner and group for file */
  }
  closedir(dirp);			/* close the directory */
}


private u_short
EtoIPriv(emode,eshift,ishift)
dword emode;
int eshift,ishift;
{
  u_short i = 0;

  emode = (emode >> eshift);
  if (emode & E_RD) i = I_RD | I_EX;
  if (emode & E_WR) i |= I_WR | I_EX;
/*  if (emode & E_SR) i |= I_EX; */
  return(i << ishift);  
}


/*
 * Is the current user in the group gid?
 *
*/
private int
OSInGroup(gid)
int gid;
{
  int i;
  int gtgid;

  /* if ngroups gets very large, do a binary search */
  for (i=0; i < ngroups; i++) {
    gtgid = groups[i];
    if (gtgid == gid)
      return(TRUE);
    if (gtgid > gid)		/* limiting case */
      return(FALSE);
  }
  return(FALSE);
}

/*
 * Get groups that a user is valid in
 *
 * Sort the array for later use
 *
*/
dogetgroups()
{
  int ng;
  int i,k,idxofmin;
  int t;

  if ((ng = getgroups(NGROUPS, groups)) < 0) {
    return(-1);
  }
  if (ng == 0)			/* not in any groups? */
    return(0);

  /* sort groups array */
  /* we assume n is very small, else possibly replace with quicker sort */
  /* n**2 performance (comparisons), but no more than n swaps */
  /* the array tends to be mostly sorted with the first element out */
  /* of place.  Don't use std quicksort -- under this assumption */
  /* it will give very poor performance on average (at much higher overhead) */
  /* NOTE: we could really speed this up if we knew that the  */
  /* array was sorted after the first "n" items, but with N so */
  /* small (usually less than 32), it's not a big deal */
  for (i = 0 ; i < ng; i++) {
    for ( idxofmin = i, k = i + 1; k < ng; k++) /* find (i+1)'th min */
      if (groups[k] < groups[idxofmin])
	idxofmin = k;
    if (i != idxofmin) {
      t = groups[idxofmin];
      groups[idxofmin] = groups[i];
      groups[i] = t;
    }
  }
  return(ng);
}

private u_short
EtoIAccess(emode)
dword emode;
{
  u_short imode;

  imode = (EtoIPriv(emode,ES_OWNER,IS_OWNER) |
	  EtoIPriv(emode,ES_GROUP,IS_GROUP) |
	  EtoIPriv(emode,ES_WORLD,IS_WORLD));

  if (DBOSI)
    printf("EtoIAccess: E=0x%x, I=o%o\n",emode,imode);

  return(imode);
}

private dword
ItoEPriv(imode,ishift,eshift)
u_short imode;
int ishift,eshift;
{
  dword e = 0;
  
  imode = (imode >> ishift);
  if (imode & I_RD) e = E_RD | E_SR;
  if (imode & I_WR) e |= E_WR;
/*  if (imode & I_EX) e |= E_SR; */
  return(e << eshift);
}

private dword
ItoEAccess(imode,uid,gid)
u_short imode;
int uid;
int gid;
{
  dword e = 0;

  /* set user rights summary byte */

  if (usruid == 0)		/* are we running as root? */
    e |= ((E_RD|E_WR|E_SR)<<ES_USER)|E_IOWN; /* we can do anything */
  else if (usruid == uid)	/* but if the real owner then */
    e |= ItoEPriv(imode,IS_OWNER,ES_USER)|E_IOWN; /* set owner bits */
  else if (OSInGroup(gid))	/* if in one of the groups */
    e |= ItoEPriv(imode,IS_GROUP,ES_USER); /* set group access as well */
  else				/* otherwise */
    e |= ItoEPriv(imode,IS_WORLD,ES_USER); /* set user rights for world */
  
  if (imode & I_SETUID) /* if directory is setuid (say on shared volume) */
    e |= E_IOWN; /* then set owner bit so FinderInfo etc. can be written */

  if (uid == NOID) /* owned by <any user> */
    e |= E_IOWN; /* must set owner bit */

  /* set owner, group and world bytes */

  e |= (ItoEPriv(imode,IS_OWNER,ES_OWNER) | /* other access */
	ItoEPriv(imode,IS_GROUP,ES_GROUP) |
	ItoEPriv(imode,IS_WORLD,ES_WORLD));

  if (DBOSI)
    printf("ItoEAccess: I=o%o, E=0x%x\n",imode,e);

  return(e);
}

export OSErr
OSAccess(idir,fn,mode)
IDirP idir;
char *fn;
int mode;
{
  int imode = 0;
  int err;
  char path[MAXPATHLEN];

  if (mode & OFK_MWR)
    imode |= W_OK;
  if (mode & OFK_MRD)
    imode |= R_OK;

  OSfname(path,idir,fn,F_DATA);		/* create unix style filename */
  err = access(path,imode);
  if (err == 0)
    return(noErr);
#ifdef NOCASEMATCH
  noCaseFind(path);
  if(access(path,imode) == 0)
    return(noErr);
#endif NOCASEMATCH
  switch (errno) {
  case ENOTDIR:
    return(aeDirNotFound);
  case ENOENT:
    return(aeObjectNotFound);
  case EACCES:
    return(aeAccessDenied);
  }
  return(aeAccessDenied);
}

/*
 * scale the AUFS volume size information to comply
 * with the upper limit of Macintosh file systems
 * (currently 2 Gigabytes)
 *
 * To avoid problems with "on disk" sizes being
 * calculated incorrectly (ie: 2,546.9MB on disk for
 * 8,903 bytes used) we seem to need an allocation
 * block size smaller than 32k, so we arbitrarily
 * choose 31k and apply the formula from IM vol IV,
 * page 241
 *
 *   abSize = (1 + ((volSize/512)/64k)) * 512
 *
 * giving a volume size of 2046820352 bytes (1,952MB)
 *
 */

#ifndef MAXMACVOLSIZE
#define MAXMACVOLSIZE	2046820352
#endif  MAXMACVOLSIZE

void
scaleVolSize(v)
VolPtr v;
{
  float scale;

  if (v->v_size >= MAXMACVOLSIZE) {
    scale = (MAXMACVOLSIZE >> 16)/(float)(v->v_size >> 16);
    v->v_free = scale * v->v_free;
    v->v_size = MAXMACVOLSIZE;
  }

  return;
}

/*
 * set the extended volume size
 * parameters defined by AFP 2.2
 *
 * size, free and blocks are 32-bit numbers.
 * We want to end up with a 64-bit number in
 * network-byte-order.
 *
 * For the moment we note that block-sizes
 * are usually a simple power of two so we
 * just bit-shift the original numbers.
 *
 */

void
extendedVolSize(v, size, free, blk)
VolPtr v;
dword size, free, blk;
{
  int off;
  int i, j;
  int shift;

  bzero((char *)v->v_esize, sizeof(v->v_esize));
  bzero((char *)v->v_efree, sizeof(v->v_efree));

  switch (blk) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
    case 128:
      off = 0;
      break;
    case 256:
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
    case 32768:
      off = 1;
      break;
    case 65536:
    case 131072:
    case 262144:
      off = 2;
      break;
    default:
      /* set some arbitrary number */
      v->v_esize[4] = 0x80;
      v->v_efree[4] = 0x40;
      return;
      break;
  }

  /*
   * initialize the array in network byte
   * order. If the multiplier is 1, 256 or
   * 65536 there is nothing else to do.
   *
   */
  v->v_esize[7-off] = size & 0xff;
  v->v_esize[6-off] = (size >> 8) & 0xff;
  v->v_esize[5-off] = (size >> 16) & 0xff;
  v->v_esize[4-off] = (size >> 24) & 0xff;

  v->v_efree[7-off] = free & 0xff;
  v->v_efree[6-off] = (free >> 8) & 0xff;
  v->v_efree[5-off] = (free >> 16) & 0xff;
  v->v_efree[4-off] = (free >> 24) & 0xff;

  if (blk == 1 || blk == 256 || blk == 65536)
    return;

  /*
   * now bit shift each group of bytes
   *
   */
  shift = (blk < 256) ? blk : ((blk < 65536) ? (blk/256) : (blk/65536));

  for (i = 1; i < 20; i++) {
    for (j = 0 ; j < 8; j++) {
      v->v_esize[j] <<= 1;
      v->v_efree[j] <<= 1;
      if (j < 7 && (v->v_esize[j+1] & 0x80))
        v->v_esize[j] |= 1;
      if (j < 7 && (v->v_efree[j+1] & 0x80))
        v->v_efree[j] |= 1;
    }
    if (shift == (0x0001 << i))
      break;
  }

  return;
}

/*
 * OSErr OSVolInfo(VolPtr v)
 *
 * Update volume information for volume pointed to by v.
 * Returns error if path does not exist.
 *
 * Update:
 *
 *  v_attr	Read only flag (V_RONLY)
 *  v_cdate	creation date (of v_path)
 *  v_mdate	modification date (of v_path)
 *  v_size 	size of volume in bytes (32 bits ##)
 *  v_free	free bytes on volume (32 bits ##)
 *
 *  ## expect trouble if the volume size exceeds 4 Gigabytes.
 *
 */

export OSErr
OSVolInfo(path,v, bitmap)
char *path;
VolPtr v;
word bitmap;			/* bitmap of info needed */
{
  struct stat buf;
#if defined(USEQUOTA) || defined(USEBSDQUOTA)
  struct dqblk dqblk;
#endif USEQUOTA
#ifdef USEUSTAT
  struct ustat ubuf;
#endif USEUSTAT
#ifdef USESTATFS
# ifdef SOLARIS
  struct statvfs fsbuf;
# else  SOLARIS
  struct statfs fsbuf;
# endif SOLARIS
#endif USESTATFS
  time_t sometime;
  void extendedVolSize();
  void scaleVolSize();

  if (stat(path,&buf) != 0)	/* directory exists? */
    return(aeObjectNotFound);	/* no... */
  if (!(S_ISDIR(buf.st_mode)))	/* check for directory */  
    return(aeParamErr);		/* not a directory! */
    
  if (bitmap & VP_CDATE) {
    /* pick out the earliest date for mac creation time */
    sometime = (buf.st_mtime > buf.st_ctime) ? buf.st_ctime : buf.st_mtime;
    v->v_cdate = sometime > buf.st_atime ? buf.st_atime : sometime;
  }
  if (bitmap & VP_MDATE) {
    /* pick the later of status change and modification for */
    /* mac modified */
    v->v_mdate = (buf.st_mtime < buf.st_ctime) ? buf.st_ctime : buf.st_mtime;
  }
#ifdef notdef
  /* had it as v->v_mdate -- probably the reason we ifdef'ed it out */
  if (bitmap & VP_BDATE)
    v->v_bdate = 0;
#endif

  if (bitmap & VP_ATTR) {
#ifdef notdef
    /* don't really want this - causes problems when you have access */
    /* futher down the tree - should only come up locked iff the */
    /* tree is write locked (no (easy) way to tell because of symbolic */
    /* links and mount points) */
    if (access(v->v_path,W_OK) != 0) 	/* ok to write into directory? */
      v->v_attr |= V_RONLY;	/* no, set read-only */
    else
#endif
      v->v_attr &= ~V_RONLY;	/* clear read-only */
  }

  if ((bitmap & (VP_FREE|VP_SIZE|VP_EFREE|VP_ESIZE)) == 0)
    return(noErr);		/* naught else to do */

  /* All the following is good and fine unless: (a) the volume */
  /* has symlinks off the volume or (b) there are mounted file systems */
  /* under the mac volume.  In those cases, returning this information */
  /* could be damaging because some "good" programs base what they */
  /* can do on the volume information --- but people really like this */
  /* information!!! so we take the trade off (even though it is one */
  /* of the most system dependent parts of aufs) */

  /* don't know how to calculate disk free and size without being su */
  /* (in the general case) */

  /* hard coded values of 512 for quotas and 1024 for ustat are bad */
  /* where are the "real" numbers defined? */

  /* careful on the ordering - these must go last and if you can possibly */
  /* define more than one then you must define in order you wish */
#ifdef USEQUOTA
#if defined(gould)
  if (gquota(Q_GETDLIM, usruid, buf.st_dev, &dqblk) == 0 &&
#elif defined(encore)
  if (equota(Q_GETDLIM, usruid, buf.st_dev, &dqblk) == 0 &&
#elif defined(SOLARIS)
  if (solaris_quota(Q_GETDLIM, usruid, path, &dqblk) == 0 &&
#else  /* gould || encore || SOLARIS */
  if (quota(Q_GETDLIM, usruid, buf.st_dev, &dqblk) == 0 &&
#endif /* gould || encore || SOLARIS */
      dqblk.dqb_bhardlimit != 0) { /* make sure not unlimited */
    v->v_size = dqblk.dqb_bhardlimit*512;
    v->v_free = (dqblk.dqb_bhardlimit-dqblk.dqb_curblocks)*512;
    extendedVolSize(v, dqblk.dqb_bhardlimit,
      dqblk.dqb_bhardlimit-dqblk.dqb_curblocks, 512);
    scaleVolSize(v);
    return(noErr);
  }
#endif USEQUOTA
#ifdef USEBSDQUOTA
  {
    char *fsname;
    private char *find_mount_spec();
    if ((fsname = find_mount_spec(buf.st_dev)) == NULL) {
        errno = EPERM;
        return(-1);
    }
    if (quotactl(fsname, QCMD(Q_GETQUOTA,USRQUOTA), usruid, &dqblk) == 0 &&
         dqblk.dqb_bhardlimit != 0) {
        v->v_size = dqblk.dqb_bhardlimit*512;
        v->v_free = (dqblk.dqb_bhardlimit-dqblk.dqb_curblocks)*512;
	extendedVolSize(v, dqblk.dqb_bhardlimit,
	  dqblk.dqb_bhardlimit-dqblk.dqb_curblocks, 512);
	scaleVolSize(v);
        return(noErr);
    }
  }
#endif USEBSDQUOTA
#ifdef USEGETMNT
  if (ultrix_volinfo(path, &buf, v) == noErr)
    return(noErr);
#endif USEGETMNT
#ifdef USEUSTAT
  if (ustat(buf.st_dev, &ubuf) >= 0) {
    if (VP_SIZE & bitmap) {
      /* should do something better here */
      v->v_size = ubuf.f_tfree*1024;
    }
    v->v_free = ubuf.f_tfree*1024;
    extendedVolSize(v, ubuf.f_tfree, ubuf.f_tfree, 1024);
    scaleVolSize(v);
    return(noErr);
  }
#endif USEUSTAT
#ifdef USESTATFS
#ifdef SOLARIS
  if (statvfs(path, &fsbuf) >= 0) {
    v->v_size = fsbuf.f_frsize * fsbuf.f_blocks;
    /* limiting factor: cannot report on overutilization of a volume */
    v->v_free = (fsbuf.f_bavail < 0) ? 0 : fsbuf.f_frsize * fsbuf.f_bavail;
    extendedVolSize(v, fsbuf.f_blocks, fsbuf.f_bavail, fsbuf.f_frsize);
    scaleVolSize(v);
    return(noErr);
  }
#else /* SOLARIS */
# if defined(sgi) || defined(apollo)
  if (statfs(path, &fsbuf, sizeof(fsbuf), 0) >= 0) {
    v->v_size = fsbuf.f_bsize * fsbuf.f_blocks;
    /* limiting factor: cannot report on overutilization of a volume */
    v->v_free = (fsbuf.f_bfree < 0) ? 0 : fsbuf.f_bsize * fsbuf.f_bfree;
    extendedVolSize(v, fsbuf.f_blocks, fsbuf.f_bfree, fsbuf.f_bsize);
    scaleVolSize(v);
    return(noErr);
  }
# else  /* sgi || apollo */
  if (statfs(path, &fsbuf) >= 0) {
#if (defined(__386BSD__) && !defined(__NetBSD__)) || defined(__osf__)
    /*
     * on 386/BSD, the block size is in f_fsize
     * and f_bsize is the optimum transfer size
     *
     */
    v->v_size = fsbuf.f_fsize * fsbuf.f_blocks;
    /* limiting factor: cannot report on overutilization of a volume */
    v->v_free = (fsbuf.f_bavail < 0) ? 0 : fsbuf.f_fsize * fsbuf.f_bavail;
    extendedVolSize(v, fsbuf.f_blocks, fsbuf.f_bavail, fsbuf.f_fsize);
#else  /* __386BSD__ || __osf__ */
    v->v_size = fsbuf.f_bsize * fsbuf.f_blocks;
    /* limiting factor: cannot report on overutilization of a volume */
    v->v_free = (fsbuf.f_bavail < 0) ? 0 : fsbuf.f_bsize * fsbuf.f_bavail;
    extendedVolSize(v, fsbuf.f_blocks, fsbuf.f_bavail, fsbuf.f_bsize);
#endif /* __386BSD__ || __osf__ */
    scaleVolSize(v);
    return(noErr);
  }
# endif /* sgi || apollo */
#endif /* SOLARIS */
#endif USESTATFS
#ifdef SIZESERVER
  getvolsize(path, &v->v_size, &v->v_free);
#else SIZESERVER
  v->v_size = 0x1000000;		/* some random number */
  v->v_free = 0x1000000;		/* same random number */
#endif SIZESERVER
  extendedVolSize(v, 0x1000000, 0x1000000, 512);
  scaleVolSize(v);
  return(noErr);			/* all ok */
}

#ifdef SIZESERVER

#ifndef SIZESERVER_PATH
#define SIZESERVER_PATH		"/usr/local/cap/sizeserver"
#endif  SIZESERVER_PATH

static jmp_buf gotpipe;

private void
getvolsize(path, ntot, nfree)
char *path;
sdword *ntot, *nfree;
{
	register int i;
	int mask, socket[2];
	struct volsize vs;
	static int server = -1, server1, servmask;
	static struct timeval servtimeout = {0, 500000L};

	if(setjmp(gotpipe)) {
		if(server >= 0)
			close(server);
		server = -1;
unknown:
		*ntot = 0x1000000;
		*nfree = 0x1000000;
		return;
	}
	if(server < 0) {
		register int pid;
		int catchsigpipe();

		if(socketpair(AF_UNIX, SOCK_STREAM, 0, socket) < 0)
			goto unknown;
		if((pid = fork()) < 0) {
			close(socket[0]);
			close(socket[1]);
			goto unknown;
		}
		if(pid == 0) {	/* the child */
			close(socket[0]);
			if(socket[1] != 0) {
				dup2(socket[1], 0);
				close(socket[1]);
			}
			execl(SIZESERVER_PATH, "sizeserver", 0);
			_exit(1);
		}
		close(socket[1]);
		server = socket[0];
		server1 = server + 1;
		servmask = 1 << server;
		signal(SIGPIPE, catchsigpipe);
	}
	for(i = 3 ; ; ) {
		if(i-- <= 0)
			goto unknown;
		lseek(server, 0L, 2);
		write(server, path, strlen(path) + 1);
		mask = servmask;
		if(select(server1, &mask, NULL, NULL, &servtimeout) < 1)
			goto unknown;
		if(read(server, (char *)&vs, sizeof(vs)) == sizeof(vs))
			break;
	}
	*ntot = vs.total;
	*nfree = vs.free;
}

catchsigpipe()
{
	longjmp(gotpipe, 1);
}
#endif SIZESERVER

#ifdef USEGETMNT
/* get info on path using buf when there is ambiguity (ultrix 2.0 or before) */
/* fill in info on v */
private OSErr
ultrix_volinfo(path,buf,v)
char *path;
struct stat *buf;
VolPtr v;
{
  int context = 0;
  int i, num;
  u_int vfree;
  /* multiple buffers are wasteful when using Ultrix 2.2, but code is */
  /* good enough */
  struct fs_data buffer[NUMGETMNTBUF];
  struct fs_data *bp;
  int nbytes = sizeof(struct fs_data)*NUMGETMNTBUF;
  void extendedVolSize();
  void scaleVolSize();

  if (!oldgetmnt) {
    /* Ultrix 2.2 */
    /* use nostat_one -- we don't want to hang on nfs */
    /* return none if error or (0) - not found */
    nbytes = sizeof(struct fs_data);
    if ((num=getmnt(&context, buffer, nbytes, NOSTAT_ONE, path)) <= 0)
      return(-1);
    bp = buffer;
  } else {
    while ((num=getmnt(&context, buffer, nbytes)) > 0) {
      for (i = 0, bp = buffer; i < num; i++, bp++)
	if (buf->st_dev == bp->fd_req.dev)
	  goto found;
      /* context is set to zero if last call to getmnt returned */
      /* all file systems */
      if (context == 0)
	return(-1);
    }
    /* should never reach here, means getmnt returned an error */
    return(-1);			/* nothing found */
  }
found:
  /* "overflow" space if any - looks used */
  v->v_size = bp->fd_req.btot * 1024;
  /* bfreen must be "good" in that it cannot go below 0 when */
  /* out of space -- it is unsigned! */
  v->v_free = ((getuid() == 0) ? bp->fd_req.bfree : bp->fd_req.bfreen) * 1024;
  extendedVolSize(v, bp->fd_req.btot,
    (getuid() == 0) ? bp->fd_req.bfree : bp->fd_req.bfreen, 1024);
  scaleVolSize(v);
  return(noErr);
}
#endif /* GETMNT */

#if defined(USEQUOTA) && defined(SOLARIS)
int
solaris_quota(cmd, uid, path, dq)
int cmd, uid;
char *path;
struct dqblk *dq;
{
  int fd, res;
  struct quotctl qctl;

  switch (cmd) {
    case Q_GETQUOTA:	/* let it be "read-only" */
      break;
    default:
      errno = EINVAL;
      return(-1);
  }

  if ((fd = open(path, O_RDONLY)) < 0)
    return(-1);

  qctl.op   = cmd;
  qctl.uid  = uid;
  qctl.addr = (caddr_t)dq;
  res = ioctl(fd, Q_QUOTACTL, &qctl);
  close(fd);

  return(res);
}
#endif /* USEQUOTA && SOLARIS */

#if defined(USESUNQUOTA) || defined(USEBSDQUOTA)

#ifndef MAXUFSMOUNTED
# ifdef NMOUNT
#  define MAXUFSMOUNTED NMOUNT	/* NMOUNT in param.h */
# else  NMOUNT
#  define MAXUFSMOUNTED 32	/* arb. value */
# endif NMOUNT
#endif  MAXUFSMOUNTED

private struct mount_points {
  char *mp_fsname;		/* name of "block" device */
  dev_t mp_dev;			/* device number */
} mount_points[MAXUFSMOUNTED];

private int num_mount_points = -1;

private struct mount_points *
find_mount_spec_intable(dev)
dev_t dev;
{
  int i;
  struct mount_points *mp;

  for (i = 0, mp = mount_points; i < num_mount_points; i++, mp++) {
    if (dev == mp->mp_dev)
      return(mp);
  }
  return(NULL);
}

#ifdef USEBSDQUOTA
/*
 * find the block special device...
 * try updating mount point table if possible
 *
 * returns name if it can, null o.w.
 *
 */
private char *
find_mount_spec(dev)
{
  struct mount_points *mp;
  struct fstab *mtent;
  struct stat sbuf;
  char *strdup();

  if ((mp = find_mount_spec_intable(dev)) != NULL) /* try once */
    return(mp->mp_fsname);

  if (num_mount_points < MAXUFSMOUNTED) {
    /* check to see if mounted */
    if (setfsent() == 0) {
      if (DBOSI)
    logit(0,"setfsent failed!");
      return(NULL);
    }
    mp = NULL;
    while ((mtent = getfsent()) != NULL) {
      if (stat(mtent->fs_file, &sbuf) < 0)
        continue;
      if (dev != sbuf.st_dev)
        continue;
      mp = mount_points+num_mount_points;
      num_mount_points++;
      mp->mp_fsname = strdup(mtent->fs_file);
      mp->mp_dev = sbuf.st_dev;
      break;
    }
    endfsent();
  } else {
    /* table would overflow, try rebuilding */
    if (build_mount_table() < 0)    /* try rebuilding table */
      return(NULL);
    mp = find_mount_spec_intable(dev);
  }

  if (mp)
    return(mp->mp_fsname);

  if (DBOSI)
    logit(0,"Cannot find file system on device (%d,%d)\n",
      major(dev), minor(dev));

  return(NULL);         /* total failure */
}

/*
 * build a table of the various mounted file systems
 * using getmntent.  We want to use /etc/mtab, but
 * if we can't, then we use /etc/fstab to get some
 * information anyway if this is the first time around (to
 * prevent constant rereads since /etc/fstab is pretty constant)
 *
 */
int
build_mount_table()
{
  struct stat sbuf;
  struct fstab *mtent;
  struct mount_points *mp;
  char *strdup();

  setfsent();

  /* free old info */
  if (num_mount_points) {
    for (mp = mount_points ; num_mount_points > 0; num_mount_points--, mp++)
      if (mp->mp_fsname)
        free(mp->mp_fsname);
  }
  num_mount_points = 0; /* paranoia */

  mp  = mount_points;
  while ((mtent = getfsent()) != NULL) {
    if (stat(mtent->fs_file, &sbuf) < 0)
      continue;
    mp->mp_fsname = strdup(mtent->fs_file);
    mp->mp_dev = sbuf.st_dev;
    num_mount_points++, mp++;
    if (num_mount_points > MAXUFSMOUNTED) {
      logit(0,"Grrr.. too many mounted file systems for build_mount_table");
      break;
    }
  }
  endfsent();

  if (num_mount_points == 0 && DBOSI)
    logit(0,"No mount points can be found");

  return(0);
}

#else  USEBSDQUOTA

/*
 * find the block special device...
 * try updating mount point table if possible
 *
 * returns name if it can, null o.w.
 *
 */
private char *
find_mount_spec(dev)
{
  struct mount_points *mp;
  struct mntent *mtent;
  struct stat sbuf;
  FILE *mt;
  char *strdup();

  if ((mp = find_mount_spec_intable(dev)) != NULL) /* try once */
    return(mp->mp_fsname);
  
  if (num_mount_points < MAXUFSMOUNTED) {
    /* check to see if mounted */
    if ((mt = setmntent("/etc/mtab", "r")) == NULL) {
      if (DBOSI)
	logit(0,"/etc/mtab is read protected or nonexistant");
      return(NULL);
    }
    mp = NULL;
    while ((mtent = getmntent(mt)) != NULL) {
      if (stat(mtent->mnt_fsname, &sbuf) < 0)
	continue;
      if (dev != sbuf.st_rdev)
	continue;
      mp = mount_points+num_mount_points;
      num_mount_points++;
      mp->mp_fsname = strdup(mtent->mnt_fsname);
      mp->mp_dev = sbuf.st_rdev;
      break;
    }
    endmntent(mt);
  } else {
    /* table would overflow, try rebuilding */
    if (build_mount_table() < 0)	/* try rebuilding table */
      return(NULL);
    mp = find_mount_spec_intable(dev);
  }
  if (mp)
    return(mp->mp_fsname);

  if (DBOSI)
    logit(0,"Cannot find file system on device (%d,%d)\n",
      major(dev), minor(dev)); 

  return(NULL);			/* total failure */
}

/*
 * build a table of the various mounted file systems
 * using getmntent.  We want to use /etc/mtab, but
 * if we can't, then we use /etc/fstab to get some 
 * information anyway if this is the first time around (to
 * prevent constant rereads since /etc/fstab is pretty constant)
 *
 */
int
build_mount_table()
{
  FILE *mt;
  struct stat sbuf;
  struct mntent *mtent;
  struct mount_points *mp;
  char *strdup();

  if ((mt = setmntent("/etc/mtab", "r")) == NULL) {
    if (DBOSI)
      logit(0,"/etc/mtab is read protected or nonexistant");
    if (num_mount_points != 0)
      return(-1);
    if ((mt = setmntent("/etc/fstab", "r")) == NULL) {
      if (DBOSI)
	logit(0,"/etc/fstab is read protected or nonexistant");
      return(-1);
    }
  }

  /* free old info */
  if (num_mount_points) {
    for (mp = mount_points ; num_mount_points > 0; num_mount_points--, mp++)
      if (mp->mp_fsname)
	free(mp->mp_fsname);
  }
  num_mount_points = 0;	/* paranoia */

  mp  = mount_points;
  while ((mtent = getmntent(mt)) != NULL) {
    if (stat(mtent->mnt_fsname, &sbuf) < 0)
      continue;
    mp->mp_fsname = strdup(mtent->mnt_fsname);
    mp->mp_dev = sbuf.st_rdev;
    num_mount_points++, mp++;
    if (num_mount_points > MAXUFSMOUNTED) {
      logit(0,"Grrr.. too many mounted file systems for build_mount_table");
      break;
    }
  }
  endmntent(mt);

  if (num_mount_points == 0 && DBOSI)
    logit(0,"No mount points can be found");

  return(0);
}

/*
 * SunOS doesn't use the Melbourne quota system - it has its own
 * private one that uses quotactl instead of quota.  We emulate
 * quota here...
 *
 */
int
#ifdef gould
gquota(cmd, uid, arg, addr)
#else  gould
#ifdef encore
equota(cmd, uid, arg, addr)
#else  encore
quota(cmd, uid, arg, addr)
#endif encore
#endif gould
int cmd, uid, arg;
caddr_t addr;
{
  char *fsname;
  private char *find_mount_spec();

  switch (cmd) {
  case Q_QUOTAON:
  case Q_QUOTAOFF:
  case Q_SETQUOTA:
  case Q_GETQUOTA:
  case Q_SETQLIM:
  case Q_SYNC:
    break;
  default:
    errno = EINVAL;
    return(-1);
  }
  
  if ((fsname = find_mount_spec(arg)) == NULL) {
    errno = EPERM;
    return(-1);
  }
#ifdef gould
  /* oh dear, whose idea was this ? */
  return(quota(cmd, fsname, uid, addr));
#else  gould
  return(quotactl(cmd, fsname, uid, addr));
#endif gould
}
#endif	USEBSDQUOTA
#endif  USESUNQUOTA || USEBSDQUOTA

export OSErr
OSCopyFile(spdir,sfile,dpdir,dfile)
IDirP spdir,dpdir;			/* source and destination parents */
char *sfile,*dfile;			/* source and destination file names */
{
  char s_path[MAXPATHLEN];
  char d_path[MAXPATHLEN];
  char spath[MAXPATHLEN];
  char dpath[MAXPATHLEN];
  struct stat stb;
  int mo;
  int err;

  OSfname(s_path,spdir,sfile,F_DATA);	/* create unix style name for data */
  OSfname(d_path,dpdir,dfile,F_DATA);	/*  same for destination */
#ifdef NOCASEMATCH
  noCaseMatch(s_path);
  noCaseMatch(d_path);
#endif NOCASEMATCH

  if (DBOSI)
    printf("OSCopyFile: %s -> %s\n",s_path,d_path);

  err = unix_stat(d_path,&stb);		/* see if destination exists... */
  if (err == noErr)			/* yes... it does */
    return(aeObjectExists);		/* return error... */

  /* get info on parent of destination */
  if ((err = unix_stat(pathstr(dpdir), &stb)) != noErr)
    return(err);
  mo = filemode(stb.st_mode, stb.st_uid, stb.st_gid);
  err = os_copy(s_path,d_path, mo);

  if (err != noErr && DBOSI)
    printf("OSCopyFile: DATA copy failed %s\n",afperr(err));

  if (err != noErr)
    return(err);

  strcpy(spath,s_path);
  toResFork(spath,sfile);		/* create unix style name for rsrc */
  strcpy(dpath,d_path);
  toResFork(dpath,dfile);		/*  same for destination */
  err = os_copy(spath,dpath,mo); /* do the copy... */
  /* allow object not found */
  if (err != noErr && err != aeObjectNotFound) { /* if failure.... */
    if (DBOSI)
      printf("OSCopyFile: RSRC copy failed %s\n",afperr(err));
    (void) os_delete(dpdir,dfile,F_DATA); /*  cleanup dest files */
    return(err);
  }

  strcpy(spath,s_path);
  toFinderInfo(spath,sfile);		/* create unix style name for fndr */
  strcpy(dpath,d_path);
  toFinderInfo(dpath,dfile);		/*  same for destination */
  err = os_copy(spath,dpath,mo); /* do the copy... */
  /* allow object not found */
  if (err != noErr && err != aeObjectNotFound) {
    if (DBOSI)
      printf("OSCopyFile: FNDR copy failed %s\n",afperr(err));
    (void) os_delete(dpdir,dfile,F_DATA); /* cleanup dest files */
    (void) os_delete(dpdir,dfile,F_RSRC); /* .... */
    return(err);
  }
  OSSetMacFileName(dpdir, dfile);

  FModified(dpdir, dfile);	/* mark as modified */
#ifdef notdef
  EModified(dpdir);			/* destination dir is modified */
#endif
  return(noErr);
}

/*
 * OSErr os_copy(char *from, char *to, mo)
 *
 * Copy the file from, to the file to.  If "to" already exists then
 * overwrite.  File is created with mode "mo".
 *
 * Should probably lock the file!
 *
 */

private OSErr
os_copy(from, to, mo)
char *from,*to;
{
  int sfd,dfd,err;
  char iobuf[IOBSIZE];
  struct stat sstb;
  struct stat dstb;
  int i;

  if ((err = unix_stat(from,&sstb)) != noErr)
    return(err);

  if (S_ISDIR(sstb.st_mode)) {		/* dirs not allowed... */
    printf("os_copy: source is directory\n");
    return(aeObjectTypeErr);
  }

  if ((err=unix_open(from,0,&sfd)) != noErr) /* open source file */
    return(err);

#ifdef APPLICATION_MANAGER
  {
    extern int fdplist[NOFILE];
    extern struct flist *applist;

    if (applist != NULL && fdplist[sfd] == 1) {
      /* we want Finder copy protection */
      (void) unix_close(sfd);
      return(aeAccessDenied);
    }
  }
#endif APPLICATION_MANAGER

  err = unix_stat(to,&dstb);	/* check on destination */
  if (err == noErr) {		/* file is there */
    if (sstb.st_dev == dstb.st_dev && sstb.st_ino == dstb.st_ino) {
      if (DBOSI)
	printf("os_copy: cannot copy to self\n");
      unix_close(sfd);
      return(aeParamErr);
    }
  } /* else ignore error from stat */

  err = unix_createo(to,TRUE,mo,&dfd);
  if (err != noErr) {
    printf("os_copy; create failed\n");
    (void) unix_close(sfd);
    if (err == aeObjectNotFound)	/* no destination? */
      err = aeParamErr;			/* then return this */
    return(err);
  }

  /* copy loop */
  for (i=0;;i++) {
    register int len;

    len = read(sfd,iobuf,IOBSIZE);
    if (len == 0)
      break;

    if (len < 0) {
      printf("os_copy: error during read\n");
      (void) unix_close(sfd);
      (void) unix_close(dfd);
      return(aeParamErr);		/* disk error */
    }

    if (write(dfd,iobuf,len) != len) {
      err = errno;			/* preserve error code */
      if (DBOSI)
	printf("os_copy: error on write %s\n",syserr());
      (void) unix_close(sfd);
      (void) unix_close(dfd);
      return(ItoEErr(err));
    }
    if (i % 5)
      abSleep(0, TRUE);
  }
  (void) unix_close(sfd);
  (void) unix_close(dfd);
  return(noErr);
}

export OSErr
OSCreateFile(pdir,file,delf)
IDirP pdir;
char *file;
int delf;			/* if want to delete existing file */
{
  char p_ath[MAXPATHLEN];
  char path[MAXPATHLEN];
  int err,derr,rerr,cerr,mo;
  struct stat stb;

  OSfname(p_ath,pdir,file,F_DATA);	/* create data file name */
#ifdef NOCASEMATCH
  noCaseMatch(p_ath);
#endif NOCASEMATCH

  if (DBOSI)
    printf("OSCreateFile: creating %s with %s\n",p_ath,
	   (delf) ? "OverWrite" : "No OverWrite");
  
  err = unix_stat(pathstr(pdir),&stb);
  if (err != noErr)
    return(err);
  mo = filemode(stb.st_mode, stb.st_uid, stb.st_gid);

  /* should never get aeObjectExists if delf was true */
  derr = unix_create(p_ath,delf,mo);	/* using user delete flag */
  if (derr != noErr && derr != aeObjectExists) {
    if (DBOSI)
      printf("OSCreateFile: DATA fork create failed\n");
    /* previously under a conditional on delf, but not necessary */
    /* anymore because we don't get here if the object was already there */
    cerr = unix_unlink(p_ath);		/* clean up... */
    if (cerr != noErr && DBOSI)
      printf("OSCreateFile: cleanup failed\n");
    return(derr);
  }

  strcpy(path,p_ath);
  toResFork(path,file);			/* try creating resource fork */
  rerr = unix_create(path,delf,mo);	/* ... */
  if (rerr != noErr && rerr != aeObjectExists && rerr != aeObjectNotFound) {
    if (DBOSI)
      printf("OSCreateFile: RSRC create failed\n");
    /* previously under a conditional on delf, but not necessary */
    /* anymore because we don't get here if the object was already there */
    cerr = unix_unlink(path);		/* clean up... */
    if (cerr != noErr && DBOSI)
      printf("OSCreateFile: cleanup failed\n");
    /* should we clean up data fork? */
    return(rerr);
  }

  strcpy(path,p_ath);
  toFinderInfo(path,file);		/* create finder fork */
  err = unix_create(path,delf,mo);
  /* ignore error here - exactly what should be done? */

  /* at this point, each had better be: aeObjectExists or noErr */
  if (rerr == aeObjectExists || derr == aeObjectExists)
    return(aeObjectExists);
  EModified(pdir);
  return(noErr);
}
  

export OSErr
OSOpenFork(pdir,file,mode,typ,fhdl)
IDirP pdir;				/* parent directory */
char *file;
word mode;
int typ,*fhdl;
{
  register int i;
  char path[MAXPATHLEN];
  char *ms;
  int mo;
  word attr;
  extern int sessvers;
#ifdef DENYREADWRITE
  int getAccessDenyMode();
  int setAccessDenyMode();
  int accessConflict();
  int cadm;
#endif DENYREADWRITE

  /* new for AFP2.0 */
  OSGetAttr(pdir,file,&attr);
  if ((mode & OFK_MWR) && (attr & FPA_WRI))
    return((sessvers == AFPVersion1DOT1) ? aeAccessDenied : aeObjectLocked);

  OSfname(path,pdir,file,typ);		/* expand name */

  if ((mode & ~(OFK_MRD|OFK_MWR)) != 0)
    if (DBOSI)
      printf("OSOpenFork: open mode bits are octal %o\n",mode);

  if ((mode & (OFK_MRD|OFK_MWR)) == (OFK_MRD|OFK_MWR)) {
    ms = "Read/Write";
    mo = O_RDWR;
  } else if (mode & OFK_MWR) {
    ms = "Write";
    mo = O_WRONLY;
  } else if (mode & OFK_MRD) {
    ms = "Read";
    mo = O_RDONLY;
  }

#ifdef DENYREADWRITE
  if (mo == O_WRONLY)
    mo = O_RDWR;
#endif DENYREADWRITE

  /* This is a special case hack for use with System 7.0 */
  if (*file == 'T') { /* improve performance a little */
    if (strcmp(file, "Trash Can Usage Map") == 0) {
      ms = "Read/Write";
      mo = O_RDWR;
    }
  }

  if (DBOSI) 
    printf("OSOpenFork: Opening %s for %s\n",path,ms);

#ifdef NOCASEMATCH
  if ((i = unix_open(path,mo,fhdl)) != noErr) {
    noCaseFind(path);
    i = unix_open(path,mo,fhdl);
  }
#else  NOCASEMATCH
  i = unix_open(path,mo,fhdl);
#endif NOCASEMATCH

#ifdef DENYREADWRITE
  if (*fhdl >= 0) {
    if ((cadm = getAccessDenyMode(path, *fhdl)) >= 0) {
      if (accessConflict(cadm, mode)) {
	unix_close(*fhdl);
	return(aeDenyConflict);
      }
    }
    setAccessDenyMode(path, *fhdl, mode);
  }
#endif DENYREADWRITE

  if (i == noErr) {
    attr |= ((typ == F_DATA) ? FPA_DAO : FPA_RAO);
    OSSetAttr(pdir, file, attr);
  }

  return(i);
}

private char *guestname = NULL;

export boolean
setguestid(nam)
char *nam;
{
  struct passwd *p;
  if ((p = getpwnam(nam)) == NULL) {
    logit(0,"Guest id %s NOT IN PASSWORD FILE",nam);
    logit(0,"Guest id %s NOT IN PASSWORD FILE",nam);
    return(FALSE);
  }
  if (p->pw_uid == 0) {
    logit(0,"Guest id %s is a root id!  NOT ALLOWED!", nam);
    logit(0,"Guest id %s is a root id!  NOT ALLOWED!", nam);
    logit(0,"Guest id %s is a root id!  NOT ALLOWED!", nam);
    return(FALSE);
  }
  if (p->pw_gid == 0) {
    logit(0,"Guest id %s is in group 0.  BE WARNED!", nam);
    logit(0,"Guest id %s is in group 0.  BE WARNED!", nam);
    logit(0,"Guest id %s is in group 0.  BE WARNED!", nam);
  }
  logit(0,"Guest id is %s, uid %d, primary gid %d",nam, p->pw_uid, p->pw_gid);
  guestname = nam;
  return(TRUE);
}

private boolean apasswdfile = FALSE;

export boolean
setpasswdfile(pw)
char *pw;
{

  if (desinit(0) < 0) {
    logit(0,"error: no des routines, so no aufs password file used");
    return(FALSE);
  }   
  apasswdfile = init_aufs_passwd(pw);
  return(apasswdfile);
}

/*
 * check if user exists
 *
 * IE: check if user in file specified with -P option
 * (not fatal if no file specified for DISTRIB_PASSWDS)
 *
 */

export OSErr
OSLoginRand(nam)
char *nam;
{
  OSErr err = aeParamErr;

#ifdef DISTRIB_PASSWDS
  char *cp, line[80];
  FILE *fp, *fopen();
  extern char *distribpassfile;
  if (distribpassfile == NULL)
    return(noErr);
  if ((fp = fopen(distribpassfile, "r")) == NULL)
    return(noErr);
  err = aeUserNotAuth;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == '#')
      continue;
    for (cp = line; *cp != '\0'; cp++) {
      if (*cp == ' ' || *cp == '\t' || *cp == '\n') {
	*cp = '\0';
	break;
      }
    }
    if (strcmp(nam, line) == 0) {
      err = noErr;
      break;
    }
  }
  fclose(fp);
#else  /* DISTRIB_PASSWDS */
  if (is_aufs_user(nam))
    return(noErr);
#endif /* DISTRIB_PASSWDS */

  return(err);
}

#ifdef PERMISSIVE_USER_NAME
/*
 * allow the specified user name to be
 * from the gcos field of the passwd file
 * IE: Chooser names don't have to be login names
 */
static struct passwd *
getpwgnam(nam)
unsigned char *nam;
{
  char *ptm;
  char nom[40];
  char nomi[200];
  struct passwd *pw;
  int match, i, j;

  /* map to lower case, translate some special characters */

  for (i = 0 ; nam[i] ; i++) {
    switch (nam[i]) {
      case 0x8d:
	nom[i] = 'c';
	break;
      case 0x8e:
      case 0x8f:
      case 0x90:
      case 0x91:
	nom[i] = 'e';
	break;
      case 0x92:
      case 0x93:
      case 0x94:
      case 0x95:
	nom[i] = 'i';
	break;
      case 0x96:
	nom[i] = 'n';
	break;
      case 0x97:
      case 0x98:
      case 0x99:
      case 0x9a:
      case 0x9b:
	nom[i] = 'o';
	break;
      case 0x9c:
      case 0x9d:
      case 0x9e:
      case 0x9f:
	nom[i] = 'u';
	break;
      default:
	if (isupper(nam[i]))
	  nom[i] = tolower(nam[i]);
	else
	  nom[i] = nam[i];
	break;
    }
  }
  nom[i] = '\0';
  setpwent();
  while ((pw = getpwent()) != 0) {
    ptm = pw->pw_gecos;
    for (i = 0 ; ptm && *ptm && (*ptm != ','); ptm++, i++) {
      if (isupper(*ptm))
	nomi[i] = tolower(*ptm);
      else
	nomi[i] = *ptm;
    }
    nomi[i] = '\0';
    for (match=i=j=0 ; ((nom[j] != 0) && (nomi[i] != 0)) ; ) {
      if (nomi[i] == nom[j]) {
	if (match == 0)
	  match = i+1;
	j++;
	i++;
      } else {
	if (match != 0) {
	  i = match;
	  match = 0;
	  j = 0;
	} else
	  i++;
      }
    }
    if (nom[j] == '\0') { /* found it */
      endpwent();
      return(pw);
    }
  }
  endpwent();
  return(NILPWD);
}
#endif PERMISSIVE_USER_NAME

#ifdef DISTRIB_PASSWDS
struct afppass *afp = NULL;
#endif /* DISTRIB_PASSWDS */

/*
 * validate user 'nam' using User Authentication Method 'uam'
 *
 */

export OSErr
OSLogin(nam,pwd,pwdother,uam)
char *nam,*pwd;
byte *pwdother;
int uam;
{
  struct passwd *p;
  boolean safedebug;
  byte encrypted[8];		/* 64 bits */
  byte passkey[8];		/* password is 8 bytes max */
  char *pass;
  char *crypt();
#ifdef ULTRIX_SECURITY
  char *ultrix_crypt();
  char *crypted_password;
#endif ULTRIX_SECURITY
#ifdef DIGITAL_UNIX_SECURITY
  char *bigcrypt();
  struct pr_passwd *pr;
#endif DIGITAL_UNIX_SECURITY
#ifdef LWSRV_AUFS_SECURITY
  extern char *userlogindir;
  int namlen;
#endif LWSRV_AUFS_SECURITY
#ifdef SHADOW_PASSWD
  struct spwd *sp;
  int pw_check;
#endif SHADOW_PASSWD
#ifdef RUTGERS
  extern char *ru_crypt();
#endif RUTGERS
#ifdef DISTRIB_PASSWDS
  OSErr err;
  void afpdp_encr();
  int afpdp_init(), afpdp_writ();
  struct afppass afpp, *afpdp_read();
#endif /* DISTRIB_PASSWDS */
  extern int nousrvol;

  safedebug = (DBOSI || (getuid() != 0 && geteuid() != 0));

  logit(0,"Login requested for %s (we are %srunning as root)",
      (uam == UAM_ANON) ? "<anonymous>" : nam,
      (getuid() == 0 || geteuid() == 0) ? "" : "not ");

#ifdef LWSRV_AUFS_SECURITY
  bin=malloc(strlen(nam)+1);
  strcpy(bin,nam);

  if ((tempbin = rindex(nam,':')) != NULL)
    *tempbin='\0';
#endif LWSRV_AUFS_SECURITY

  guestlogin = 0;

  switch (uam) {
  case UAM_RANDNUM:
#ifndef DISTRIB_PASSWDS
    /*
     * 'Randnum Exchange' UAM using lookaside password file
     *
     */
    if (!apasswdfile)
      return(aeBadUAM);
    if ((pass = user_aufs_passwd(nam)) == NULL) {
      logit(0, "Login: entry %s not found in password file", nam);
      return(aeUserNotAuth);
    }
    bzero(passkey,sizeof(passkey));		/* make sure zero */
    strncpy((char *)passkey, pass, 8);
#ifdef SUNOS4_FASTDES
    des_setparity(passkey);
    /* copy the data to be encrypted */
    bcopy(pwdother, encrypted, sizeof(encrypted));
    ecb_crypt(passkey,encrypted,sizeof(encrypted),DES_ENCRYPT|DES_SW);
#else SUNOS4_FASTDES
    dessetkey(passkey);
    /* copy the data to be encrypted */
    bcopy(pwdother, encrypted, sizeof(encrypted));
    endes(encrypted);
#endif SUNOS4_FASTDES
    if (bcmp(encrypted, pwd, 8) != 0) {
      logit(0, "Login: encryption failed for user %s", nam);
      return(aeUserNotAuth);
    }
    if ((p = aufs_unix_user(nam)) == NULL) {
      logit(0, "Login: no UNIX user %s", nam);
      return(aeUserNotAuth);
    }
    usrgid = p->pw_gid;
    usruid = p->pw_uid;
    usrnam = (char *)malloc(strlen(p->pw_name)+1);
    strcpy(usrnam,p->pw_name);
    usrdir = (char *)malloc(strlen(p->pw_dir)+1);
    strcpy(usrdir,p->pw_dir);
    break;
#else  /* DISTRIB_PASSWDS */
  case UAM_2WAYRAND:
    /*
     * 'Randnum Exchange' or '2-Way Randnum exchange'
     * UAM using distributed passwords (in ~/.afppass)
     *
     */
    err = aeUserNotAuth;
    if ((p = (struct passwd *)getpwnam(nam)) != NILPWD) {
      if (afpdp_init(AFP_DISTPW_FILE) >= 0) {
        if ((afp = afpdp_read(nam, p->pw_uid, p->pw_dir)) != NULL) {
          bcopy(pwdother, encrypted, sizeof(encrypted));
          if (uam == UAM_RANDNUM) {
            desinit(0);
            dessetkey(afp->afp_password);
            endes(encrypted);
            desdone();
          } else /* use key-shifted DES code */
            afpdp_encr(encrypted, afp->afp_password, NULL);
	  /* compare encrytpted passwords */
          if (bcmp(encrypted, pwd, 8) == 0)
            err = noErr;
	  /* enforce & count failed login attempts */
          if (afp->afp_numattempt >= afp->afp_maxattempt
	   && afp->afp_maxattempt > 0)
            err = aeParamErr;
	  if (err != noErr) {
	    if (afp->afp_numattempt < 255)
	      afp->afp_numattempt++;
	  } else
	    afp->afp_numattempt = 0;
	  /* update user password file (using copy of structure) */
	  bcopy((char *)afp, (char *)&afpp, sizeof(struct afppass));
	  (void)afpdp_writ(nam, p->pw_uid, p->pw_dir, &afpp);
	}
      }
    }
    if (err != noErr) {
      logit(0, "Login failed for %s (%s)", nam, afperr(err));
      return(err);
    }
    /* save details */
    usrgid = p->pw_gid;
    usruid = p->pw_uid;
    usrnam = (char *)malloc(strlen(p->pw_name)+1);
    strcpy(usrnam,p->pw_name);
    usrdir = (char *)malloc(strlen(p->pw_dir)+1);
    strcpy(usrdir,p->pw_dir);
    break;
#endif /* DISTRIB_PASSWDS */
  case UAM_ANON:
    /*
     * 'No User Authent' UAM
     *
     */
    if (guestname == NULL)
      return(aeParamErr);
    p = (struct passwd *)getpwnam(guestname);
    if (p == NILPWD) {
      logit(0, "Login: guest user not valid %s", guestname);
      return(aeParamErr);	/* unknown user */
    }  
    usrgid = p->pw_gid;
    usruid = p->pw_uid;
    usrnam = (char *)malloc(strlen(guestname)+1);
    strcpy(usrnam,guestname);
    guestlogin = 1;
    usrdir = NULL;
    break;
  case UAM_CLEAR:
    /*
     * 'Cleartxt Passwrd' UAM
     *
     */
    if (!apasswdfile) {
      p = (struct passwd *)getpwnam(nam); /* user name */
      if (p == NILPWD) {
        logit(0, "Login: user name %s NOT found in password file", nam);
#ifdef PERMISSIVE_USER_NAME
	if ((p = (struct passwd *)getpwgnam(nam)) != NILPWD) /* gcos name */
	  logit(0, "Login: mapping \"%s\" to login name %s", nam, p->pw_name);
#endif PERMISSIVE_USER_NAME
      } else {
	logit(0, "Login: user %s found, real name is %s", nam, p->pw_gecos);
      }
      if (p == NILPWD) {
	logit(0, "Login: Unknown user %s", nam);
	return(aeParamErr);	/* unknown user */
      }
      if (strlen(pwd) <= 0) {
	logit(0, "Login: NULL password access denied for %s", nam);
	return(aeUserNotAuth);	/* null user passwords not allowed */
      }
#ifdef SHADOW_PASSWD
      if (shadow_flag) {
	sp = (struct spwd *)getspnam(p->pw_name); /* get shadow info */
	if (sp == NILSPWD) {
	  logit(0, "Login: user %s NOT found in shadow file", p->pw_name);
	  return(aeParamErr);	/* unknown user */
	} else {
	  logit(0, "Login: user %s found in shadow password file", p->pw_name);
	}
	endspent();
      }
#else  SHADOW_PASSWD
      /* cope with some adjunct password file schemes */
      if (strlen(p->pw_passwd) <= 0 || strlen(pwd) <= 0) {
	logit(0, "Login: NULL password access denied for %s", nam);
	return(aeUserNotAuth);
      }
#endif SHADOW_PASSWD
#ifdef ULTRIX_SECURITY
      /* avoid evaluation order problem */
      crypted_password = ultrix_crypt(pwd, p);
      if (strcmp(crypted_password, p->pw_passwd) != 0) {
#else  ULTRIX_SECURITY
# ifdef DIGITAL_UNIX_SECURITY
      pr = getprpwnam(nam);
      if (pr == NULL || strcmp(bigcrypt(pwd, pr->ufld.fd_encrypt),
	pr->ufld.fd_encrypt) != 0) {
# else  DIGITAL_UNIX_SECURITY
#  ifndef SHADOW_PASSWD
#   ifdef RUTGERS
      if (strcmp(ru_crypt(pwd,p->pw_passwd,p->pw_uid,p->pw_name),
	p->pw_passwd) != 0) {
#   else  RUTGERS
      if (strcmp(crypt(pwd,p->pw_passwd),p->pw_passwd) != 0) {
#   endif RUTGERS
#  else  SHADOW_PASSWD
      pw_check = (shadow_flag) ?
#   ifdef RUTGERS
	strcmp(ru_crypt(pwd,sp->sp_pwdp,p->pw_uid,p->pw_name),sp->sp_pwdp) :
	strcmp(ru_crypt(pwd,p->pw_passwd,p->pw_uid,p->pw_name),p->pw_passwd);
#   else  RUTGERS
	strcmp(crypt(pwd,sp->sp_pwdp),sp->sp_pwdp) :
	strcmp(crypt(pwd,p->pw_passwd),p->pw_passwd);
#   endif RUTGERS
      if (pw_check) {
#  endif SHADOW_PASSWD
# endif DIGITAL_UNIX_SECURITY
#endif ULTRIX_SECURITY
	logit(0, "Login: Incorrect password for user %s", nam);
	if (!safedebug)
	  return(aeUserNotAuth);
      }
    } else {
      if ((p = aufs_unix_user(nam)) == NULL)
	return(aeUserNotAuth);
      if ((pass = user_aufs_passwd(nam)) == NULL)
	return(aeUserNotAuth);
      if (strcmp(pass,pwd) != 0)
	return(aeUserNotAuth);
    }
    usrgid = p->pw_gid;
    usruid = p->pw_uid;
    usrnam = (char *)malloc(strlen(p->pw_name)+1);
    strcpy(usrnam,p->pw_name);
    usrdir = (char *)malloc(strlen(p->pw_dir)+1);
    strcpy(usrdir,p->pw_dir);
    break;
  }

#ifdef ADMIN_GRP
    if (uam != UAM_ANON) {
      struct group *grps;
      if ((grps = getgrnam(ADMIN_GRP)) != NULL) {
	while (*(grps->gr_mem) != NULL) {
	  if (strcmp(p->pw_name, *grps->gr_mem) == 0) {
	    logit(0, "User %s has admin privs, logging in as superuser.",
	      p->pw_name);
	    usrgid = grps->gr_gid;
	    usruid = 0;
	    break;
	  }
	  *(grps->gr_mem)++;
	}
      }
    }
#endif /* ADMIN_GRP */

#ifdef LOG_WTMP
  /*
   * write a 'wtmp' entry for user name, address & time (then we
   * write a null entry to terminate, since wtmp is often set to
   * mode 0644 and we're not running as root when we disconnect).
   *
   * This is unduly complex for the end result
   *
   */
#ifdef LOG_WTMPX
# ifdef WTMPX_FILE
#  undef WTMP_FILE
#  define WTMP_FILE WTMPX_FILE
#  define ut_time ut_xtime
#  define utmp utmpx
# endif /* WTMPX_FILE */
#endif /* LOG_WTMPX */
  { extern AddrBlock addr;
    struct utmp ut;
    int fd;

    bzero((char *)&ut, sizeof(struct utmp));

#if defined(WTMP_FILE) && defined(USER_PROCESS)
# define utusername	ut.ut_user
    ut.ut_type = USER_PROCESS;
    ut.ut_id[0] = 'a';
    ut.ut_id[1] = 'f';
    ut.ut_id[2] = 'p';
    ut.ut_id[3] = '\0';
#else  /* WTMP_FILE && USER_PROCESS */
# define utusername	ut.ut_name
# ifdef _PATH_WTMP
#  define WTMP_FILE	_PATH_WTMP
# else  /* _PATH_WTMP */
#  define WTMP_FILE	"/var/adm/wtmp"
# endif /* _PATH_WTMP */
#endif /* WTMP_FILE && USER_PROCESS */

#ifdef LOG_WTMP_FILE
#undef  WTMP_FILE
#define WTMP_FILE	LOG_WTMP_FILE
#endif /* LOG_WTMP_FILE */

    (void)sprintf(ut.ut_host, "%d.%d.%d",
      ntohs(addr.net), addr.node, addr.skt);
    (void)strncpy(ut.ut_line, "aufs", sizeof(ut.ut_line));
    (void)strncpy(utusername, usrnam, sizeof(utusername));
    ut.ut_time = time((time_t *)0);
#ifdef LOG_WTMPX
    (void)updwtmpx(WTMP_FILE, &ut);
    ut.ut_host[0] = '\0'; /* null hostname */
    utusername[0] = '\0'; /* null username */
    (void)updwtmpx(WTMP_FILE, &ut);
#else  /* LOG_WTMPX */
    if ((fd = open(WTMP_FILE, O_WRONLY|O_APPEND, 0)) >= 0) {
      (void)write(fd, (char *)&ut, sizeof(struct utmp));
      ut.ut_host[0] = '\0'; /* null hostname */
      utusername[0] = '\0'; /* null username */
      (void)write(fd, (char *)&ut, sizeof(struct utmp));
      (void)close(fd);
    }
#endif /* LOG_WTMPX */
  }
#endif /* LOG_WTMP */

#ifdef LWSRV_AUFS_SECURITY
    /* budd... */
    if( userlogindir != NULL ) {	/* need to save user logins? */
      extern AddrBlock addr;		/* is this valid now?? seems to be! */
      char fname[ 100 ];
      FILE *f;

#ifdef HIDE_LWSEC_FILE
      if (hideLWSec(fname, userlogindir, usruid, usrgid, addr) < 0) {
	logit(0, "OSLogin: error in hideLWSec() for %s", fname);
	return(aeMiscErr);
      }
#else  HIDE_LWSEC_FILE
      /* create file before setuid call so we can write in directory. */
      make_userlogin( fname, userlogindir, addr );
#endif HIDE_LWSEC_FILE

      if( (f = fopen( fname, "w" )) != NULL ) {	/* sigh. leaves race. */
	logit(0, "writing user %s into auth-file for %s", usrnam, bin);
	  fprintf( f, "%s\n", usrnam );	/* perhaps write temp */
	  fclose( f );			/* and rename?  */
	  /* sigh. fchown and fchmod are BSDisms */
	  chmod( fname, 0644 );		/* make owned by user so they */
	  chown( fname, usruid, -1 );	/* can truncate it on exit!! */
      } /* fopen ok */
      else
	  logit(0,"Login: could not create %s: %s", fname, syserr() );
    } /* userlogindir and not guest */
    /* ...budd */
#endif LWSRV_AUFS_SECURITY
 
  if (!safedebug && setgid(usrgid) != 0) {
    logit(0,"Login: setgid failed for %s because %s",nam,syserr());
    return(aeUserNotAuth);
  }
#ifdef RUTGERS
  if ((getuid() == 0 || geteuid() == 0) && ru_initgroups(usrnam, usrgid) < 0)
#else  RUTGERS
  if ((getuid() == 0 || geteuid() == 0) && initgroups(usrnam, usrgid) < 0)
#endif RUTGERS
    logit(0,"OSLogin: initgroups failed for %s!: reason: %s",syserr(),usrnam);
  if ((ngroups = dogetgroups()) < 0) {
    logit(0,"OSLogin: getgroups failed for %s!: reason: %s",syserr(), usrnam);
    ngroups = 0;
  }

  if (!safedebug && setuid(usruid) != 0) {
    logit(0,"Login: setuid failed for %s because %s",nam,syserr());
    return(aeUserNotAuth);	/* or something */
  }

  logit(0,"Login: user %s, home directory %s",
      usrnam, usrdir == NULL ? "none" : usrdir);

  if ((usrdir != NULL) && (nousrvol != TRUE))
    VInit(usrnam,usrdir);	/* initialize volume stuff */

#ifdef USR_FILE_TYPES
  {
    char uftpath[MAXPATHLEN];
    extern char *uftfilename;

    uft_init(); /* initialize */

    /* ~user UFT file */
    if (usrdir != NULL) {
      sprintf(uftpath, "%s/%s", usrdir, TYPFILE);
      if (access(uftpath, R_OK) == 0) {
        read_uft(uftpath); /* ~/afpfile */
      } else {
        sprintf(uftpath, "%s/%s", usrdir, TYPFILE1);
        if (access(uftpath, R_OK) == 0)
          read_uft(uftpath); /* ~/.afpfile */
      }
    }
    /* global UFT file */
    if (uftfilename != NULL)
      if (access(uftfilename, R_OK) == 0)
	read_uft(uftfilename);
  }
#endif USR_FILE_TYPES

  return(noErr);
}

/*
 * change user password.
 *
 */

OSChangePassword(nam, pwdold, pwdnew, uam)
char *nam;
byte *pwdold;
byte *pwdnew;
int uam;
{
  switch (uam) {
    case UAM_CLEAR:
    case UAM_2WAYRAND:
      return(aeBadUAM);
      break;
    case UAM_RANDNUM:
#ifdef DISTRIB_PASSWDS
      { struct passwd *p;
	struct afppass *afpdp_read();
	int afpdp_writ(), afpdp_pwex();
	void afpdp_decr(), afpdp_upex();

        /* must be an existing UNIX user */
	if ((p = (struct passwd *)getpwnam(nam)) == NILPWD)
	  return(aeUserNotAuth);
        /* grab user password details */
	if ((afp = afpdp_read(nam, p->pw_uid, p->pw_dir)) == NULL)
	  return(aeUserNotAuth);
	/* enforce different passwords */
	if (bcmp(pwdold, pwdnew, 8) == 0)
	  return(aePwdSameErr);
	desinit(0);
	/* use current password to decrypt new */
	dessetkey(afp->afp_password);
	dedes(pwdnew);
	/* use new password to decrypt old */
	dessetkey(pwdnew);
	dedes(pwdold);
	desdone();
	/* old password OK ? */
	if (strncmp((char *)afp->afp_password, (char *)pwdold,
	 strlen((char *)afp->afp_password)) != 0)
	  return(aeUserNotAuth);
	/* enforce password length control */
	if (strlen((char *)pwdnew) < afp->afp_minmpwlen)
	  return(aePwdTooShort);
	/* global expiry date, user changes not allowed */
	if (afpdp_pwex(afp) < 0)
	  return(aeAccessDenied);
	/* update structure & write */
	afpdp_upex(afp);
	afp->afp_numattempt = 0;
	bcopy((char *)pwdnew, (char *)afp->afp_password, 8);
	if (afpdp_writ(nam,  p->pw_uid, p->pw_dir, afp) < 0)
	  return(aeUserNotAuth);
	return(noErr);
      }
#else  /* DISTRIB_PASSWDS */
      return(aeBadUAM);
#endif /* DISTRIB_PASSWDS */
      break;
  }

  return(aeBadUAM);
}

/*
 * OSErr OSExchangeFiles(IDirP apdir, char *afile, IDirP apdir, char *bfile)
 *
 * OSExchangeFiles swaps the data and resource forks but not the finder info
 * of two files.
 *
 * Inputs:
 *  apdir     parent directory id of one file.
 *  afile     name of second file.
 *  bpdir     parent directory id of second file.
 *  bfile     name of second file.
 *
 * Outputs:
 *  OSErr     Function result.
 *
 * Error recovery during renaming process is problematic ...
 *
 */

export OSErr
OSExchangeFiles(apdir,afile,bpdir,bfile)
IDirP apdir,bpdir;                    /* parent dirs */
char *afile, *bfile;                  /* file names */
{
  char a_path[MAXPATHLEN], b_path[MAXPATHLEN], t_path[MAXPATHLEN];
  char apath[MAXPATHLEN], bpath[MAXPATHLEN], tpath[MAXPATHLEN];
  char *temp = ".tXXX";
  int err, cerr, amo, bmo;
  extern int sessvers;
  struct stat stb;
  word attr;

  /*
   * either file rename-inhibited ?
   *
   */
  OSGetAttr(apdir, afile, &attr);
  if (attr & FPA_RNI)
    return((sessvers == AFPVersion1DOT1) ? aeAccessDenied : aeObjectLocked);

  OSGetAttr(bpdir, bfile, &attr);
  if (attr & FPA_RNI)
    return((sessvers == AFPVersion1DOT1) ? aeAccessDenied : aeObjectLocked);

  OSfname(a_path, apdir, afile, F_DATA); /* build A data file name */
  OSfname(b_path, bpdir, bfile, F_DATA); /* same for B file */
  OSfname(t_path, apdir, temp,  F_DATA); /* same for tmp file */

#ifdef NOCASEMATCH
  noCaseMatch(a_path);
  noCaseMatch(b_path);
#endif NOCASEMATCH

  if (DBOSI)
    printf("OSExchangeFiles A=%s, B=%s\n", a_path, b_path);

  /*
   * can't exchange a file with itself
   *
   */
  if (strcmp(a_path, b_path) == 0)
    return(aeSameObjectErr);

  /*
   * Not allowed if one doesn't have a resource directory
   * and the other does.
   *
   */
  if ((apdir->flags & DID_RESOURCE) != (bpdir->flags & DID_RESOURCE))
    return(aeParamErr);

  /*
   * get info on existing files so we can set them back afterwards
   *
   */
  if ((err = unix_stat(a_path, &stb)) != noErr)
    return(err);
  amo = filemode(stb.st_mode, stb.st_uid, stb.st_gid);

  if ((err = unix_stat(b_path, &stb)) != noErr)
    return(err);
  bmo = filemode(stb.st_mode, stb.st_uid, stb.st_gid);

  /*
   * build resource file names
   *
   */
  if (apdir->flags & DID_RESOURCE) {
      strcpy(apath, a_path);
      strcpy(bpath, b_path);
      strcpy(tpath, t_path);
      toResFork(apath, afile);
      toResFork(bpath, bfile);
      toResFork(tpath, temp);
  }

  /*
   * First: Rename the A data and resource forks as a temporary.
   *
   */
  err = unix_rename(a_path, t_path);
  if (err != noErr)                   /* if an error on data files */
    return(err);                      /*  then give up */

  if (apdir->flags & DID_RESOURCE) {
    err = unix_rename(apath, tpath);
    /* allow non-existant resource */
    if (err != noErr && err != aeObjectNotFound) { /* error on rename? */
      if (DBOSI)
        printf("os_rename: failed %s for %s -> %s\n",
          afperr(err), apath, tpath);
      cerr = unix_rename(t_path, a_path); /* rename data back to original */
      if (cerr != noErr && DBOSI)
        printf("os_rename: cleanup failed\n");
      unix_chmod(b_path, bmo); /* file:try to reset protection */
      return(err);
    }
  }

  /*
   * Second: Rename the B file as A.
   *
   */
  err = unix_rename(b_path, a_path);
  if (err != noErr) {
    /* put A back as it was */
    unix_rename(t_path, a_path);
    unix_rename(tpath, apath);
    return(err);
  }

  if (apdir->flags & DID_RESOURCE) {
    err = unix_rename(bpath, apath);
    /* allow non-existant resource */
    if (err != noErr && err != aeObjectNotFound) { /* error on rename? */
      if (DBOSI)
        printf("os_rename: failed %s for %s -> %s\n",
	  afperr(err), bpath, apath);
      cerr = unix_rename(a_path, b_path); /* rename data back to original */
      if (cerr != noErr && DBOSI)
        printf("os_rename: cleanup failed\n");
      unix_chmod(b_path, bmo); /* file:try to reset protection */
      /* put A back as it was */
      unix_rename(t_path, a_path);
      unix_rename(tpath, apath);
      return(err);
    }
  }

  /*
   * Third: Rename the T file as B.
   *
   */
  err = unix_rename(t_path, b_path);
  if (err != noErr) {
    /* put B back as it was */
    unix_rename(a_path, b_path);
    unix_rename(apath, bpath);
    /* put A back as it was */
    unix_rename(t_path, a_path);
    unix_rename(tpath, apath);
    return(err);
  }

  if (apdir->flags & DID_RESOURCE) {
    err = unix_rename(tpath, bpath);
    /* allow non-existant resource */
    if (err != noErr && err != aeObjectNotFound) { /* error on rename? */
      if (DBOSI)
        printf("os_rename: failed %s for %s -> %s\n",
	  afperr(err), tpath, bpath);
      cerr = unix_rename(b_path, t_path); /* rename data back to original */
      if (cerr != noErr && DBOSI)
        printf("os_rename: cleanup failed\n");
      unix_chmod(t_path, bmo); /* file:try to reset protection */
      /* put B back as it was */
      unix_rename(a_path, b_path);
      unix_rename(apath, bpath);
      /* put A back as it was */
      unix_rename(t_path, a_path);
      unix_rename(tpath, apath);
      return(err);
    }
  }

  FModified(apdir, afile);    /* does an emodified */
  FModified(bpdir, bfile);    /* does an emodified */

  return(noErr);
}

export word 
OSRandom()
{
  static time_t t = 0;

  if (t == 0) {
    time(&t);
#ifdef USERAND
    srand(t);
#else
    srandom(t);
#endif
  }
#ifdef USERAND
  return((word) rand());
#else
  return((word) random());
#endif
}

sdword
CurTime()
{
  return(time(0));
}

/*
 * char *tilde(char *s)
 *
 * Expands a path starting with tilde, the same as the shell.
 * Returns the expanded path.
 *
 */
export char *
tilde(s)
char *s;
{
  static char path[MAXPATHLEN];
  char *sp,*logdir(),*l;
  
  if (*s != '~')			/* start with tilde? */
    return(s);				/* no, return original */
  s++;					/* skip over tilde */
  if (*s == '\0')			/* if nothing more, return */
    return(usrdir);

  if (*s == '/') {			/* case of ~/ */
    strcpy(path,usrdir);		/* use user's dir */
    strcat(path,s);			/*  and then the remainder */
    return(path);			/* return that */
  }

  if ((sp = index(s,'/')) == NULL)	/* check for slash */
    return(logdir(s));			/*  return ~john expanded */

  *sp = '\0';				/* otherwise tie off ~bill/mac */
  if ((l = logdir(s)) == NULL)		/* does the user exist? */
	return NULL;
  strcpy(path,l);			/* copy in the expanded ~bill */
  *sp = '/';				/* ... put back slash */
  strcat(path,sp);			/* append the remainder */
  return(path);				/* and return it */
}
  
export char *
logdir(user)
char *user;
{
  struct passwd *p;

  if (usrnam != NULL && strcmp(user,usrnam) == 0)
    return(usrdir);		/* already know logged in user dir */
    
  p = (struct passwd *) getpwnam(user);
  if (p != NILPWD)
    return(p->pw_dir);
  return(NULL);
}
  
private OSErr
unix_rmdir(path)
char *path;
{
  if (DBUNX)
    printf("unix_rmdir: path=%s\n",path);

  if (rmdir(path) == 0)			/* and try to delete it */
    return(noErr);

  if (DBUNX)
    printf("unix_rmdir: failed %s\n",syserr());

  return(ItoEErr(errno));
}

private OSErr
unix_unlink(path)
char *path;
{
  if (DBUNX)
    printf("unix_unlink: path=%s\n",path);

  if (unlink(path) == 0)	/* remove the file */
    return(noErr);		/* no error */

  if (DBUNX)
    printf("unix_unlink: failed %s\n",syserr());

  return(ItoEErr(errno));
}  

private OSErr
unix_rename(from,to)
char *from,*to;
{
  if (DBUNX)
    printf("unix_rename: from %s to %s\n",from,to);

#ifdef aux
  if (strcmp(from, to) == 0)
    return(noErr);
#endif aux
  if (rename(from,to) == 0)
    return(noErr);

#ifdef XDEV_RENAME
  return(xdev_rename(from,to));
#else  XDEV_RENAME
  if (DBUNX)
    printf("unix_rename: failed %s\n",syserr());
  return(ItoEErr(errno));
#endif XDEV_RENAME
}

#ifdef XDEV_RENAME
private OSErr
xdev_rename(from,to)
char *from, *to;
{
  struct stat fstb, tstb;
  int err, mode, ffd, tfd;

  if (DBUNX)
    printf("xdev_rename: from %s to %s\n",from,to);

  if ((err = unix_stat(from,&fstb)) != noErr)
    return(ItoEErr(errno));

  /* if "from" is a directory, recursively copy it */
  if (S_ISDIR(fstb.st_mode)) {
#ifdef USEDIRENT
    struct dirent *dinfp, *readdir();
#else  USEDIRENT
    struct direct *dinfp, *readdir();
#endif USEDIRENT
    char fname[MAXPATHLEN];
    char tname[MAXPATHLEN];
    char *fend, *tend;
    DIR *dptr;

    if (DBUNX)
      printf("xdev_rename: copying directory ...\n");

    /* Create a destination directory with same owner, group */
    if ((err = unix_mkdir(to,fstb.st_mode)) != noErr)
      return(err);
    if ((err = unix_chown(to,fstb.st_uid,fstb.st_gid)) != noErr)
      return(err);

    /* Read each item in the "from" dir and recurse to move it */
    if ((dptr = opendir(from)) == NULL)
      return(ItoEErr(errno));

    fend = fname + strlen(strcpy(fname,from));
    tend = tname + strlen(strcpy(tname,to));
    *fend++ = '/';
    *tend++ = '/';

    for (dinfp = readdir(dptr); dinfp != NULL; dinfp = readdir(dptr)) {
      if (*dinfp->d_name == '.') {
	int namlen;
#if defined(USEDIRENT) && !defined(SOLARIS)
        namlen = dinfp->d_namlen;
#else  /* USEDIRENT */
        namlen = strlen(dinfp->d_name);
#endif /* USEDIRENT */
	if ((namlen == 1) ||
	   ((namlen == 2) && (*(dinfp->d_name+1) == '.')))
	  continue;
      }
      *fend = *tend = '\0';
      strcat(fname,dinfp->d_name);
      strcat(tname,dinfp->d_name);
      if ((err = xdev_rename(fname,tname)) != noErr) {
	if (DBUNX)
	  printf("xdev_rename: copy failed %s\n",syserr());
	closedir(dptr);
	return(err);
      }
    }
    closedir(dptr);

    /* Finally, remove the directory */
    return(unix_rmdir(from));
  } else {
    if ((err = os_copy(from,to,fstb.st_mode)) != noErr)
      return(ItoEErr(err));

    /* Remove the copied file */
    return(unix_unlink(from));
  }
}
#endif XDEV_RENAME

private OSErr
unix_open(path,mode,fd)
char *path;
int mode;
int *fd;
{

  *fd = open(path,mode);

#ifdef APPLICATION_MANAGER
  {
    int lockn, protect;
    extern int fdplist[NOFILE];
    extern struct flist *applist;

    /* don't check if we aren't read-only or open failed */
    if (applist != NULL && *fd >=0 && mode == O_RDONLY) {
      if (wantLock(path, &lockn, &protect) == 0) {
        if (enforceLock(*fd, lockn) == 0) {
          if (DBUNX)
            printf("unix_open: open refused for %s (O > %d)\n", path, lockn);
	  close(*fd);
	  return(aeLockErr);
        }
	if (protect == 1) /* protect from copying */
	  fdplist[*fd] = 1;
      }
    }
  }
#endif APPLICATION_MANAGER

  if (DBUNX)
    printf("unix_open: fd=%d, mode=%d, path=%s\n",*fd,mode,path);

  if ((*fd) > 0)
    return(noErr);

  if (DBUNX)
    printf("unix_open: failed %s\n",syserr());

  return(ItoEErr(errno));
}

private OSErr
unix_close(fd)
int fd;
{
  if (DBUNX)
    printf("unix_close: fd=%d\n",fd);

#ifdef APPLICATION_MANAGER
  {
    extern int fdplist[NOFILE];
    fdplist[fd] = -1;
  }
#endif APPLICATION_MANAGER

#ifdef DENYREADWRITE
  {
    struct accessMode *p, *q;

    p = accessMQueue;
    q = (struct accessMode *)NULL;

    while (p != (struct accessMode *)NULL) {
      if (p->fd == fd) { /* delete from Q */
	if (q == (struct accessMode *)NULL)
	  accessMQueue = p->next;
	else
	  q->next = p->next;
	free((char *)p);
	break;
      }
      q = p;
      p = p->next;
    }
  }
#endif DENYREADWRITE

  if (close(fd) == 0)
    return(noErr);

  if (DBUNX)
    printf("unix_close: failed %s\n",syserr());

  return(ItoEErr(errno));		/* this would be a problem */
}

private OSErr
unix_mkdir(path,prot)
char *path;
int prot;				/* protection */
{
  if (DBUNX)
    printf("unix_mkdir: path = %s\n",path);

  if (mkdir(path,prot) == 0)
    return(noErr);

  if (DBUNX)
    printf("unix_mkdir: failed %s\n",syserr());

  return(ItoEErr(errno));
}

/*
 * OSErr unix_create(char *path, int delf, int mode)
 *
 * Create a file.
 *
 */

private OSErr
unix_create(path,delf,mode)
char *path;
int delf;
int mode;
{
  int fd,flg;

  if (DBUNX)
    printf("unix_create: delf=%d, mode=o%o, path=%s\n",delf,mode,path);

  flg = (delf) ? (O_CREAT | O_TRUNC) : (O_CREAT | O_EXCL);

  if ((fd = open(path,flg,mode)) != -1) {
    (void) close(fd); 
    return(noErr);
  }

  if (DBUNX)
    printf("unix_create: failed %s\n",syserr());

  return(ItoEErr(errno));
}

/*
 * OSErr unix_createo(char *path, int delf, int mode, int *fd)
 *
 * Create a file and return the open file handle.
 *
 */

private OSErr
unix_createo(path,delf,mode,fd)
char *path;
int delf;
int mode;
int *fd;
{
  int flg;

  if (DBUNX)
    printf("unix_createo: delf=%d, path=%s\n",delf,path);

  flg = (delf) ? (O_CREAT | O_TRUNC) : (O_CREAT | O_EXCL);
  flg |= O_RDWR;

  if ((*fd = open(path,flg,mode)) != -1)
    return(noErr);

  if (DBUNX)
    printf("unix_createo: failed %s\n",syserr());

  return(ItoEErr(errno));
}

#ifdef NOCHGRPEXEC
#ifndef USECHOWN
#define USECHOWN
#endif  USECHOWN
#endif NOCHGRPEXEC

private OSErr
unix_chown(path,own,grp)
char *path;
int own,grp;
{
  char gid[20];			/* big enough */
  int pid, npid;
  WSTATUS status;
#ifndef USECHOWN
  struct stat stb;
  OSErr err;
#endif  USECHOWN

  if (DBOSI) 
    printf("unix_chown: Attempting chown %s to owner %d, group %d\n",
	   path,own,grp);
#ifndef USECHOWN
  if (usruid != 0) {		/* not root, then do it hard way */
    if (grp < 0) {
      if (DBOSI)
	printf("unix_chown: skipping owner and group for %s\n",path);
      return(noErr);
    }
    if (DBOSI) 
      printf("unix_chown: skipping owner, chgrp %s to group %d\n",path,grp);
    if ((err = unix_stat(path, &stb)) != noErr)
      return(err);
    if (stb.st_gid == grp)	/* naught to do */
      return(noErr);
    sprintf(gid, "%d",grp);
#ifdef NOVFORK
    if ((pid=fork()) == 0) {
#else  NOVFORK
    if ((pid=vfork()) == 0) {
#endif NOVFORK
      execl("/bin/chgrp","chgrp",gid,path, 0);
      _exit(1);			/* no false eofs */
    }
    while ((npid = wait(&status)) != pid) 
      /* NULL */;
    /* right half of status is non-zero if */
    /*  (a) stopped (&0xff == 0177) */
    /* or */
    /*  (b) signaled  (0x7f != 0) */
    /*  (c) coredumped (0x80 != 0) */
#if defined(WIFSTOPPED) && defined(WIFSIGNALED) && defined(W_COREDUMP)
    if (WIFSTOPPED(status) || WIFSIGNALED(status) || (W_COREDUMP(status) != 0))
      return(aeAccessDenied);	/* oh well */
#else /* WIFSTOPPED && WIFSIGNALED && W_COREDUMP */
    if ((status & 0xff) != 0)
      return(aeAccessDenied);	/* oh well */
#endif/* WIFSTOPPED && WIFSIGNALED && W_COREDUMP */
    /* retcode is leftmost 8 bits */
#ifdef W_RETCODE
    if (W_RETCODE(status) != 0)
      return(aeAccessDenied);	/* oh well */
#else /* W_RETCODE */
    if ((status>>8) != 0)
      return(aeAccessDenied);	/* oh well */
#endif /* W_RETCODE */
    return(noErr);
  }
#endif USECHOWN
#ifdef NOCHGRPEXEC
  if (usruid != 0) {		/* not root, ignore user */
    if (grp < 0) {
      if (DBOSI)
	printf("unix_chown: skipping owner and group for %s\n",path);
      return(noErr);
    }
    own = -1;
    if (DBOSI)
      printf("unix_chown: skipping owner, chgrp %s to group %d\n",path,grp);
  }
#endif NOCHGRPEXEC
  /* root can do what it pleases, so can any user on sysv */
  if (chown(path, own, grp) < 0)
    return(ItoEErr(errno));
  return(noErr);
}


private OSErr
unix_chmod(path,mode)
char *path;
u_short mode;
{
  if (DBUNX)
    printf("unix_chmod: mode=o%o path=%s\n",mode,path);

  if (chmod(path,(int) mode) == 0)
    return(noErr);

  if (DBUNX)
    printf("unix_chmod: failed %s\n",syserr());

  return(ItoEErr(errno));
}

private OSErr
unix_stat(path,stb)
char *path;
struct stat *stb;
{
  if (DBUNX)
    printf("unix_stat: path=%s\n",path);

  if (stat(path,stb) == 0)
    return(noErr);

  if (DBUNX)
    printf("unix_stat: failed %s\n",syserr());

  return(ItoEErr(errno));
}

/*
 * figure out the mode a file should have based on the uid, gid, and mode
 * of its parents.  Mainly for drop folders.
 *
 * really shouldn't have to do this -- instead change the owner
 * of the file -- however: (a) bsd doesn't allow and (b) must do after
 * all file operations because we don't have handle -- mucho work --
 * if we could.
 *
*/
private int
filemode(mode, uid, gid)
int mode, uid, gid;
{
  int mo = mode & 0777;		/* convert st_mode to mode */
  if (uid != usruid) {
    /* check for conditions that would mean drop folder for us */
    /* (but, don't accept a drop folder on basis of group that is */
    /* world viewable even though it really is a drop folder for us) */
    if ((mo & 04) == 0 && (mo & 040) == 0 && OSInGroup(gid))
      mo |= 0666;		/* let everyone else read/write */
    /* We need to do this because the poor person who get's the file */
    /* can't do anything with it otherwise */
  }
  return(mo);
}

private char *
syserr()
{
#if !(defined(__FreeBSD__) || defined(__NetBSD__))
  extern char *sys_errlist[];
#endif
  extern int sys_nerr;
  static char buf[50];
  int serrno;

  serrno = errno;
  if (serrno < 0 || serrno > sys_nerr) {
    sprintf(buf,"Unknown error %d",serrno);
    return(buf);
  }
  return(sys_errlist[serrno]);
}

private OSErr
ItoEErr(num)
int num;
{
  extern int sessvers;			/* AFP protocol version */

  switch (num) {
  case EPERM:				/* Not owner */
    return(aeAccessDenied);
  case ENOENT:				/* No such file or directory */
    return(aeObjectNotFound);
  case EACCES:				/* Permission denied  */
    return(aeAccessDenied);
  case EEXIST:				/* File exists */
    return(aeObjectExists);
  case ENOTDIR:				/* Not a directory */
    return(aeDirNotFound);
  case EISDIR:				/* Is a directory */
    return(aeObjectTypeErr);
  case ENFILE:				/* File table overflow */
    return(aeDiskFull);
  case EMFILE:				/* Too many files open */
    return(aeTooManyFilesOpen);
  case ETXTBSY:				/* Text file busy */
    return(aeFileBusy);
  case ENOSPC:				/* No space left on device */
    return(aeDiskFull);
  case EROFS:				/* read only file system */
    if (sessvers == AFPVersion1DOT1)
      return(aeAccessDenied);
    else
      return(aeVolumeLocked);
#ifndef AIX
# ifdef ENOTEMPTY
  case ENOTEMPTY:
    return(aeDirNotEmpty);
# endif ENOTEMPTY
#endif  AIX
#ifdef EDQUOT
  case EDQUOT:
    return(aeDiskFull);
#endif EDQUOT
  default:
    if (DBUNX)
      printf("ItoEErr: Unknown unix error code %d\n",errno);
    return(aeMiscErr);
  }
}

#ifdef ULTRIX_SECURITY
char *
ultrix_crypt(pwd, pw)
char *pwd;
struct passwd *pw;
{
  extern char *crypt(), *crypt16();
  extern AUTHORIZATION *getauthuid();
  AUTHORIZATION *au;
  struct svcinfo *si;
  char *passwd;

  /*
   * the asterisk means that the real encrypted password
   * is in the auth file.  But we really should check to
   * see if the security level is either SEC_UPGRADE or
   * SEC_ENHANCED and the password is an asterisk because
   * the security level could be BSD and someone put an
   * asterisk in to turn an account off, but if that's the
   * case the right thing will happen here anyways (i.e.,
   * nothing encrypts to a single asterisk so the test will
   * fail).
   */
  if (strcmp(pw->pw_passwd, "*") == 0) {
    si = getsvc();
    if ((si->svcauth.seclevel == SEC_UPGRADE) ||
        (si->svcauth.seclevel == SEC_ENHANCED)) {
      /*
       * if they aren't in the auth file return
       * the empty string.  this can't match since
       * we've already thrown out empty passwords.
       */
        if ((au = getauthuid(pw->pw_uid)) == NULL)
          return("");
        pw->pw_passwd = au->a_password;
    }
    return(crypt16(pwd, pw->pw_passwd));
  }
  return(crypt(pwd, pw->pw_passwd));
}
#endif ULTRIX_SECURITY
#ifdef APPLICATION_MANAGER

/*
 * Enforce control on the number of file opens (or Applications
 * run) by checking our 'Application List' and attempting to apply
 * a single byte-range read lock on the file resource fork at byte N.
 * Can also specify no Finder copying with 'P' flag on number.
 *
 * djh@munnari.OZ.AU
 * September, 1991
 *
 */

int
wantLock(file, num, protect)
char *file;
int *num, *protect;
{
	int cmpval;
	struct flist *applp;
	extern struct flist *applist;

	applp = applist;
	while (applp != NULL) {
	  /* check the SORTED list, return 0 if found */
	  if ((cmpval = strcmp(file, applp->filename)) <= 0) {
	    *num = applp->incarnations;
	    *protect = applp->protected;
	    return(cmpval);
	  }
	  applp = applp->next;
	}
	return(1);
}

int
enforceLock(fd, maxm)
int fd, maxm;
{
	int i;
	struct flock flck;

	for (i = 1; i <= maxm; i++) {
	  flck.l_type = F_WRLCK;
	  flck.l_whence = 0;	/* SEEK_SET */
	  flck.l_start = i+4;
	  flck.l_len = 1;

	  if (fcntl(fd, F_GETLK, &flck) == -1)
	    return(-1); /* not supported ? */

	  if (flck.l_type == F_RDLCK)
	    continue;

	  if (flck.l_type == F_UNLCK) {
	    flck.l_type = F_RDLCK;
	    flck.l_whence = 0;	/* SEEK_SET */
	    flck.l_start = i+4;
	    flck.l_len = 1;
	    if (fcntl(fd, F_SETLK, &flck) == -1)
	      return(-1); /* not supported ? */

	    return(1); /* success */
	  }
	}
	return(0); /* no locks left */
}
#endif APPLICATION_MANAGER
#ifdef HIDE_LWSEC_FILE
/* 
 * int HideLWSec(char *fname, char *userlogindir, int usruid, int
 *		 usrgid, AddrBlock addr )
 *
 * Add additional security to LW security flag file when using
 * LWSRV_AUFS_SECURITY. Only relevant if both HIDE_LWSEC_FILE
 * and LWSRV_AUFS_SECURITY defined in m4.features.
 * Original flag file in world read/writeable directory
 * permitted links and "borrowing" laserWriters from others, thus
 * circumventing laser page charges. 
 * This creates a directory with user id ownership and the flag
 * file is placed in this directory.
 *
 */

int
hideLWSec(fname, userlogindir, usruid, usrgid, addr)
char *fname, *userlogindir;
int usruid, usrgid;
AddrBlock addr;
{
  char protecteddir[MAXPATHLEN], flagfile[MAXPATHLEN];
  struct stat dbuf;
  DIR *locdirp;

  (void) strcpy(protecteddir, userlogindir);
  make_userlogin(fname, protecteddir, addr);
  (void) strcpy(protecteddir, fname);
  fname[0] = '\0';
  make_userlogin(fname, protecteddir, addr); /* create flag file */

  if (stat(protecteddir, &dbuf) == 0) {
    /* dir found and stat sucessful, we need to zap dir */
    if (stat(fname, &dbuf) == 0)
      if (S_ISREG(dbuf.st_mode))
        if (unlink(fname) < 0 )
          logit(0, "hideLWSec: errno=%d unlinking %s\n", errno, fname);
    if (rmdir(protecteddir ) < 0 ) {
      logit(0, "hideLWSec: errno=%d Can't zap %s\n", errno, fname);
      return(-1);
    }
  } else /* error occured in stat, but not no entry */
    if (errno != ENOENT ) {
      logit(0, "hideLWSec: stat errno= %d for %s\n", errno, fname);
      return(-1);
    }
  if (mkdir(protecteddir, 0700) < 0) {
    logit(0, "hideLWSec: unable to create %s,errno=%d\n", fname, errno);
    return(-1);
  } else {
    chown(protecteddir, usruid, usrgid);
  }
  return(0);
}
#endif HIDE_LWSEC_FILE
#ifdef DENYREADWRITE
/*
 * Implement full "access modes" and "deny modes" as
 * specified in Inside AppleTalk 2nd Ed. page 13-35.
 *
 * Reserve four single byte locks at the beginning of
 * each fork (ie: byte range locks are offset by four
 * from their protected data. This is OK since locks
 * are maintained by kernel tables and are independent
 * of the data and the actual file length).
 *
 * byte 0 = access mode read bit
 * byte 1 = access mode write bit
 * byte 2 = deny mode read bit
 * byte 3 = deny mode write bit
 *
 * DENYREADWRITE uses fcntl(2) advisory F_RDLCKs. F_GETLK
 * returns F_UNLCK if the requesting process holds the
 * specified lock. Thus, there is no way for a process to
 * determine if it is still holding a specific lock. For
 * this reason we maintain a separate queue of access modes
 * and file names for files opened by this process.
 *
 */

int
getAccessDenyMode(path, fd)
char *path;
int fd;
{
  int i;
  int mode = 0;
  int mask = 0x01;
  struct flock flck;
  struct accessMode *p = accessMQueue;

  for (i = 1; i <= 4; i++) {
    flck.l_type = F_WRLCK;
    flck.l_whence = 0; /* SEEK_SET */
    flck.l_start = i;
    flck.l_len = 1;
    if (fcntl(fd, F_GETLK, &flck) != -1)
      if (flck.l_type != F_UNLCK)
        mode |= mask;
    mask <<= 1;
  }

  while (p != (struct accessMode *)NULL) {
    if (strcmp(p->path, path) == 0)
      mode |= p->mode;
    p = p->next;
  }

  return(((mode & 0x0c) << 2) | (mode & 0x03));
}

/*
 * set access and deny modes on fd for 'path'
 *
 */

int
setAccessDenyMode(path, fd, mode)
char *path;
int fd, mode;
{
  int i;
  int mask = 0x01;
  struct flock flck;
  struct accessMode *p;

  mode = ((mode & 0x30) >> 2) | (mode & 0x03);

  for (i = 1; i <= 4; i++) {
    if (mode & mask) {
      flck.l_type = F_RDLCK;
      flck.l_whence = 0; /* SEEK_SET */
      flck.l_start = i;
      flck.l_len = 1;
      if (fcntl(fd, F_SETLK, &flck) == -1)
	mode &= ~mask;
    }
    mask <<= 1;
  }

  if ((p = (struct accessMode *)malloc(sizeof(struct accessMode))) != NULL) {
    strcpy(p->path, path);
    p->fd = fd;
    p->mode = mode;
    p->next = accessMQueue;
    accessMQueue = p;
  }

  return(((mode & 0x0c) << 2) | (mode & 0x03));
}

/*
 * Inside AppleTalk, 2nd Ed.
 * Table 13-1: Synchronization rules
 *
 */

int accessTable[16] = {
  0x0000, 0xf0f0, 0xff00, 0xfff0,
  0xaaaa, 0xfafa, 0xffaa, 0xfffa,
  0xcccc, 0xfcfc, 0xffcc, 0xfffc,
  0xeeee, 0xfefe, 0xffee, 0xfffe
};

/*
 * check current file access/deny
 * mode against requested mode
 *
 */

int
accessConflict(cadm, mode)
int cadm, mode;
{
  cadm = ((cadm & 0x30) >> 2) | (cadm & 0x03);
  mode = ((mode & 0x30) >> 2) | (mode & 0x03);

  return((accessTable[cadm] >> mode) & 0x01);
}
#endif DENYREADWRITE
