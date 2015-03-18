/*
 *	cvt2cap
 *
 *	This program converts apple single or double files to CAP/aufs
 *	format files (primarily for A/UX).
 *
 *		cvt2cap apple-file cap-file
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


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include <netat/macfile.h>

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

FILE *fiopen();

main(argc, argv)
char **argv;
{
	register struct hdr *hp;
	register struct entry *ep;
	char *fin, *fout;
	FILE *f, *fd, *fx;
	register int i, j;
	int resource, data;
	int retry;
	char rname[33];

	hp = (struct hdr *)headers;
	bzero(&finfo, sizeof(finfo)); /* make sure clear first */

	/*
	 *	validate the flags and input/output file
 	 *	names
	 */

	prog = argv[0];
	if (argc != 3)
		usage();
	fin = argv[1];
	fout = argv[2];

	name_expand(fin);
	for (retry = 0;;retry++) {
		switch(retry) {
		case 0:
			f = fiopen(dir, NULL, file, "r");
			break;
		case 1:
			fd = f; 
			f = fiopen(dir, "%", file, "r");
			break;
		case 2:
			error("Cannot find valid input file '%s'", fin);
		}
		if (f == NULL) 
			continue;

		/*
		 *	Read the header 
		 */

		if (fread(hp, HDR_SIZE, 1, f) < 1)
			continue;
		if (ntohl(hp->magic) != APPLE_SINGLE &&
		    ntohl(hp->magic) != APPLE_DOUBLE)
			continue;
		if (ntohl(hp->version) != VERSION)
			continue;
		if (ntohl(hp->magic) == APPLE_DOUBLE &&
		    file[0] != '%' && retry != 1) 
			error("Apple double file name must begin with %% '%s'",
			      fin);
		if (strncmp(hp->home, "Macintosh                 ", 16) != 0) {
			hp->home[15] = 0;
			error("Invalid file type '%s' in '%s'", hp->home, fin);
		}
		if (fread(hp->entries, ENTRY_SIZE, ntohs(hp->nentries), f) < 1)
			continue;
		break;
	}
	if (file[0] == '%') {
		strncpy(rname, &file[1], 32);
	} else {
		strncpy(rname, file, 32);
	}

	data = 0;
	resource = 0;
	name_expand(fout);
	for (i = 0, ep = hp->entries; i < (int)ntohs(hp->nentries); i++, ep++) {
		switch(ntohl(ep->id)) {
		case ID_DATA:
			fx = fiopen(dir, NULL, file, "w");
			if (fx == NULL)
			  error("Cannot create output data file '%s'", fout);
			fseek(f, ntohl(ep->offset), 0);
			fcopy(fx, f, ntohl(ep->length));
			fclose(fx);
			data = 1;
			break;

		case ID_RESOURCE:
			fx = fiopen(dir, ".resource/", file, "w");
			if (fx == NULL)
			  error("Cannot create output resource file '%s'",
				fout);
			fseek(f, ntohl(ep->offset), 0);
			fcopy(fx, f, ntohl(ep->length));
			fclose(fx);
			resource = 1;
			break;

		case ID_COMMENT:
			fseek(f, ntohl(ep->offset), 0);
  			j = ntohl(ep->length);
			if (j > MAXCLEN)
				j = MAXCLEN;
			if (j > 0)
			if (fread(finfo.fi_comnt, j, 1, f) < 1)
				error("Couldn't read input file '%s'", fin);
			finfo.fi_comln = j;
			break;

		case ID_FINDER:
			fseek(f, ntohl(ep->offset), 0);
			if (fread(finfo.fi_fndr, ntohl(ep->length), 1, f) < 1)
				error("Couldn't read input file '%s'", fin);
			break;
		}
	}
	if (!data && hp->magic == APPLE_DOUBLE && fd) {
		fseek(fd, 0, 0);
		fx = fiopen(dir, NULL, file, "w");
		if (fx == NULL)
			error("Cannot create output data file '%s'", fout);
		fcopy(fx, fd, ntohl(ep->length));
		fclose(fx);
	} else 
	if (!data) {
		fx = fiopen(dir, NULL, file, "w");
		if (fx == NULL)
			error("Cannot create output data file '%s'", fout);
		fclose(fx);
	}
	fx = fiopen(dir, ".finderinfo/", file, "w");
	if (fx == NULL)
		error("Cannot create output finder info file '%s'", fout);
	finfo.fi_magic = FI_MAGIC;
  	finfo.fi_magic1 = FI_MAGIC1;
  	finfo.fi_version = FI_VERSION;
  	strcpy(finfo.fi_macfilename, rname);
  	finfo.fi_bitmap = FI_BM_MACINTOSHFILENAME;
	if (fwrite(&finfo, sizeof(finfo), 1, fx) < 1)
		error("Cannot write output finder info file '%s'", fout);
	fclose(fx);
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
	fprintf(stderr, "Usage: %s apple-file cap-file\n",
			prog);
	exit(1);
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
