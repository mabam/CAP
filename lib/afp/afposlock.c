/*
 * $Author: djh $ $Date: 1996/03/07 09:52:04 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afposlock.c,v 2.12 1996/03/07 09:52:04 djh Rel djh $
 * $Revision: 2.12 $
 *
 */

/*
 * afposlock.c - Appletalk Filing Protocol OS Interface for Byte Range Lock
 * and other file lock routines
 *
 * Four cases:	(a) have flock, (b) have lockf, (c) have sysV locking
 *		(d) have both lockf and flock
 * For A: can't do byte range lock
 * For B: can't lock out writers on read-only files
 * For C: just right
 * For D: just right
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July 1987     CCKim	Created.
 *
 */

#ifdef NeXT
#define NOLOCKF
#endif NeXT

/*
 * This is a fall-back in case Configure messes up.
 * flock() lives in /lib/libbsd.a under IBM AIX and Configure won't
 * normally find it. Usually we could live with that except that the
 * result of not using flock() with AIX is a disaster, second and
 * subsequent AppleShare sessions hang until the first one finishes.
 */

#ifdef AIX
# ifdef NOFLOCK
#  undef NOFLOCK
# endif NOFLOCK
#endif AIX

#ifdef SOLARIS
#define USEFCNTLLOCK
#include <sys/types.h>
#include <fcntl.h>
#endif SOLARIS

#ifndef NOLOCKF
/* on convex, lockf requires fcntl.h */
# if defined(LOCKFUSESFCNTL) || defined(USEFCNTLLOCK)
#  include <fcntl.h>
# else /* LOCKFUSESFCNTL || USEFCNTLLOCK */
#  ifdef apollo
    /* include F_LOCK etc. in unistd.h defns. */
#   define _INCLUDE_SYS5_SOURCE
#  endif apollo
#  include <unistd.h>
#  ifdef apollo
#    include <sys/param.h>
#    include <netat/fcntldomv.h>
#  endif apollo
   /*
    * unistd defines these unnecessarily
    * (and problematically)
    *
    */
#  ifdef ultrix
#   undef R_OK
#   undef W_OK
#   undef X_OK
#   undef F_OK
#  endif ultrix
#  ifdef pyr
#   undef R_OK
#   undef W_OK
#   undef X_OK
#   undef F_OK
#  endif pyr
# endif /* LOCKFUSESFCNTL || USEFCNTLLOCK */
#endif NOLOCKF

#ifdef gould
#include <lockf.h>
#endif gould

#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>

#ifdef NOFLOCK
# ifndef NOLOCKF
#  ifndef LOCKF_ONLY
#   define LOCKF_ONLY
#  endif  LOCKF_ONLY
# endif  NOLOCKF
#endif NOFLOCK

#ifdef xenix5
# include <sys/locking.h>
# define XENIX_LOCKING
#endif xenix5

/*
 * The following routines define Byte Range Locking
 * 
 * The following system calls (that only exist on some machines) will
 * do the right thing:
 *  lockf - system V routine 
 *
 * For systems without lockf, etc., someone has to hack together
 * some system daemon or kernel driver.
 *
*/

#ifdef NOLOCKF
/* These are here for systems that can't do byte range lock */

OSByteRangeLock(fd, offset, length, unlockflag, startendflag, fpos)
int fd;
sdword offset, length;
int unlockflag, startendflag;
sdword *fpos;
{
#ifdef DEBUG
  printf("OS Byte Range lock: unimplemented on this OS\n");
#endif DEBUG
#ifdef notdef
  return(aeMiscErr);
#endif notdef
  return(aeParamErr);	/* specially for System 7.0 */
}  

OSErr
OSTestLock(fd, length)
int fd;
sdword length;
{
  return(noErr);
}
#else NOLOCKF
/*
 * Institute a byte range lock on a file
 *
 * offset can be negative if startendflag = 1 and then denotes
 * offset relative from end of fork
 * length must be positive
 * unlockflag is set if want unlock
 *
 * Unlike AFP Spec: multiple ranges may overlap and unlock thusly
 * may return NoMoreLocks as an error.
 *
 */

