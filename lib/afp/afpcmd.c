/*
 * $Author: djh $ $Date: 1995/05/30 08:51:20 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afpcmd.c,v 2.5 1995/05/30 08:51:20 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * afpcmd.c - Packing and unpacking commands
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * WARNING: if your machine is not a vax, sun, ibm pc/rt or pyramid you
 * (or Configure) must define BYTESWAPPED if your machine is byteswapped
 *
 * Edit History:
 *
 *  Thu Oct 30	Schilit    Created
 *
 */

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>		/* for ntohs, etc. */
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>

#ifndef BYTESWAPPED
# ifdef ultrix
#  ifdef mips
#   define BYTESWAPPED
#  endif mips
# endif ultrix
#endif  BYTESWAPPED

#ifndef BYTESWAPPED
# ifdef vax
#  define BYTESWAPPED
# endif vax
#endif  BYTESWAPPED

#ifndef BYTESWAPPED
# ifdef ns16000
#  define BYTESWAPPED
# endif ns16000
#endif  BYTESWAPPED

#ifndef BYTESWAPPED
# ifdef ns32000
#  define BYTESWAPPED
# endif ns32000
#endif  BYTESWAPPED

/* add this in case sequent doesn't define nsxxxxx */
#ifndef BYTESWAPPED
# ifdef sequent
#  define BYTESWAPPED
# endif sequent
#endif  BYTESWAPPED

#ifndef BYTESWAPPED
# ifdef MIPSEL
#  define BYTESWAPPED
# endif MIPSEL
#endif  BYTESWAPPED

#ifndef BYTESWAPPED
# ifdef i386
#  define BYTESWAPPED
# endif i386
#endif  BYTESWAPPED

/* machines that aren't byteswapped include: ibm032, pyr, sun, hpux, etc */

#ifndef NULL
#define NULL 0
#endif  NULL

export void InitPackTime();

char *PackNames[] = {
  "Word",			/* P_WORD */
  "Byte",			/* P_BYTE */
  "Double Word",		/* P_DWRD */
  "Bytes",			/* P_BYTS */
  "Pascal String",		/* P_PSTR */
  "BitMap",			/* P_BMAP */
  "Offset String",		/* P_OSTR */
  "Path String",		/* P_PATH */
  "Offset to Pointer",		/* P_OPTR */
  "Offset Path",		/* P_OPTH */
  "Even boundary",		/* P_EVEN */
  "Zero",			/* P_ZERO */
  "Time"			/* P_TIME */
  };

/*
 * Convert between Internal Unix time and External AFP time.
 * 
 * All date and time quantities used by AFP are Greenwich Mean Time (GMT)
 * values.  They are 32 bit signed integers corresponding to the number of
 * seconds measured from 12:00AM January 1, 2000 (the start of the next
 * century corresponds to datetime=0).
 *
 * BSD time base is 12:00AM January 1, 1970
 * AFP time base is 12:00AM January 1, 2000
 *
 * Actually, it seems that the Mac sends MacTime - could it be
 * that our copy of AppleShare does the wrong thing?  Version 1.00 was
 * bad.  Fixed in 1.10 and (hopefully) beyond.
 * 
 * Another problem exists with DST which is not handled here.
 * I mean, things will be within 2 hours isn't that good enough :-)
 *
 * WARNING: if this code breaks, it will be on or near Jan 1, 2000
 *  or Jan 18, 2038.  Of course, it's doubtful that this code
 *  will last that long :-)
 *
 */

private int mactime_not_inited = 1;
private long mymactime;
private long maczerotime;

/* difference btwn unix and mac in seconds */
/* mac starts at jan 1, 1904 for appleshare 1.0 */
#define AS1DOT0_MACZEROTIME 0	/* zero time - see comments above */
#define AS1DOT0_MACTIME (66*365+17)*24*60*60L

/* difference btween midnight jan 1, 1970 and midnight jan 1, 2000 */
/* for all others */
#define MACTIME (((30*365+7)*(-24)*3600L))
#define MACZEROTIME 0x80000000	/* zero time - from AppleShare 1.1 on */

/* Does time zone adjustment */

/*
 * call with flag set for appleshare 10 style
 *
 */

