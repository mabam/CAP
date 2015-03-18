/*
 * $Author: djh $ $Date: 91/02/15 22:50:34 $
 * $Header: fsync.c,v 2.1 91/02/15 22:50:34 djh Rel $
 * $Revision: 2.1 $
 */

/*
 * Fsync() for non-BSD systems.
 *
 * For now, this is a no-op, since we can't force writes without
 * three system calls.
 */

int
fsync(fd)
    int fd;
{
    return 0;
}
