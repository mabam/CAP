/*
 * $Author: djh $ $Date: 1993/08/04 15:23:33 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpudb.c,v 2.3 1993/08/04 15:23:33 djh Rel djh $
 * $Revision: 2.3 $
*/

/*
 * afpudb.c - Appletalk Filing Protocol Unix Finder Information
 *  database.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  August 1987     CCKim	Created.
 *
 */

/* PATCH: XENIX:file.3, djh@munnari.OZ.AU, 20/11/90 */

#include <stdio.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include <netat/appletalk.h>
#include "afpudb.h"
#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
#endif

#ifdef bsd
# define BSDSYSTEM
#endif

#ifdef bsd4_2
# define BSDSYSTEM
#endif

/*
 * Deal with unix file types
 *
*/

/* This is the "file" icon - generic for unix files */
private byte unix_text[] = {
  0x0f,0xff,0xfc,0x00,		/*     XXXXXXXXXXXXXXXXXX           */
  0x08,0x00,0x06,0x00,		/*     X                XX          */
  0x08,0x00,0x05,0x00,		/*     X                X X         */
  0x08,0x00,0x04,0x80,		/*     X                X  X        */
  0x08,0x00,0x04,0x40,		/*     X                X   X       */
  0x08,0x0c,0x04,0x20,		/*     X       XX       X    X      */
  0x09,0xf8,0x87,0xf0,		/*     X  XXXXXX   X    XXXXXXX     */
  0x0b,0xf9,0xc0,0x10,		/*     X XXXXXXX  XXX         X     */
  0x08,0x9b,0xe0,0x10,		/*     X   X  XX XXXXX        X     */
  0x09,0x29,0x84,0x50,		/*     X  X  X X  XX    X   X X     */
  0x0b,0x29,0x84,0x50,		/*     X XX  X X  XX    X   X X     */
  0x0b,0x29,0x85,0x50,		/*     X XX  X X  XX    X X X X     */
  0x0b,0x69,0x85,0x50,		/*     X XX XX X  XX    X X X X     */
  0x0b,0x69,0x85,0x50,		/*     X XX XX X  XX    X X X X     */
  0x0b,0x49,0x85,0x50,		/*     X XX X  X  XX    X X X X     */
  0x0b,0x89,0x85,0x50,		/*     X XXX   X  XX    X X X X     */
  0x09,0x8b,0xa5,0x10,		/*     X  XX   X XXX X  X X   X     */
  0x09,0xfe,0xc5,0x50,		/*     X  XXXXXXXX XX   X X X X     */
  0x08,0x7c,0xc0,0x10,		/*     X    XXXXX  XX         X     */
  0x08,0x00,0x00,0x10,		/*     X                      X     */
  0x08,0x40,0x44,0x50,		/*     X    X       X   X   X X     */
  0x09,0x54,0x45,0x50,		/*     X  X X X X   X   X X X X     */
  0x09,0x55,0x55,0x50,		/*     X  X X X X X X X X X X X     */
  0x09,0x51,0x55,0x50,		/*     X  X X X   X X X X X X X     */
  0x09,0x55,0x55,0x50,		/*     X  X X X X X X X X X X X     */
  0x09,0x55,0x55,0x50,		/*     X  X X X X X X X X X X X     */
  0x09,0x15,0x14,0x50,		/*     X  X   X X X   X X   X X     */
  0x09,0x55,0x54,0x10,		/*     X  X X X X X X X X     X     */
  0x09,0x41,0x50,0x10,		/*     X  X X     X X X       X     */
  0x08,0x00,0x00,0x10,		/*     X                      X     */
  0x08,0x00,0x00,0x10,		/*     X                      X     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  /* mask */
  0x0f,0xff,0xfc,0x00,		/*     XXXXXXXXXXXXXXXXXX           */
  0x0f,0xff,0xfe,0x00,		/*     XXXXXXXXXXXXXXXXXXX          */
  0x0f,0xff,0xff,0x00,		/*     XXXXXXXXXXXXXXXXXXXX         */
  0x0f,0xff,0xff,0x80,		/*     XXXXXXXXXXXXXXXXXXXXX        */
  0x0f,0xff,0xff,0xc0,		/*     XXXXXXXXXXXXXXXXXXXXXX       */
  0x0f,0xff,0xff,0xe0,		/*     XXXXXXXXXXXXXXXXXXXXXXX      */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0		/*     XXXXXXXXXXXXXXXXXXXXXXXX     */
};