export void
InitPackTime(flag)
int flag;
{
  struct timeval tp;
  struct timezone tzp;

  mactime_not_inited = 0;
  if (flag) {
    mymactime = AS1DOT0_MACTIME;
    maczerotime = AS1DOT0_MACZEROTIME;
  } else {
    mymactime = MACTIME;
    maczerotime = MACZEROTIME;
  }
#ifdef SOLARIS
  tzset();
  mymactime -= ((long)timezone);
#else /* SOLARIS */
  gettimeofday(&tp, &tzp);
  mymactime -= ((long)tzp.tz_minuteswest*60);
#endif /* SOLARIS */
  return;
}

int
ntohPackX(pt, net, nlen, host)
PackEntry *pt;
byte *net;
byte *host;
int nlen;
{
  int ntohPackXbitmap();

  return(ntohPackXbitmap(pt, net, nlen, host, (word)0));
}

int
ntohPackXbitmap(pt, net, nlen, host, bitmap)
PackEntry *pt;
byte *net;
byte *host;
int nlen;
word bitmap;
{
  int i;
  int si = 0;
  word bmap = bitmap;
  void upack();

  /* need to find bmap */

  for (i=0; pt[i].pe_typ != P_END; i++)
    if (si < nlen)
      upack(&pt[i],net,&si,host,&bmap); /* unpack each item */

  return(si);
}

typedef struct Var_Object {
  int vo_typ;				/* type of variable object */
  byte *vo_offset;			/* store offset word (INT) here */
  byte *vo_object;			/* pointer to the object to store */
} VarObject;

typedef struct {
  int vol_first;			/* index at start of var objects */
  int vol_count;			/* count of var objects */
  VarObject vol_vos[10];		/* array of variable objects */
} VarObjList;

/*
 * Convert Packet from Host to Net, returns size of net packet.
 *
 */

int
htonPackX(pe, host, net)
PackEntry *pe;
byte *host;
byte *net;
{
  return(htonPackXbitmap(pe, host, net, 0));
}

int
htonPackXbitmap(pe, host, net, bitmap)
PackEntry *pe;
byte *host;
byte *net;
word bitmap;
{
  VarObjList vol;
  int i;
  word bmap = bitmap;
  int nidx = 0;
  void pack();

  vol.vol_first = 0;			/* position 0 for first var object */
  vol.vol_count = 0;			/* no var objects */

  for (i=0; pe[i].pe_typ != P_END; i++)
   pack(&pe[i],host,net,&nidx,&bmap,&vol);

  /* scan variable length object list and pack them now */
  
  for (i = 0; i < vol.vol_count; i++) {
    byte *d;
    VarObject *vo;
    OPTRType *optr,**optrr;
    word wrd;

    vo = &vol.vol_vos[i];		/* handle on a var object */
    /* set offset from start (in remember spot) */
    wrd = htons(nidx-vol.vol_first);
    bcopy(&wrd, vo->vo_offset, sizeof(word));
    d = &net[nidx];			/* destination */
					/* assume object is PSTR */
    switch (vo->vo_typ) {
    case P_OSTR:
      cpyc2pstr(d,vo->vo_object);	/* copy the pascal string */
      nidx += (*d)+1;
      break;
    case P_OPTR:			/* is a variable length object */
      optrr = (OPTRType **) (vo->vo_object); 
      optr = *optrr;
      bcopy(optr->optr_loc,d,optr->optr_len); /* copy the bytes */
      nidx += optr->optr_len;		/* increment length */
      break;
    }
  }
  return(nidx);			/* return length */
}  
    

/*
 * Pack host to net.
 *
 */

