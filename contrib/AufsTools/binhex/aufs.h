/********************************************************************************/
/* added for aufs, nicked from various places... */

#ifdef sun
#ifdef __svr4__
#define SOLARIS
#endif __svr4__
#endif sun

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
} FinderInfo;

/* end aufs */
/********************************************************************************/
