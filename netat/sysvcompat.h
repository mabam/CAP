/*
 * $Author: djh $ $Date: 1996/09/10 13:51:34 $
 * $Header: /mac/src/cap60/netat/RCS/sysvcompat.h,v 2.12 1996/09/10 13:51:34 djh Rel djh $
 * $Revision: 2.12 $
 *
 */

#ifndef _sysvcompat_h_
#define _sysvcompat_h_

/*
 * sysvcompat.h - header file to allow us to port to sys v system machines
 *  without building a library of "compatible function" for functions that
 *  have slightly different name, etc.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  June, 1987 	CCKim    Created
 *
 */

/*
 * Mappings from bsd to sysv string and bytestring function
 *
 */

#ifdef SOLARIS
# define B2S_STRING_MAPON       /* map strings */
# define B2S_BSTRING_MAPON      /* map byte string instructions */
# define USETIMES               /* use times instead of getrusage */
# define NOWAIT3                /* no wait3 */
# define NOSIGMASK		/* sigblock() doesn't exist */
# define USERAND                /* use srand, rand */
# define USEGETCWD              /* use getcwd instead of bsd getwd */
# define NOPGRP                 /* no process groups (setpgrp, killpg) */
# define NOVFORK                /* no vfork in system */
# define NEEDFCNTLDOTH		/* if need fcntl.h for O_... */
# define USESYSVLP		/* use system V lp command instead of lpr */
# define USEDIRENT		/* use struct dirent */
# define SHADOW_PASSWD
/* Hopefully, this is safe */
# define L_SET		0	/* absolute offset */
# define L_INCR		1	/* relative to current offset */
# define L_XTND		2	/* relative to end of file */
# define LB_SET		3	/* abs. block offset */
# define LB_INCR	4	/* rel. block offset */
# define LB_XTND	5	/* block offset rel. to eof */
#endif SOLARIS

#ifdef hpux
# define B2S_STRING_MAPON	/* map strings */
# define B2S_BSTRING_MAPON	/* map byte string instructions */
# define USECHOWN		/* sysv allows us */
# define NEEDFCNTLDOTH		/* if need fcntl.h for O_... */
# define USETIMES		/* use times instead of getrusage */
/* some versions may have this */
#ifndef __hpux
# define NOWAIT3		/* no wait3 */
# define NODUP2			/* no dup2 */
# define NOLSTAT		/* no symbolic links */
# define NOPGRP			/* no process groups (setpgrp, killpg) */
#else  __hpux
# define _BSD
# define POSIX
# define NOWAIT3		/* no rusage support for AUFS under HP-UX */
# define WSTATUS union wait     /* at least for HP-UX 9.01 if _BSD ... */
/* # define ADDRINPACK */
#endif __hpux
# define USERAND		/* use srand, rand */
# define USEGETCWD		/* use getcwd instead of bsd getwd */
# define NOUTIMES		/* no utimes - use utime */
#endif hpux

#ifdef aux
# define B2S_STRING_MAPON	/* map strings */
# define B2S_BSTRING_MAPON	/* map byte string instructions */
# define USECHOWN		/* sysv allows us */
# define USETIMES		/* use times instead of getrusage */
# define USERAND		/* use srand, rand */
# define NOVFORK		/* no vfork in system */
#endif aux

#ifdef uts
# define USETIMES	/* getrusage - "use times not rusage" */
# define NOWAIT3	/* wait3 - "no wait3, use wait" */
# define USERAND	/* random - "use rand,srand not random" */
# define USEGETCWD	/* getwd - "use getcwd not getwd" */
# define NOUTIMES	/* utimes - "use utime not utimes" */
# define NOPGRP	/* killpg - "missing setpgrp or killpg" */
# define NOVFORK	/* vfork - "novfork, use fork" */
# define NEEDFCNTLDOTH /* if need fcntl.h for O_... */
# define USECHOWN /* sysv allows us */
# define NORUSAGE /* getrusage() is in resource.h, but not supported */
# define NOSIGMASK     /* sigblock() doesn't exist */
# define USEDIRENT     /* use struct dirent */
# define USESTRINGDOTH /* use system V string.h */
# define USESYSVLP	/* use system V lp command instead of lpr */
#endif uts

