static char rcsid[] = "$Author: djh $ $Date: 1996/06/18 10:50:20 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/procset.c,v 2.13 1996/06/18 10:50:20 djh Rel djh $";
static char revision[] = "$Revision: 2.13 $";

/*
 * procset - UNIX AppleTalk spooling program: act as a laserwriter
 *   handles simple procset list - assumes procsets in a directory
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *  Created Sept 5, 1987 by cck from lwsrv.
 *
 *
 */

#include <stdio.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>		/* assume included by param.h */
#endif /* _TYPES */
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#include <netat/compat.h>

#ifdef USESTRINGDOTH
# include <string.h>
#else USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef USEDIRENT
# include <dirent.h>
#else  USEDIRENT
# ifdef xenix5
#  include <sys/ndir.h>
# else xenix5
#  include <sys/dir.h>
# endif xenix5
#endif USEDIRENT

#if defined(xenix5) || defined(SOLARIS)
# include <unistd.h>
#endif /* xenix5 || SOLARIS */
#ifdef linux
# include <unistd.h>
#endif /* linux */

#ifdef LWSRV8
#include "list.h"
#include "query.h"
#endif /* LWSRV8 */
#include "procset.h"
#include "spmisc.h"

extern char *myname;
extern int verbose;

#ifndef LWSRV8
DictList *dicthead = (DictList *)NULL;	/* dictionary list header */
#else /* LWSRV8 */
private KVTree **dictlist = NULL;	/* dictionary list header */
#endif /* LWSRV8 */

void
#ifndef LWSRV8
newdictionary(dl)
#else /* LWSRV8 */
set_dict_list(dl)
KVTree **dl;
{
  dictlist = dl;
}

void
newdictionary(name, dl)
char *name;
#endif /* LWSRV8 */
DictList *dl;
{
#ifndef LWSRV8
  dl->ad_next = dicthead;		/* link it into the list */
  dicthead = dl;
#else /* LWSRV8 */
  AddToKVTree(dictlist, (void *)name, (void *)dl, strcmp);
#endif /* LWSRV8 */
}

DictList *
GetProcSet(psn)
char *psn;
{
#ifndef LWSRV8
  DictList *dl;
  for (dl = dicthead; dl != (DictList *) NULL; dl = dl->ad_next)
    if (strcmp(psn,dl->ad_ver) == 0)
      return(dl);
  return(NULL);
#else /* LWSRV8 */
  return((DictList *)SearchKVTree(dictlist, psn, strcmp));
#endif /* LWSRV8 */
}

void
ClearProcSetsSent()
{
#ifdef LWSRV8
  private void clearprocsent();
  clearprocsent(*dictlist);
#else  /* LWSRV8 */
  DictList *dl;
  for (dl = dicthead; dl != (DictList *) NULL; dl = dl->ad_next) {
    dl->ad_sent = FALSE;
  }
#endif /* LWSRV8 */
}

#ifdef LWSRV8
private void
clearprocsent(dl)
register KVTree *dl;
{
  ((DictList *)dl->val)->ad_sent = FALSE;
  if (dl->left)
    clearprocsent(dl->left);
  if (dl->right)
    clearprocsent(dl->right);
  return;
}
#endif /* LWSRV8 */

/*
 * checks if the file is there and should be readable
 *
*/
private boolean
checkfile(fn)
char *fn;
{
  struct stat stb;
  if (stat(fn, &stb) < 0)
    return(FALSE);
#ifdef USEDIRENT
#if defined (AIX) || defined (drsnx) || defined(__osf__) || defined(SOLARIS)
  if (S_ISREG(stb.st_mode) == 0)		/* make sure regular file */
    return(FALSE);
#else  /* AIX || drsnx || __osf__ || SOLARIS */
  /* sysv follows xpg standards */
  if (S_ISREG(&stb) == 0)			/* make sure regular file */
    return(FALSE);
#endif /* AIX || drsnx || __osf__ || SOLARIS */
#else  USEDIRENT
  if (S_ISREG(stb.st_mode) == 0)		/* make sure regular file */
    return(FALSE);
#endif USEDIRENT
  if (access(fn, R_OK) < 0)
    return(FALSE);
  return(TRUE);
}

