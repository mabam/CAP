/*
 * sit - Stuffit for UNIX
 *  Puts unix data files into stuffit archive suitable for downloading
 *	to a Mac.  Automatically processes files output from xbin.
 *
 *  Reverse engineered from unsit by Allan G. Weber, which was based on
 *  macput, which was based on ...
 *  Just like unsit this uses the host's version of compress to do the work.
 *
 * Examples:
 *   1) take collection of UNIX text files and make them LSC text files 
 *	when uncompressed on the mac:
 *	   sit -u -T TEXT -C KAHL file ...
 *   2) Process output from xbin:
 *	   xbin file1	 (produces FileOne.{info,rsrc,data})
 *	   sit file1
 *
 *  Tom Bereiter
 *	..!{rutgers,ames}!cs.utexas.edu!halley!rolex!twb
 *
 * This version for CAP aufs files based on info from aufs source + mcvert etc.
 * Aufs version is program is called AUFSNAME (default stuffit)
 *
 * Aug 90. Nigel Perry, np@doc.ic.ac.uk
 *
 */

#define BSD

#ifdef sun
#ifdef __svr4__
#define SOLARIS
#undef BSD
#endif __svr4__
#endif sun

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "sit.h"
#ifdef BSD
#include <sys/time.h>
#include <sys/timeb.h>
#else  BSD
#include <time.h>
extern long timezone;
#endif BSD

#ifdef SOLARIS
#include <netat/sysvcompat.h>
#endif SOLARIS

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/* Mac time of 00:00:00 GMT, Jan 1, 1970 */
#define TIMEDIFF 0x7c25b080

/* if called by this name, program will work on aufs files */
#define AUFSNAME "stuffit"

struct sitHdr sh;
struct fileHdr fh;

char buf[BUFSIZ];
char *defoutfile = "archive.sit";
int ofd;
ushort crc;
int clen;
int rmfiles;
int	unixf;
char *Creator, *Type;
int aufs;

usage() { fprintf(stderr,"Usage: sit file\n"); }
extern char *optarg;
extern int optind;

/********************************************************************************/
/* added for aufs, nicked from various places... */

/* following from mcvert program */

/* Useful, though not particularly Mac related, values */
typedef unsigned char byte;     /* one byte, obviously */
typedef unsigned short word;    /* must be 2 bytes */
#ifndef SOLARIS
typedef unsigned long ulong;    /* 4 bytes */
#endif  SOLARIS

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

main(argc,argv) char **argv; {
	int i,n;
	int total, nfiles;
	int c;
	char *s, *progname;

    rmfiles = unixf = 0;
    progname = argv[0];
    if ((s = (char *) rindex(progname, '/')) != NULL)
	progname = ++s;
    aufs = strcmp(progname, AUFSNAME) == 0;

    while ((c=getopt(argc, argv, "ro:uC:T:")) != EOF)
	switch (c) {
		case 'r':
			rmfiles++;	/* remove files when done */
			break;
		case 'o':		/* specify output file */
			defoutfile = optarg;
			break;
		case 'u':		/* unix file -- change '\n' to '\r' */
			unixf++;
			break;
		case 'C':		/* set Mac creator */
			Creator = optarg;
			break;
		case 'T':		/* set Mac file type */
			Type = optarg;
			break;
		case '?':
			usage();
			exit(1);
	}

	if(aufs && (strlen(defoutfile) > 32))
	{   fprintf(stderr, "Output name must not exceed 32 characters: %s\n", defoutfile);
	    exit(-1);
	}

	if(aufs)
	{    /* make the .finderinfo file */
	    char buf[32+12+1];

	    strcpy(buf, ".finderinfo/");
	    strcat(buf, defoutfile);
	    if ((ofd=creat(buf,0644))<0)
	    {   perror(buf);
		    exit(1);
	    }
	    bzero(&fndr_info, sizeof(FileInfo));
	    bcopy("SIT!", fndr_info.fndr_type, 4);
	    bcopy("SIT!", fndr_info.fndr_creator, 4);
	    fndr_info.fi_magic1 = FI_MAGIC1;
	    fndr_info.fi_version = FI_VERSION;
	    fndr_info.fi_magic = FI_MAGIC;
	    fndr_info.fi_bitmap = FI_BM_MACINTOSHFILENAME;
	    strcpy(fndr_info.fi_macfilename, defoutfile);
	    write(ofd, &fndr_info, sizeof(FileInfo));
	    close(ofd);
	}

	if ((ofd=creat(defoutfile,0644))<0) {
		perror(defoutfile);
		exit(1);
	}
	/* empty header, will seek back and fill in later */
	write(ofd,&sh,sizeof sh);

	for (i=optind; i<argc; i++) {
		n = put_file(argv[i]);
		if (n) {
			total += n;
			nfiles++;
		}
	}
	lseek(ofd,0,0);

	total += sizeof(sh);
	/* header header */
    strncpy(sh.sig1,"SIT!",4);
    cp2(nfiles,sh.numFiles);
    cp4(total,sh.arcLen);
    strncpy(sh.sig2,"rLau",4);
    sh.version = 1;

	write(ofd,&sh,sizeof sh);
}

