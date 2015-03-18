/*
 * $Author: djh $ $Date: 91/02/15 22:59:15 $
 * $Header: compat.h,v 2.1 91/02/15 22:59:15 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * compat.h - tries to cover up differences between various BSD based
 * machines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * Edit History:
 *
 *  August 6, 1987    CCKim	Created.
 *
 */


/* for those who have straight 4.2 or ultrix 1.1/1.2/2.0 systems */
/* many of these systems don't include macros to handle select args */
/* correctly */
#ifndef FD_SETSIZE
# ifndef NOFILE
NO GUESSING - set this to the number of files allowed on your machine
# endif 
# define NFDBITS (sizeof(int)*8)
# define FD_SETSIZE NOFILE
# ifndef howmany
#  define howmany(x, y) (((x)+((y)-1))/(y))
# endif
# define FD_SET(n, p)  (p)->fds_bits[(n)/NFDBITS] |= (1<<((n)%NFDBITS))
# define FD_CLR(n, p)  (p)->fds_bits[(n)/NFDBITS] &= ~(1<<((n)%NFDBITS))
# define FD_ISSET(n, p) ((p)->fds_bits[(n)/NFDBITS] & (1<<((n)%NFDBITS))
# define FD_ZERO(p) bzero((char *)(p), sizeof(*(p)))
typedef int gfd_mask;
typedef struct gfd_set {
  gfd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} gfd_set;
#else
#define gfd_mask fd_mask
#define gfd_set fd_set 
#endif

#ifndef sigmask
/* mask for sigblock, etc */
#define sigmask(sig) (1<<((sig)-1))
#endif


/* some useful usr group standardizations */

#ifndef S_ISBLK
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
