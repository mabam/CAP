/* Modified Aug & Dec 90, Nigel Perry, Dept of Computing, Imperial College,
   London SW7 2BZ, np@doc.ic.ac.uk

   If called by AUFSNAME (default unstuffit) will input/output to
   CAPS aufs disc.

   Will now untiff multiple files.

 */
/*
	       unsit - Macintosh StuffIt file extractor

		     Version 1.5f, for StuffIt 1.5

			    July 23, 1990

This program will unpack a Macintosh StuffIt file into separate files.
The data fork of a StuffIt file contains both the data and resource
forks of the packed files.  The program will unpack each Mac file into
either separate .data, .rsrc., and .info files that can be downloaded
to a Mac using macput and MacTerminal over a tty line, or into a
single MacBinary format file.  The MacBinary format is generally more
convenient for those with network connections and FTP capability.  The
program is much like the "unpit" program for breaking apart Packit
archive files.

			***** IMPORTANT *****
To extract StuffIt files that have been compressed with the Lempel-Ziv
compression method, unsit pipes the data through the "compress"
program with the appropriate switches, rather than incorporate the
uncompression routines within "unsit".  Therefore, it is necessary to
have the "compress" program on the system and in the search path to
make "unsit" work.  "Compress" is available from the comp.sources.unix
archives.

The program syntax is much like unpit and macput/macget, with some added
options:

	unsit [-rdulM] [-vqfm] stuffit-file.data

Only one of the flags r, d, u, l, or M should be specified.  The
default mode is to create the three macput/MacTerminal compatible
file.  The -M flag will cause the output to be in MacBinary format (a
single file).  This can be swapped (default = MacBinary, -M = macput)
by changing the definitions of DEFAULT_MODE and OTHER_MODE below.  The
-r and -d flags will cause only the resource and data forks to be
written.  The -u flag will cause only the data fork to be written and
to have carriage return characters changed to Unix newline characters.
The -l flag will make the program only list the files in the StuffIt
file.

The -v flag causes the program to list the names, sizes, type, and
creators of the files it is writing.  The -q flag causes it to list
the name, type and size of each file and wait for a 'y' or 'n' for
either writing that file or skipping it, respectively.  The -m flag is
used when the input file in in the MacBinary format instead of just
the data fork.  It causes the program to skip the 128 byte MacBinary
header before looking for the StuffIt header.  It is not necessary to
specify the -m flag since the program now checks for MacBinary format
input files and handles them correctly

Version 1.5 of the unsit supports extracting files and folders as
implemented by StuffIt 1.5's "Hierarchy Maintained Folder" feature.
Each folder is extracted as a subdirectory on the Unix system with the
files in the folder placed in the corresponding subdirectory.  The -f
option can be used to "flatten" out the hierarchy and unsit will store
all the files in the current directory.  If the query option (-q) is
used and a "n" response is given to a folder name, none of the files
or folders in that folder will be extraced.

Some of the program is borrowed from the macput.c/macget.c programs.
Many, many thanks to Raymond Lau, the author of StuffIt, for including
information on the format of the StuffIt archives in the
documentation.  Several changes and enhancements supplied by David
Shanks (cde@atelabs.UUCP) have been incorporated into the program for
doing things like supporting System V and recognizing MacBinary files.
Christopher Bingham <kb@umnstat.stat.umn.edu> supplied some Macbinary
patches.  Code was also borrowed from the macbin program by Jim Budler
for convert macput format files to MacBinary.  I'm always glad to
receive advice, suggestions, or comments about the program so feel free
to send whatever you think would be helpful


	Author: Allan G. Weber
		weber@sipi.usc.edu
		...!usc!sipi!weber
	Date:   July 23, 1990

*/

#ifdef sun
#ifdef __svr4__
#define SOLARIS
#endif __svr4__
#endif sun

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef SOLARIS
#include <string.h>
#include <netat/sysvcompat.h>
#endif SOLARIS

typedef long OSType;

#include "stuffit.h"

/*
 * Define the following if your Unix can only handle 14 character file names
 * (e.g. Version 7 and System V).
 */
/* #define SHORTNAMES */

/*
 * The following defines the name of the compress program that is used for the
 * uncompression of Lempel-Ziv compressed files.  If the path is set up to
 * include the right directory, this should work.
 */
#define COMPRESS   "compress"

#define IOBUFSIZ   4096

#define MACBINHDRSIZE  128L

#define INIT_CRC 0L
extern unsigned short updcrc();

#define INFOBYTES 128

#define BYTEMASK 0xff

#define S_SIGNATURE    0
#define S_NUMFILES     4
#define S_ARCLENGTH    6
#define S_SIGNATURE2  10
#define	S_VERSION     14
#define SITHDRSIZE    22

