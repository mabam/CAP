/*
 * $Author: djh $ $Date: 91/02/15 22:50:35 $
 * $Header: ftruncate.c,v 2.1 91/02/15 22:50:35 djh Rel $
 * $Revision: 2.1 $
 */

/*
 * Ftruncate() for non-BSD systems.
 *
 * This module gives the basic functionality for ftruncate() which
 * truncates the given file descriptor to the given length.
 * ftruncate() is a Berkeley system call, but does not exist in any
 * form on many other versions of UNIX such as SysV. Note that Xenix
 * has chsize() which changes the size of a given file descriptor,
 * so that is used if M_XENIX is defined.
 *
 * Since there is not a known way to support this under generic SysV,
 * there is no code generated for those systems.
 *
 * SPECIAL NOTE: On Xenix, using this call in the BSD library
 * will REQUIRE the use of -lx for the extended library since chsize()
 * is not in the standard C library.
 *
 * By Marc Frajola, 3/27/87
 */

#ifdef M_XENIX

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

ftruncate(fd,length)
    int fd;			/* File descriptor to truncate */
    off_t length;		/* Length to truncate file to */
{
    int status;			/* Status returned from truncate proc */

#if defined(M_XENIX)
    status = chsize(fd,length);
#else
    /* Stuff for Microport here */
    status = -1;
    NON-XENIX SYSTEMS CURRENTLY NOT SUPPORTED
#endif

    return(status);
}

#endif /* M_XENIX */
