/*
 * Copyright (c) 1988, The Regents of the University of California.
 * Edward Moy, Workstation Software Support Group, Workstation Support Serices,
 * Information Systems and Technology.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of California makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#ifndef CONFIGDIR
#ifndef lint
static char *SCCSid = "@(#)ucbwhich.c	2.2 10/24/89";
#endif lint

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ucbwhich.h"

#define	F_OK		0	/* does file exist */
#define	X_OK		1	/* is it executable by caller */
#define	W_OK		2	/* writable by caller */
#define	R_OK		4	/* readable by caller */

#define	LIBLEN		4
#ifdef SYSV
#define	index		strchr
#define	rindex		strrchr
#endif SYSV

static char lib[] = "/lib";

char ucblib[UCBMAXPATHLEN];
int ucbalternate = 0;
char ucbpath[UCBMAXPATHLEN];

ucbwhich(str)
char *str;
{
	register char *dir, *name, *cp, *tp;
	register int len;
	char dirbuf[UCBMAXPATHLEN], namebuf[UCBMAXPATHLEN];
	struct stat sbuf;
	char *index(), *rindex(), *getwd(), *getenv();

	strcpy(name = namebuf, str);
	if(*name == '/')	/* absolute pathname */
		*(rindex(dir = name, '/')) = 0 ; /* remove tail */
	else {
		if(cp = index(name, '/')) { /* relative pathname */
			if((dir = getwd(dirbuf)) == NULL)
				return(0);
			 /* if any errors occurs assume standard version */
			*cp++ = 0;
			for( ; ; ) {
				if(*name != 0) { /* multiple slashes */
					if(strcmp(name, "..") == 0) {
						/* parent directory */
						if((tp = rindex(dir, '/')) ==
						 NULL)
						 	return(0);
						if(tp == dir)
							tp++;
						 /* root directory */
						*tp = 0;
						 /* remove last component */
					} else if(strcmp(name, ".") != 0) {
						/* subdirectory */
						strcat(dir, "/");
						strcat(dir, name);
					}
				}
				name = cp;
				if((cp = index(name, '/')) == NULL) break;
				/* ignore last component */
				*cp++ = 0;
			}
		} else { /* look through $PATH variable */
			if((tp = getenv("PATH")) == NULL)
				return(0);
			for(name = namebuf ; ; ) {
				if(*tp == 0)
					return(0);
				else if(*tp == ':')
					tp++;
				if((cp = index(tp, ':')) == NULL)
					cp = tp + strlen(tp);
				 /* positioned on null */
				for(dir = dirbuf ; tp < cp ; )
					*dir++ = *tp++;
				*dir = 0;
				strcpy(name, dir = dirbuf);
				strcat(name, "/");
				strcat(name, str);
				if(stat(name, &sbuf) < 0 || (sbuf.st_mode &
				 S_IFMT) != S_IFREG)
					continue;
				if(access(name, X_OK) == 0) {
					if(strcmp(dir, ".") == 0 &&
					 (dir = getwd(dirbuf)) == NULL)
						return(0);
					break;
				}
			}
		}
	}
	strcpy(ucbpath, dir);
	strcpy(ucblib, dir);
	if((len = strlen(dir)) < LIBLEN || strcmp(&dir[len - LIBLEN], lib)
	 != 0)
		strcat(ucblib, lib);
	else
		ucbpath[len - LIBLEN] = 0;
	ucbalternate = (strcmp(ucbpath, UCBSTANDARD) != 0);
#ifdef EBUG
	fprintf(stderr, "ucbwhich: alt=%d path=%s lib=%s\n", ucbalternate,
	 ucbpath, ucblib);
#endif EBUG
	return(ucbalternate);
}
#endif CONFIGDIR