#define F_COMPRMETHOD    0
#define F_COMPDMETHOD    1
#define F_FNAME          2
#define F_FTYPE         66
#define F_CREATOR       70
#define F_FNDRFLAGS     74
#define F_CREATIONDATE  76
#define F_MODDATE       80
#define F_RSRCLENGTH    84
#define F_DATALENGTH    88
#define F_COMPRLENGTH   92
#define F_COMPDLENGTH   96
#define F_RSRCCRC      100
#define F_DATACRC      102
#define F_HDRCRC       110
#define FILEHDRSIZE    112

#define F_NAMELEN 63
#ifdef SHORTNAMES		/* short file names */
# define I_NAMELEN 15		/* 14 char file names + '\0' terminator */
#else
# define I_NAMELEN 69		/* 63 + strlen(".info") + 1 */
#endif

/* The following are copied out of macput.c/macget.c */
#define I_NAMEOFF 1
/* 65 <-> 80 is the FInfo structure */
#define I_TYPEOFF 65
#define I_AUTHOFF 69
#define I_FLAGOFF 73
#define I_LOCKOFF 81
#define I_DLENOFF 83
#define I_RLENOFF 87
#define I_CTIMOFF 91
#define I_MTIMOFF 95

#define INITED_BUG
#define INITED_OFF	I_FLAGOFF	/* offset to byte with Inited flag */
#define INITED_MASK	(~1)		/* mask to '&' with byte to reset it */

#define TEXT 0
#define DATA 1
#define RSRC 2
#define MACPUT 3
#define DUMP 4
#define MACBINARY 5

/* Swap the following definitions if you want the output to default to
   MacBinary, and the -M switch to create macput file (.data, .rsrc, .info) */
#define DEFAULT_MODE MACPUT
#define OTHER_MODE   MACBINARY

/* #define ADDBIN */	/* add .bin to macbinary file names */

#define NODECODE 0
#define DECODE   1

#define H_ERROR -1
#define H_EOF    0
#define H_WRITE  1
#define H_SKIP   2

/* if called by this name program works on aufs files */
#define AUFSNAME "unstuffit"

struct node {
	int flag, byte;
	struct node *one, *zero;
} nodelist[512], *nodeptr, *read_tree();	/* 512 should be big enough */

struct sitHdr sithdr;

char f_info[I_NAMELEN];
char f_data[I_NAMELEN];
char f_rsrc[I_NAMELEN];

char info[INFOBYTES];
char mname[F_NAMELEN+1];
char uname[F_NAMELEN+1];
char dname[F_NAMELEN+1];	/* directory name for aufs */
char iobuf[IOBUFSIZ];
char zbuf[128];			/* buffer of zeros to pad MacBinary forks */

int mode, txtmode, listonly, verbose, query, flatten;
int bit, chkcrc, numfiles, depth;
int aufs;			/* aufs flag */
int debug = 0;
FILE *infp;

long get4();
short get2();
unsigned short write_file();

/********************************************************************************/
/* added for aufs, nicked from various places... */

/* following from mcvert program */

/* Useful, though not particularly Mac related, values */
typedef unsigned char byte;     /* one byte, obviously */
typedef unsigned short word;    /* must be 2 bytes */

#define NAMELEN 63              /* maximum legal Mac file name length */

/* Format of a bin file:
A bin file is composed of 128 byte blocks.  The first block is the
info_header (see below).  Then comes the data fork, null padded to fill the
last block.  Then comes the resource fork, padded to fill the last block.  A
proposal to follow with the text of the Get Info box has not been implemented,
to the best of my knowledge.  Version, zero1 and zero2 are what the receiving
program looks at to determine if a MacBinary transfer is being initiated.
*/ 
typedef struct {     /* info file header (128 bytes). Unfortunately, these
                        longs don't align to word boundaries */
            byte version;           /* there is only a version 0 at this time */
            byte nlen;              /* Length of filename. */
            byte name[NAMELEN];     /* Filename (only 1st nlen are significant)*/
            byte type[4];           /* File type. */
            byte auth[4];           /* File creator. */
            byte flags;             /* file flags: LkIvBnSyBzByChIt */
            byte zero1;             /* Locked, Invisible,Bundle, System */
                                    /* Bozo, Busy, Changed, Init */
            byte icon_vert[2];      /* Vertical icon position within window */
            byte icon_horiz[2];     /* Horizontal icon postion in window */
            byte window_id[2];      /* Window or folder ID. */
            byte protect;           /* = 1 for protected file, 0 otherwise */
            byte zero2;
            byte dflen[4];          /* Data Fork length (bytes) -   most sig.  */
            byte rflen[4];          /* Resource Fork length         byte first */
            byte cdate[4];          /* File's creation date. */
            byte mdate[4];          /* File's "last modified" date. */
            byte ilen[2];           /* GetInfo message length */
	    byte flags2;            /* Finder flags, bits 0-7 */
	    byte unused[14];       
	    byte packlen[4];        /* length of total files when unpacked */
	    byte headlen[2];        /* length of secondary header */
	    byte uploadvers;        /* Version of MacBinary II that the uploading program is written for */
	    byte readvers;          /* Minimum MacBinary II version needed to read this file */
            byte crc[2];            /* CRC of the previous 124 bytes */
	    byte padding[2];        /* two trailing unused bytes */
            } info_header;

