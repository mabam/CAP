/*
 * $Author: djh $ $Date: 1996/05/01 15:30:26 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abmisc.c,v 2.9 1996/05/01 15:30:26 djh Rel djh $
 * $Revision: 2.9 $
 *
 */

/*
 * abmisc.c - miscellaneous, but nevertheless useful routines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 13, 1986    Schilit    Created.
 *  June 15, 1986    CCKim	move to abmisc.c
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netat/appletalk.h>

/*
 * cpyc2pstr(char *ps, char *cs)
 * 
 * Copy a C style string to an Apple Pascal style string
 *
 * Restrictions: sizeof(*ps) must be >= 1+sizeof(*cs)
 */ 

void
cpyc2pstr(ps,cs)
char *ps,*cs;
{

  *ps++ = (u_char) strlen(cs);	/* copy in length, one byte */
  while (*ps++ = *cs++) ;	/* copy in the rest... */
}

/*
 * cpyp2cstr(char *cs, char *ps)
 * 
 * Copy a Apple Pascal style string to C string
 *
 */ 

void
cpyp2cstr(cs,ps)
char *cs,*ps;
{
  bcopy(ps+1,cs,(u_char) *ps);
  cs[*ps] = '\0';		/* tie off */
}

/*
 * pstrcpy(d, s) - like strcpy, but for pascal strings
 *
*/
pstrcpy(d, s)
byte *d, *s;
{
  int len = (int)*s;

  bcopy(s, d, len+1);		/* +1 for length too */
}

/* like strncpy, but for pascal strings */
pstrncpy(d, s, n)
byte *s;
byte *d;
int n;
{
  int len = (int)*s;

  if (len > n)
    len = n;
  bcopy(s+1, d+1, len);
  *d = len&0xff;		/* single byte */
}
/*
 * pstrlen(s) - like strlen, but for pascal strings
 *
*/
pstrlen(s)
byte *s;
{
  return((int)*s);
}

/*
 * pstrcmp(s1, s2) - like strcmp, but for pascal strings
 *
*/
pstrcmp(s1, s2)
byte *s1;
byte *s2;
{
  int len1 = ((int)*s1)+1;	/* account for length */

  s1++, s2++;			/* skip length fields */
  /* no special check for length since this will also do the "right" thing */
  /* (also, know we have at least one) */
  while (len1--) {
    if (*s1 != *s2)
      return(*s1 - *s2);	/* returns right thing? */
    s1++, s2++;
  }
  return(0);			/* equal */
}

/* 
 * dbugarg(char *argv)
 *
 * Process the -d argument from the command line, setting debug options
 * in global dbug structure.
 *
 * a - db_atp; l - db_lap; d - db_ddp; n - db_nbp;  
 * i - db_ini; p - db_pap;   
 *
*/

dbugarg(s)
char *s;
{
  int err = 0;
  struct cap_version *v, *what_cap_version();

  while (*s != '\0' && !err) {
    switch(*s) {
    case 'l': dbug.db_lap = TRUE; printf("debugging LAP\n"); break;
    case 'd': dbug.db_ddp = TRUE; printf("debugging DDP\n"); break;
    case 'a': dbug.db_atp = TRUE; printf("debugging ATP\n"); break;
    case 'n': dbug.db_nbp = TRUE; printf("debugging NBP\n"); break;
    case 'p': dbug.db_pap = TRUE; printf("debugging PAP\n"); break;
    case 'i': dbug.db_ini = TRUE; printf("debugging INI\n"); break;
    case 's': dbug.db_asp = TRUE; printf("debugging ASP\n"); break;
    case 'k': dbug.db_skd = TRUE; printf("debugging SKD\n"); break;
    case 'v': /* print useful version information and exit */
      v = what_cap_version();
      printf("%s version %d.%d, patch level %d, %s, %s\n%s\n",
	v->cv_name, v->cv_version, v->cv_subversion,
	v->cv_patchlevel, v->cv_rmonth, v->cv_ryear, v->cv_type);
      exit(0);
      break;
    default: 
      err++;
    }
    s++;
  }
  if (err) {
    fprintf(stderr,"l lap, d ddp, a atp, n nbp, p pap, i ini, s asp\n");
    fprintf(stderr,"k scheduler\n");
    return(-1);
  }
  return(0);
}