put_file(name)
char name[];
{
	struct stat st;
	struct infohdr ih;
	int i,n,fd;
	long fpos1, fpos2;
	char nbuf[256], *p;
	int fork=0;
	long tdiff;
	struct tm *tp;
#ifdef BSD
	struct timeb tbuf;
#else
	long bs;
#endif

	fpos1 = lseek(ofd,0,1); /* remember where we are */
	/* write empty header, will seek back and fill in later */
	bzero(&fh,sizeof fh);
	write(ofd,&fh,sizeof fh);

	/* look for resource fork */
	if(aufs)
	{   strcpy(nbuf, ".resource/");
	    strcat(nbuf, name);
	}
	else
	{   strcpy(nbuf,name);
	    strcat(nbuf,".rsrc");
	}
	if (stat(nbuf,&st)>=0 && st.st_size) {	/* resource fork exists */
		dofork(nbuf);
		cp4(st.st_size,fh.rLen);
		cp4(clen,fh.cRLen);
		cp2(crc,fh.rsrcCRC);
		fh.compRMethod = lpzComp;
		fork++;
	}
	if (rmfiles) unlink(nbuf);	/* ignore errors */

	/* look for data fork */
	st.st_size = 0;
	strcpy(nbuf,name);
	if (stat(nbuf,&st)<0) {		/* first try plain name */
		strcat(nbuf,".data");
		stat(nbuf,&st);
	}
	if (st.st_size) {		/* data fork exists */
		dofork(nbuf);
		cp4(st.st_size,fh.dLen);
		cp4(clen,fh.cDLen);
		cp2(crc,fh.dataCRC);
		fh.compDMethod = lpzComp;
		fork++;
	}
	if (fork == 0) {
		fprintf(stderr,"%s: no data or resource files\n",name);
		return 0;
	}
	if (rmfiles) unlink(nbuf);	/* ignore errors */

	/* look for .info file */
	if(aufs)
	{   strcpy(nbuf, ".finderinfo/");
	    strcat(nbuf, name);
	}
	else
	{   strcpy(nbuf,name);
	    strcat(nbuf,".info");
	}
	if((fd=open(nbuf,0))>=0
	   && ((!aufs && read(fd,&ih,sizeof(ih))==sizeof(ih))
	       || (aufs && read(fd,&fndr_info,sizeof(FileInfo))==sizeof(FileInfo))
	      )
	  )
	{   if(aufs)
	    {   char *np;
		
		np = (char *)(fndr_info.fi_bitmap & FI_BM_MACINTOSHFILENAME ? fndr_info.fi_macfilename
									    : fndr_info.fi_shortfilename);
		fh.fName[0] = (char)strlen(np);
		strncpy(fh.fName+1, np, 64);
		bcopy(fndr_info.fndr_type, fh.fType, 4);
		bcopy(fndr_info.fndr_creator, fh.fCreator, 4);
		bcopy(&fndr_info.fndr_flags, fh.FndrFlags, 2);
#ifdef BSD
		ftime(&tbuf);
		tp = localtime(&tbuf.time);
		tdiff = TIMEDIFF - tbuf.timezone * 60;
		if (tp->tm_isdst)
			tdiff += 60 * 60;
#else
		/* I hope this is right! -andy */
		time(&bs);
		tp = localtime(&bs);
		tdiff = TIMEDIFF - timezone;
		if (tp->tm_isdst)
			tdiff += 60 * 60;
#endif
		cp4(st.st_ctime + tdiff, fh.cDate);
		cp4(st.st_mtime + tdiff, fh.mDate);
	    }
	    else
	    {   strncpy(fh.fName, ih.name,64);
		strncpy(fh.fType, ih.type, 4);
		strncpy(fh.fCreator, ih.creator, 4);
		strncpy(fh.FndrFlags, ih.flag, 2);
		strncpy(fh.cDate, ih.ctime, 4);
		strncpy(fh.mDate, ih.mtime, 4);
	    }
	}
	else {	/* no info file so fake it */
		strncpy(&fh.fName[1], name,63); fh.fName[0] = min(strlen(name),63);
		/* default to LSC text file */
		strncpy(fh.fType, Type ? Type : "TEXT", 4);
		strncpy(fh.fCreator, Creator ? Creator : "KAHL", 4);
		/* convert unix file time to mac time format */
#ifdef BSD
		ftime(&tbuf);
		tp = localtime(&tbuf.time);
		tdiff = TIMEDIFF - tbuf.timezone * 60;
		if (tp->tm_isdst)
			tdiff += 60 * 60;
#else
		/* I hope this is right! -andy */
		time(&bs);
		tp = localtime(&bs);
		tdiff = TIMEDIFF - timezone;
		if (tp->tm_isdst)
			tdiff += 60 * 60;
#endif
		cp4(st.st_ctime + tdiff, fh.cDate);
		cp4(st.st_mtime + tdiff, fh.mDate);
	}
	close(fd);
	if (rmfiles) unlink(nbuf);	/* ignore errors */

	crc = updcrc(0,&fh,(sizeof fh)-2);
	cp2(crc, fh.hdrCRC);

	fpos2 = lseek(ofd,0,1);		/* remember where we are */
	lseek(ofd,fpos1,0);				/* seek back over file(s) and header */
	write(ofd,&fh,sizeof fh);		/* write back header */
	fpos2=lseek(ofd,fpos2,0);				/* seek forward file */

	return (fpos2 - fpos1);
}
	