/* end of mcvert stuff */
/* from CAP aufs documentation */

#define FINFOLEN 32
#define MAXCLEN 199
typedef struct
{
   /* be careful with alignment */
   byte fndr_type[4];
   byte fndr_creator[4];
   word fndr_flags;
   byte fndr_loc[4];
   word fndr_fldr;
   word fndr_icon;
   byte fndr_unused[8];
   word fndr_comment;
   byte fndr_putaway[4];
   word fi_attr;			/* attributes */
#define FI_MAGIC1 255
   byte fi_magic1;		/* was: length of comment */
#define FI_VERSION 0x10		/* version major 1, minor 0 */
				/* if more than 8 versions then */
				/* something wrong anyway */
   byte fi_version;		/* version number */
#define FI_MAGIC 0xda
   byte fi_magic;		/* magic word check */
   byte fi_bitmap;		/* bitmap of included info */
#define FI_BM_SHORTFILENAME 0x1	/* is this included? */
#define FI_BM_MACINTOSHFILENAME 0x2 /* is this included? */
   byte fi_shortfilename[12+1];	/* possible short file name */
   byte fi_macfilename[32+1];	/* possible macintosh file name */
   byte fi_comln;		/* comment length */
   byte fi_comnt[MAXCLEN+1];	/* comment string */
} FileInfo;

FileInfo fndr_info;

/* end aufs */
/********************************************************************************/

main(argc, argv)
int argc;
char *argv[];
{
    int status;
    int c;
    extern int optind;
    extern char *optarg;
    int errflg;
    int macbin;
    char *s, *progname;

    mode = DEFAULT_MODE;
    errflg = 0;
    macbin = 0;
    flatten = 0;
    numfiles = 0;
    depth = 0;

    progname = argv[0];
    if ((s = (char *) rindex(progname, '/')) != NULL)
	progname = ++s;
    aufs = strcmp(progname, AUFSNAME) == 0;

    while ((c = getopt(argc, argv, "DMdflmqruvx")) != EOF)
	switch (c) {
	  case 'r':		/* extract resource fork only */
	    mode = RSRC;
	    break;
	  case 'd':		/* extract data fork only */
	    mode = DATA;
	    break;
	  case 'u':		/* extract data fork as Unix text file */
	    mode = TEXT;
	    break;
	  case 'l':		/* list contents of archive */
	    listonly++;
	    break;
	  case 'q':		/* query user on each extraction */
	    query++;
	    break;
	  case 'v':		/* verbose mode */
	    verbose++;
	    break;
	  case 'x':		/* don't decode data, just dump to files*/
	    mode = DUMP;
	    break;
	  case 'm':		/* input file is in Macbinary format */
	    macbin = 1;
	    break;
	  case 'M':		/* output file in OTHER_MODE */
	    mode = OTHER_MODE;
	    break;
	  case 'f':		/* don't create flat directory tree */
	    flatten = 1;
	    break;
	  case 'D':		/* debugging mode */
	    debug = 1;
	    break;
	  case '?':
	    errflg++;
	    break;
	}
    if (errflg) {
	usage();
	exit(1);
    }

    /* -a incompatible with -M and -u */
    if(aufs && (mode == MACBINARY || mode == TEXT))
    {   fprintf(stderr, "Aufs mode cannot be combined with MacBinary (-M) or text (-t) mode\n");
	exit(1);
    }

    if (optind == argc) {
	usage();
	exit(1);
    }
    else
	while(optind < argc)
        {   if ((infp = fopen(argv[optind], "r")) == NULL)
	    {   fprintf(stderr,"Can't open input file \"%s\"\n",argv[optind]);
	        exit(1);
	    }

	    if (macbin) {
		if (fseek(infp, MACBINHDRSIZE, 0) == -1) {
		    fprintf(stderr, "Can't skip over MacBinary header\n");
		    exit(1);
		}
	    }

	    if (readsithdr(&sithdr) == 0) {
		fprintf(stderr, "Can't read file header\n");
		exit(1);
	    }
	    if (debug) {
		printf("archive header (%d bytes):\n", SITHDRSIZE);
		printf("numFiles=%d, arcLength=%ld, version=%d\n",
		       sithdr.numFiles, sithdr.arcLength, sithdr.version & 0xff);
	    }
	    status = extract("", 0);
	    if(status < 0) exit(1);

	    optind++;
	}

	exit(0);
}

usage()
{
    fprintf(stderr, "Usage: unsit/unstuffit [-rdulM] [-vqfm] filename\n");
}

