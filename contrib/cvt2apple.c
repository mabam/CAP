/*
 *	cvt2apple
 *
 *	This program converts CAP/aufs style files to apple single or
 *	apple double format files (primarily for A/UX - you drag files
 *	from a client via aufs to a Unix volume than convert them to
 *	apple single format, then launch them using the A/UX launch
 *	utility).
 *
 *		cvt2apple [-d] cap-file apple-file
 *
 *		(if -d is specified an apple double file (pair) is created)
 *
 *	Bugs: doesn't support icons from the desktop file
 *	      (doesn't know how to find them :-(
 *
 *	COPYRIGHT NOTICE
 *	
 *	Copyright (c) May 1988, Paul Campbell, All Rights Reserved.
 *	
 *	Permission is granted to any individual or institution to use, copy,
 *	or redistribute this software so long as it is not sold for profit,
 *	provided that this notice and the original copyright notices are
 *	retained.  Paul Campbell makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *	
 *  History:
 *    4/23/88 Paul Campbell, submitted to CAP distribution
 *    4/23/88 Charlie C. Kim, clean up and modify to work with
 *            byte swapped machines
 *
 */	


#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include <netat/macfile.h>

#ifdef USEDIRENT
# include <dirent.h>
#else  USEDIRENT
# ifdef xenix5
#  include <sys/ndir.h>
# else  xenix5
#  include <sys/dir.h>
# endif xenix5
#endif USEDIRENT

char *prog;

struct entry {
	dword id;
	dword offset;
	dword length;
};


struct hdr {
	dword	magic;
	dword	version;
	char	home[16];
	word	nentries;
	struct entry	entries[1];
};

#define HDR_SIZE	26
#define	ENTRY_SIZE	sizeof(struct entry)

#define VERSION		0x00010000
#define APPLE_SINGLE	0x00051600
#define APPLE_DOUBLE	0x00051607
#define	ID_DATA		1
#define	ID_RESOURCE	2
#define	ID_COMMENT	4
#define	ID_FINDER	9
	
char dir[1025];
char file[1025];
char headers[1024];
FileInfo finfo;
byte *comment;

FILE *fiopen();
FILE *fdata, *fresource, *ffinder;

main(argc, argv)
char **argv;
{
	register struct hdr *hp;
	register struct entry *ep;
	int dbl;
	unsigned dlen;
	char *fin, *fout;
	FILE *f;
	register int i;
	struct stat s;

	dbl = 0;
	hp = (struct hdr *)headers;
	hp->nentries = 0;
	ep = hp->entries;

	/*
	 *	validate the flags and input/output file
 	 *	names
	 */

	prog = argv[0];
	if (argc < 3)
		usage();
	if (strcmp(argv[1], "-d") == 0) {
		dbl = 1;
		if (argc != 4)
			usage();
		fin = argv[2];
		fout = argv[3];
	} else {
		if (argc > 3)
			usage();
		fin = argv[1];
		fout = argv[2];
	}

	/*
	 *	pick apart the input file name
	 */

	name_expand(fin);

	/*
	 *	try and open the CAP finder info
	 */

	ffinder = fiopen(dir, ".finderinfo/", file, "r");
	if (ffinder) {
		if (fread(&finfo, 1, sizeof(finfo), ffinder) < sizeof(OldFileInfo))
			error("error reading finder info file");
		ep->id = ID_FINDER;
		ep->length = sizeof(finfo.fi_fndr);
		ep++;
		hp->nentries++;
  		if (finfo.fi_magic1 == FI_MAGIC1 &&
  		    finfo.fi_magic == FI_MAGIC) {
			ep->id = ID_COMMENT;
			ep->length = finfo.fi_comln;
			comment = finfo.fi_comnt;
			ep++;
			hp->nentries++;
		} else {
			ep->id = ID_COMMENT;
			ep->length = ((OldFileInfo *)&finfo)->fi_comln;
			comment = ((OldFileInfo *)&finfo)->fi_comnt;
			ep++;
			hp->nentries++;
		}
	}

	/*
	 *	try and open the CAP resource fork
	 */

	fresource = fiopen(dir, ".resource/", file, "r");
	if (fresource) {
		if (fstat(fileno(fresource), &s) >= 0) {
			ep->id = ID_RESOURCE;
			ep->length = s.st_size;
			ep++;
			hp->nentries++;
		}
	}

	/*
	 *	try and open the CAP data fork
	 */

	fdata = fiopen(dir, NULL, file, "r");
	if (fdata) {
		if (fstat(fileno(fdata), &s) >= 0) {
			if (dbl) {
				dlen = s.st_size;
			} else {
				ep->id = ID_DATA;
				ep->length = s.st_size;
				ep++;
				hp->nentries++;
			}
		}
	}

	/*
	 *	now pick apart the output name
	 */

	name_expand(fout);

	if (dbl) {

		/*
	 	 *	for a double file copy the forks that are
		 *	present, if nothing just give an empty data file
		 */

		if (hp->nentries == 0 && fdata == NULL)
			error("cannot open %s", fin);
		if (strlen(file) > MAXNAMLEN-1) {
		  fprintf(stderr, 
	  "%s: warning: output file name more than %d characters '%s'\n",
			  prog, (MAXNAMLEN-1), fout);
		}
		if (fdata && (dlen > 0 || hp->nentries == 0)) {
			f = fiopen(dir, NULL, file, "w");
			if (f == NULL) 
				error("cannot create data fork %s", fout);
			fcopy(f, fdata, dlen);
			fclose(f);
		}
		if (hp->nentries) {
			f = fiopen(dir, "%", file, "w");
			if (f == NULL) 
				error("cannot create resource %s", fout);
			write_single(f, hp, APPLE_DOUBLE);
			fclose(f);
		}
	} else {

		/*
		 *	if a single file just copy it in
		 */

		if (hp->nentries == 0)
			error("cannot open %s", fin);
		f = fiopen(dir, NULL, file, "w");
		if (f == NULL) 
			error("cannot open %s", fout);
		write_single(f, hp, APPLE_SINGLE);
		fclose(f);
	}
}