void
pack(pe, src, dest, di, bmap, vol)
PackEntry *pe;
byte *src, *dest;
int *di;
VarObjList *vol;
word *bmap;
{
  byte *s, *d, *t;
  int siz;
  word wrd;
  dword dwrd;
  sdword sdwrd;

  if (pe->pe_bit != 0) {		/* check if bitmap item */
    if ((*bmap & pe->pe_bit) == 0)	/* see if a requested bitmap item */
      return;				/*  no, forget it */
  }

  d = &dest[*di];			/* destination is net */
  s = &src[pe->pe_off];			/* source is host */
  siz = pe->pe_siz;

  switch (pe->pe_typ) {			/* according to the data type */
  case P_BMAP:				/* bitmap (2 bytes) */
    t = (byte *)bmap;
    t[0] = s[0];
    t[1] = s[1];
    vol->vol_first = *di;		/* offset */
    break;				/* BITMAP does NOT get included */
  case P_WORD:				/* word (2 bytes) */
#ifdef BYTESWAPPED
    d[0] = s[1];
    d[1] = s[0];
#else  BYTESWAPPED
    d[0] = s[0];
    d[1] = s[1];
#endif BYTESWAPPED
    *di += 2;
    break;
  case P_DWRD:				/* double word (4 bytes) */
#ifdef BYTESWAPPED
    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];
#else  BYTESWAPPED
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = s[3];
#endif BYTESWAPPED
    *di += 4;
    break;
  case P_BYTE:				/* single byte */
    d[0] = s[0];
    *di += 1;
    break;
  case P_BYTS:				/* multiple bytes */
    bcopy((char *)s, (char *)d, siz);
    *di += siz;
    break;
  case P_TIME:				/* internal to external time conv. */
    t = (byte *)&sdwrd;
    t[0] = s[0];
    t[1] = s[1];
    t[2] = s[2];
    t[3] = s[3];
    if (mactime_not_inited)
      InitPackTime(0);			/* assume version 1.0 obsolete */
    if ((time_t)sdwrd == 0)
      sdwrd = maczerotime;
    else
      sdwrd += mymactime;
#ifdef BYTESWAPPED
    d[0] = t[3];
    d[1] = t[2];
    d[2] = t[1];
    d[3] = t[0];
#else  BYTESWAPPED
    d[0] = t[0];
    d[1] = t[1];
    d[2] = t[2];
    d[3] = t[3];
#endif BYTESWAPPED
    *di += 4;				/* advance */
    break;
  case P_PSTR:				/* c to pascal string */
    cpyc2pstr(d,s);
    *di += (*d)+1;
    break;
  case P_OPTR:
  case P_OSTR:				/* offset string */
    {
      VarObject *vo;

      vo = &vol->vol_vos[vol->vol_count++]; /* handle on object entry */
      vo->vo_typ = pe->pe_typ;		/* save type */
      vo->vo_object = s;		/* the object to pack */
      vo->vo_offset = d;		/* location to store offset */
      *di += 2;				/* increment by offset size */
    }
    break;
  case P_PATH:
    bcopy(s,d,(int) (*s)+1);	/* pascal string with imbedded nulls */
    *di += (*s)+1;
    break;
  case P_EVEN:
    if ((*di % 2) != 0) {
      *d = 0;				/* zero the current byte */
      (*di)++;
    }
    break;
  case P_ZERO:
    bzero(d, siz);
    *di += siz;
    break;
  }
  return;
}


/*
 * Unpack net to host. Source is net, dest is host.
 *
 */

