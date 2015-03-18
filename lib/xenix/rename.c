/*
 * $Author: djh $ $Date: 91/02/15 22:50:37 $
 * $Header: rename.c,v 2.1 91/02/15 22:50:37 djh Rel $
 * $Revision: 2.1 $
 */

/*
 * Rename system call -- Replacement for Berzerkeley 4.2 rename system
 * call that is missing in Xenix.
 *
 * By Marc Frajola and Chris Paris.
 * Directory hack by Chip Salzenberg.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

rename(src,dest)
    char *src;			/* Source file to rename */
    char *dest;			/* Name for renamed file */
{
    int status;			/* Status returned from link system call */
    struct stat stbuf;		/* Buffer for statting destination file */

    /* Find out what the destination is: */
    status = stat(dest,&stbuf);
    if (status >= 0) {
	/* See if the file is a regular file; if not, return error: */
	if ((stbuf.st_mode & S_IFMT) != S_IFREG) {
	    return(-1);
	}
    }

    /* Unlink destination since it is a file: */
    unlink(dest);

    /* Find out what the source is: */
    status = stat(src,&stbuf);
    if (status < 0)
	return -1;
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR)
    {
	/* Directory hack for SCO Xenix */

	static char mvdir[] = "/usr/lib/mv_dir";
	void (*oldsigcld)();
	int pid;

	oldsigcld = signal(SIGCLD, SIG_DFL);
	while ((pid = fork()) == -1)
	{
	    if (errno != EAGAIN)
		return -1;
	    sleep(5);
	}
	if (pid == 0)
	{
	    execl(mvdir, mvdir, src, dest, (char *) 0);
	    perror(mvdir);
	    exit(1);
	}
	if (wait(&status) != pid)
	{
	    fprintf(stderr, "rename: wait failure\n");
	    status = -1;
	}
	(void) signal(SIGCLD, oldsigcld);
    }
    else
    {
	/* Link source to destination file: */
	status = link(src,dest);
	if (status != 0) {
	    return(-1);
	}
	status = unlink(src);
    }
    return((status == 0) ? 0 : (-1));
}