/*
 * Establish a bds (of size most numbds) with buffer buf of size bufsiz 
 * set all bds userdata fields to userdata
 *
 * return count of bds's.  More buffer than bds is okay.
 *
*/
int
setup_bds(bds, numbds, segsize, buf, bufsiz, userdata)
BDS bds[];
int numbds;
int segsize;
char *buf;
int bufsiz;
atpUserDataType userdata;
{
  int cnt, i, cursize;

  i = cnt = 0;
  do {
    bds[i].userData = userdata;
    cursize = min(segsize, bufsiz-cnt);
    bds[i].buffSize = cursize;
    bds[i].buffPtr = buf+cnt;
    cnt += cursize;
    i++;
  } while ((cnt - bufsiz) < 0 && i < numbds);
  return(i);
}

int
sizeof_bds(bds, numbds)
BDS bds[];
int numbds;
{
  int i, cnt;

  for (i = 0, cnt = 0; i < numbds; i++)
    cnt += bds[i].dataSize;
  return(cnt);
}

/* routines to deal with "Indexed" strings */
/**** should be in abmisc ******/

/*
 * void IniIndStr(byte *istr)
 *
 * Initialize an Indexed string for calls to AddIndStr
 *
 */

void
IniIndStr(istr)
byte *istr;
{
  *istr = 0;			/* index count is zero */
}

/*
 * void AddIndStr(char *src,byte *istr)
 *
 *
 * Add a c string to a Indexed string.  The c string always
 * is added to the end of the indexed string.
 *
 */

void
AddIndStr(src,istr)
char *src;
byte *istr;
{
  byte *idx = istr;
  int i;

  for (i = *istr++; i > 0; i--)	/* step past index, count down on it */
    istr += (*istr)+1;		/* move past each pascal string */
  cpyc2pstr(istr,src);		/* copy into idx string */
  (*idx)++;			/* increment the index */
}

/*
 * GetIndStr(char *dest,byte *istr,int index)
 * 
 * Copy from an indexed string into a c string.  Use index to select
 * the string entry within the indexed string type.
 *
 */ 

void
GetIndStr(dest,istr,idx)
char *dest;
byte *istr;
int idx;
{
  if (idx > 255 || idx >= (int)*istr || idx < 0) {
    fprintf(stderr,"GetIndString: idx out of range\n");
    return;
  }

  istr++;			/* step past idx count */
  while (idx--)
    istr += (*istr)+1;		/* step past this pstr */
  cpyp2cstr(dest,istr);		/* then copy into destination */
}


/*
 * int IndStrCnt(byte *istr)
 *
 * Return the count of entries in an indexed string.
 *
 */

int
IndStrCnt(istr)
byte *istr;
{
  return(*istr);		/* this is easy... */
}


/*
 * IndStrLen(byte *istr)
 *
 * Return the length of the indexed string istr including the count byte.
 *
 */

int 
IndStrLen(istr)
byte *istr;
{
  byte *idx = istr;
  int i;

  istr++;			/* step past index */
  for (i=1; i <= (int)*idx; i++)
    istr += (*istr)+1;		/* move to next pascal string */
  return((istr-idx));		/* here is the count... */
}

/*
 * PrtIndStr(istr)
 *
 * For debugging, dump the content indexed string.
 *
 */

PrtIndStr(istr)
byte *istr;
{
  char cstr[256];
  byte *idx = istr;
  int i;

  printf("Entries in indexed string: %d\n",*istr);
  istr++;			/* step past index */
  for (i=1; i <= (int)*idx; i++) {
    cpyp2cstr(cstr,istr);	/* copy into c string */
    printf("%d: '%s'\n",i,cstr);
    istr += (*istr)+1;		/* move to next pascal string */
  }
}


/*
 * int strcmpci(char *s, char *t)
 *
 * Case insensitive version of strcmp.
 *
*/