OSErr
OSByteRangeLock(fd, offset, length, flags, fpos)
int fd;
sdword offset, length;
byte flags;
sdword *fpos;
{
  int pos, err;
  int startendflag = (BRL_START & flags);
  int unlockflag = (BRL_UNLOCK & flags);
  extern int errno;

#ifdef DEBUG
  printf("OSBRL: %slocking offset %ld, length %x (%ld), from %s\n",
	 unlockflag ? "un" : "", offset, length, length,
	 startendflag ? "end" : "start");
#endif DEBUG

  if (length < 0		/* can only be -1 */
   || length == 0x7fffffff)	/* or magic flag used by MicroSoft */
    length = 0;			/* length zero means entire file! */

#ifdef USEFCNTLLOCK
  { struct flock flck;
    flck.l_type = unlockflag ? F_UNLCK : F_WRLCK;
    flck.l_whence = startendflag ? L_XTND : L_SET;
#ifdef DENYREADWRITE
    flck.l_start = offset+4;
#else  DENYREADWRITE
    flck.l_start = offset;
#endif DENYREADWRITE
    flck.l_len = length;

    if (fcntl(fd, F_SETLK, &flck) < 0) {
      switch (errno) {
	case EAGAIN:
	case EBADF:
	case EDEADLK:
	case EINVAL:
	case ENOLCK:
	  return(aeNoMoreLocks);
	  break;
	default:
	  return(aeParamErr);
	  break;
      }
    }
  }
#else  USEFCNTLLOCK
  if ((pos = lseek(fd, (off_t)offset, (startendflag ? L_XTND : L_SET)))  < 0) {
    *fpos = -1;			/* unknown */
    return(aeParamErr);
  }
  *fpos = pos;

#ifdef DENYREADWRITE
  lseek(fd, 4, L_INCR);
#endif DENYREADWRITE

  err = lockf(fd, unlockflag ? F_ULOCK : F_TLOCK, length);

#ifdef DENYREADWRITE
  lseek(fd, -4, L_INCR);
#endif DENYREADWRITE

  if (err < 0) {
#ifdef DEBUG
    printf("CSByteRangeLock");
#endif DEBUG
    switch (errno) {
#ifdef EREMOTE
      /* not sure if this is the best thing to do */
    case EREMOTE:
      return(noErr);
#endif EREMOTE
#ifdef notdef
      return(aeRangeNotLocked);
      return(aeRangeOverlap);
#endif notdef
    case EACCES:
    case EAGAIN:
#ifdef EINTR
    case EINTR:
#endif EINTR
#ifdef notdef
      return(aeLockErr);
#endif notdef
#ifdef EBADF
    case EBADF:			/* SunOS */
#endif EBADF
    /* really permission denied (or, unlikely: bad file) */
#ifndef gould
#ifdef ENOLCK			/* sunos */
    case ENOLCK:
#endif ENOLCK
#endif gould
#ifdef EDEADLK
    case EDEADLK:
#endif EDEADLK
#ifdef notdef
      return(aeNoMoreLocks);
#endif notdef
    default:
      return(aeParamErr);	/* specially for System 7.0 */
    }
  }
#endif USEFCNTLLOCK
  return(noErr);
}

/*
 * returns noErr if the range starting at the current file position
 * for length "length" is not locked.
 *
 */

OSErr
OSTestLock(fd, length)
int fd;
sdword length;
{
  int n;
  extern int errno;

#ifdef XENIX_LOCKING
  if ((n = locking(fd, LK_NBRLCK, length)) >= 0)
    n = locking(fd, LK_UNLCK, length);
#else XENIX_LOCKING
#if defined(DENYREADWRITE) || defined(USEFCNTLLOCK)
  { struct flock flck;

    n = 0;
    flck.l_type = F_RDLCK;
    flck.l_whence = L_INCR; /* SEEK_CUR */
#ifdef DENYREADWRITE
    flck.l_start = 4;
#else  DENYREADWRITE
    flck.l_start = 0;
#endif DENYREADWRITE
    flck.l_len = length;
    if (fcntl(fd, F_GETLK, &flck) != -1) {
      if (flck.l_type == F_WRLCK) {
        errno = EAGAIN;
        n = -1;
      }
    }
  }
#else  /* DENYREADWRITE || USEFCNTLLOCK */
  n = lockf(fd, F_TEST, length);
#endif /* DENYREADWRITE || USEFCNTLLOCK */
#endif XENIX_LOCKING

  if (n < 0) {
#ifdef DEBUG
#ifdef EBADF
    if (errno == EBADF) {
      printf("OSTestLock: Bad File Descriptor %d\n", fd);
      return(aeLockErr);
    }
#endif EBADF
#ifdef EAGAIN
    if (errno == EAGAIN) {
      printf("OSTestLock: File already locked %d\n", fd);
      return(aeLockErr);
    }
#endif EAGAIN
#endif DEBUG
#ifdef EREMOTE
    if (errno == EREMOTE)
      return(noErr);
#endif EREMOTE
#ifdef DEBUG
    printf("File is locked\n");
#endif DEBUG
    return(aeLockErr);
  }
  return(noErr);
}
#endif NOLOCKF

