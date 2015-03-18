static char rcsid[] = "$Author: djh $ $Date: 1995/08/30 08:13:25 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/query.c,v 2.2 1995/08/30 08:13:25 djh Rel djh $";
static char revision[] = "$Revision: 2.2 $";

/*
 * query.c -  handle the LaserWriter 8 printer queries
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#ifdef USESTRINGDOTH
#include <string.h>
#else  /* USESTRINGDOTH */
#include <strings.h>
#endif /* USESTRINGDOTH */
#include "list.h"
#include "query.h"
#include "papstream.h"

import char *myname;

private char newline[] = "\n";
private char no[] = ":No\n";
private char star[] = "*\n";
private char yes[] = ":Yes\n";

private void _SendResourceKVTree();
private char *buildProc();

void
SendMatchedKVTree(pf, list, prefix, str) /* only used by procset */
PFILE *pf;
KVTree **list;
char *prefix, *str;
{
  register char *cp, *tp;
  char buf[256], proc[256];

  if (prefix == NULL)
    cp = buf;
  else {
    strcpy(buf, prefix);
    cp = buf + strlen(buf);
  }
  while (buildProc(&str, proc)) {
    strcpy(cp, proc);
    strcat(cp, SearchKVTree(list, proc, strcmp) ? yes : no);
    p_write(pf,buf,strlen(buf),FALSE);
  }
  p_write(pf,star,strlen(star),FALSE);
}

void
SendMatchedResources(pf, list, prefix, str)
PFILE *pf;
List *list;
char *prefix, *str;
{
  register char *cp, *tp;
  char buf[256];

  if (prefix == NULL)
    cp = buf;
  else {
    strcpy(buf, prefix);
    cp = buf + strlen(buf);
  }
  while (tp = nextoken(&str)) {
    strcpy(cp, tp);
    strcat(cp, SearchList(list, tp, strcmp) ? yes : no);
    p_write(pf,buf,strlen(buf),FALSE);
  }
  p_write(pf,star,strlen(star),FALSE);
}

void
SendResourceKVTree(pf, list, prefix)
PFILE *pf;
KVTree **list;
char *prefix;
{
  register char *cp;
  char buf[256];

  if (prefix == NULL)
    cp = buf;
  else {
    strcpy(buf, prefix);
    cp = buf + strlen(buf);
  }
  _SendResourceKVTree(pf, *list, buf, cp);
  p_write(pf,star,strlen(star),FALSE);
}

private void
_SendResourceKVTree(pf, lp, buf, cp)
PFILE *pf;
register KVTree *lp;
char *buf, *cp;
{
  if (lp == NULL)
    return;
  if (lp->left)
    _SendResourceKVTree(pf, lp->left, buf, cp);
  strcpy(cp, (char *)lp->key);
  strcat(cp, newline);
  p_write(pf,buf,strlen(buf),FALSE);
  if (lp->right)
    _SendResourceKVTree(pf, lp->right, buf, cp);
}

void
SendResourceList(pf, list, prefix)
PFILE *pf;
List *list;
char *prefix;
{
  register int i;
  register char **ip;
  register char *cp;
  char buf[256];

  if (prefix == NULL)
    cp = buf;
  else {
    strcpy(buf, prefix);
    cp = buf + strlen(buf);
  }
  for (ip = (char **)AddrList(list), i = NList(list); i > 0; ip++, i--) {
    strcpy(cp, *ip);
    strcat(cp, newline);
    p_write(pf,buf,strlen(buf),FALSE);
  }
  p_write(pf,star,strlen(star),FALSE);
}

void
SendQueryResponse(pf, list)
PFILE *pf;
List *list;
{
  register char **lp;
  register int i;

  for (i = NList(list), lp = (char **)AddrList(list); i > 0; lp++, i--)
    p_write(pf, *lp, strlen(*lp), FALSE);
}

private char *
buildProc(str, buf)
char **str;
char *buf;
{
  register char *tp;

  if (!(tp = nextoken(str)))
    return(NULL);
  strcpy(buf, tp);
  if (!(tp = nextoken(str)))
    return(buf);
  strcat(buf, " ");
  strcat(buf, tp);
  if (!(tp = nextoken(str)))
    return(buf);
  strcat(buf, " ");
  strcat(buf, tp);
  return(buf);
}

char *
nextoken(cp)
char **cp;
{
  register char *bp, *ep;

  bp = *cp;
  while (isspace(*bp))
    bp++;
  if (*bp == 0)
    return(NULL);
  ep = bp + 1;
  if (*bp == '"') {
    while (*ep && *ep != '"')
      ep++;
    if (*ep)
      ep++;
  } else {
    while (*ep && !isspace(*ep))
      ep++;
  }
  if (*ep)
    *ep++ = 0;
  *cp = ep;
  return(bp);
}