strcmpci(s,t)
u_char *s,*t;
{
  register char c,d;

  for (;;) {
    c = *s++;
    if (isascii(c) && isupper(c))
      c = tolower(c);
    d = *t++;
    if (isascii(d) && isupper(d))
      d = tolower(d);
    if (c != d)
      return(c-d);
    if (c == '\0')
      return(0);
  }
}

/*
 * int strncmpci(char *s, char *t,int n)
 *
 * Case insensitive version of strcmp.
 *
*/

strncmpci(s,t,n)
char *s,*t;
int n;
{
  register char c,d;

  for (;n > 0;n--) {
    c = *s++;
    if (isascii(c) && isupper(c))
      c = tolower(c);
    d = *t++;
    if (isascii(d) && isupper(d))
      d = tolower(d);
    if (c != d)
      return(c-d);
    if (c == '\0')
      return(0);
  }
  return(0);			/* success on runnout */
}

/*
 * Take a string pointed to by p and duplicate it in a malloc'ed block
 * of memory.
 *
 * If the passed pointer is null, a null string is returned.
 *
 * returns: pointer to new string, null if no space
 *
*/
char *
strdup(p)
char *p;
{
  char *r = (char *)malloc((p == NULL) ? 1 : (strlen(p)+1));

  if (r == NULL)
    return(NULL);
  if (p == NULL) {
    *r = 0;
    return(r);
  }
  strcpy(r, p);
  return(r);
}

/*
 * int
 * cmptime (struct timeval *t1,*t2)
 *
 * values must be pairwise across <relative,relative> or
 *  <absolute,absolute>
 *
 * cmptime compares two time values and returns an integer
 * greater than, equal to, or less than zero according to
 * whether the time represented in t1 is greater than, equal
 * to, or less than t2.
 *
*/
int
cmptime(t1,t2)
struct timeval *t1,*t2;
{
  if ((t1->tv_sec == t2->tv_sec)) 	/* seconds the same? */
    if (t1->tv_usec == t2->tv_usec) 	/* and usec the same? */
      return(0);			/* then equal */
    else				/* otherwise depends on usec */
      return((t1->tv_usec < t2->tv_usec) ? -1 : 1);
  return((t1->tv_sec < t2->tv_sec) ? -1 : 1);
}

#ifdef ISO_TRANSLATE
/*
 * Macintosh/ISO Character Translation Routines
 *
 */

/*
 * Table for translating Macintosh characters 0x80-0xff to ISO
 * (Macintosh characters 0x00-0x7f map directly to ISO equivalent).
 *
 * The top half of the table contains 0x00 in positions where
 * no reversible character mapping exists, the bottom half of
 * the table contains a reversible mapping for text translation.
 *
 */