dofork(name)
char name[];
{
	FILE *fs;
	int n, fd, ufd;
	char *p;

	if ((fd=open(name,0))<0) {
		perror(name);
		return 0;
	}   
	if (unixf)		/* build conversion file */
		if ((ufd=creat("sit+temp",0644))<0) {
			perror("sit+temp");
			return 0;
		}   
	/* do crc of file: */
	crc = 0;
	while ((n=read(fd,buf,BUFSIZ))>0) {
		if (unixf) {	/* convert '\n' to '\r' */
			for (p=buf; p<&buf[n]; p++)
				if (*p == '\n') *p = '\r';
			write(ufd,buf,n);
		}
		crc = updcrc(crc,buf,n);
	}
	close(fd);
	/*
	 * open pipe to compress file
	 *   If a unix file ('\n' -> '\r' conversion) 'sit+temp' will be a new copy
	 *   with the conversion done.	Otherwise, 'sit+temp' is just a link to 
	 *   the input file.
	 */
	if (unixf)
		close(ufd);
	else link(name,"sit+temp");
	fs = popen("compress -c -n -b 14 sit+temp","r");
	if (fs == NULL) {
		perror(name);
		return 0;
	}
	/* write out compressed file */
	clen = 0;
	while ((n=fread(buf,1,BUFSIZ,fs))>0) {
		write(ofd,buf,n);
		clen += n;
	}
	pclose(fs);
	unlink("sit+temp");
}

cp2(x,dest)
unsigned short x;
char dest[];
{
	dest[0] = x>>8;
	dest[1] = x;
}

cp4(x,dest)
unsigned long x;
char dest[];
{
	dest[0] = x>>24;
	dest[1] = x>>16;
	dest[2] = x>>8;
	dest[3] = x;
}
