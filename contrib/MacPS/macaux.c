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

#ifndef lint
static char *SCCSid = "@(#)macaux.c	2.2 10/24/89";
#endif lint

#include <ctype.h>
#include <stdio.h>
#include "str.h"

#define	FALSE		0
#define	TRUE		1

extern char *myname;
int rawmode = FALSE;

STR *
STRalloc()
{
	register STR *str;
	char *malloc();

	if((str = (STR *)malloc(sizeof(STR))) == NULL ||
	 (str->bufptr = (unsigned char *)malloc(STRSIZE)) == NULL) {
		fprintf(stderr, "%s: STRalloc: Out of memory\n", myname);
		exit(1);
	}
	str->curendptr = str->bufptr;
	str->realendptr = str->bufptr + STRSIZE;
	return(str);
}

STRfree(str)
STR *str;
{
	free((char *)str->bufptr);
	free((char *)str);
}

STRexpand(str)
register STR *str;
{
	register int curend, realend;
	char *realloc();

	curend = str->curendptr - str->bufptr;
	realend = (str->realendptr - str->bufptr) + STRSIZEDELTA;
	if((str->bufptr = (unsigned char *)realloc((char *)str->bufptr,
	 realend)) == NULL) {
		fprintf(stderr, "%s: STRexpand: Out of memory\n", myname);
		exit(1);
	}
	str->curendptr = str->bufptr + curend;
	str->realendptr = str->bufptr + realend;
}

STRgets(str, fp)
register STR *str;
register FILE *fp;
{
	register int c;

	str->curendptr = str->bufptr;
	for( ; ; ) {
		if((c = getc(fp)) == EOF)
			return(str->curendptr > str->bufptr);
		if(str->curendptr >= str->realendptr)
			STRexpand(str);
		*str->curendptr++ = c;
		if(c == '\n' || c == '\r')
			return(TRUE);
	}
}

STRputsptr(str, cp, fp)
register STR *str;
register unsigned char *cp;
register FILE *fp;
{
	if(rawmode) {
		for( ; cp < str->curendptr ; cp++)
			putc(*cp, fp);
		return;
	}
	for( ; cp < str->curendptr ; cp++) {
		if(!isascii(*cp))
			fprintf(fp, "\\%03o", *cp);
		else if(isprint(*cp))
			putc(*cp, fp);
		else {
			switch(*cp) {
			 case '\n':
			 case '\r':
				putc('\n', fp);
				continue;
			 case '\t':
				putc('\t', fp);
				continue;
			 default:
				fprintf(fp, "\\%03o", *str);
				continue;
			}
		}
	}
}

STRcompareptr(str, cp, sp)
register STR *str;
register unsigned char *cp, *sp;
{
	register int comp;

	for( ; ; ) {
		if(*sp == 0)
			return(cp >= str->curendptr ? 0 : 1);
		if(cp >= str->curendptr)
			return(-1);
		if(*sp == '\n') {
			if(*cp != '\n' && *cp != '\r')
				return((int)*cp - (int)*sp);
		} else if((comp = (int)*cp - (int)*sp) != 0)
			return(comp);
		cp++;
		sp++;
	}
}

STRheadcmpptr(str, cp, sp)
register STR *str;
register unsigned char *cp, *sp;
{
	register int comp;

	for( ; ; ) {
		if(*sp == 0)
			return(0);
		if(cp >= str->curendptr)
			return(-1);
		if(*sp == '\n') {
			if(*cp != '\n' && *cp != '\r')
				return((int)*cp - (int)*sp);
		} else if((comp = (int)*cp - (int)*sp) != 0)
			return(comp);
		cp++;
		sp++;
	}
}

unsigned char *
STRmatch(str, sp)
register STR *str;
register unsigned char *sp;
{
	register unsigned char *mp, *last;
	register int firstchar;

	firstchar = *sp;
	last = str->curendptr - strlen(sp);
	mp = str->bufptr;
	while(mp <= last) {
		if(*mp == firstchar && STRheadcmpptr(str, mp, sp) == 0)
			return(mp);
		mp++;
	}
	return(NULL);
}
