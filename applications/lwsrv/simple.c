static char rcsid[] = "$Author: djh $ $Date: 1996/06/18 10:50:20 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/simple.c,v 2.13 1996/06/18 10:50:20 djh Rel djh $";
static char revision[] = "$Revision: 2.13 $";

/*
 * lwsrv - UNIX AppleTalk spooling program: act as a laserwriter
 * simple.c - simple spooler.  Understands enough of Adobe's Document
 * Structuring Conventions to work, but doesn't really abide by them.
 *
 * should be replaced by a conforming implementation.
 *
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 * Sept 5, 1987 created by cck
 * Jan 21, 1992 gkl300 - added simple pass thru of all received files
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#ifndef _TYPES
/* assume included by param.h */
# include <sys/types.h>
#endif  /* _TYPES */
#ifdef LWSRV8
#include <sys/time.h>
#endif /* LWSRV8 */
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#include <netat/compat.h>

#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef LWSRV8
#include "list.h"
#include "query.h"
#include "parse.h"
#endif /* LWSRV8 */

#include "spmisc.h"
#include "procset.h"
#include "fontlist.h"
#include "papstream.h"

#ifdef PAGECOUNT
#ifndef LBUFFER
#define	LBUFFER	PAGECOUNT
#endif LBUFFER
#endif PAGECOUNT

#ifdef LWSRV8
#define MAXTOKSTR 1024
#endif /* LWSRV8 */

/* TOKEN types */
/* token types above: 4095 are not valid */
/* integer type should be at least 16 bits */
/* top 4 bits are used in to validate, invalidate things */
#define TOK_INVALID 0x1000	/* token shouldn't be considered */
#define TOK_VAL_MASK 0xfff	/* token value mask */
#define TOK_EOF 200		/* end of file marker */
#define TOK_UNK 201		/* unknown token */
#define TOK_ADV 0		/* AppleDict Version */
#define TOK_FLS 1		/* FontList */
#define TOK_ENDOLD 2		/* End of one of above */
#define TOK_TIT 3
#define TOK_CRE 4
#define TOK_FOR 5
#define TOK_ENC 6
#define TOK_BPSQ 7		/* begin procset query */
#define TOK_EPSQ 8		/* end procset query */
#define TOK_BFLQ 9		/* begin fontlist query */
#define TOK_EFLQ 10		/* end fontlist query */
#define TOK_BPS 11
#define TOK_EPS 12
#define TOK_IPS 13		/* include procset */
#ifndef LWSRV8
#ifdef PAGECOUNT
#define TOK_PGS 14		/* Number of pages */
#endif PAGECOUNT
#else  /* not LWSRV8 */
#define TOK_PGS 14		/* Number of pages */
#define TOK_BFOQ 15		/* begin font query */
#define TOK_EFOQ 16		/* end font query */
#define TOK_BFEQ 17		/* begin feature query */
#define TOK_EFEQ 18		/* end feature query */
#define TOK_BVQ 19		/* begin vm status query */
#define TOK_EVQ 20		/* end vm status query */
#define TOK_BPRQ 21		/* begin printer query */
#define TOK_EPRQ 22		/* end printer query */
#define TOK_BQU 23		/* begin query */
#define TOK_EQU 24		/* end query */
#define TOK_BFIQ 25		/* begin file query */
#define TOK_EFIQ 26		/* end file query */
#define TOK_BREQ 27		/* begin resource query */
#define TOK_EREQ 28		/* end resource query */
#define TOK_BRLQ 29		/* begin resource list query */
#define TOK_ERLQ 30		/* end resource list query */
#define TOK_PSA 31		/* %!PS-Adobe- */
#define TOK_TEOF 32		/* %%EOF */
#define TOK_PAGE 33		/* beginning of each page */
#endif /* LWSRV8 */
#define TOK_BEGINR 101		/* begin of query */
#define TOK_ENDR 102		/* end query with response */
#define TOK_BEGIN 103		/* begin item */
#define TOK_END 104		/* end item */
#define TOK_DROP 105		/* simply drop */
#ifdef PROCSET_PATCH
#define TOK_PATCH 106		/* server patches (fails on non-Apple LW) */
#endif PROCSET_PATCH

#ifdef LWSRV8
#define	ECHO_UNCHANGED	0
#define	ECHO_ON		1
#define	ECHO_OFF	2
#define	ECHO_DROP	3
#endif /* LWSRV8 */