/*
  extract(parent, skip) - Extract all files from the current folder.
  char *parent;           name of parent folder
  int  skip;              1 to skip all files and folders in this one
                          0 to extract them

  returns 1 if came an endFolder record
          0 if EOF
	 -1 if error (bad fileHdr, bad file, etc.)
*/

extract(parent, skip)
char *parent;
int skip;
{
    struct fileHdr filehdr;
    struct stat sbuf;
    int status, rstat, sstat, skipit;
    char name[256];

    while (1) {
	rstat = readfilehdr(&filehdr, skip);
	if (rstat == H_ERROR || rstat == H_EOF) {
	    status = rstat;
	    break;
	}
	if (debug) {
	    printf("file header (%d bytes):\n", FILEHDRSIZE);
	    printf("compRMethod=%d, compDMethod=%d\n",
		   filehdr.compRMethod, filehdr.compDMethod);
	    printf("rsrcLength=%ld, dataLength=%ld\n",
		   filehdr.rsrcLength, filehdr.dataLength);
	    printf("compRLength=%ld, compDLength=%ld\n",
		   filehdr.compRLength, filehdr.compDLength);
	    printf("rsrcCRC=%d=0x%04x, dataCRC=%d=0x%04x\n",
		   filehdr.rsrcCRC, filehdr.rsrcCRC,
		   filehdr.dataCRC, filehdr.dataCRC);
	}

	skipit = (rstat == H_SKIP) ? 1 : 0;

	if (filehdr.compRMethod == endFolder && 
	    filehdr.compDMethod == endFolder) {
	    status = 1;		/* finished with this folder */
	    break;
	}
	else if (filehdr.compRMethod == startFolder && 
		 filehdr.compDMethod == startFolder) {
	    if (!listonly && rstat == H_WRITE && !flatten) {
		sstat = stat(uname, &sbuf);
		if (sstat == -1) {	/* directory doesn't exist */
		    if (mkdir(uname, 0777) == -1) {
			fprintf(stderr,
				"Can't create subdirectory %s\n", uname);
			return(-1);
		    }
		}
		else {		/* something exists with this name */
		    if ((sbuf.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr, "Directory name %s already in use\n",
				uname);
			return(-1);
		    }
		}
		if(aufs) /* create .finderinfo & .resource subdirectories */
		{   strcpy(dname, uname);
		    strcat(dname, "/.finderinfo");
		    sstat = stat(dname, &sbuf);
		    if (sstat == -1) {	/* directory doesn't exist */
			if (mkdir(dname, 0777) == -1) {
			    fprintf(stderr,
				    "Can't create subdirectory %s\n", dname);
			    return(-1);
			}
		    }
		    else {		/* something exists with this name */
			if ((sbuf.st_mode & S_IFMT) != S_IFDIR) {
			    fprintf(stderr, "Directory name %s already in use\n",
				    dname);
			    return(-1);
			}
		    }
		    strcpy(dname, uname);
		    strcat(dname, "/.resource");
		    sstat = stat(dname, &sbuf);
		    if (sstat == -1) {	/* directory doesn't exist */
			if (mkdir(dname, 0777) == -1) {
			    fprintf(stderr,
				    "Can't create subdirectory %s\n", dname);
			    return(-1);
			}
		    }
		    else {		/* something exists with this name */
			if ((sbuf.st_mode & S_IFMT) != S_IFDIR) {
			    fprintf(stderr, "Directory name %s already in use\n",
				    dname);
			    return(-1);
			}
		    }
		}
		if (chdir(uname) == -1) {
		    fprintf(stderr, "Can't chdir to %s\n", uname);
		    return(-1);
		}
		sprintf(name,"%s:%s", parent, uname);
	    }
	    depth++;
	    status = extract(name, skipit);
	    depth--;
	    if (status != 1)
		break;		/* problem with folder */
	    if (depth == 0)	/* count how many top-level files done */
		numfiles++;
	    if (!flatten)
		chdir("..");
	}
	else {
	    if ((status = extractfile(&filehdr, skipit)) != 1)
		break;
	    if (depth == 0)	/* count how many top-level files done */
		numfiles++;
	}
	if (numfiles == sithdr.numFiles)
	    break;
    }
    return(status);
}