void
upack(pe, src, si, dest, bmap)
PackEntry *pe;
int *si;
byte *src, *dest;
word *bmap;
{
  word sos;
  byte *d, *s, *t;
  word wrd;
  dword dwrd;
  time_t timet;
  int siz;

  if (pe->pe_bit != 0)			/* check bitmap active */
    if ((*bmap & pe->pe_bit) == 0) 	/* then if not a bitmap item */
      return;				/*  forget it */

  d = &dest[pe->pe_off];		/* remember host destination */
  s = &src[*si];			/* net source */
  siz = pe->pe_siz;

  switch (pe->pe_typ) {			/* according to the data type */
  case P_BMAP:				/* bitmap (2 bytes) */
    t = (byte *)bmap;
    t[0] = d[0];
    t[1] = d[1];
    break;
  case P_WORD:				/* word (2 bytes) */ 
#ifdef BYTESWAPPED
    d[0] = s[1];
    d[1] = s[0];
#else  BYTESWAPPED
    d[0] = s[0];
    d[1] = s[1];
#endif BYTESWAPPED
    *si += 2;
    break;
  case P_DWRD:				/* double word (4 bytes) */
#ifdef BYTESWAPPED
    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];
#else  BYTESWAPPED
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = s[3];
#endif BYTESWAPPED
    *si += 4;
    break;
  case P_BYTE:				/* byte (1 byte) */
    d[0] = s[0];
    *si += 1;
    break;
  case P_BYTS:				/* byte string  */
    bcopy(s,d,siz);			/* copy bytes */
    *si += siz;
    break;
  case P_TIME:				/* external to internal time conv. */
    t = (byte *)&timet;
#ifdef BYTESWAPPED
    t[0] = s[3];
    t[1] = s[2];
    t[2] = s[1];
    t[3] = s[0];
#else  BYTESWAPPED
    t[0] = s[0];
    t[1] = s[1];
    t[2] = s[2];
    t[3] = s[3];
#endif BYTESWAPPED
    if (mactime_not_inited)
      InitPackTime(0);			/* assume version 1.0 obsolete */
    if ((sdword)timet == maczerotime)
      timet = 0;
    else
      timet = (time_t)((sdword)timet - mymactime);
    d[0] = t[0];
    d[1] = t[1];
    d[2] = t[2];
    d[3] = t[3];
    *si += 4;				/* advance */
    break;
  case P_PSTR:				/* c to pascal string */
    cpyp2cstr(d, s);
    *si += (*s)+1;
    break;
  case P_OSTR:				/* offset string */
    t = (byte *)&sos;
#ifdef BYTESWAPPED
    t[0] = s[1];
    t[1] = s[0];
#else  BYTESWAPPED
    t[0] = s[0];
    t[1] = s[1];
#endif BYTESWAPPED
    *si += 2;
    cpyp2cstr(d, &src[sos]);		/* copy string */
    break;
  case P_OPTH:
    t = (byte *)&sos;
#ifdef BYTESWAPPED
    t[0] = s[1];
    t[1] = s[0];
#else  BYTESWAPPED
    t[0] = s[0];
    t[1] = s[1];
#endif BYTESWAPPED
    *si += 2;
    bcopy(&src[sos], d, (int)src[sos]+1);	/* copy string */
    break;
  case P_PATH:
    bcopy(s, d, (int)(*s)+1);		/* pascal string with imbedded nulls */
    *si += (*s)+1;
    break;
  case P_OPTR:
    t = (byte *)&sos;
#ifdef BYTESWAPPED
    t[0] = s[1];
    t[1] = s[0];
#else  BYTESWAPPED
    t[0] = s[0];
    t[1] = s[1];
#endif BYTESWAPPED
    *si += 2;
    *((char **)d) = (char *)&src[sos];
    break;
  case P_EVEN:
    if ((*si % 2) != 0)			/* even boundary? */
      (*si)++;				/* no, move ahead */
    break;
  case P_ZERO:				/* supposed to be zero? */
    *si += siz;				/* then, just skip */
    *d = 0;				/* zero dest */
    break;
  }
  return;
}

/*
 * void PackDWord(dword s, byte *d)
 *
 * Pack a double word into destination.
 *
 */

void
PackDWord(s, d)
dword s;
byte *d;
{
  byte *t = (byte *)&s;
#ifdef BYTESWAPPED
  d[0] = t[3];
  d[1] = t[2];
  d[2] = t[1];
  d[3] = t[0];
#else  BYTESWAPPED
  d[0] = t[0];
  d[1] = t[1];
  d[2] = t[2];
  d[3] = t[3];
#endif BYTESWAPPED
  return;
}

/*
 * void PackWord(dword s, byte *d)
 *
 * Pack a word into destination.
 *
 */

void
PackWord(s, d)
word s;
byte *d;
{
  byte *t = (byte *)&s;
#ifdef BYTESWAPPED
  d[0] = t[1];
  d[1] = t[0];
#else  BYTESWAPPED
  d[0] = t[0];
  d[1] = t[1];
#endif BYTESWAPPED
  return;
}

/*
 *
 * Unpack double word (4 bytes) from src into dest.
 * The src pointer is updated.
 *
 */

void
UnpackDWord(src, dest)
byte **src;
dword *dest;
{
  byte *s = *src;
  byte *d = (byte *)dest;
#ifdef BYTESWAPPED
  d[0] = s[3];
  d[1] = s[2];
  d[2] = s[1];
  d[3] = s[0];
#else  BYTESWAPPED
  d[0] = s[0];
  d[1] = s[1];
  d[2] = s[2];
  d[3] = s[3];
#endif BYTESWAPPED
  *src += 4;			/* update source pointer */
  return;
}

/*
 *
 * Unpack word (2 bytes) from src into dest.
 * The src pointer is updated.
 *
 */

void
UnpackWord(src, dest)
byte **src;
word *dest;
{
  byte *s = *src;
  byte *d = (byte *)dest;
#ifdef BYTESWAPPED
  d[0] = s[1];
  d[1] = s[0];
#else  BYTESWAPPED
  d[0] = s[0];
  d[1] = s[1];
#endif BYTESWAPPED
  *src += 2;			/* update source pointer */
  return;
}