u_char Mac2ISO[256] = {
      0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1,  /* 80 - 87 */
      0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,  /* 88 - 8F */
      0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3,  /* 90 - 97 */
      0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,  /* 98 - 9F */
      0x84, 0xB0, 0xA2, 0xA3, 0xA7, 0xB7, 0xB6, 0xDF,  /* A0 - A7 */
      0xAE, 0xA9, 0x85, 0xB4, 0xA8, 0xAD, 0xC6, 0xD8,  /* A8 - AF */
      0x86, 0xB1, 0xB2, 0xB3, 0xA5, 0xB5, 0x87, 0x88,  /* B0 - B7 */
      0xBC, 0xB9, 0xBE, 0xAA, 0xBA, 0xBD, 0xE6, 0xF8,  /* B8 - BF */
      0xBF, 0xA1, 0xAC, 0x89, 0x8A, 0x8B, 0x8C, 0xAB,  /* C0 - C7 */
      0xBB, 0x8D, 0xA0, 0xC0, 0xC3, 0xD5, 0x8E, 0x8F,  /* C8 - CF */
      0xD0, 0x90, 0x91, 0x92, 0x93, 0x94, 0xF7, 0xD7,  /* D0 - D7 */
      0xFF, 0xDD, 0x2F, 0xA4, 0x3C, 0x3E, 0xDE, 0x95,  /* D8 - DF */
      0x96, 0x97, 0x98, 0x99, 0x9A, 0xC2, 0xCA, 0xC1,  /* E0 - E7 */
      0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,  /* E8 - EF */
      0xF0, 0xD2, 0xDA, 0xDB, 0xD9, 0x9B, 0x5E, 0x7E,  /* F0 - F7 */
      0xAF, 0x9C, 0x9D, 0x9E, 0xB8, 0xFD, 0xFE, 0x9F,  /* F8 - FF */

      0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1,  /* 80 - 87 */
      0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,  /* 88 - 8F */
      0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3,  /* 90 - 97 */
      0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,  /* 98 - 9F */
      0x00, 0xB0, 0xA2, 0xA3, 0xA7, 0xB7, 0xB6, 0xDF,  /* A0 - A7 */
      0xAE, 0xA9, 0x00, 0xB4, 0xA8, 0x00, 0xC6, 0xD8,  /* A8 - AF */
      0x00, 0xB1, 0x00, 0x00, 0xA5, 0xB5, 0x00, 0x00,  /* B0 - B7 */
      0x00, 0x00, 0x00, 0xAA, 0xBA, 0x00, 0xE6, 0xF8,  /* B8 - BF */
      0xBF, 0xA1, 0xAC, 0x00, 0x00, 0x00, 0x00, 0xAB,  /* C0 - C7 */
      0xBB, 0x00, 0xA0, 0xC0, 0xC3, 0xD5, 0x00, 0x00,  /* C8 - CF */
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7, 0x00,  /* D0 - D7 */
      0xFF, 0x00, 0x2F, 0xA4, 0x3C, 0x3E, 0x00, 0x00,  /* D8 - DF */
      0x00, 0x00, 0x00, 0x00, 0x00, 0xC2, 0xCA, 0xC1,  /* E0 - E7 */
      0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,  /* E8 - EF */
      0x00, 0xD2, 0xDA, 0xDB, 0xD9, 0x00, 0x5E, 0x7E,  /* F0 - F7 */
      0xAF, 0x00, 0x00, 0x00, 0xB8, 0x00, 0x00, 0x00   /* F8 - FF */
};

/*
 * Table for translating ISO characters 0x80-0xff to Macintosh
 * (ISO characters 0x00-0x7f map directly to Macintosh equivalent).
 *
 * The top half of the table contains 0x00 in positions where
 * no reversible character mapping exists, the bottom half of
 * the table contains a reversible mapping for text translation.
 *
 */