extractfile(fh, skip)
struct fileHdr *fh;
int skip;
{
    unsigned short crc;
    FILE *fp, *fp1;
    int n;

    f_data[0] = f_rsrc[0] = f_info[0] = '\0'; /* assume no output files */
    /* figure out what file names to use and what to do */
    if (!listonly && !skip) {
	switch (mode) {
	  case MACPUT:		/* do both rsrc and data forks */
	    if(aufs)
	    {   sprintf(f_data, "%.*s", I_NAMELEN - 1, uname);
		sprintf(f_rsrc, ".resource/%.*s", I_NAMELEN - 1, uname);
		sprintf(f_info, ".finderinfo/%.*s", I_NAMELEN - 1, uname);
	    }
	    else
	    {   sprintf(f_data, "%.*s.data", I_NAMELEN - 6, uname);
		sprintf(f_rsrc, "%.*s.rsrc", I_NAMELEN - 6, uname);
		sprintf(f_info, "%.*s.info", I_NAMELEN - 6, uname);
	    }
	    break;
	  case RSRC:		/* rsrc fork only */
	    if(aufs)
	    {   sprintf(f_rsrc, ".resource/%.*s", I_NAMELEN - 1, uname);
		sprintf(f_info, ".finderinfo/%.*s", I_NAMELEN - 1, uname);
	    }
	    else
		sprintf(f_rsrc, "%.*s.rsrc", I_NAMELEN - 6, uname);
	    break;
	  case DATA:		/* data fork only */
	    if(aufs)
	    {   sprintf(f_data, "%.*s", I_NAMELEN - 1, uname);
		sprintf(f_info, ".finderinfo/%.*s", I_NAMELEN - 1, uname);
	    }
	    else
	        sprintf(f_data, "%.*s.data", I_NAMELEN - 6, uname);
	    break;
	  case TEXT:
	    sprintf(f_data, "%.*s.text", I_NAMELEN - 6, uname);
	    break;
	  case DUMP:		/* for debugging, dump data as is */
	    sprintf(f_data, "%.*s.ddump", I_NAMELEN - 7, uname);
	    sprintf(f_rsrc, "%.*s.rdump", I_NAMELEN - 7, uname);
	    fh->compRMethod = fh->compDMethod = noComp;
	    break;
	  case MACBINARY:	/* output file in MacBinary format */
	    sprintf(f_data, "%.*s.data", I_NAMELEN - 6, uname);
	    sprintf(f_rsrc, "%.*s.rsrc", I_NAMELEN - 6, uname);
#ifndef ADDBIN
	    sprintf(f_info, "%.*s", I_NAMELEN - 1, uname);
#else
	    sprintf(f_info, "%.*s.bin", I_NAMELEN - 5, uname);
#endif /*ADDBIN*/
	    break;
	}
    }

    fp = NULL;			/* so we can tell if a file is open */
    if (f_info[0] != '\0' && check_access(f_info) != -1) {
	fp = fopen(f_info, "w");
	if (fp == NULL) {
	    perror(f_info);
	    exit(1);
	}
	if(aufs) /* convert info structure */
	{   register info_header *pinfo;
	
	    pinfo = (info_header *)info;

	    /* make the .finderinfo file */
	    bzero(&fndr_info, sizeof(FileInfo));
	    bcopy(pinfo->type, fndr_info.fndr_type, 4);
	    bcopy(pinfo->auth, fndr_info.fndr_creator, 4);
	    bcopy(&pinfo->flags, &fndr_info.fndr_flags, 2);
	    fndr_info.fi_magic1 = FI_MAGIC1;
	    fndr_info.fi_version = FI_VERSION;
	    fndr_info.fi_magic = FI_MAGIC;
	    fndr_info.fi_bitmap = FI_BM_MACINTOSHFILENAME;
	    bcopy(pinfo->name, fndr_info.fi_macfilename, pinfo->nlen);

	    fwrite(&fndr_info, sizeof(FileInfo), 1, fp);
	}
	else
	{   if (mode == MACBINARY) { /* convert to MacBinary header */
		/* taken from the macbin program */
		if (info[74] & 0x40) info[81] = '\1'; /* protected */
		info[74] = '\0'; /* clear zero2 */
		info[82] = '\0'; /* force zero3 clear */
	    }
	    fwrite(info, 1, INFOBYTES, fp);
	}
    }

    if (f_rsrc[0] != '\0') {
	txtmode = 0;
	crc = write_file(f_rsrc, fh->compRLength,
			 fh->rsrcLength, fh->compRMethod);
	if (chkcrc && fh->rsrcCRC != crc) {
	    fprintf(stderr,
		    "CRC error on resource fork: need 0x%04x, got 0x%04x\n",
		    fh->rsrcCRC, crc);
	    return(-1);
	}
    }
    else {
	fseek(infp, (long) fh->compRLength, 1);
    }
    if (f_data[0] != '\0') {
	txtmode = (mode == TEXT);
	crc = write_file(f_data, fh->compDLength,
			 fh->dataLength, fh->compDMethod);
	if (chkcrc && fh->dataCRC != crc) {
	    fprintf(stderr,
		    "CRC error on data fork: need 0x%04x, got 0x%04x\n",
		    fh->dataCRC, crc);
	    return(-1);
	}
    }
    else {
	fseek(infp, (long) fh->compDLength, 1);
    }
    if (fp != NULL) {
	/* if Macbinary output, copy the data and resource forks to the
	   end of the info file, and pad each to multiples of 128 bytes. */
	if (mode == MACBINARY) {
	    fp1 = fopen(f_data, "r"); /* re-open the file we just wrote */
	    if (fp1 == NULL) {
		perror(f_data);
		exit(1);
	    }
	    while ((n = fread(iobuf, 1, IOBUFSIZ, fp1)) > 0)
		fwrite(iobuf, 1, n, fp); /* append it to the info file */
	    /* pad out to multiple of 128 if in MacBinary format */
	    n = fh->dataLength % 128;
	    if (n > 0)
		outc(zbuf, 128 - n, fp);
	    fclose(fp1);
	    unlink(f_data);
	    fp1 = fopen(f_rsrc, "r"); /* re-open the file we just wrote */
	    if (fp1 == NULL) {
		perror(f_rsrc);
		exit(1);
	    }
	    while ((n = fread(iobuf, 1, IOBUFSIZ, fp1)) > 0)
		fwrite(iobuf, 1, n, fp); /* append it to the info file */
	    /* pad out to multiple of 128 if in MacBinary format */
	    n = fh->rsrcLength % 128;
	    if (n > 0)
		outc(zbuf, 128 - n, fp);
	    fclose(fp1);
	    unlink(f_rsrc);
	}
	fclose(fp);
    }
    return(1);
}