/* 
 * The following calls are used to coordinate read/writes for various
 * files used by aufs.
 *
 * The basic primatives are:
 *   Lock File for Read - lock a file for reading (others may do the
 *     same).  Do it if no write locks are in effect.
 *   Lock File for Write - lock a file for writing (other may not at the
 *     same time).  Do it if no read or write locks are in effect.
 *   Unlock file
 *
 *  Note, lffr is essentially a shared lock while lffw is an exclusive
 *     lock.
 *
 *  Since most unix systems only issue advisory locks, the deal is that
 *   you call the routines before you do any reading or writing - this
 *   should be sufficient.
 *  Note: the lock calls are assumed to block - routines must be "good"
 *   about unlocking or things will break in a big way.
 *
 * Implementations:
 *  flock - implements shared and exclusive locks: perfect
 *  lockf - implements exclusive locks only (and only on writable fds):
 *           okay, but reads can get "out of date" or "bad" data
 *
 *
 */

boolean
OSLockFileforRead(fd)
int fd;
{
#ifdef USEFCNTLLOCK
  { struct flock flck;

    flck.l_type = F_RDLCK;
    flck.l_whence = L_SET;
    flck.l_start = 0;
    flck.l_len = 0;

    if (fcntl(fd, F_SETLKW, &flck) < 0) {
      switch (errno) {
	case EBADF: /* pass read-only files */
	  return(TRUE);
	  break;
	default:
	  return(FALSE);
          break;
      }
    }
  }
#else  USEFCNTLLOCK
#ifdef hpux
  off_t saveoffs;
#endif hpux
#ifdef xenix5
  lseek(fd, 0L, L_SET);
#endif xenix5
#ifdef XENIX_LOCKING
  if (locking(fd, LK_RLCK, 0L) < 0) {
#ifdef DEBUG
    printf("OSLockFileforRead");
#endif DEBUG
    return(FALSE);		/* problem!!! */
  }
#else XENIX_LOCKING
# ifndef NOFLOCK
  if (flock(fd, LOCK_SH) < 0)
    return(FALSE);		/* problem!!! */
# else
#  ifdef LOCKF_ONLY
#   ifdef hpux
  saveoffs = lseek(fd, 0L, SEEK_CUR);
  lseek(fd, 0L, SEEK_SET);
#   endif hpux
  if (lockf(fd, F_LOCK, 0) < 0) {
#   ifdef hpux
    lseek(fd, saveoffs, SEEK_SET);
#   endif hpux
#   ifdef EREMOTE
    if (errno == EREMOTE)
      return(TRUE);
#   endif
    if (errno == EBADF)		/* file is open read-only */
      return(TRUE);		/* can't do lock, so let it go on */
    return(FALSE);
  }
#   ifdef hpux
  lseek(fd, saveoffs, SEEK_SET);
#   endif hpux
#  endif
# endif
#endif XENIX_LOCKING
#endif USEFCNTLLOCK
  return(TRUE);
}

