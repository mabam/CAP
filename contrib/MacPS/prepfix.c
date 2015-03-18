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
static char *SCCSid = "@(#)prepfix.c	2.2 10/25/89";
#endif lint

#include <ctype.h>
#include <stdio.h>
#ifdef SYSV
#include <string.h>
#else SYSV
#include <strings.h>
#endif SYSV
#include "str.h"

#define	CLEARTOMARK	12
#define EEXECLEN	80
#define EXTRA		(NZEROLINE * ZEROLINE + CLEARTOMARK)
#define LINELEN		256
#define	NPRODUCTS	32
#define NZEROLINE	7
#define ZEROLINE	65
#ifdef SYSV
#define	index		strchr
#define	rindex		strrchr
#endif SYSV

char exstr[] = "\
%ck userdict/%s known not and{currentfile eexec}{%d{currentfile read\n\
pop pop}repeat}ifelse\n\
";
char *match();
char *myname;
int maxproducts = NPRODUCTS;
int nproducts = 0;
char Ok[] = "\
/Ok{ok{true}{save /Pd statusdict /product get def false 0 1 ProdArr length\n\
1 sub{Pd exch ProdArr exch get anchorsearch exch pop{pop pop true exit}if}for\n\
exch restore}ifelse}bind def\n\
";
char ProdArr0[] = "/ProdArr [\n";
char ProdArr1[] = "] def\n";
char **products;
char tempname[] = "/tmp/prepfixXXXXXX";

main(argc, argv)
int argc;
char **argv;
{
	register STR *str;
	register FILE *tp;
	register int i;
	register unsigned char *lp;
	char buf[BUFSIZ];
	char *malloc(), *realloc();

	if(myname = rindex(*argv, '/'))
		myname++;
	else
		myname = *argv;
	for(argc--, argv++ ; argc > 0 && **argv == '-' ; argc--, argv++) {
		switch((*argv)[1]) {
		 case 'h':
			usage();
		 case 'l':
			if(nproducts <= 0 && (products =
			 (char **)malloc(maxproducts*sizeof(char *))) == NULL) {
				fprintf(stderr,
				 "%s: Out of memory creating products array\n",
				 myname);
				exit(1);
			} else if(nproducts >= maxproducts - 1 && (products =
			 (char **)realloc(products, (maxproducts += NPRODUCTS)
			 * sizeof(char *))) == NULL) {
				fprintf(stderr,
				 "%s: Out of memory expanding products array\n",
				 myname);
				exit(1);
			}
			if((*argv)[2])
				products[nproducts++] = &(*argv)[2];
			else {
				if(argc < 2) {
					fprintf(stderr,
					 "%s: No argument for -l\n", myname);
					exit(1);
				}
				argc--;
				argv++;
				products[nproducts++] = *argv;
			}
			break;
		}
	}
	if(argc > 1)
		usage();
	if(argc > 0 && freopen(*argv, "r", stdin) == NULL) {
		fprintf(stderr, "%s: Can't open %s\n", myname, *argv);
		exit(1);
	}
	mktemp(tempname);
	if((tp = fopen(tempname, "w+")) == NULL) {
		fprintf(stderr, "%s: Can't create temp file %s\n",
		 myname, tempname);
		exit(1);
	}
	unlink(tempname);
	str = STRalloc();
	if(!STRgets(str, stdin)) {
		fprintf(stderr, "%s: Null input\n", myname);
		exit(1);
	}
	for( ; ; ) {
		if(STRheadcompare(str, "% \251") == 0) {
			fputs("% ", tp);
			str->bufptr[0] = '(';
			str->bufptr[1] = 'C';
			str->bufptr[2] = ')';
		} else if(STRheadcompare(str, "%%BeginProcSet:") == 0) {
			STRputs(str, stdout);
			fseek(tp, 0L, 0);
			while((i = fread(buf, 1, BUFSIZ, tp)) > 0)
				fwrite(buf, 1, i, stdout);
			fclose(tp);
			break;
		}
		STRputs(str, tp);
		if(!STRgets(str, stdin)) {
			fprintf(stderr, "%s: No BeginProcSet\n", myname);
			exit(1);
		}
	}
	while(STRgets(str, stdin)) {
		if(nproducts > 0 && STRheadcompare(str, "/ok{") == 0) {
			STRputs(str, stdout);
			fputs(ProdArr0, stdout);
			for(i = 0 ; i < nproducts ; i++)
				printf("(%s)\n", products[i]);
			fputs(ProdArr1, stdout);
			fputs(Ok, stdout);
			continue;
		} else if(STRmatch(str, "setdefaulttimeouts")
		 || STRmatch(str, "setsccinteractive"))
			continue;
		else if(STRmatch(str, "/stretch") && STRmatch(str, "eexec")) {
			eexec("stretch", str);
			continue;
		} else if(STRmatch(str, "/smooth4") && STRmatch(str, "eexec")) {
			eexec("smooth4", str);
			continue;
		} else if(STRmatch(str, " checkload")) {
			checkload(str);
			continue;
		} else if(STRmatch(str, "(LaserWriter II NT)")) {
			while(STRgets(str, stdin) && STRheadcompare(str, "35de")
			 != 0)
				{ /* ignore line */ }
			while(STRgets(str, stdin) && isxdigit(*str->bufptr))
				{ /* ignore line */ }
		} else if(lp = STRmatch(str, "scaleby96{ppr")) {
			STRputsptr(str, lp, stdout);
			continue;
		} else if(STRmatch(str, "waittimeout"))
			continue;
		else if(STRheadcompare(str, "%%EndProcSet") == 0) {
			STRputs(str, stdout);
			break;
		}
		STRputs(str, stdout);
	}
	exit(0);
}

eexec(name, str)
char *name;
register STR *str;
{
	register int len;

	if(!STRgets(str, stdin)) {
		fprintf(stderr, "%s: EOF during reading eexec\n", myname);
		exit(1);
	}
	len = (str->curendptr - str->bufptr) - 1;
	printf(exstr, nproducts > 0 ? 'O' : 'o', name, len + (len / EEXECLEN)
	 + (len % EEXECLEN ? 1 : 0) + EXTRA);
	spliteexec(str);
}

checkload(str)
register STR *str;
{
	if(nproducts > 0)
		*str->bufptr = 'O';
	STRputs(str, stdout);
	if(!STRgets(str, stdin)) {
		fprintf(stderr, "%s: EOF during reading eexec\n", myname);
		exit(1);
	}
	spliteexec(str);
}

spliteexec(str)
register STR *str;
{
	register int len;
	register unsigned char *bp;

	bp = str->bufptr;
	len = (str->curendptr - bp) - 1;
	while(len >= 80) {
		fwrite(bp, 80, 1, stdout);
		putchar('\n');
		bp += 80;
		len -= 80;
	}
	if(len > 0) {
		fwrite(bp, len, 1, stdout);
		putchar('\n');
	}
	for( ; ; ) {
		if(!STRgets(str, stdin)) {
			fprintf(stderr, "%s: EOF reached before cleartomark\n",
			 myname);
			exit(1);
		}
		STRputs(str, stdout);
		if(STRheadcompare(str, "cleartomark") == 0)
			return;
	}
}

usage()
{
	fprintf(stderr,
	 "Usage: %s [-l product_name1 [-l product_name2]...] [file]\n",
	 myname);
	fprintf(stderr, "       %s -help\n", myname);
	exit(1);
}