readsithdr(s)
struct sitHdr *s;
{
    char temp[FILEHDRSIZE];
    int count = 0;

    for (;;) {
	if (fread(temp, 1, SITHDRSIZE, infp) != SITHDRSIZE) {
	    fprintf(stderr, "Can't read file header\n");
	    return(0);
	}
    	
	if (strncmp(temp + S_SIGNATURE,  "SIT!", 4) == 0 &&
	    strncmp(temp + S_SIGNATURE2, "rLau", 4) == 0) {
	    s->numFiles = get2(temp + S_NUMFILES);
	    s->arcLength = get4(temp + S_ARCLENGTH);
	    return(1);
	}
    
	if (++count == 2) {
	    fprintf(stderr, "Not a StuffIt file\n");
	    return(0);
	}
	
	if (fread(&temp[SITHDRSIZE], 1, FILEHDRSIZE - SITHDRSIZE, infp) !=
	    FILEHDRSIZE - SITHDRSIZE) {
	    fprintf(stderr, "Can't read file header\n");
	    return(0);
	}
    
	if (strncmp(temp + I_TYPEOFF, "SIT!", 4) == 0 &&
	    strncmp(temp + I_AUTHOFF, "SIT!", 4) == 0) {	/* MacBinary format */
	    fseek(infp, (long)(INFOBYTES-FILEHDRSIZE), 1);	/* Skip over header */
	}
    }
}

/*
  readfilehdr - reads the file header for each file and the folder start
  and end records.

  returns: H_ERROR = error
	   H_EOF   = EOF
	   H_WRITE = write file/folder
	   H_SKIP  = skip file/folder
*/