#ifdef SMART_UNIX_FINDERINFO
private byte unix_framemaker[] = {
  0x0F,0xFF,0xFE,0x00,          /*     XXXXXXXXXXXXXXXXXXX          */
  0x08,0x00,0x03,0x00,          /*     X                 XX         */
  0x0B,0xFF,0xFE,0x80,          /*     X XXXXXXXXXXXXXXXXX X        */
  0x0B,0xFF,0xFE,0x40,          /*     X XXXXXXXXXXXXXXXXX  X       */
  0x0B,0xFF,0xFE,0x20,          /*     X XXXXXXXXXXXXXXXXX   X      */
  0x08,0x00,0x02,0x10,          /*     X                 X    X     */
  0x08,0x40,0x03,0xFA,          /*     X    X            XXXXXXX X  */
  0x38,0xA0,0x00,0x08,          /*   XXX   X X                 X    */
  0x09,0x53,0xF7,0x8F,          /*     X  X X X  XXXXXXXXX X   XXXX */
  0xF9,0xB0,0x00,0x0B,          /* XXXXX  XX XX                X XX */
  0x19,0x53,0x7F,0x4B,          /*    XX  X X X  XX XXXXXXX X  X XX */
  0x18,0xA0,0x00,0x08,          /*    XX   X X                 X    */
  0x18,0x40,0x00,0x0E,          /*    XX    X                  XXX  */
  0x88,0x07,0xDF,0x48,          /* X   X        XXXXX XXXXX X  X    */
  0x28,0x00,0x00,0x0B,          /*   X X                       X XX */
  0x09,0xBE,0xDF,0xCC,          /*     X  XX XXXXX XX XXXXXXX  XX   */
  0xA8,0x00,0x00,0x0E,          /* X X X                       XXX  */
  0x49,0xEE,0xFB,0xC8,          /*  X  X  XXXX XXX XXXXX XXXX  X    */
  0x08,0x00,0x00,0x08,          /*     X                       X    */
  0x09,0xDF,0x3D,0xC8,          /*     X  XXX XXXXX  XXXX XXX  X    */
  0x08,0x00,0x00,0x08,          /*     X                       X    */
  0x08,0x00,0x03,0xC8,          /*     X                 XXXX  X    */
  0x09,0xFA,0xEB,0xC8,          /*     X  XXXXXX X XXX X XXXX  X    */
  0x08,0x00,0x03,0xC9,          /*     X                 XXXX  X  X */
  0x09,0xDF,0x5B,0xCD,          /*     X  XXX XXXXX X XX XXXX  XX X */
  0x48,0x00,0x00,0x0B,          /*  X  X                       X XX */
  0x08,0x00,0x00,0x08,          /*     X                       X    */
  0x7B,0xFF,0xFF,0xEB,          /*  XXXX XXXXXXXXXXXXXXXXXXXXX X XX */
  0x0B,0xFF,0xFF,0xE8,          /*     X XXXXXXXXXXXXXXXXXXXXX X    */
  0x7B,0xFF,0xFF,0xEE,          /*  XXXX XXXXXXXXXXXXXXXXXXXXX XXX  */
  0xC8,0x00,0x00,0x08,          /* XX  X                       X    */
  0x0F,0xFF,0xFF,0xF8,          /*     XXXXXXXXXXXXXXXXXXXXXXXXX    */
  /* mask */
  0x0f,0xff,0xfc,0x00,          /*     XXXXXXXXXXXXXXXXXX           */
  0x0f,0xff,0xfe,0x00,          /*     XXXXXXXXXXXXXXXXXXX          */
  0x0f,0xff,0xff,0x00,          /*     XXXXXXXXXXXXXXXXXXXX         */
  0x0f,0xff,0xff,0x80,          /*     XXXXXXXXXXXXXXXXXXXXX        */
  0x0f,0xff,0xff,0xc0,          /*     XXXXXXXXXXXXXXXXXXXXXX       */
  0x0f,0xff,0xff,0xe0,          /*     XXXXXXXXXXXXXXXXXXXXXXX      */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0,          /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
  0x0f,0xff,0xff,0xf0           /*     XXXXXXXXXXXXXXXXXXXXXXXX     */
};

/* other icons */
#endif SMART_UNIX_FINDERINFO

#define FNDR_NOFNDR 0
#define FNDR_DEF 1
#define FNDR_DEVICE 2
#define FNDR_SOCKET 3
#ifdef SMART_UNIX_FINDERINFO
#define FNDR_BIN 4
#define FNDR_UPGM 5
#define FNDR_UOBJ 6
#define FNDR_ARCHIVE 7
#define FNDR_CPIO 8
#define FNDR_LOCKED 9
#define FNDR_FRAME 10
#endif SMART_UNIX_FINDERINFO

#define deffinfo(C1,C2,C3,C4,c1,c2,c3,c4,com,icon) {{(C1),(C2),(C3),(C4)}, \
                                          {(c1),(c2),(c3),(c4)}, \
					  (com), sizeof(com)-1, \
					  (icon), sizeof((icon))}