private boolean
dictselect(d)
#ifdef USEDIRENT
struct dirent *d;
#else  USEDIRENT
struct direct *d;
#endif USEDIRENT
{
  return(checkfile(d->d_name));
}

#ifdef LWSRV8
DictList *
CreateDict()
{
  register DictList *dl;

  if ((dl = (DictList *)malloc(sizeof(DictList))) == NULL)
    errorexit(1, "CreateDict: Out of memory\n");
  return(dl);
}

private void
verbosedict(lp, dfn)
register KVTree *lp;
char *dfn;
{
  if (lp->left)
    verbosedict(lp->left, dfn);
  fprintf(stderr,"%s: ProcSet '%s' file is '%s/%s'\n",
   myname,(char *)lp->key,dfn,((DictList *)lp->val)->ad_fn);
  if (lp->right)
    verbosedict(lp->right, dfn);
}
#endif /* LWSRV8 */

/*
 * scan dict dir for valid proc sets
 *
*/
#ifndef LWSRV8
void
scandicts(dfn)
char *dfn;
#else /* LWSRV8 */
KVTree **
scandicts(dfn, lastried)
char *dfn;
time_t *lastried;
#endif /* LWSRV8 */
{
  DictList *dl;
#ifdef USEDIRENT
  struct dirent **namelist;
  register struct dirent *dp;
#else  USEDIRENT
  struct direct **namelist;
  register struct direct *dp;
#endif USEDIRENT
  struct stat stb;
  int i, j, nlst;
  char path[MAXPATHLEN];
  char line[1024];			/* reasonable size */
  char *psn;
  FILE *fp;
#ifndef LWSRV8
  static time_t lastried = 0;
#else  /* LWSRV8 */
  register KVTree **lp;
  int free();
#endif /* LWSRV8 */

  if (stat(dfn, &stb) >= 0) {
#ifndef LWSRV8
    if (lastried == stb.st_mtime) /* directory modified? */
      return;			/* no, nothing to do */
    lastried = stb.st_mtime;
#else /* LWSRV8 */
    if (*lastried == stb.st_mtime) /* directory modified? */
      return(NULL);			/* no, nothing to do */
    *lastried = stb.st_mtime;
#endif /* LWSRV8 */
  }
  if (verbose)
    fprintf(stderr, "%s: (Re)scanning for Procsets\n", myname);
#ifdef USEGETCWD
  if (getcwd(path,sizeof(path)-1) == 0) {
    fprintf(stderr, "%s: Can't get working directory: %s\n", myname, path);
#ifndef LWSRV8
    return;
#else /* LWSRV8 */
    return(NULL);
#endif /* LWSRV8 */
  }
#else /* USEGETCWD */
  if (getwd(path) == 0) {
    fprintf(stderr, "%s: Can't get working directory: %s\n", myname, path);
#ifndef LWSRV8
    return;
#else /* LWSRV8 */
    return(NULL);
#endif /* LWSRV8 */
  }
#endif /* USEGETCWD */
  if (chdir(dfn) < 0) {
    perror("chdir");
    fprintf(stderr, "on path %s\n",dfn);
    (void)chdir(path);			/* try to go back in case... */
#ifndef LWSRV8
    return;
#else /* LWSRV8 */
    return(NULL);
#endif /* LWSRV8 */
  }
  if ((nlst = scandir(".", &namelist, dictselect, NULL)) < 0) {
    perror(dfn);
    fprintf(stderr, "Can't find dictionary files!\n");
#ifndef LWSRV8
    return;
#else /* LWSRV8 */
    return(NULL);
#endif /* LWSRV8 */
  }
#ifdef LWSRV8
  lp = CreateKVTree();
#endif /* LWSRV8 */
  for (i = 0, dp = namelist[0]; i < nlst ; i++, dp = namelist[i]) {
    if ((fp = fopen(dp->d_name, "r")) == NULL) 
      goto baddict;
    if (fgets(line, sizeof(line)-1, fp) == NULL) {
      fclose(fp);
      goto baddict;
    }
    if (strncmp(line, "%%BeginProcSet: ",sizeof("%%BeginProcSet: ")-1) != 0) {
      fclose(fp);
      goto baddict;
    }
    fclose(fp);
    for (j = 0; line[j] != '\0'; j++)	/* treat '\r' as eol */
      if (line[j] == '\r' || line[j] == '\n') {
	line[j] = '\0';
	break;
      }
    psn = line+sizeof("%%BeginProcSet:");
    stripspaces(psn);
    if (*psn == '\0')
      goto baddict;
#ifndef LWSRV8
    if ((dl=GetProcSet(psn)) != NULL) {
      if (checkfile(dl->ad_fn))
	goto baddict;
      free(dl->ad_fn);		/* reuse old dl, but drop fn space */
      free(dl->ad_ver);		/* drop ver space */
      dl->ad_fn = NULL;
    } else {
      if ((dl = (DictList *)malloc(sizeof(DictList))) == NULL)
	goto baddict;
      newdictionary(dl);		/* link it into the list */
    }
    dl->ad_ver = strdup(psn);		/* remember proc set name */
#else /* LWSRV8 */
    dl = CreateDict();
#endif /* LWSRV8 */
    dl->ad_fn = strdup(dp->d_name);	/* remember file name */
    dl->ad_sent = FALSE;
#ifdef LWSRV8
    AddToKVTree(lp, strdup(psn), dl, strcmp);
#endif /* LWSRV8 */
baddict:
    free(dp);
  }
  free(namelist);

  (void)chdir(path);

#ifndef LWSRV8
  if (verbose) {
    for (dl = dicthead; dl != (DictList *) NULL; dl = dl->ad_next)
      fprintf(stderr,"lwsrv: ProcSet '%s' file is '%s/%s'\n",
	    dl->ad_ver,dfn,dl->ad_fn);
  }
#else /* LWSRV8 */
  if (verbose)
    verbosedict(*lp, dfn);
  return(lp);
#endif /* LWSRV8 */
}

