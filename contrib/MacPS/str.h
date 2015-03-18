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

/*
 * SCCSid = "@(#)str.h	2.2 10/24/89"
 */

#define	STRSIZEDELTA	1024
#define	STRSIZE		1024

#define	STRcompare(str,fp)	STRcompareptr((str), (str)->bufptr, (fp))
#define	STRheadcompare(str,fp)	STRheadcmpptr((str), (str)->bufptr, (fp))
#define	STRputs(str,fp)		STRputsptr((str), (str)->bufptr, (fp))

typedef struct {
	unsigned char *bufptr;
	unsigned char *curendptr;
	unsigned char *realendptr;
} STR;

extern int rawmode;

STR *STRalloc();
int STRcompareptr();
int STRfree();
int STRgets();
int STRheadcmpptr();
unsigned char *STRmatch();
int STRputsptr();