#ifdef xenix5
# define B2S_STRING_MAPON	/* map strings */
# define B2S_BSTRING_MAPON	/* map byte string instructions */
# define USETIMES		/* use times instead of getrusage */
# define NOWAIT3		/* no wait3 */
# define NODUP2			/* no dup2 */
# define NOLSTAT		/* no symbolic links */
# define USERAND		/* use srand, rand */
# define USEGETCWD		/* use getcwd instead of bsd getwd */
# define NOUTIMES		/* no utimes - use utime */
# define NOPGRP			/* no process groups (setpgrp, killpg) */
# define NOVFORK		/* no vfork in system */
# define NEEDFCNTLDOTH		/* if need fcntl.h for O_... */
# define USECHOWN		/* sysv allows us */
/* added by hand: */
# define NEEDMSGHDR		/* the one defined for us is broken */
# define MAXPATHLEN 256		/* a guess! */
# define NGROUPS 1		/* max number of groups per process */
# define NOSIGMASK		/* setsigmask() et al not available */
# define L_SET	0
# define L_INCR	1
# define L_XTND	2
#endif xenix5

#ifdef drsnx
#define USESYSVLP
#define USEDIRENT
#endif drsnx

#ifdef EPIX
#define WSTATUS union wait
#endif EPIX

#if defined (hp300) && !defined(__BSD_4_4__)
#define WSTATUS union wait
#endif /* hp300 && __BSD_4_4__) */

#ifdef NeXT
#define WSTATUS union wait
/* WIFSTOPPED and WIFSIGNALED are defined in "sys/wait.h". */
#define W_TERMSIG(status) ((status).w_termsig)
#define W_COREDUMP(status) ((status).w_coredump)
#define W_RETCODE(status) ((status).w_retcode)
#endif /* NeXT */

/* FIXED CONFIGURATION -- ALL NEW CONFIGURATIONS MUST PRECEED */

/* map sigcld to sigchld if sigchld isn't there */
#ifndef SIGCHLD
# ifdef SIGCLD
#  define SIGCHLD SIGCLD
# endif SIGCLD
#endif  SIGCHLD

#ifdef B2S_STRING_MAPON
# ifndef USESTRINGDOTH
#  define USESTRINGDOTH		/*  must use string.h */
# endif  USESTRINGDOTH
# define index(s,c)	strchr((char *)(s),(c))
# define rindex(s,c)	strrchr((char *)(s),(c))
#endif B2S_STRING_MAPON

#ifdef B2S_BSTRING_MAPON
# ifdef SOLARIS
#  define bcopy(s,d,l)	memmove((char *)(d),(char *)(s),(l))
# else  SOLARIS
#  define bcopy(s,d,l)	memcpy((char *)(d),(char *)(s),(l))
# endif SOLARIS
# define bcmp(b1,b2,l)	memcmp((char *)(b1),(char *)(b2),(l))
# define bzero(b,l)	memset((char *)(b),0,(l))
#endif B2S_BSTRING_MAPON

/*
 *  WAIT/WAIT3 Compatibility Section
 *
 *  We should allow the system to define the WIF macros if possible.
 *  It's not our place to do so unless we have to, even then
 *  it is somewhat dubious... should be done per machine, especially
 *  the W_COREDUMP() macro.
 *
 *  If your system uses (union wait) as its status parameter it
 *  is a bit out of touch with modern times. Nevertheless
 *  add "#define WSTATUS union wait" under a conditional
 *  for your machine in the code above.
 *
 *  We have NO guarantee that if the system doesn't define these
 *  (or their POSIX/SYSV equivalents) that the ones we define
 *  WILL work. It is up to the person porting this code to
 *  determine this and define the functions beforehand to
 *  do something else if their functionality is not supported
 *  by your system.
 *
 */

#include <sys/wait.h>

#ifndef WSTATUS
#define WSTATUS int
#endif  WSTATUS

#ifndef WIFSTOPPED
#define WIFSTOPPED(status)	((*((int *)&status) & 0xff) == 0177)
#endif  WIFSTOPPED

#ifndef WIFSIGNALED
#define WIFSIGNALED(status)	((*((int *)&status) & 0177) != 0)
#endif  WIFSIGNALED

#ifndef W_TERMSIG
#ifdef  WTERMSIG
#define W_TERMSIG WTERMSIG
#else   WTERMSIG
#define W_TERMSIG(status)	(*((int *)&status) & 0177)
#endif  WTERMSIG
#endif  W_TERMSIG

#ifndef W_COREDUMP
#ifdef  WCOREDUMP
#define W_COREDUMP WCOREDUMP
#else   WCOREDUMP
#define W_COREDUMP(status)	(*((int *)&status) & 0200)
#endif  WCOREDUMP
#endif  W_COREDUMP

#ifndef W_RETCODE
#ifdef  WEXITSTATUS
#define W_RETCODE WEXITSTATUS
#else   WEXITSTATUS
#define W_RETCODE(status)	(*((int *)&status) >> 8)
#endif  WEXITSTATUS
#endif  W_RETCODE

#endif /* _sysvcompat_h_ */