readfilehdr(f, skip)
struct fileHdr *f;
int skip;
{
    unsigned short crc;
    int i, n, write_it, isfolder;
    char hdr[FILEHDRSIZE];
    char ch, *mp, *up;
    char *tp, temp[10];

    for (i = 0; i < INFOBYTES; i++)
	info[i] = '\0';

    /* read in the next file header, which could be folder start/end record */
    n = fread(hdr, 1, FILEHDRSIZE, infp);
    if (n == 0)			/* return 0 on EOF */
	return(H_EOF);
    else if (n != FILEHDRSIZE) {
	fprintf(stderr, "Can't read file header\n");
	return(H_ERROR);
    }

    /* check the CRC for the file header */
    crc = INIT_CRC;
    crc = updcrc(crc, hdr, FILEHDRSIZE - 2);
    f->hdrCRC = get2(hdr + F_HDRCRC);
    if (f->hdrCRC != crc) {
	fprintf(stderr, "Header CRC mismatch: got 0x%04x, need 0x%04x\n",
		f->hdrCRC, crc);
	return(H_ERROR);
    }

    /* grab the name of the file or folder */
    n = hdr[F_FNAME] & BYTEMASK;
    if (n > F_NAMELEN)
	n = F_NAMELEN;
    info[I_NAMEOFF] = n;
    copy(info + I_NAMEOFF + 1, hdr + F_FNAME + 1, n);
    strncpy(mname, hdr + F_FNAME + 1, n);
    mname[n] = '\0';
    /* copy to a string with no illegal Unix characters in the file name */
    mp = mname;
    up = uname;
    while ((ch = *mp++) != '\0') {
	if (ch <= ' ' || ch > '~' || index("/!()[]*<>?\\\"$\';&`", ch) != NULL)
	    ch = '_';
	*up++ = ch;
    }
    *up = '\0';

    /* get lots of other stuff from the header */
    f->compRMethod = hdr[F_COMPRMETHOD];
    f->compDMethod = hdr[F_COMPDMETHOD];
    f->rsrcLength = get4(hdr + F_RSRCLENGTH);
    f->dataLength = get4(hdr + F_DATALENGTH);
    f->compRLength = get4(hdr + F_COMPRLENGTH);
    f->compDLength = get4(hdr + F_COMPDLENGTH);
    f->rsrcCRC = get2(hdr + F_RSRCCRC);
    f->dataCRC = get2(hdr + F_DATACRC);

    /* if it's an end folder record, don't need to do any more */
    if (f->compRMethod == endFolder && f->compDMethod == endFolder)
	return(H_WRITE);

    /* prepare an info file in case its needed */

    copy(info + I_TYPEOFF, hdr + F_FTYPE, 4);
    copy(info + I_AUTHOFF, hdr + F_CREATOR, 4);
    copy(info + I_FLAGOFF, hdr + F_FNDRFLAGS, 2);
#ifdef INITED_BUG
    info[INITED_OFF] &= INITED_MASK; /* reset init bit */
#endif
    copy(info + I_DLENOFF, hdr + F_DATALENGTH, 4);
    copy(info + I_RLENOFF, hdr + F_RSRCLENGTH, 4);
    copy(info + I_CTIMOFF, hdr + F_CREATIONDATE, 4);
    copy(info + I_MTIMOFF, hdr + F_MODDATE, 4);

    isfolder = f->compRMethod == startFolder && f->compDMethod == startFolder;
	
    /* list the file name if verbose or listonly mode, also if query mode */
    if (skip)			/* skip = 1 if skipping all in this folder */
	write_it = 0;
    else {
	write_it = 1;
	if (listonly || verbose || query) {
	    for (i = 0; i < depth; i++)
		putchar(' ');
	    if (isfolder)
		printf("Folder: \"%s\"", uname);
	    else
		printf("name=\"%s\", type=%4.4s, author=%4.4s, data=%ld, rsrc=%ld",
		       uname, hdr + F_FTYPE, hdr + F_CREATOR,
		       f->dataLength, f->rsrcLength);
	    if (query) {	/* if querying, check with the boss */
		printf(" ? ");
		fgets(temp, sizeof(temp) - 1, stdin);
		tp = temp;
		write_it = 0;
		while (*tp != '\0') {
		    if (*tp == 'y' || *tp == 'Y') {
			write_it = 1;
			break;
		    }
		    else
			tp++;
		}
	    }
	    else		/* otherwise, terminate the line */
		putchar('\n');
	}
    }
    return(write_it ? H_WRITE : H_SKIP);
}

check_access(fname)	/* return 0 if OK to write on file fname, -1 otherwise */
char *fname;
{
    char temp[10], *tp;

    if (access(fname, 0) == -1) {
	return(0);
    }
    else {
	printf("%s exists.  Overwrite? ", fname);
	fgets(temp, sizeof(temp) - 1, stdin);
	tp = temp;
	while (*tp != '\0') {
	    if (*tp == 'y' || *tp == 'Y') {
		return(0);
	    }
	    else
		tp++;
	}
    }
    return(-1);
}