/*
 *	open the file "dir""ext""file" with mode "mode"
 */

FILE *
fiopen(dir, ext, file, mode)
char *dir, *ext, *file, *mode;
{
	char name[1025];

	strcpy(name, dir);
	if (ext)
		strcat(name, ext);
	strcat(name, file);
	return(fopen(name, mode));
}

/*
 *	print a nasty message
 */

usage()
{
	fprintf(stderr, "Usage: %s [-d] cap-file apple-file\n",
			prog);
	exit(1);
}

/*
 *	calculate a file header, write it out, then tack on
 *	all the contents
 *
 * on return: the header and entries are all converted to network
 * order
 */
write_single(fout, hp, magic)
FILE *fout;
struct hdr *hp;
{
	unsigned hsize, offset;
	register struct entry *ep;
	register int i;
	int n;
	dword el;

	n = hp->nentries;
	hsize = n*ENTRY_SIZE;
	offset = hsize + HDR_SIZE;

	for (i = 0, ep = hp->entries; i < n; i++, ep++) {
	  ep->id = htonl(ep->id); /* swap */
	  ep->offset = htonl(offset); /* swap */
	  offset += ep->length;
	  ep->length = htonl(ep->length); /* byte swap */
	}
	strncpy(hp->home, "Macintosh        ", 16);
	hp->magic = htonl(magic);
	hp->version = htonl(VERSION);
	hp->nentries = htons(n);

	/* must do as two writes because of padding problems in way */
	/* hdr is defined (double word aligment comes into play) */
	if (fwrite(hp, 1, HDR_SIZE, fout) != HDR_SIZE)
		error("error writing output file");
	if (fwrite(hp->entries, 1, hsize, fout) != hsize)
		error("error writing output file");
	for (i = 0, ep = hp->entries; i < n; i++, ep++) {
	  el = htonl(ep->length);
	  switch(ntohl(ep->id)) {
	  case ID_DATA:
	    fcopy(fout, fdata, el);
	    break;
	  case ID_RESOURCE:
	    fcopy(fout, fresource, el);
	    break;
	  case ID_COMMENT:
	    if (el) 
	      if (fwrite(comment, 1, el, fout) != el)
		error("error writing output file");
	    break;
	  case ID_FINDER:
	    if (fwrite(finfo.fi_fndr, el, 1, fout) != 1)
	      error("error writing output file");
	    break;
	  }
	}
}

/*
 *	copy length bytes from fin to fout
 */

fcopy(fout, fin, length)
FILE *fin, *fout;
unsigned length;
{
	char buffer[4096];
	register unsigned l;

	for (;;) {
		l = fread(buffer, 1, sizeof(buffer), fin);
		if (l > length)
			l = length;
		if (l > 0) {
			if (fwrite(buffer, 1, l, fout) != l)
				error("error writing output file");
		} else break;
		length -= l;
	}
}

/*
 * 	print another nasty message and quit 
 */

error(s, a, b, c, d, e, f)
char *s;
{
	fprintf(stderr, "%s: ", prog);
	fprintf(stderr, s, a, b, c, d, e, f);
	fprintf(stderr, "\n");
	exit(2);
}

/*
 *	expand a file name to directory and filename parts
 */

name_expand(name)
char *name;
{
	register char *cp;

	strcpy(dir, name);
	cp = &dir[strlen(dir)];
	for (;;) {
		if (*cp == '/') {
			strcpy (file, cp+1);
			*(cp+1) = 0;
			if (file[0] == 0)
				error("empty filename part in file name %s", name);
			break;
		}
		if (cp == dir) {
			strcpy(file, cp);
			if (file[0] == 0)
				error("empty filename part in file name %s", name);
			strcpy(dir, "./");
			break;
		}
		cp--;
	}
}
