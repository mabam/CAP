static char rcsid[] = "$Author: djh $ $Date: 1995/08/30 08:13:25 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/fontlist.c,v 2.3 1995/08/30 08:13:25 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * fontlist - UNIX AppleTalk spooling program: act as a laserwriter
 *   handles simple font list - assumes we can place in a file
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *  Created Sept 5, 1987 by cck from lwsrv.
 *
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#include <netat/compat.h>

#ifdef USESTRINGDOTH
# include <string.h>
#else  /* USESTRINGDOTH */
# include <strings.h>
#endif /* USESTRINGDOTH */

#ifdef LWSRV8
#include "list.h"
#include "query.h"
#include "parse.h"
#endif /* LWSRV8 */

#include "fontlist.h"
#include "spmisc.h"
#include "papstream.h"

#ifndef LWSRV8
FontList *fontlist = (FontList *)NULL;	/* fontlist header */
#endif /* not LWSRV8 */

#define FBMAX 100
extern char *myname;

#ifndef LWSRV8
boolean
#else /* LWSRV8 */
List *
#endif /* LWSRV8 */
LoadFontList(fn)
char *fn;
{
  char fb[FBMAX];
#ifdef LWSRV8
  register List *lp;
  register FILE *ff;
  register char *cp;
#else  /* LWSRV8 */
  FILE *ff;
  FontList *fp = NULL;
  
  if (fontlist)			/* already loaded */
    return(TRUE);
#endif /* LWSRV8 */

  if ((ff = fopen(fn,"r")) == NULL)
#ifndef LWSRV8
    return(FALSE);
#else /* LWSRV8 */
    return(NULL);
  lp = CreateList();
#endif /* LWSRV8 */
  while (fgets(fb,FBMAX,ff) != NULL)
    if (fb[0] != '%' &&	fb[0] != '\n') {
#ifndef LWSRV8
      fp = (FontList *) malloc(sizeof(FontList));
      fp->fl_name = (char *) malloc(strlen(fb)+1);
      strcpy(fp->fl_name,fb);
      fp->fl_next = fontlist;
      fontlist = fp;
#else /* LWSRV8 */
      if (cp = (char *)index(fb, '\n'))
	*cp = '\0';
      AddToList(lp, strdup(fb));
#endif /* LWSRV8 */
    }
  fclose(ff);
#ifndef LWSRV8
  return(TRUE);
#else /* LWSRV8 */
  SortList(lp, StringSortItemCmp);
  return(lp);
#endif /* LWSRV8 */
}

#ifdef LWSRV8
private char newline[] = "\n";
private char star[] = "*\n";
#endif /* LWSRV8 */
  
void
SendFontList(pf)
PFILE *pf;
{
#ifndef LWSRV8
  int i = 0;
  FontList *fl = fontlist;
  char status[255];
#else /* LWSRV8 */
  register int i;
  register char **ip;
  register char *cp;
  register List *fp;
  char buf[256];
#endif /* LWSRV8 */

  /* won't do much good unless single fork */
  NewStatus("initializing fonts");
#ifndef LWSRV8
  while (fl != NULL) {
    i++; 
    p_write(pf,fl->fl_name,strlen(fl->fl_name),FALSE);
    fl = fl->fl_next;
  }
#else /* LWSRV8 */
  if (fp = (List *)SearchKVTree(thequery, "font", strcmp)) {
    cp = buf;
    for (ip = (char **)AddrList(fp), i = NList(fp); i > 0; ip++, i--) {
      strcpy(cp, *ip);
      strcat(cp, newline);
      p_write(pf,buf,strlen(buf),FALSE);
    }
    fprintf(stderr,"%s: Sending fontList: %d entries\n", myname, NList(fp));
  }
#endif /* LWSRV8 */
#ifndef LWSRV8
  p_write(pf,"*\n",strlen("*\n"),FALSE);
  fprintf(stderr,"lwsrv: Sending fontList: %d entries\n",i);
  sprintf(status,"receiving job");
  NewStatus(status);
#else /* LWSRV8 */
  p_write(pf,star,strlen(star),FALSE);
  NewStatus("receiving job");
#endif /* LWSRV8 */
}
