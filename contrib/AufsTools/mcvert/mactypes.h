#ifdef sun
#ifdef __svr4__
#define SOLARIS
#endif __svr4__
#endif sun

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#ifdef SOLARIS
#include <time.h>
#include <dirent.h>
#include <netat/sysvcompat.h>
#else  SOLARIS
#include <sys/dir.h>
#include <sys/timeb.h>
#endif SOLARIS

/* Useful, though not particularly Mac related, values */
typedef unsigned char byte;     /* one byte, obviously */
typedef unsigned short word;    /* must be 2 bytes */
#ifndef SOLARIS
typedef unsigned long ulong;    /* 4 bytes */
#endif  SOLARIS
#define TRUE  1
#define FALSE 0
#define CR 0x0d
#define LF 0x0a

/* Compatibility issues */
#ifdef BSD
#define mac2word (word) ntohs
#define mac2long (ulong) ntohl
#define word2mac (word) htons
#define long2mac (ulong) htonl
#else
#define mac2word
#define mac2long
#define word2mac
#define long2mac
#endif

#ifdef MAXNAMLEN/* 4.2 BSD, stdio.h */
#define SYSNAMELEN MAXNAMLEN
#else
#define SYSNAMELEN DIRSIZ
#endif

#define NAMELEN 63              /* maximum legal Mac file name length */
#define BINNAMELEN 68           /* NAMELEN + len(".bin\0") */

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
            byte dlen[4];           /* Data Fork length (bytes) -   most sig.  */
            byte rlen[4];           /* Resource Fork length         byte first */
            byte ctim[4];           /* File's creation date. */
            byte mtim[4];           /* File's "last modified" date. */
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

/* The *.info file of a MacTerminal file transfer either has exactly this
structure or has the protect bit in bit 6 (near the sign bit) of byte zero1.
The code I have for macbin suggests the difference, but I'm not so sure */

/* Format of a hqx file:
It begins with a line that begins "(This file
and the rest is 64 character lines (except possibly the last, and not
including newlines) where the first begins and the last ends with a colon.
The characters between colons should be only from the set in tr86, below,
each of which corresponds to 6 bits of data.  Once that is translated to
8 bit bytes, you have the real data, except that the byte 0x90 may 
indicate, if the following character is nonzero, that the previous
byte is to be repeated 1 to 255 times.  The byte 0x90 is represented by
0x9000.  The information in the file is the hqx_buf (see below),
a CRC word, the data fork, a CRC word, the resource fork, and a CRC word.
There is considerable confusion about the flags.  An official looking document
unclearly states that the init bit is always clear, as is the following byte.
The experience of others suggests, however, that this is not the case.
*/

#define HQXLINELEN 64
typedef struct {
            byte version;           /* there is only a version 0 at this time */
            byte type[4];           /* File type. */
            byte auth[4];           /* File creator. */
            byte flags;             /* file flags: LkIvBnSyBzByChIt */
            byte protect;           /* ?Pr??????, don't know what ? bits mean */
            byte dlen[4];           /* Data Fork length (bytes) -   most sig.  */
            byte rlen[4];           /* Resource Fork length         byte first */
            byte bugblank;             /* to fix obscure sun 3/60 problem
                                          that always makes sizeof(hqx_header
                                          even */
            } hqx_header;
typedef struct {     /* hqx file header buffer (includes file name) */
            byte nlen;              /* Length of filename. */
            byte name[NAMELEN];     /* Filename: only nlen actually appear */
            hqx_header all_the_rest;/* and all the rest follows immediately */
            } hqx_buf;

/* Format of a Packit file:
Repeat the following sequence for each file in the Packit file:
    4 byte identifier ("PMag" = not compressed, "Pma4" = compressed)
    320 byte compression data (if compressed file)
        = preorder transversal of Huffman tree
        255 0 bits corresponding to nonleaf nodes
        256 1 bits corresponding to leaf nodes
        256 bytes associating leaf nodes with bytes
        1   completely wasted bit
    92 byte header (see pit_header below) *
    2 bytes CRC word for header *
    data fork (length from header) *
    resource fork (length from header) *
    2 bytes CRC word for forks *

Last file is followed by the 4 byte Ascii string, "Pend", and then the EOF.
The CRC calculations differ from those in the binhex format.

* these are in compressed form if compression is on for the file

*/

typedef struct {     /* Packit file header (92 bytes) */
            byte nlen;              /* Length of filename. */
            byte name[NAMELEN];     /* Filename (only 1st nlen are significant)*/
            byte type[4];           /* File type. */
            byte auth[4];           /* File creator. */
            byte flags;             /* file flags: LkIvBnSyBzByChIt */
            byte zero1;
            byte protect;           /* = 1 for protected file, 0 otherwise */
            byte zero2;
            byte dlen[4];           /* Data Fork length (bytes) -   most sig.  */
            byte rlen[4];           /* Resource Fork length         byte first */
            byte ctim[4];           /* File's creation date. */
            byte mtim[4];           /* File's "last modified" date. */
            } pit_header;

/* types for constructing the Huffman tree */
typedef struct branch_st {
            byte flag;
            struct branch_st *one, *zero;
            } branch;

typedef struct leaf_st {
            byte flag;
            byte data;
            } leaf;
