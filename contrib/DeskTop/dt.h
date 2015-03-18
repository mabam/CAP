/*
 * (re)build/dump CAP desktop files .IDeskTop and .ADeskTop
 *
 * Copyright (c) 1993-1997, The University of Melbourne.
 * All Rights Reserved. Permission to publicly redistribute this
 * package (other than as a component of CAP) or to use any part
 * of this software for any purpose, other than that intended by
 * the original distribution, *must* be obtained in writing from
 * the copyright owner.
 *
 * djh@munnari.OZ.AU
 * 15 February 1993
 * 30 November 1993
 * 10 August   1997
 *
 * Refer: "Inside Macintosh", Volume 1, page I-128 "Format of a Resource File"
 *
 * $Author: djh $
 * $Revision: 2.2 $
 *
 */

#ifdef sun
#ifdef __svr4__
#define SOLARIS
#endif __svr4__
#endif sun

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fcntl.h>

#ifdef SOLARIS
#include <dirent.h>
#include <netat/sysvcompat.h>
#else  SOLARIS
#include <sys/dir.h>
#endif SOLARIS

#if defined(hpux) || defined(SOLARIS)
# include <netinet/in.h>
#endif /* hpux || SOLARIS */

/*
 * header for .ADeskTop
 *
 */

struct adt {
  int magic;
  u_char creat[4];
  u_char userb[4];
  int dlen;
  int flen;
  /* names follow */
};

/*
 * header for .IDeskTop
 *
 */

struct idt {
  int magic;
  int isize;
  u_char creat[4];
  u_char ftype[4];
  u_char itype;
  u_char pad1;
  u_char userb[4];
  u_char pad2[2];
  /* bitmap follows */
};

/*
 * headers for .finderinfo files
 *
 */

struct fi {
  u_char ftype[4];
  u_char creat[4];
  u_short flags;
  u_short locn[2];
  u_short win;
  u_short iconID;
  u_short pad[4];
  u_short commID;
  u_int dirID;
};

struct finfo {
  struct fi fi;
  u_short attr;
#define FMAGIC1	255
  u_char magic1;
#define FVERS	0x10
  u_char version;
#define FMAGIC2	0xda
  u_char magic2;
  u_char pad1;
  /* ignore rest */
};

/*
 * headers for .resource files
 *
 */

/* resource hdr */
struct rhdr {
  int rdataOffset;
  int rmapOffset;
  int rdataLength;
  int rmapLength;
  u_char filler[240];
};

/* resource map */
struct rmap {
  u_char filler[24];
  short listOffset;
  short nameOffset;
};

/* resource type list */
struct tlist {
  u_char rtype[4];
  short rnum;
  short roff;
};

/* resource reference list */
struct rlist {
  short rsrcID;
  short nameOffset;
  u_int attrOff;
  u_int handle;
};

/* bundle header */
struct bhdr {
  u_char creat[4];
  u_short version;
  short numType;
};

/* bundle data */
struct bdata {
  u_char btype[4];
  short numID;
};

/* bundle IDs */
struct bids {
  short localID;
  short actualID;
};

/* FREF header */
struct fdata {
  u_char ftype[4];
  short localID;
  /* ignore filename */
};

/* icon family */
struct icn {
  char ityp[6];
  short iconnum;
  int iconoffset;
};

/* defines */

#define MAGIC	0x00010002
#define IFILE	".IDeskTop"
#define AFILE	".ADeskTop"
#define RSRCF	".resource"
#define FNDRI	".finderinfo"