boolean
OSLockFileforWrite(fd)
int fd;
{
#ifdef USEFCNTLLOCK
  { struct flock flck;
  
    flck.l_type = F_WRLCK;
    flck.l_whence = L_SET;
    flck.l_start = 0;
    flck.l_len = 0;
    
    if (fcntl(fd, F_SETLKW, &flck) < 0) {
      switch (errno) {
        case EAGAIN:
        case EBADF:
        default:
          return(FALSE);
          break;
      }
    }
  }
#else  USEFCNTLLOCK
#ifdef hpux
  off_t saveoffs;
#endif hpux
#ifdef xenix5
  lseek(fd, 0L, L_SET);
#endif xenix5
#ifdef XENIX_LOCKING
  if (locking(fd, LK_LOCK, 0L) < 0
   || locking(fd, LK_UNLCK, 0L) < 0) {
#ifdef DEBUG
    printf("OSLockFileforWrite");
#endif DEBUG
    return(FALSE);		/* problem!!! */
  }
#else XENIX_LOCKING
# ifndef NOFLOCK
  if (flock(fd, LOCK_EX) < 0)
    return(FALSE);		/* problem!!! */
# else NOFLOCK
#  ifdef LOCKF_ONLY
#   ifdef hpux
  saveoffs = lseek(fd, 0L, SEEK_CUR);
  lseek(fd, 0L, SEEK_SET);
#   endif hpux
  if (lockf(fd, F_LOCK, 0) < 0) {
#   ifdef hpux
    lseek(fd, saveoffs, SEEK_SET);
#   endif hpux
#   ifdef EREMOTE
    if (errno == EREMOTE)
      return(TRUE);
#   endif /* EREMOTE */
    return(FALSE);
  }
#   ifdef hpux
  lseek(fd, saveoffs, SEEK_SET);
#   endif hpux
#  endif /* LOCKF ONLY */
# endif NOFLOCK
#endif XENIX_LOCKING
#endif USEFCNTLLOCK
  return(TRUE);
}

/*
 * This implements an exclusive lock on the file.  It differs from
 * OSLockFileforRead in that it doesn't block
 *
 */

#ifdef notdef			/* not used */
boolean
OSMaybeLockFile(fd)
{
#ifdef xenix5
  lseek(fd, 0L, L_SET);
#endif xenix5
#ifdef XENIX_LOCKING
    if (locking(fd, LK_NBLCK, 0L) < 0
     || locking(fd, LK_NBRLCK, 0L) < 0) {
#ifdef DEBUG
      printf("OSMaybeLockFile");
#endif DEBUG
      return(FALSE);		/* problem!!! */
    }
#else XENIX_LOCKING
# ifndef NOFLOCK
  if (flock(fd, LOCK_EX|LOCK_NB) < 0)
    return(FALSE);		/* problem!!! */
#  else
#  ifdef LOCKF_ONLY
  if (lockf(fd, F_TLOCK, 0) < 0) {
#   ifdef EREMOTE
    if (errno == EREMOTE)
      return(TRUE);
#   endif EREMOTE
    return(FALSE);
  }
#  endif LOCKF_ONLY
# endif NOFLOCK
#endif XENIX_LOCKING
  return(true);
}
#endif

boolean
OSUnlockFile(fd)
int fd;
{
#ifdef USEFCNTLLOCK
  { struct flock flck;
  
    flck.l_type = F_UNLCK;
    flck.l_whence = L_SET;
    flck.l_start = 0;
    flck.l_len = 0;
    
    if (fcntl(fd, F_SETLKW, &flck) < 0)
      return(FALSE);
  }
#else  USEFCNTLLOCK
#ifdef hpux
  off_t saveoffs;
#endif hpux
#ifdef xenix5
  lseek(fd, 0L, L_SET);
#endif xenix5
#ifdef XENIX_LOCKING
  if (locking(fd, LK_UNLCK, 0L) < 0) {
#ifdef DEBUG
    printf("OSUnlockFile");
#endif DEBUG
    return(FALSE);		/* problem!!! */
  }
#else XENIX_LOCKING
# ifndef NOFLOCK
  if (flock(fd, LOCK_UN) < 0)
    return(FALSE);
# else /* NOFLOCK */
#  ifdef LOCKF_ONLY
#   ifdef hpux
  saveoffs = lseek(fd, 0L, SEEK_CUR);
  lseek(fd, 0L, SEEK_SET);
#   endif hpux
  if (lockf(fd, F_ULOCK, 0) < 0) {
#   ifdef hpux
    lseek(fd, saveoffs, SEEK_SET);
#   endif hpux
#  ifdef EREMOTE
    if (errno == EREMOTE)
      return(TRUE);
#  endif /* EREMOTE */
    return(FALSE);
  }
#   ifdef hpux
  lseek(fd, saveoffs, SEEK_SET);
#   endif hpux
#  endif /* LOCKF_ONLY */
# endif /* end else NOFLOCK */
#endif XENIX_LOCKING
#endif USEFCNTLLOCK
  return(TRUE);
}

getlockinfo(haveflock,havelockf)
int *haveflock;
int *havelockf;
{
#ifdef NOFLOCK
  *haveflock = 0;
#else
  *haveflock = 1;
#endif
#ifdef NOLOCKF
  *havelockf = 0;
#else
  *havelockf = 1;
#endif
}