struct ufinderdb uf[] = {
  {{0, 0, 0, 0}, {0, 0, 0, 0}, "", 0, NULL, 0},
  deffinfo('u','n','i','x','T','E','X','T',
    "This is a Unix\252 created file", unix_text),
  deffinfo('u','n','i','x','D','E','V',' ',
    "This is a Unix\252 device", NULL),
  deffinfo('u','n','i','x','S','K','T',' ',
    "This is a Unix\252 socket", NULL),
#ifdef SMART_UNIX_FINDERINFO
  deffinfo('u','n','i','x','D','A','T','A',
    "This is a Unix\252 binary file", NULL),
  deffinfo('u','n','i','x','P','G','R','M',
    "This is a Unix\252 program", NULL),
  deffinfo('u','n','i','x','O','B','J',' ',
    "This is a Unix\252 object file", NULL),
  deffinfo('u','n','i','x','A','R',' ',' ',
    "This is a Unix\252 archive file", NULL),
  deffinfo('u','n','i','x','C','P','I','O',
    "This is a Unix\252 cpio file", NULL),
  deffinfo('u','n','i','x','L','C','K','D',
    "This file is not readable", NULL),
  deffinfo('F','r','a','m','F','A','S','L',
    "This is a Unix\252 FrameMaker file", unix_framemaker)
#endif SMART_UNIX_FINDERINFO
};

export int uf_len = sizeof(uf)/sizeof(struct ufinderdb);

#ifdef USR_FILE_TYPES
struct uft uft[NUMUFT];
#endif USR_FILE_TYPES

int
os_getunixtype(path, stb)
char *path;
struct stat *stb;
{
#ifdef SMART_UNIX_FINDERINFO
  char buf[BUFSIZ];
  int fd, k, l;
#endif SMART_UNIX_FINDERINFO
  int i;

  if ((stb->st_mode & S_IFMT) == S_IFDIR) /* a directory? */
    return(FNDR_NOFNDR);

  switch (stb->st_mode & S_IFMT) {
  case S_IFCHR:
  case S_IFBLK:
    /* super wanky thing to do would be to return an type */
    /* based upon the device type */
    return(FNDR_DEVICE);
    break;
#ifdef S_IFSOCK
  case S_IFSOCK:
    return(FNDR_SOCKET);
    break;
#endif S_IFSOCK
  }

#ifdef SMART_UNIX_FINDERINFO
  if ((fd=open(path,O_RDONLY)) < 0)
    return(FNDR_DEF);
  if ((i = read(fd, buf, BUFSIZ)) <= 0) {
    (void)close(fd);		/* ignore error here */
    return(FNDR_DEF);
  }
  (void)close(fd);		/* ignore error here */
  switch (*(int *)buf) {
#ifdef BSDSYSTEM
  case 0413:
  case 0410:
  case 0411:
  case 0407:
    return(FNDR_UPGM);
  case 0177555:
  case 0177545:
    return(FNDR_ARCHIVE);
#endif BSDSYSTEM
  case 070707:
    return(FNDR_CPIO);
  }
  if (strncmp(buf, "<MakerFile", sizeof("<MakerFile")-1) == 0)
    return(FNDR_FRAME);
#ifdef BSDSYSTEM
  if (strncmp(buf, "!<arch>\n", sizeof("!<arch>\n")-1) == 0)
    return(FNDR_ARCHIVE);
#endif BSDSYSTEM
#ifndef BIN_FUZZ
# define BIN_FUZZ BUFSIZ/3
#endif  BIN_FUZZ
  for (k = 0, l = 0; k < i; k++)
    if (buf[k] & 0x80)
      l++;
  if (l > BIN_FUZZ)
    return(FNDR_BIN);
#endif SMART_UNIX_FINDERINFO
#ifdef USR_FILE_TYPES
  if ((i = uft_match(path)) >= 0)
    return(i+FNDR_UFT);
#endif USR_FILE_TYPES
  return(FNDR_DEF);
}

int
os_issmartunixfi()
{
#ifdef SMART_UNIX_FINDERINFO
  return(1);
#else
  return(0);
#endif
}

#ifdef USR_FILE_TYPES
/*
 * initialize the UFT structure
 *
 */

int
uft_init()
{
  uft[0].uft_suffixlen = -1;
}

/*
 * check the file name against the suffix list
 *
 */

int
uft_match(file)
char *file;
{
  int i = 0;

  while (uft[i].uft_suffixlen >= 0) {
    if (suffix_match(file, &uft[i]))
      return(i);
    i++;
  }
  return(-1);
}

int
suffix_match(file, uft)
char *file;
struct uft *uft;
{
  int i;

  if (uft->uft_suffixlen == 0) /* wildcard */
    return(1);

  if ((i = strlen(file)) >= uft->uft_suffixlen
   && strcmp(uft->uft_suffix, file+i-uft->uft_suffixlen) == 0)
    return(1);

  return(0);
}

/*
 * check the file creator and type against the list
 *
 */

int
uft_match_finfo(creat, ftype)
char *creat, *ftype;
{
  int i = 0;

  while (uft[i].uft_suffixlen >= 0) {
    if (bcmp(creat, uft[i].uft_creat, 4) == 0
     && bcmp(ftype, uft[i].uft_ftype, 4) == 0)
      return(i);
    i++;
  }
  return(-1);
}
#endif USR_FILE_TYPES