#define ADBUFZ MAXPATHLEN

private int domultijob = FALSE;

void
setflag_encrypted_instream(flg)
int flg;
{
  domultijob = flg;
}

void
ListProcSet(fn, dictdir, outfile, patchprocset)
char *fn;
char *dictdir;
FILE *outfile;
int patchprocset;
{
  char adbuf[ADBUFZ+1];
  FILE *fd;
  int cnt;

  sprintf(adbuf, "%s/%s",dictdir,fn);
  if ((fd = fopen(adbuf,"r")) != NULL) {
    if (patchprocset && domultijob) {
      fprintf(stderr, "%s: Running in eexec mode\n", myname);
      fprintf(outfile, "%%The following fixes problems where the prep file\n");
      fprintf(outfile, "%%assumes that it is permanently downloaded and an\n");
      fprintf(outfile, "%%eof occurs at the end of the prep (e.g. eexec)\n");
      fprintf(outfile, "2 {(%%stdin) (r) file cvx exec } repeat\n");
    }
    while ((cnt = fread(adbuf,sizeof(char),ADBUFZ,fd)) > 0)
      fwrite(adbuf,sizeof(char),cnt,outfile);
    if (patchprocset && domultijob)
      fprintf(outfile, "%%%%EOF\n");
    fclose(fd);
  } else {
    fprintf(stderr,"%s: ListProcSet: file not found %s\n", myname, adbuf);
    fprintf(outfile,"%% ** ProcSet file %s not found **\n",adbuf);
  }
}

