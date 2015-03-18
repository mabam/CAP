/* unmacbin - reverse of macbin - change a MacBinary file back in to
   the .info .data .rsrc style that xbin, macput and macget understand.
   Stole some from macbin. */

/* Written by John M. Sellens, jmsellens@watdragon.uwaterloo.ca,
   Math Faculty Computing Facility,
   University of Waterloo
   Waterloo, Ontario, Canada
   N2L 3G1	*/

/* capit - convert a macbin file into a CAP file on a CAP Unix Mac disc.
   Basically:
      file.data => file
      file.rsrc => .resource/file
      file.info => mangle it then .finderinfo/file
 
   Nigel Perry, Dept of Computing, Imperial College, London SW7 2BZ, England. July 90.
   np@doc.ic.ac.uk
 */

#ifdef sun
#ifdef __svr4__
#define SOLARIS
#endif __svr4__
#endif sun

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef SOLARIS
#include <dirent.h>
#include <string.h>
#include <netat/sysvcompat.h>
#else  SOLARIS
#include <sys/dir.h>
#include <strings.h>
#endif SOLARIS

#define BSIZE	(128)	/* size of blocks in MacBinary file */
typedef long	int4;


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

/* end aufs */

static info_header info;
static FileInfo fndr_info;

static union trans {
    int4	num;
    char	ch[4];
} trans;


main(argc,argv)
int argc;
char **argv;
{
    FILE *fp, *ofp;
    char bname[MAXNAMLEN];
    char iname[MAXNAMLEN];
    char dname[MAXNAMLEN];
    char rname[MAXNAMLEN];
    char buf[BSIZE];
    char * charp;
    int verbose = 0;
    int len;
    int arg;
    int err = 0;
    long dflen, rflen;
    char *ext, *disc;
    extern char *getenv();

    if((ext = getenv("MAC_EXT")) == NULL) ext = ".bin";
    if((disc = getenv("MAC_DISC")) == NULL) disc = ".";

    arg = 1;
    if (argc > 1 && strcmp(argv[1], "-v") == 0 ) {
	verbose = 1;
	++arg;
    }
    if ( arg >= argc ) {
	fprintf(stderr,"%s: Usage: %s [-v] filename(s)\n",argv[0],argv[0]);
	exit(1);
    }
    for ( ; arg < argc; arg++ ) {
	if ( (charp = rindex (argv[arg], '.')) != NULL
	    && strcmp(charp, ext) == 0 ) {
	    *charp = '\0';
	    strcpy(bname, argv[arg]);
	    *charp = '.';
	} else
	    strcpy(bname, argv[arg]);

	sprintf(iname, "%s/.finderinfo/%s", disc, bname);
	sprintf(dname, "%s/%s", disc, bname);
	sprintf(rname, "%s/.resource/%s", disc, bname);

	if (verbose)
	    printf( "Converting '%s'\n", argv[arg] );
	if ( (fp = fopen( argv[arg], "r" )) == NULL ) {
	    fprintf( stderr, "%s: couldn't open '%s' for reading",
		argv[0], argv[arg] );
	    perror( "" );
	    exit(++err);
	}
	if ( fread(&info, sizeof(info), 1, fp) <= 0 ) {
	    fprintf( stderr, "%s: couldn't read .info header from '%s'",
		argv[0], argv[arg] );
	    perror( "" );
	    exit(++err);
	}
	if ( info.zero1 || info.zero2 || info.version ) {
	    fprintf( stderr, "%s: '%s' is not in MacBinary format - skipped\n",
		argv[0], argv[arg] );
	    ++err;
	    continue;
	}

	/* make the .finderinfo file */
	bzero(&fndr_info, sizeof(FileInfo));
	bcopy(info.type, fndr_info.fndr_type, 4);
	bcopy(info.auth, fndr_info.fndr_creator, 4);
        if(info.protect == '\1' ) fndr_info.fndr_flags = 0x40; /* maybe? */
	fndr_info.fi_magic1 = FI_MAGIC1;
	fndr_info.fi_version = FI_VERSION;
	fndr_info.fi_magic = FI_MAGIC;
	fndr_info.fi_bitmap = FI_BM_MACINTOSHFILENAME;
	bcopy(info.name, fndr_info.fi_macfilename, info.nlen);

	/* write the .finderinfo file */
	if ( (ofp = fopen( iname, "w" )) == NULL ) {
	    fprintf( stderr, "%s: couldn't open '%s' for writing",
		argv[0], iname );
	    perror( "" );
	    exit(++err);
	}
	fwrite( &fndr_info, sizeof(FileInfo), 1, ofp );
	fclose( ofp );

	/* It appears that the .data and .rsrc parts of the MacBinary file
	   are padded to the nearest 128 (BSIZE) byte boundary, but they
	   should be trimmed to their proper size when we split them. */

	trans.ch[0] = info.dflen[0]; trans.ch[1] = info.dflen[1];
	trans.ch[2] = info.dflen[2]; trans.ch[3] = info.dflen[3];
	dflen = ntohl( trans.num );
	trans.ch[0] = info.rflen[0]; trans.ch[1] = info.rflen[1];
	trans.ch[2] = info.rflen[2]; trans.ch[3] = info.rflen[3];
	rflen = ntohl( trans.num );

	/* write the data fork */
	if ( (ofp = fopen( dname, "w" )) == NULL ) {
	    fprintf( stderr, "%s: couldn't open '%s' for writing",
		argv[0], dname );
	    perror( "" );
	    exit(++err);
	}
	for ( len=0; len<dflen;  ) {
	    if ( fread( buf, sizeof(char), BSIZE, fp ) != BSIZE ) {
		fprintf( stderr, "%s: couldn't read %d bytes from '%s'",
		    argv[0], BSIZE, argv[arg] );
		fprintf(stderr,"got %d of %d'n",len,dflen);
		perror( "" );
		exit(++err);
	    }
	    len += BSIZE;
	    if ( len > dflen )
		fwrite( buf, sizeof(char), BSIZE-len+dflen, ofp );
	    else
		fwrite( buf, sizeof(char), BSIZE, ofp );
	}
	fclose( ofp );
	    
	/* write the .resource file */
	if ( (ofp = fopen( rname, "w" )) == NULL ) {
	    fprintf( stderr, "%s: couldn't open '%s' for writing",
		argv[0], rname );
	    perror( "" );
	    exit(++err);
	}
	for ( len=0; len<rflen;  ) {
	    if ( fread( buf, sizeof(char), BSIZE, fp ) != BSIZE ) {
		fprintf( stderr, "%s: couldn't read %d bytes from '%s'",
		    argv[0], BSIZE, argv[arg] );
		fprintf(stderr,"got %d of %d'n",len,rflen);
		perror( "" );
		exit(++err);
	    }
	    len += BSIZE;
	    if ( len > rflen )
		fwrite( buf, sizeof(char), BSIZE-len+rflen, ofp );
	    else
		fwrite( buf, sizeof(char), BSIZE, ofp );
	}
	fclose( ofp );
	fclose( fp );
    }
    exit( err );
}