/* token table */
static struct atoken {
  char *token;
#ifndef LWSRV8
  int toklen;
  int tokval;
#else /* LWSRV8 */
  short tokval;
  byte toklen;
  byte changeecho;
#endif /* LWSRV8 */
} toktbl[] =  {
#ifndef LWSRV8
#define TOKEN(token, tag) { (token), (sizeof(token)-1), (tag)}
  TOKEN("%%EOF", TOK_DROP),
  TOKEN("%%Title", TOK_TIT),
  TOKEN("%%Creator", TOK_CRE),
  TOKEN("%%For", TOK_FOR),
  TOKEN("%%EndComments", TOK_ENC),
  TOKEN("%%?BeginProcSetQuery", TOK_BPSQ),
  TOKEN("%%?EndProcSetQuery", TOK_EPSQ),
  TOKEN("%%?BeginFontListQuery", TOK_BFLQ),
  TOKEN("%%?EndFontListQuery", TOK_EFLQ),
  TOKEN("%%?BeginFontQuery", TOK_BEGINR),
  TOKEN("%%?EndFontQuery", TOK_ENDR),
  TOKEN("%%?BeginFeatureQuery", TOK_BEGINR),
  TOKEN("%%?EndFeatureQuery", TOK_ENDR), 
  TOKEN("%%?BeginVMStatus", TOK_BEGINR),
  TOKEN("%%?EndVMStatus", TOK_ENDR),
  TOKEN("%%BeginExitServer", TOK_BEGIN),
  TOKEN("%%EndExitServer", TOK_END),
  TOKEN("%%BeginProcSet", TOK_BPS),
  TOKEN("%%EndProcSet", TOK_EPS),
  TOKEN("%%?BeginPrinterQuery", TOK_BEGINR),
  TOKEN("%%?EndPrinterQuery", TOK_ENDR),
  TOKEN("%%?BeginQuery", TOK_BEGINR),
  TOKEN("%%?EndQuery", TOK_ENDR),
#else /* LWSRV8 */
/* Now in SORTED order, because we do a binary search on it */
#define TOKEN(token, tag, e) {(token), (tag), (sizeof(token)-1), (e)}
  TOKEN("%!PS-Adobe-", TOK_PSA, ECHO_OFF),
  TOKEN("%%?BeginFeatureQuery", TOK_BFEQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginFileQuery", TOK_BFIQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginFontListQuery", TOK_BFLQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginFontQuery", TOK_BFOQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginPrinterQuery", TOK_BPRQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginProcSetQuery", TOK_BPSQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginQuery", TOK_BQU, ECHO_UNCHANGED),
  TOKEN("%%?BeginResourceListQuery", TOK_BRLQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginResourceQuery", TOK_BREQ, ECHO_UNCHANGED),
  TOKEN("%%?BeginVMStatus", TOK_BVQ, ECHO_UNCHANGED),
  TOKEN("%%?EndFeatureQuery", TOK_EFEQ, ECHO_UNCHANGED),
  TOKEN("%%?EndFileQuery", TOK_EFIQ, ECHO_UNCHANGED),
  TOKEN("%%?EndFontListQuery", TOK_EFLQ, ECHO_UNCHANGED),
  TOKEN("%%?EndFontQuery", TOK_EFOQ, ECHO_UNCHANGED),
  TOKEN("%%?EndPrinterQuery", TOK_EPRQ, ECHO_UNCHANGED),
  TOKEN("%%?EndProcSetQuery", TOK_EPSQ, ECHO_UNCHANGED),
  TOKEN("%%?EndQuery", TOK_EQU, ECHO_UNCHANGED),
  TOKEN("%%?EndResourceListQuery", TOK_ERLQ, ECHO_UNCHANGED),
  TOKEN("%%?EndResourceQuery", TOK_EREQ, ECHO_UNCHANGED),
  TOKEN("%%?EndVMStatus", TOK_EVQ, ECHO_UNCHANGED),
  TOKEN("%%BeginExitServer", TOK_BEGIN, ECHO_OFF),
  TOKEN("%%BeginProcSet", TOK_BPS, ECHO_OFF),
  TOKEN("%%Creator", TOK_CRE, ECHO_UNCHANGED),
  TOKEN("%%EOF", TOK_TEOF, ECHO_DROP),
  TOKEN("%%EndComments", TOK_ENC, ECHO_UNCHANGED),
  TOKEN("%%EndExitServer", TOK_END, ECHO_ON),
  TOKEN("%%EndProcSet", TOK_EPS, ECHO_ON),
  TOKEN("%%For", TOK_FOR, ECHO_UNCHANGED),
#endif /* LWSRV8 */
#ifdef ADOBE_DSC2_CONFORMANT
#ifndef LWSRV8
  TOKEN("%%IncludeProcSet", TOK_IPS),
#else /* LWSRV8 */
  TOKEN("%%IncludeProcSet", TOK_IPS, ECHO_UNCHANGED),
#endif /* LWSRV8 */
#else  ADOBE_DSC2_CONFORMANT
#ifndef LWSRV8
  TOKEN("%%IncludeProcSet", TOK_IPS|TOK_INVALID),
#else /* LWSRV8 */
  TOKEN("%%IncludeProcSet", TOK_IPS|TOK_INVALID, ECHO_UNCHANGED),
#endif /* LWSRV8 */
#endif ADOBE_DSC2_CONFORMANT
#ifndef LWSRV8
#ifdef PAGECOUNT
  TOKEN("%%Pages", TOK_PGS),
#endif PAGECOUNT
#else /* LWSRV8 */
  TOKEN("%%Page:", TOK_PAGE, ECHO_UNCHANGED),
  TOKEN("%%Pages", TOK_PGS, ECHO_UNCHANGED),
#endif /* LWSRV8 */
#ifdef PROCSET_PATCH
#ifndef LWSRV8
  TOKEN("%%Patches", TOK_PATCH),
#else /* LWSRV8 */
  TOKEN("%%Patches", TOK_PATCH, ECHO_OFF),
#endif /* LWSRV8 */
#endif PROCSET_PATCH
#ifdef LWSRV8
  TOKEN("%%Title", TOK_TIT, ECHO_UNCHANGED),
#endif /* LWSRV8 */
  /* very old type of queries */
#ifndef LWSRV8
  TOKEN("%?appledict version #", TOK_ADV),
  TOKEN("%?fontList", TOK_FLS),
  TOKEN("%?end",TOK_ENDOLD),
  {NULL, TOK_UNK, 0}
#else /* LWSRV8 */
  TOKEN("%?appledict version #", TOK_ADV, ECHO_OFF),
  TOKEN("%?end",TOK_ENDOLD, ECHO_ON),
  TOKEN("%?fontList", TOK_FLS, ECHO_OFF),
#endif /* LWSRV8 */
};

#ifndef LWSRV8
private char *tracefile;
#endif /* not LWSRV8 */
private char *dictdir;
private char *prtname;
private FILE *procsetfile = NULL;
private char tmpstr[MAXPATHLEN];
private FILE *outfile;
private int crtolf = FALSE;
private int needquote = FALSE;
private int nodsc = FALSE;
#ifdef ADOBE_DSC2_CONFORMANT
private int adobe_dsc2_conformant = TRUE;
#else  ADOBE_DSC2_CONFORMANT
private int adobe_dsc2_conformant = FALSE;
#endif ADOBE_DSC2_CONFORMANT
#ifdef PASS_THRU
private int simple_pass_thru = FALSE;
#endif PASS_THRU

import char *myname;
#ifndef LWSRV8
#ifdef DONT_PARSE
import int dont_parse;
#endif DONT_PARSE
#else /* LWSRV8 */
import boolean tracing;
import char *queryfile;
private char querystr[] = "Query";
#define	QUERYSTRLEN	(sizeof(querystr) - 1)
private boolean query3 = FALSE;
private char newline[] = "\n";
private FILE *qfp = NULL;
private byte tokstr[MAXTOKSTR + 1];
private byte tokeol = 0;
private int toklen;
private int echo;
extern int capture;
#endif /* LWSRV8 */

export int is_simple_dsc();
export int simple_dsc_option();
export int simple_TranscriptOption();
export int spool_setup();

private void validate_token_type();
private void invalidate_token_type();
private int tokval();
private void dumpout();
#ifdef LWSRV8
private void dumpone();
private void dumpoutfile();
private void dumpprocsetfile();
private void dumpquery();
private void logquery();
private int nextline();
private char *unparen();
#endif /* LWSRV8 */
private int scantoken();
int getjob();
private void SendVAck();

#ifdef PASS_THRU
export int
#ifndef LWSRV8
set_simple_pass_thru()
#else /* LWSRV8 */
set_simple_pass_thru(flg)
int flg;
#endif /* LWSRV8 */
{
#ifndef LWSRV8
  simple_pass_thru = TRUE;
  return(0);
#else /* LWSRV8 */
  simple_pass_thru = flg;
#endif /* LWSRV8 */
}
#endif PASS_THRU

export int
is_simple_dsc()
{
  return(adobe_dsc2_conformant);
}

#ifdef LWSRV8
export
set_simple_dsc(flg)
int flg;
{
  adobe_dsc2_conformant = flg;
}
#endif /* LWSRV8 */

export int
simple_dsc_option(ioption)
char *ioption;
{
  int i;
  char *p;
  char option[5];		/* big enough for biggest */

  for (i = 0, p = ioption; i < 4; i++, p++)
    if (*p == '\0')
      break;
    else
      option[i] = (isupper(*p)) ? tolower(*p) : *p;
  option[i] = '\0';		/* tie off string */

  if (strcmp(option, "on") == 0) {
    fprintf(stderr, "%s: simple: Turning on DSC2 compatibility, was %s\n",
	        myname, adobe_dsc2_conformant ? "on" : "off");
    validate_token_type(TOK_IPS);
#ifdef LWSRV8
    return(TRUE);
#endif /* LWSRV8 */
  } else if (strcmp(option, "off") == 0) {
    fprintf(stderr, "%s: simple: Turning off DSC2 compatibility, was %s\n",
	        myname, adobe_dsc2_conformant ? "on" : "off");
#ifndef LWSRV8
    adobe_dsc2_conformant = FALSE;
#endif /* LWSRV8 */
    invalidate_token_type(TOK_IPS);
#ifdef LWSRV8
    return(FALSE);
#endif /* LWSRV8 */
  } else {
    fprintf(stderr,"%s: simple: unknown Transcript compatiblity option: %s\n",
	    myname, option);
    return(-1);
  }
#ifndef LWSRV8
  return(0);
#endif /* not LWSRV8 */
}

/*
 * establish transcript compatibility options if any
 *
 */

export int
simple_TranscriptOption(ioption)
char *ioption;
{
  register char *p;
  register char *q;
  int i;
  char option[100];		/* big enough for biggest option */

  /* leave one for null (99 vs. 100) */
  for (p = ioption, q = option, i = 0; i < 99; i++, p++)
    if (*p == '\0') {
      break;
    } else if (*p == ' ' || *p == '\t' || *p == '-' || *p == '_')
      continue;
    else if (isupper(*p))
      *q++ = tolower(*p);
    else
      *q++ = *p;
  *q = '\0';
    
  if (strcmp(option, "quote8bit") == 0) {
    fprintf(stderr, "%s: simple: quoting 8 bit characters\n", myname);
#ifndef LWSRV8
    needquote = TRUE;
#else /* LWSRV8 */
    return(T_NEEDQUOTE);
#endif /* LWSRV8 */
  } else if (strcmp(option, "crtolf") == 0) {
    fprintf(stderr,"%s: simple: translate carrage return to line feed\n",
      myname);
#ifndef LWSRV8
    crtolf = TRUE;
#else /* LWSRV8 */
    return(T_CRTOLF);
#endif /* LWSRV8 */
  } else if (strcmp(option, "makenondscconformant") == 0) {
    fprintf(stderr,"%s: simple: will make documents non DSC conformant\n",
      myname);
#ifndef LWSRV8
    nodsc = TRUE;
#else /* LWSRV8 */
    return(T_NODSC);
#endif /* LWSRV8 */
  } else {
    fprintf(stderr,"%s:simple: unknown Transcript compatibility option: %s\n",
      myname, ioption);
    return(-1);
  }
#ifndef LWSRV8
  return(0);
#endif /* LWSRV8 */
}

#ifdef LWSRV8
export
set_toptions(flg)
int flg;
{
  needquote = (flg & T_NEEDQUOTE);
  crtolf = (flg & T_CRTOLF);
  nodsc = (flg & T_NODSC);
}
#endif /* LWSRV8 */

/*
 * establish tracefile if any
 * establish fontfile name
 * (prtname unused) 
 * establish procset/dictionary directory and scan for dictionaries
 *
 */

export int
#ifndef LWSRV8
spool_setup(itracefile, fontfile, iprtname, idictdir)
char *itracefile;
char *fontfile;
#else /* LWSRV8 */
spool_setup(iprtname, idictdir)
#endif /* LWSRV8 */
char *iprtname;
char *idictdir;
{
#ifndef LWSRV8
  int errs;

  tracefile = itracefile;
#endif /* not LWSRV8 */
  prtname = iprtname;
  dictdir = idictdir;

  if (prtname == NULL)
    prtname = "unknown";
    
#ifndef LWSRV8
  if (fontfile == NULL || !LoadFontList(fontfile)) {
    fprintf(stderr,"lwsrv: simple: Bad FontList\n");
    return(FALSE);
  }
  fprintf(stderr,"lwsrv: simple: Font list from file %s\n",fontfile);
  scandicts(dictdir);			/* scan for dictionary files */
  return(TRUE);
#endif /* not LWSRV8 */

}

/*
 * validate/invalidate a token type for use
 *
*/
private void
validate_token_type(toktype)
int toktype;
{
  register struct atoken *tp;
#ifdef LWSRV8
  register int i;
#endif /* LWSRV8 */

#ifndef LWSRV8
  for (tp = toktbl; tp->token != NULL; tp++)
#else /* LWSRV8 */
  i = sizeof(toktbl) / sizeof(struct atoken);
  for (tp = toktbl; i > 0; tp++, i--)
#endif /* LWSRV8 */
    if ((tp->tokval & TOK_VAL_MASK) == toktype)
      tp->tokval &= (~TOK_INVALID);
}

private void
invalidate_token_type(toktype)
int toktype;
{
  register struct atoken *tp;
#ifdef LWSRV8
  register int i;
#endif /* LWSRV8 */

  /* can't invalidate these */
  if (toktype == TOK_UNK || toktype == TOK_EOF)
    return;
#ifndef LWSRV8
  for (tp = toktbl; tp->token != NULL; tp++)
#else /* LWSRV8 */
  i = sizeof(toktbl) / sizeof(struct atoken);
  for (tp = toktbl; i > 0; tp++, i--)
#endif /* LWSRV8 */
    if ((tp->tokval & TOK_VAL_MASK) == toktype)
      tp->tokval |= TOK_INVALID;
}

/*
 * scan "str" for a token and return the token type.
 * set ptr to the position after the token
 * 
 */

private int
#ifndef LWSRV8
tokval(str,ptr)
#else /* LWSRV8 */
tokval(str, ptr, changeecho)
int *changeecho;
#endif /* LWSRV8 */
char *str, **ptr;
{
  register char *p;
#ifdef LWSRV8
  register int i, low, high, cmp;
#endif /* LWSRV8 */
  register struct atoken *tp;

#ifdef LWSRV8
  *changeecho = ECHO_UNCHANGED;
#endif /* LWSRV8 */
#ifdef PASS_THRU
  /* do we really want to process these tokens? */
#ifndef LWSRV8
  if (simple_pass_thru == TRUE)
#else /* LWSRV8 */
  if (simple_pass_thru)
#endif /* LWSRV8 */
    return(TOK_UNK);
#endif PASS_THRU
  /* all tokens start with "%?" or "%%" or "%!" */
  if (str[0] != '%')
    return(TOK_UNK);
  if (str[1] != '?' && str[1] != '%' && str[1] != '!')
    return(TOK_UNK);

#ifndef LWSRV8
  for (tp = toktbl; tp->token != NULL; tp++) {	/* locate token value */
    if (tp->tokval & TOK_INVALID)
      continue;
    if (strncmp(str,tp->token,tp->toklen) == 0) {
#else /* LWSRV8 */
  /* binary search the token table */
  low = 0;
  high = (sizeof(toktbl) / sizeof(struct atoken)) - 1;
  while (low <= high) {
    tp = toktbl + (i = (low + high) / 2);
    if ((cmp = strncmp(str,tp->token,tp->toklen)) == 0) {
      if (tp->tokval & TOK_INVALID)
        return(TOK_UNK);
#endif /* LWSRV8 */
      p = &str[tp->toklen];
      if (*p == ':')		/* skip ':' */
	p++;
      /* skip leading white space */
      while (*p != '\0' && (*p == ' ' || *p == '\t'))
	p++;
      *ptr = p;				/* set tokstr */
#ifdef LWSRV8
      /* strip trailing white space */
      p += (i = strlen(p)) > 0 ? i - 1 : 0;
      while (p >= *ptr && (*p == ' ' || *p == '\t'))
	*p-- = 0;
      *changeecho = tp->changeecho;
#endif /* LWSRV8 */
      return(tp->tokval);		/* and return the value */
    }
#ifdef LWSRV8
    if (cmp < 0)
      high = i - 1;
    else
      low = i + 1;
#endif /* LWSRV8 */
  }
  return(TOK_UNK);
}

#ifdef LWSRV8
private void
dumpout(str, len, eol)
register byte *str;
register int len;
int eol;
{
  if (tracing || echo)
    dumpoutfile(str, len, eol);
  if (procsetfile)
    dumpprocsetfile(str, len, eol);
  if (qfp)
    dumpquery(str, len, eol);
}

private void
dumpone(c)
register unsigned c;
{
  if (tracing || echo) {
    /* is it standard ascii? */
    if (needquote && (!isascii(c) || (!isprint(c) && !isspace(c))))
      fprintf(outfile, "\\%03o", c);
    else
      putc(c, outfile);
  }
  if (procsetfile)		/* never quote */
    putc(c, procsetfile);
  if (qfp)
    putc(c, qfp);
}

private void
dumpoutfile(str, len, eol)
register byte *str;
register int len;
int eol;
{
  register unsigned c;

  if (needquote) {
    while (len-- > 0) {
      c = *str++;
      /* is it standard ascii? */
      if (!isascii(c) || (!isprint(c) && !isspace(c)))
        fprintf(outfile, "\\%03o", c);
      else
        putc(c, outfile);
    }
  } else {
    fwrite(str, 1, len, outfile);
  }
  if (eol)
    putc(eol, outfile);
}

private void
dumpprocsetfile(str, len, eol)
byte *str;
int len;
int eol;
{
  fwrite(str, 1, len, procsetfile);
  if (eol)
    putc(eol, procsetfile);
}

private void
dumpquery(str, len, eol)
byte *str;
int len;
int eol;
{
  fwrite(str, 1, len, qfp);
  if (eol)
    putc(eol, qfp);
}

private int
nextline(pf)
register PFILE *pf;
{
  register int c;

  while ((c = p_getc(pf)) != EOF) {
    dumpone(c);
    if (c == '\r' || c == '\n')
      return(c);
  }
  return(c);
}

#else /* LWSRV8 */

private void
dumpout(str,len)
char *str;
int len;
{
  int i,c;

  for (i=0; i < len; i++) {
    c = str[i];
    if (crtolf && c == '\r')
      c = '\n';
    if (needquote) {
      if (isascii(c) && (isspace(c) || !iscntrl(c))) /* is it standard ascii? */
	putc(c,outfile);	/* yes... so echo */
      else		/* otherwise echo \ddd */
	fprintf(outfile,"\\%03o",(unsigned char) c);
    } else
      putc(c,outfile);
    if (procsetfile)		/* never quote */
      putc(c, procsetfile);
  }
  putc('\n',outfile);
  if (procsetfile)
    putc('\n', procsetfile);
}
#endif /* LWSRV8 */

/*
 * int scantoken(PFILE *pf,int echo,char *tokstr)	[lwsrv]
 * int scantoken(PFILE *pf, char *tokstr)		[lwsrv8]
 *
 * Read characters from the PAP connection specified by pf.
 * Echo characters to stdout if echo is TRUE.  Return token value
 * and tokstr pointing past token characters.
 *
 * We assume all tokens begin with "%"
 *
 */

#ifdef LWSRV8
private int
scantoken(pf, tptr)
register PFILE *pf;
char **tptr;
{
  register byte *cp;
  register int i, c, tv;
  int changeecho;
#ifdef LBUFFER
  byte lbuffer[LBUFFER];
#endif LBUFFER;
#ifdef PAGECOUNT
  extern int pcopies;
#endif PAGECOUNT

  c = !EOF;
  while (c != EOF) {
    switch(c = p_getc(pf)) {
     case EOF:
      break;
     case '%':
      cp = tokstr;
      *cp++ = '%';
      toklen = 0;
      for ( ; ; ) {
        /*
         * Either the line is too long or there is a non-ascii character,
         * both mean it can't be a real DSC comment line, so flush.
         */
	if (++toklen >= MAXTOKSTR || !isascii(c)) {
	  dumpout(tokstr, toklen, 0);
	  c = nextline(pf);
	  break;
	}
	if ((c = p_getc(pf)) == EOF) {
	  dumpout(tokstr, toklen, 0);
	  break;
	}
	if (c == '\r' || c == '\n') {	/* end of line */
	  *cp = 0;
	  tv = tokval(tokstr, tptr, &changeecho);
	  tokeol = crtolf ? '\n' : c;
	  if (changeecho == ECHO_OFF)
	    echo = FALSE;
	  if (changeecho != ECHO_DROP)
	    dumpout(tokstr, toklen, tokeol);
	  if (changeecho == ECHO_ON)
	    echo = TRUE;
	  if (tv != TOK_UNK)
	    return(tv);
	  break;
	}
	*cp++ = c;
      }
      break;
     case '\r':
     case '\n':
      dumpone(c);
      break;
     default:
#ifdef PAGECOUNT
      cp = lbuffer;
      i = 0;
      tokeol = 0;
      for ( ; ; ) {
	if (c != '\r' && c != '\n') {
	  *cp++ = c;
	  i++;
	} else
	  tokeol = crtolf ? '\n' : c;
	if (i >= (LBUFFER - 1) || tokeol) {
	  *cp = 0;
	  if (strncmp(lbuffer, "userdict /#copies ", 18) == 0)
	    pcopies = atoi(&lbuffer[18]);
	  dumpout(lbuffer, i, tokeol);
	  if (!tokeol)
	    c = nextline(pf);
	  break;
        }
	if ((c = p_getc(pf)) == EOF) {
	  dumpout(lbuffer, i, 0);
	  break;
	}
      }
#else PAGECOUNT
      dumpone(c);
      c = nextline(pf);
#endif PAGECOUNT
      break;
    }
  }

  /* found EOF */
  return(TOK_EOF);			/* return now */
}

#else  /* LWSRV8 */

#define MAXTOKSTR 1024

private int
scantoken(pf,echo,tptr)
PFILE *pf;
int echo;
char **tptr;
{
  static char tokstr[MAXTOKSTR];
  int atstart,i,c,tv,maybetoken;
#ifdef LBUFFER
  static char lbuffer[LBUFFER];
  int l;
#endif LBUFFER;
#ifdef PAGECOUNT
  extern int pcopies;
#endif PAGECOUNT

  i = 0;
  atstart = TRUE;
  maybetoken = FALSE;
#ifdef LBUFFER
  l = 0;
#endif LBUFFER;
  while ((c = p_getc(pf)) != EOF) {
    if (c == '%' && atstart)
      maybetoken = TRUE;
#ifdef DONT_PARSE
    if (dont_parse)
      maybetoken = FALSE;
#endif DONT_PARSE
    atstart = (c == '\r' || c == '\n') ? TRUE : FALSE;
    if (maybetoken) {
      if (atstart) {		/* last char is cr or lf */
	tokstr[i] = '\0';	/* tie off token */
	if ((tv = tokval(tokstr,tptr)) != TOK_UNK) {
	  if (tracefile != NULL)
	    dumpout(tokstr,i);
	  return(tv);
	} else {
	  if ((tracefile != NULL) || echo)
	    dumpout(tokstr, i);
	  if (procsetfile != NULL)
	    fprintf(procsetfile, "%s%c", tokstr, crtolf ? c : '\n');
	}
	i = 0;
	maybetoken = FALSE;
	continue;		/* skip everything else */
      }
      if (i < MAXTOKSTR)
	tokstr[i++] = c;
      continue;
    }
    /* papif handles it right, but others like transcript don't, so... */
    if ((tracefile != NULL) || echo) { /* do we want to echo this? */
      /* papif handles it right, but others like transcript don't, so... */
      /* make it a compilable option */
      if (crtolf && c == '\r')
	c = '\n';
      if (needquote) {
        if (isascii(c) && (isspace(c) || !iscntrl(c))) { /* standard ascii? */
	  putc(c,outfile);	/* yes... so echo */
#ifdef LBUFFER
	  if (l < LBUFFER)
	    lbuffer[l++] = c;
#endif LBUFFER
	} else {		/* otherwise echo \ddd */
	  fprintf(outfile,"\\%03o",(unsigned char) c);
#ifdef LBUFFER
	  if (l < LBUFFER)
	    lbuffer[l++] = '*';
#endif LBUFFER
	}
      } else {
	putc(c,outfile);
#ifdef LBUFFER
	if (l < LBUFFER)
	  lbuffer[l++] = c;
#endif LBUFFER
      }
    }
    if (crtolf && c == '\r')
      c = '\n';
    if (procsetfile)
      putc(c, procsetfile);
#ifdef LBUFFER
    if (atstart) {
#ifdef PAGECOUNT
      if (strncmp(lbuffer, "userdict /#copies ", 18) == 0)
	pcopies = atoi(&lbuffer[18]);
#endif PAGECOUNT
      l = 0;
    }
#endif LBUFFER
  }

  /* found EOF */
  
  if ((tracefile != NULL) || echo)
    putc('\n',outfile);			/* yes... echo eol */
  if (procsetfile)
    putc('\n', procsetfile);
  return(TOK_EOF);			/* return now */
}
#endif /* LWSRV8 */

#ifdef LWSRV8
/*
 * Remove leading left and trailing right parenthesis
 * (now part of LaserWriter 8.0 %%For: and %%Title: fields).
 *
 */

private char *
unparen(str)
char *str;
{
  register char *cp;

  if (*str != '(' || *(cp = str + strlen(str) - 1) != ')')
    return(str);
  *cp = 0;
  return(str + 1);
}
#endif /* LWSRV8 */

/*
 * handle an incoming job
 *
 */

#ifdef LWSRV8
int
getjob(pf,of,actualprintout,doeof)
PFILE *pf;
FILE *of;
int *actualprintout,*doeof;
{
  char *ts,querybuf[MAXTOKSTR];
  char procname[256];
  int ltokval,tokval;
  DictList *dl = NULL;		/* current dictionary */
  DictList *dlnew = NULL;	/* new dictionary */
  char buf[256];
  register void *vp;
#ifdef PROCSET_PATCH
  int patchprocset = FALSE;
#endif PROCSET_PATCH
  char *resp, *queryp;
  char prefix[80], status[256];
  extern int pagecount;
#ifdef PAGECOUNT
  extern int pcopies;
#endif PAGECOUNT
  
  outfile = of;
  p_clreof(pf);				/* clear EOF indicator */
  echo = TRUE;
  *doeof = TRUE;
  if (nodsc) {
    fprintf(outfile, "%%!\n%% PostScript Document, but non-conformant\n");
    fprintf(outfile, "%%  so psrv is not invoked\n");
  }
  sprintf(status,"receiving job for %s", prtname);

  clearstack();
  while ((tokval = scantoken(pf, &ts)) != TOK_EOF) {
    switch (tokval) {
    case TOK_DROP:
      break;
    case TOK_BPS:
      /* really smart would install into dict */
      /* remember to flush off "appledict" part */
      push(tokval);			/* remember where */
      stripspaces(ts);			/* clear off extra spaces */
      strcpy(procname, ts);
      if ((dl = GetProcSet(procname)) != NULL) {
	fprintf(stderr, "%s: simple: procset %s already known\n", myname,
	 procname);
	break;
      }
      if (!capture) {
#ifdef PROCSET_PATCH
	if (!patchprocset) {
	  echo = TRUE;
	  if (!tracing)
	    dumpoutfile(tokstr, toklen, tokeol);
	}
	patchprocset = FALSE;
#else  PROCSET_PATCH
	echo = TRUE; /* pass unknown procsets through */
	if (!tracing)
	  dumpoutfile(tokstr, toklen, tokeol);
#endif PROCSET_PATCH
	break;
      }
      sleep(1); /* raise odds of unique timestamp */
#ifdef xenix5
      sprintf(tmpstr, "%s/Found.%d",dictdir,time(0));
#else xenix5
      sprintf(tmpstr, "%s/FoundProcSet.%d",dictdir,time(0));
#endif xenix5
      fprintf(stderr, "%s: simple: Saving to %s BeginProcSet: %s\n",
	      myname, tmpstr, procname);
      if (procsetfile != NULL) {
	fprintf(stderr, "%s: simple: Already logging prep file!", myname);
	break;
      }
      if ((procsetfile = fopen(tmpstr, "w+")) == NULL)
	perror(tmpstr);
      else {
	dlnew = dl = CreateDict();
	dl->ad_fn = strdup(tmpstr+strlen(dictdir)+1);
	dl->ad_sent = FALSE;
	dumpprocsetfile(tokstr, toklen, tokeol);
      }
      /* copy from BPS to end into file or memory */
      /* tag given is new proc set */
      break;
    case TOK_EPS:
      if (pop() != TOK_BPS) {
	fprintf(stderr,"%s: simple: Framing error on Begin/End Proc Set\n",
	 myname);
      }
      if (procsetfile)			/* if remember file */
	fclose(procsetfile);		/* close outfile */
      procsetfile = NULL;
      if (dlnew) {
	newdictionary(strdup(procname), dlnew);
	dlnew = NULL;		/* close off */
      }
      if (dl && !dl->ad_sent) {
	fprintf(stderr,
		((adobe_dsc2_conformant) ?
		 "%s: simple: dsc2: document supplied procset %s = %s\n" :
		 "%s: simple: non-dsc2: Including ProcSet %s = %s\n"),
		myname, procname,dl->ad_fn);
	fprintf(outfile,"%% ** Procset From File: %s **\n",dl->ad_fn);
	ListProcSet(dl->ad_fn, dictdir, outfile, 0);
	fprintf(outfile,"%% ** End of ProcSet **\n");
	/* only suppress second download of a procset if an AppleDict */
	if (strncmp("\"(AppleDict md)\"", procname, 16) == 0)
	  dl->ad_sent = TRUE;
      }
      dl = NULL;		/* Close off */
      break;
    case TOK_BEGIN:
      push(tokval);
      break;
    case TOK_END:
      if (pop() != TOK_BEGIN)
	fprintf(stderr, "%s: simple: Framing error on TOK_BEGIN/END\n", myname);
      break;
    case TOK_BEGINR:
      push(tokval);			/* remember last token value */
      break;
    case TOK_ENDR:
      if (pop() != TOK_BEGINR)
       fprintf(stderr,"%s: simple: Stack Frame error on TOK_BEGINR/ENDR\n",
	myname);
      strcat(ts, newline);
      p_write(pf,ts,strlen(ts),FALSE);
      break;
#ifdef PROCSET_PATCH
    case TOK_PATCH:
      patchprocset = TRUE;		/* also skip next procset */
      break;
#endif PROCSET_PATCH
    case TOK_PSA:
     {
      register int i;

      i = strlen(ts) - QUERYSTRLEN;
      query3 = FALSE;
      if (*actualprintout < 0)
	*actualprintout = 0;
      /*
       * If we have !PS-Adobe x.x Query, then leave echo off and see if it
       * is level 3 or higher.  If not a query, turn on echo and output
       * the line.
       */
      if (i > 0 && strcmp(ts + i, querystr) == 0)	/* Query */
        query3 = (atoi(ts) >= 3);
      else {
	echo = TRUE;
	if (!tracing)
	  dumpoutfile(tokstr, toklen, tokeol);
      }
      break;
     }
    case TOK_TEOF:
      *doeof = echo;
      query3 = FALSE;
      echo = TRUE;
      break;
    case TOK_BFOQ:			/* BeginFontQuery */
      strcpy(querybuf,ts);		/*  remember arguments */
      /* drop through */
    case TOK_BFLQ:			/* BeginFontListQuery */
      if (query3 && thequery) {
	if ((vp = SearchKVTree(thequery, "font", strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      strcpy(querybuf,ts);		/*  remember arguments */
      break;
    case TOK_BFEQ:			/* BeginFeatureQuery */
      if (query3 && thequery) {
	strcpy(buf, "FeatureQuery\t");
	strcat(buf, ts);
	if ((vp = SearchKVTree(thequery, buf, strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      break;
    case TOK_BVQ:			/* BeginVMStatusQuery */
      if (query3 && thequery) {
	strcpy(buf, "VMStatusQuery\t");
	strcat(buf, ts);
	if ((vp = SearchKVTree(thequery, buf, strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      break;
    case TOK_BPRQ:			/* BeginPrinterQuery */
      if (query3 && thequery) {
	strcpy(buf, "PrinterQuery\t");
	strcat(buf, ts);
	if ((vp = SearchKVTree(thequery, buf, strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      break;
    case TOK_BQU:			/* BeginQuery */
      if (query3 && thequery) {
	strcpy(buf, "Query\t");
	strcat(buf, ts);
	if ((vp = SearchKVTree(thequery, buf, strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      break;
    case TOK_BFIQ:			/* BeginFileQuery */
      if (query3 && thequery) {
	if ((vp = SearchKVTree(thequery, "file", strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      strcpy(querybuf,ts);		/*  remember arguments */
      break;
    case TOK_BREQ:			/* BeginResourceQuery */
      if (query3 && thequery) {
        strcpy(querybuf, ts);
	queryp = querybuf;
	if (resp = nextoken(&queryp)) {
	  if ((vp = SearchKVTree(thequery, resp, strcmp)) == NULL)
	    logquery();
	} else
	  vp = NULL;
      } else
        vp = NULL;
      push(tokval);
      break;
    case TOK_BRLQ:			/* BeginResourceListQuery */
      if (query3 && thequery) {
	if ((vp = SearchKVTree(thequery, ts, strcmp)) == NULL)
	  logquery();
      } else
        vp = NULL;
      push(tokval);
      strcpy(querybuf,ts);		/*  remember arguments */
      break;
    case TOK_BPSQ:			/* BeginProcSetQuery */
    case TOK_ADV:			/* appledict version */
      strcpy(querybuf,ts);		/*  remember arguments */
      /* drop through */
    case TOK_FLS:			/* fontList */
      push(tokval);
      break;
    case TOK_FOR:			/* For: */
      setusername(unparen(ts));
      break;
    case TOK_TIT:			/* Title: */
      setjobname(unparen(ts));
      break;
    case TOK_ENC:			/* EndComments */
      break;
    case TOK_IPS:
      if (!adobe_dsc2_conformant)
	break;
      stripspaces(ts);		/* strip extra spaces */
      if ((dl = GetProcSet(ts)) != NULL) {
	fprintf(stderr,"%s: simple: Including ProcSet %s = %s\n",
		myname,ts,dl->ad_fn);
	fprintf(outfile,"%% ** Include Procset From File: %s **\n",dl->ad_fn);
	ListProcSet(dl->ad_fn, dictdir, outfile, 1);
	fprintf(outfile,"%% ** End of ProcSet **\n");
	dl = NULL;		/* close off */
      } else
	fprintf(stderr,"%s: simple: Unknown ProcSet file for '%s'\n",myname,ts);
      break;
    case TOK_PAGE:			/* Page: */
      queryp = ts;
      if (nextoken(&queryp)) {
	if (resp = nextoken(&queryp)) {	/* got 2nd token */
	  if (pagecount > 0)
	    sprintf(status,"receiving page %s of %d for %s",
	     resp, pagecount, prtname);
	  else
	    sprintf(status,"receiving page %s for %s", resp, prtname);
	} else {			/* no 2nd token, so use 1st */
	  resp = nextoken(&ts);
	  if (pagecount > 0)
	    sprintf(status,"receiving page %s of %d for %s",
	     resp, pagecount, prtname);
	  else
	    sprintf(status,"receiving page %s for %s", resp, prtname);
	}
      } else	/* no page count */
	sprintf(status,"receiving a page for %s", prtname);
      NewStatus(status);		/* update status */
      *actualprintout = 1;
      break;
    case TOK_PGS:			/* Pages: */
      if (isdigit(*ts))
	pagecount = atoi(ts);
      break;
    case TOK_EPSQ:			/* EndProcSetQuery */
    case TOK_EFLQ:			/* EndFontListQuery */
    case TOK_EFOQ:			/* EndFontQuery */
    case TOK_EFEQ:			/* EndFeatureQuery */
    case TOK_EVQ:			/* EndVMStatusQuery */
    case TOK_EPRQ:			/* EndPrinterQuery */
    case TOK_EQU:			/* EndQuery */
    case TOK_EFIQ:			/* EndFileQuery */
    case TOK_EREQ:			/* EndResourceQuery */
    case TOK_ERLQ:			/* EndResourceListQuery */
    case TOK_ENDOLD:			/* End */
      if (qfp) {
        fclose(qfp);
        qfp = NULL;
      }
      ltokval = pop();
      switch (ltokval) {		/* according to last found token */
      case TOK_BPSQ:
      case TOK_ADV:			/* if last token was AppleDict */
	SendVAck(pf,querybuf);		/*  then handle appledict */
	break;
      case TOK_BFLQ:
	if (vp) {
	  SendResourceList(pf, (List *)vp, "/");
	  break;
	}
	/* drop through for default action */
      case TOK_FLS:			/* if last token was FontList */
	SendFontList(pf);		/*  then send out the fontlist  */
	break;
      case TOK_BFOQ:
	if (vp) {
	  SendMatchedResources(pf, (List *)vp, "/", querybuf);
	  break;
	}
        strcat(ts, newline);
	p_write(pf,ts,strlen(ts),FALSE);
	break;
      case TOK_BFEQ:
      case TOK_BVQ:
      case TOK_BPRQ:
      case TOK_BQU:
	if (vp) {
	  SendQueryResponse(pf, (List *)vp);
	  break;
	}
        strcat(ts, newline);
	p_write(pf,ts,strlen(ts),FALSE);
	break;
      case TOK_BFIQ:
	if (vp) {
	  SendMatchedResources(pf, (List *)vp, NULL, querybuf);
	  break;
	}
        strcat(ts, newline);
	p_write(pf,ts,strlen(ts),FALSE);
	break;
      case TOK_BREQ:
	if (vp) {
	  strcpy(prefix, resp);
	  if (islower(*prefix))
	    *prefix = toupper(*prefix);
	  strcat(prefix, " ");
	  if (strcmp(resp, "procset") == 0)
	    SendMatchedKVTree(pf, (KVTree **)vp, prefix, queryp);
	  else {
	    if (strcmp(resp, "font") == 0)
	      strcat(prefix, "/");
	    SendMatchedResources(pf, (List *)vp, prefix, queryp);
	  }
	  break;
	}
        strcat(ts, newline);
	p_write(pf,ts,strlen(ts),FALSE);
	break;
      case TOK_BRLQ:
	if (vp) {
	  strcpy(prefix, querybuf);
	  if (islower(*prefix))
	    *prefix = toupper(*prefix);
	  strcat(prefix, " ");
	  if (strcmp(querybuf, "procset") == 0)
	    SendResourceKVTree(pf, (KVTree **)vp, prefix);
	  else {
	    if (strcmp(querybuf, "font") == 0)
	      strcat(prefix, "/");
	    SendResourceList(pf, (List *)vp, prefix);
	  }
	  break;
	}
        strcat(ts, newline);
	p_write(pf,ts,strlen(ts),FALSE);
	break;
      default:
	fprintf(stderr, "%s: simple: Stack framing error: got %d\n",
		myname, ltokval);
	break;
      }
      break;
    }
  }
  if (!isstackempty()) {
    fprintf(stderr, "%s: simple: EOF and tokens still on stack!\n", myname);
    dumpstack();
  }
  if (procsetfile) {
    fclose(procsetfile);
    procsetfile = NULL;
    if (dlnew) {
      unlink(dlnew->ad_fn);
      free(dlnew->ad_fn);	/* free fn */
      free(dlnew);
    }
  }
  if (p_isopn(pf))			/* if connection is still open */
    p_write(pf,"",0,TRUE);		/* send EOF for this job */
  return(p_isopn(pf));			/* return open state */
}

private void
logquery()
{
  if (!queryfile)
    return;
  if (qfp = fopen(queryfile, "a"))
    dumpquery(tokstr, toklen, tokeol);
}

#else  /* LWSRV8 */

int
getjob(pf,of)
PFILE *pf;
FILE *of;
{
  char *ts,adictver[80];
  int ltokval,tokval,echo;
  DictList *dl = NULL;		/* current dictionary */
  DictList *dlnew = NULL;	/* new dictionary */
  extern int capture;
#ifdef PAGECOUNT
  extern int pagecount;
  extern int pcopies;
#endif PAGECOUNT
#ifdef PROCSET_PATCH
  int patchprocset = FALSE;
#endif PROCSET_PATCH
  
  outfile = of;
  p_clreof(pf);				/* clear EOF indicator */
  echo = TRUE;
  if (nodsc) {
    fprintf(outfile, "%%!\n%% PostScript Document, but non-conformant\n");
    fprintf(outfile, "%%  so psrv is not invoked\n");
  }

  clearstack();
  while ((tokval = scantoken(pf,echo,&ts)) != TOK_EOF) {
    switch (tokval) {
    case TOK_DROP:
      break;
    case TOK_BPS:
      /* really smart would install into dict */
      /* remember to flush off "appledict" part */
      push(tokval);			/* remember where */
      echo = FALSE;
      stripspaces(ts);			/* clear off extra spaces */
      if ((dl = GetProcSet(ts)) != NULL) {
	fprintf(stderr, "lwsrv: simple: procset %s already known\n", ts);
	break;
      }
      if (!capture) {
#ifdef PROCSET_PATCH
	if (!patchprocset)
	  echo = TRUE;
	patchprocset = FALSE;
#else  PROCSET_PATCH
	echo = TRUE; /* pass unknown procsets through */
#endif PROCSET_PATCH
	break;
      }
      sleep(1); /* raise odds of unique timestamp */
#ifdef xenix5
      sprintf(tmpstr, "%s/Found.%d",dictdir,time(0));
#else xenix5
      sprintf(tmpstr, "%s/FoundProcSet.%d",dictdir,time(0));
#endif xenix5
      fprintf(stderr, "lwsrv: simple: Saving to %s BeginProcSet: %s\n",
	      tmpstr, ts);
      if (procsetfile != NULL) {
	fprintf(stderr, "lwsrv: simple: Already logging prep file!");
	break;
      }
      if ((procsetfile = fopen(tmpstr, "w+")) == NULL) {
	procsetfile = NULL;
	perror(tmpstr);
      } else {
	dlnew = dl = (DictList *)malloc(sizeof(DictList));
	dl->ad_ver = strdup(ts);
	dl->ad_fn = strdup(tmpstr+strlen(dictdir)+1);
	dl->ad_next = NULL;
	dl->ad_sent = FALSE;
	fprintf(procsetfile, "%%%%BeginProcSet: %s\n", ts);
      }
      /* copy from BPS to end into file or memory */
      /* tag given is new proc set */
      break;
    case TOK_EPS:
      if (pop() != TOK_BPS) {
	fprintf(stderr,"lwsrv: simple: Framing error on Begin/End Proc Set\n");
      }
      if (procsetfile) {			/* if remember file */
	fprintf(procsetfile, "%%%%EndProcSet\n");
	fclose(procsetfile);		/* close outfile */
      }
      procsetfile = NULL;
      echo = TRUE;
      if (dlnew) {
	newdictionary(dlnew);
	dlnew = NULL;		/* close off */
      }
      if (dl && !dl->ad_sent) {
	fprintf(stderr,
		((adobe_dsc2_conformant) ?
		 "lwsrv: simple: dsc2: document supplied procset %s = %s\n" :
		 "lwsrv: simple: non-dsc2: Including ProcSet %s = %s\n"),
		dl->ad_ver,dl->ad_fn);
	fprintf(outfile,"%% ** Procset From File: %s **\n",dl->ad_fn);
	ListProcSet(dl->ad_fn, dictdir, outfile, 0);
	fprintf(outfile,"%% ** End of ProcSet **\n");
	/* only suppress second download of a procset if an AppleDict */
	if (strncmp("\"(AppleDict md)\"", dl->ad_ver, 16) == 0)
	  dl->ad_sent = TRUE;
      }
      dl = NULL;		/* Close off */
      break;
    case TOK_BEGIN:
      push(tokval);
      echo = FALSE;
      break;
    case TOK_END:
      if (pop() != TOK_BEGIN)
	fprintf(stderr, "lwsrv: simple: Framing error on TOK_BEGIN/END\n");
      echo = TRUE;
      break;
    case TOK_BEGINR:
      push(tokval);			/* remember last token value */
      echo = FALSE;
      break;
    case TOK_ENDR:
      if (pop() != TOK_BEGINR)
       fprintf(stderr,"lwsrv: simple: Stack Frame error on TOK_BEGINR/ENDR\n");
      echo = TRUE;			/* we are now echoing */
      p_write(pf,ts,strlen(ts),FALSE);
      break;
#ifdef PROCSET_PATCH
    case TOK_PATCH:
      echo = FALSE;			/* until EOF or valid section */
      patchprocset = TRUE;		/* also skip next procset */
      break;
#endif PROCSET_PATCH
    case TOK_BPSQ:
    case TOK_BFLQ:
    case TOK_FLS:			/* fontList */
    case TOK_ADV:			/* appledict version */
      push(tokval);
      if (tracefile == NULL)		/* unless tracing there is no */
	echo = FALSE;			/*  echoing between token and End */
      if (tokval == TOK_ADV ||		/* is this appledict query? */
	  tokval == TOK_BPSQ)
	strcpy(adictver,ts);		/*  yes, remember appledict version */
      break;
    case TOK_FOR:			/* For: */
      setusername(ts);
      break;
    case TOK_TIT:			/* Title: */
      setjobname(ts);
      break;
    case TOK_ENC:			/* EndComments */
      break;
    case TOK_IPS:
      if (!adobe_dsc2_conformant)
	break;
      stripspaces(ts);		/* strip extra spaces */
      if ((dl = GetProcSet(ts)) != NULL) {
	fprintf(stderr,"lwsrv: simple: Including ProcSet %s = %s\n",
		dl->ad_ver,dl->ad_fn);
	fprintf(outfile,"%% ** Include Procset From File: %s **\n",dl->ad_fn);
	ListProcSet(dl->ad_fn, dictdir, outfile, 1);
	fprintf(outfile,"%% ** End of ProcSet **\n");
	dl = NULL;		/* close off */
      } else
	fprintf(stderr,"lwsrv: simple: Unknown ProcSet file for '%s'\n",ts);
      break;
#ifdef PAGECOUNT
    case TOK_PGS:			/* Pages: */
      if (isdigit(*ts))
	pagecount = atoi(ts);
      break;
#endif PAGECOUNT
    case TOK_EFLQ:
    case TOK_EPSQ:
    case TOK_ENDOLD:			/* End */
      echo = TRUE;			/* we are now echoing */
      ltokval = pop();
      switch (ltokval) {		/* according to last found token */
      case TOK_BPSQ:
      case TOK_ADV:			/* if last token was AppleDict */
	SendVAck(pf,adictver);		/*  then handle appledict */
	break;
      case TOK_BFLQ:
      case TOK_FLS:			/* if last token was FontList */
	SendFontList(pf);		/*  then send out the fontlist  */
	break;
      default:
	fprintf(stderr, "lwsrv: simple: Stack framing error: got %d\n",
		ltokval);
	break;
      }
    }
  }
  if (!isstackempty()) {
    fprintf(stderr, "lwsrv: simple: EOF and tokens still on stack!\n");
    dumpstack();
  }
  if (procsetfile) {
    fclose(procsetfile);
    procsetfile = NULL;
    if (dlnew) {
      unlink(dlnew->ad_fn);
      free(dlnew->ad_fn);	/* free fn */
      free(dlnew);
    }
  }
  if (p_isopn(pf))			/* if connection is still open */
    p_write(pf,"",0,TRUE);		/* send EOF for this job */
  return(p_isopn(pf));			/* return open state */
}
#endif /* LWSRV8 */

#define MD_UNK		"0\n"		/* Mac dictionary not loaded */
#define MD_AVOK		"1\n"		/* AppleDict version ok (av) */
#define MD_AVBAD	"2\n"		/* AppleDict version not ok */

#ifdef LWSRV8
private char procset7[] = "\"(AppleDict md)\" 71";
#endif /* LWSRV8 */

/*
 * answer a query as to whether a particular procset is known or not
 *
 */

#ifdef LWSRV8
private void
SendVAck(pf,ver)
PFILE *pf;
char *ver;
{
  DictList *dl;
  char status[80];

  stripspaces(ver);			/* strip any extra spaces */

  if (!capture && strncmp(ver, procset7, sizeof(procset7) - 1) == 0) {
    p_write(pf,MD_AVOK,2,FALSE);	/* respond yes to system 7 */
    fprintf(stderr,"%s: simple: Responding Yes to ProcSet %s\n", myname, ver);
    return;
  }

  dl = GetProcSet(ver);			/* find any procset */

  if (tracing || dl == (DictList *)NULL) {
    p_write(pf,MD_UNK,2,FALSE);		/* if tracing download the dict */
    fprintf(stderr,"%s: simple: Receiving AppleDict version %s\n", myname, ver);
    if (tracing)
      fprintf(outfile,"%% %s: simple: Receiving AppleDict version %s\n",
       myname, ver);
    /* won't do much good unless single fork */
    sprintf(status,"Receiving AppleDict version #%s",ver);
    NewStatus(status);
    return;			/* by pass the prepend */
  } else
    p_write(pf,MD_AVOK,2,FALSE);	/* otherwise we use local file */

  if (!adobe_dsc2_conformant) {
    if (dl != (DictList *) NULL) {
      if (!tracing) {
	/* won't do much good unless single fork */
	sprintf(status,"prepending AppleDict version #%s",ver);
	NewStatus(status);
	fprintf(stderr,"%s: simple: Using ProcSet %s = %s\n",
	 myname,ver,dl->ad_fn);
	fprintf(outfile,"%% ** Prepending ProcSet from: %s **\n",dl->ad_fn);
	ListProcSet(dl->ad_fn, dictdir, outfile, 1); /* prepend appledict */
	fprintf(outfile,"%% ** End of ProcSet **\n");
	dl->ad_sent = TRUE;
      }
    } else {
      fprintf(stderr,"%s: simple: Unknown ProcSet file for '%s'\n",myname,ver);
    }
  }
}

tracewrite(str, len)
char *str;
int len;
{
  if (len > 0) {
    fprintf(outfile, "--%s=>", myname);
    fwrite(str, 1, len, outfile);
  } else
    fprintf(outfile, "--%s=|EOF|\n", myname);
}

#else  /* LWSRV8 */

private void
SendVAck(pf,ver)
PFILE *pf;
char *ver;
{
  DictList *dl;
  char status[80];

  stripspaces(ver);			/* strip any extra spaces */

  dl = GetProcSet(ver);			/* find any procset */

  if (tracefile != NULL || dl == (DictList *)NULL) { 
    p_write(pf,MD_UNK,2,FALSE);		/* if tracing download the dict */
    fprintf(stderr,"lwsrv: simple: Receiving AppleDict version %s\n",ver);
    /* won't do much good unless single fork */
    sprintf(status,"Receiving AppleDict version #%s",ver);
    NewStatus(status);
    return;			/* by pass the prepend */
  } else
    p_write(pf,MD_AVOK,2,FALSE);	/* otherwise we use local file */

  if (!adobe_dsc2_conformant) {
    if (dl != (DictList *) NULL) {
      if (tracefile == NULL) {
	/* won't do much good unless single fork */
	sprintf(status,"prepending AppleDict version #%s",ver);
	NewStatus(status);
	fprintf(stderr,"lwsrv: simple: Using ProcSet %s = %s\n",ver,dl->ad_fn);
	fprintf(outfile,"%% ** Prepending ProcSet from: %s **\n",dl->ad_fn);
	ListProcSet(dl->ad_fn, dictdir, outfile, 1); /* prepend appledict */
	fprintf(outfile,"%% ** End of ProcSet **\n");
	dl->ad_sent = TRUE;
      }
    } else {
      fprintf(stderr,"lwsrv: simple: Unknown ProcSet file for '%s'\n",ver);
    }
  }
}
#endif /* LWSRV8 */
