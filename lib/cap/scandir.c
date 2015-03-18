/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef SOLARIS
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)scandir.c	5.3 (Berkeley) 6/18/88";
#endif /* LIBC_SCCS and not lint */

/*
 * Scan the directory dirname calling select to make a list of selected
 * directory entries then sort using qsort and compare routine dcomp.
 * Returns the number of entries and a pointer to a list of pointers to
 * struct direct (through namelist). Returns -1 if there were any errors.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>

struct dirent *GetNextDir(buf,nbytes)
char buf[];
int	*nbytes;
{
	struct dirent *p,*d;
	
	p = (struct dirent *) buf;
	
	*nbytes = *nbytes - p->d_reclen;
	if(*nbytes == 0)
		return(NULL);

	d = (struct dirent *) &buf[p->d_reclen];
	return(d);
}

scandir(dirname, namelist, select, dcomp)
	char *dirname;
	struct dirent *(*namelist[]);
	int (*select)(), (*dcomp)();
{
	int	dir_fd;
	int	nbytes;
	int 	nitems,done;
	char	buf[8192];
	char *cp1, *cp2;
	struct stat stb;
	struct dirent *d, *p, **names,*bp;
	long arraysz;

	if ((dir_fd = open(dirname,O_RDONLY))== -1)
		return(-1);
	if (fstat(dir_fd, &stb) < 0)
		return(-1);

	/*
	 * estimate the array size by taking the size of the directory file
	 * and dividing it by a multiple of the minimum size entry. 
	 */
	arraysz = (stb.st_size / 24);
	names = (struct dirent **)malloc(arraysz * sizeof(struct dirent *));
	if (names == NULL)
		return(-1);

	nitems = 0;
	d = (struct dirent *) buf;
	while ((nbytes = getdents(dir_fd, d, 8192)) > 0) 
	{

		while(d != NULL)
		{
			if (select != NULL && !(*select)(d))
			{
				d = GetNextDir(d,&nbytes);
				continue;	/* just selected names */
			}
			/*
		 	* Make a minimum size copy of the data
		 	*/
			p = (struct dirent *)malloc(d->d_reclen + 1);
			if (p == NULL)
				return(-1);
			p->d_ino = d->d_ino;
			p->d_off = d->d_off;
			p->d_reclen = d->d_reclen;
			for (cp1 = p->d_name, cp2 = d->d_name; 
				*cp1++ = *cp2++; );
			/*
		 	* Check to make sure the array has space left and
		 	* realloc the maximum size.
		 	*/
			if (++nitems >= arraysz) 
			{

				/* just might have grown */
				if (fstat(dir_fd, &stb) < 0)
					return(-1);	
				arraysz = stb.st_size / 12;
				names = (struct dirent **)realloc((char *)names,
					arraysz * sizeof(struct dirent *));
				if (names == NULL)
					return(-1);
			}
			names[nitems-1] = p;
			d = GetNextDir(d,&nbytes);
		}
		d = (struct dirent *) buf;
	}
	close(dir_fd);
	if (nitems && dcomp != NULL)
		qsort(names, nitems, sizeof(struct dirent *), dcomp);
	*namelist = names;
	return(nitems);
}

/*
 * Alphabetic order comparison routine for those who want it.
 */
alphasort(d1, d2)
	struct dirent **d1, **d2;
{
	return(strcmp((*d1)->d_name, (*d2)->d_name));
}
#else  SOLARIS
int scan_dummy_for_ld;	/* keep the loader and ranlib happy */
#endif SOLARIS