u_char ISO2Mac[256] = {
      0x00, 0x00, 0x00, 0x00, 0xA0, 0xAA, 0xB0, 0xB6,  /* 80 - 87 */
      0xB7, 0xC3, 0xC4, 0xC5, 0xC6, 0xC9, 0xCE, 0xCF,  /* 88 - 8F */
      0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xDF, 0xE0, 0xE1,  /* 90 - 97 */
      0xE2, 0xE3, 0xE4, 0xF5, 0xF9, 0xFA, 0xFB, 0xFF,  /* 98 - 9F */
      0xCA, 0xC1, 0xA2, 0xA3, 0xDB, 0xB4, 0x7C, 0xA4,  /* A0 - A7 */
      0xAC, 0xA9, 0xBB, 0xC7, 0xC2, 0xAD, 0xA8, 0xF8,  /* A8 - AF */
      0xA1, 0xB1, 0xB2, 0xB3, 0xAB, 0xB5, 0xA6, 0xA5,  /* B0 - B7 */
      0xFC, 0xB9, 0xBC, 0xC8, 0xB8, 0xBD, 0xBA, 0xC0,  /* B8 - BF */
      0xCB, 0xE7, 0xE5, 0xCC, 0x80, 0x81, 0xAE, 0x82,  /* C0 - C7 */
      0xE9, 0x83, 0xE6, 0xE8, 0xED, 0xEA, 0xEB, 0xEC,  /* C8 - CF */
      0xD0, 0x84, 0xF1, 0xEE, 0xEF, 0xCD, 0x85, 0xD7,  /* D0 - D7 */
      0xAF, 0xF4, 0xF2, 0xF3, 0x86, 0xD9, 0xDE, 0xA7,  /* D8 - DF */
      0x88, 0x87, 0x89, 0x8B, 0x8A, 0x8C, 0xBE, 0x8D,  /* E0 - E7 */
      0x8F, 0x8E, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,  /* E8 - EF */
      0xF0, 0x96, 0x98, 0x97, 0x99, 0x9B, 0x9A, 0xD6,  /* F0 - F7 */
      0xBF, 0x9D, 0x9C, 0x9E, 0x9F, 0xFD, 0xFE, 0xD8,  /* F8 - FF */

      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 80 - 87 */
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 88 - 8F */
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 90 - 97 */
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 98 - 9F */
      0xCA, 0xC1, 0xA2, 0xA3, 0xDB, 0xB4, 0x7C, 0xA4,  /* A0 - A7 */
      0xAC, 0xA9, 0xBB, 0xC7, 0xC2, 0x00, 0xA8, 0xF8,  /* A8 - AF */
      0xA1, 0xB1, 0x00, 0x00, 0xAB, 0xB5, 0xA6, 0xA5,  /* B0 - B7 */
      0xFC, 0x00, 0xBC, 0xC8, 0x00, 0x00, 0x00, 0xC0,  /* B8 - BF */
      0xCB, 0xE7, 0xE5, 0xCC, 0x80, 0x81, 0xAE, 0x82,  /* C0 - C7 */
      0xE9, 0x83, 0xE6, 0xE8, 0xED, 0xEA, 0xEB, 0xEC,  /* C8 - CF */
      0x00, 0x84, 0xF1, 0xEE, 0xEF, 0xCD, 0x85, 0x00,  /* D0 - D7 */
      0xAF, 0xF4, 0xF2, 0xF3, 0x86, 0x00, 0x00, 0xA7,  /* D8 - DF */
      0x88, 0x87, 0x89, 0x8B, 0x8A, 0x8C, 0xBE, 0x8D,  /* E0 - E7 */
      0x8F, 0x8E, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,  /* E8 - EF */
      0x00, 0x96, 0x98, 0x97, 0x99, 0x9B, 0x9A, 0xD6,  /* F0 - F7 */
      0xBF, 0x9D, 0x9C, 0x9E, 0x9F, 0x00, 0x00, 0xD8   /* F8 - FF */
};

/*
 * Translate Macintosh characters in the supplied C-string
 * to their ISO equivalents using a reversible mapping.
 *
 */

void
cMac2ISO(cStr)
u_char *cStr;
{
  while (*cStr) {
    if (*cStr & 0x80)
      *cStr = Mac2ISO[*cStr & 0x7f];
    cStr++;
  }

  return;
}

/*
 * Translate ISO characters in the supplied C-string to
 * their Macintosh equivalents using a reversible mapping.
 *
 */

void
cISO2Mac(cStr)
u_char *cStr;
{
  while (*cStr) {
    if (*cStr & 0x80)
      *cStr = ISO2Mac[*cStr & 0x7f];
    cStr++;
  }

  return;
}

/*
 * Translate Macintosh characters in the supplied P-string
 * to their ISO equivalents using a reversible mapping.
 *
 */

void
pMac2ISO(pStr)
u_char *pStr;
{
  int i;
  int len = *pStr++;

  for (i = 0; i < len; i++) {
    if (*pStr & 0x80)
      *pStr = Mac2ISO[*pStr & 0x7f];
    pStr++;
  }

  return;
}

/*
 * Translate ISO characters in the supplied P-string
 * to their Macintosh equivalents using a reversible mapping.
 *
 */

void
pISO2Mac(pStr)
u_char *pStr;
{
  int i;
  int len = *pStr++;

  for (i = 0; i < len; i++) {
    if (*pStr & 0x80)
      *pStr = ISO2Mac[*pStr & 0x7f];
    pStr++;
  }

  return;
}

int
isISOprint(c)
u_char c;
{
  if (c & 0x80)
    return(ISO2Mac[c]);
  else
    return(isprint(c));
}

#endif ISO_TRANSLATE