unsigned short write_file(fname, ibytes, obytes, type)
char *fname;
unsigned long ibytes, obytes;
unsigned char type;
{
    unsigned short crc;
    int i, n, ch, lastch;
    FILE *outf;
    char temp[256];

    crc = INIT_CRC;
    chkcrc = 1;			/* usually can check the CRC */

    if (check_access(fname) == -1) {
	fseek(infp, ibytes, 1);
	chkcrc = 0;		/* inhibit crc check if file not written */
    	return(-1);
    }
	
    switch (type) {
      case noComp:		/* no compression */
	outf = fopen(fname, "w");
	if (outf == NULL) {
	    perror(fname);
	    exit(1);
	}
	while (ibytes > 0) {
	    n = (ibytes > IOBUFSIZ) ? IOBUFSIZ : ibytes;
	    n = fread(iobuf, 1, n, infp);
	    if (n == 0)
		break;
	    crc = updcrc(crc, iobuf, n);
	    outc(iobuf, n, outf);
	    ibytes -= n;
	}
	fclose(outf);
	break;
      case rleComp:		/* run length encoding */
	outf = fopen(fname, "w");
	if (outf == NULL) {
	    perror(fname);
	    exit(1);
	}
	while (ibytes > 0) {
	    ch = getc(infp) & 0xff;
	    ibytes--;
	    if (ch == 0x90) {	/* see if its the repeat marker */
		n = getc(infp) & 0xff; /* get the repeat count */
		ibytes--;
		if (n == 0) { /* 0x90 was really an 0x90 */
		    iobuf[0] = 0x90;
		    crc = updcrc(crc, iobuf, 1);
		    outc(iobuf, 1, outf);
		}
		else {
		    n--;
		    for (i = 0; i < n; i++)
			iobuf[i] = lastch;
		    crc = updcrc(crc, iobuf, n);
		    outc(iobuf, n, outf);
		}
	    }
	    else {
		iobuf[0] = ch;
		crc = updcrc(crc, iobuf, 1);
		lastch = ch;
		outc(iobuf, 1, outf);
	    }
	}
	fclose(outf);
	break;
      case lzwComp:		/* LZW compression */
	sprintf(temp, "%s%s", COMPRESS, " -d -c -n -b 14 ");
	if (txtmode) {
	    strcat(temp, "| tr \'\\015\' \'\\012\' ");
	    chkcrc = 0;		/* can't check CRC in this case */
	}
	strcat(temp, "> '");
	strcat(temp, fname);
	strcat(temp, "'");
	outf = popen(temp, "w");
	if (outf == NULL) {
	    perror(fname);
	    exit(1);
	}
	while (ibytes > 0) {
	    n = (ibytes > IOBUFSIZ) ? IOBUFSIZ : ibytes;
	    n = fread(iobuf, 1, n, infp);
	    if (n == 0)
		break;
	    fwrite(iobuf, 1, n, outf);
	    ibytes -= n;
	}
	pclose(outf);
	if (chkcrc) {
	    outf = fopen(fname, "r"); /* read the file to get CRC value */
	    if (outf == NULL) {
		perror(fname);
		exit(1);
	    }
	    while (1) {
		n = fread(iobuf, 1, IOBUFSIZ, outf);
		if (n == 0)
		    break;
		crc = updcrc(crc, iobuf, n);
	    }
	    fclose(outf);
	}
	break;
      case hufComp:		/* Huffman compression */
	outf = fopen(fname, "w");
	if (outf == NULL) {
	    perror(fname);
	    exit(1);
	}
	nodeptr = nodelist;
	bit = 0;		/* put us on a byte boundary */
	read_tree();
	while (obytes > 0) {
	    n = (obytes > IOBUFSIZ) ? IOBUFSIZ : obytes;
	    for (i = 0; i < n; i++)
		iobuf[i] = gethuffbyte(DECODE);
	    crc = updcrc(crc, iobuf, n);
	    outc(iobuf, n, outf);
	    obytes -= n;
	}
	fclose(outf);
	break;
      default:
	fprintf(stderr, "Unknown compression method\n");
	chkcrc = 0;		/* inhibit crc check if file not written */
	return(-1);
    }

    return(crc & 0xffff);
}

outc(p, n, fp)
char *p;
int n;
FILE *fp;
{
    register char *p1;
    register int i;
    if (txtmode) {
	for (i = 0, p1 = p; i < n; i++, p1++)
	    if ((*p1 & BYTEMASK) == '\r')
		*p1 = '\n';
    }
    fwrite(p, 1, n, fp);
}

long get4(bp)
char *bp;
{
    register int i;
    long value = 0;

    for (i = 0; i < 4; i++) {
	value <<= 8;
	value |= (*bp & BYTEMASK);
	bp++;
    }
    return(value);
}

short get2(bp)
char *bp;
{
    register int i;
    int value = 0;

    for (i = 0; i < 2; i++) {
	value <<= 8;
	value |= (*bp & BYTEMASK);
	bp++;
    }
    return(value);
}

copy(p1, p2, n)
char *p1, *p2;
int n;
{
	while (n-- > 0)
		*p1++ = *p2++;
}

/* This routine recursively reads the Huffman encoding table and builds
   and decoding tree. */

struct node *read_tree()
{
	struct node *np;
	np = nodeptr++;
	if (getbit() == 1) {
		np->flag = 1;
		np->byte = gethuffbyte(NODECODE);
	}
	else {
		np->flag = 0;
		np->zero = read_tree();
		np->one  = read_tree();
	}
	return(np);
}

/* This routine returns the next bit in the input stream (MSB first) */

getbit()
{
	static char b;
	if (bit == 0) {
		b = getc(infp) & 0xff;
		bit = 8;
	}
	bit--;
	return((b >> bit) & 1);
}

/* This routine returns the next 8 bits.  If decoding is on, it finds the
byte in the decoding tree based on the bits from the input stream.  If
decoding is not on, it either gets it directly from the input stream or
puts it together from 8 calls to getbit(), depending on whether or not we
are currently on a byte boundary
*/
gethuffbyte(decode)
int decode;
{
	register struct node *np;
	register int i, b;
	if (decode == DECODE) {
		np = nodelist;
		while (np->flag == 0)
			np = (getbit()) ? np->one : np->zero;
		b = np->byte;
	}
	else {
		if (bit == 0)	/* on byte boundary? */
			b = getc(infp) & 0xff;
		else {		/* no, put a byte together */
			b = 0;
			for (i = 8; i > 0; i--) {
				b = (b << 1) + getbit();
			}
		}
	}
	return(b);
}
