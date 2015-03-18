/*
 * $Author: djh $ $Date: 91/03/13 20:29:11 $
 * $Header: afppasswd.c,v 2.2 91/03/13 20:29:11 djh Exp $
 * $Revision: 2.2 $
*/

/*
 * afppasswd.c - Appletalk Filing Protocol - support for internal password file
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  June 1987     CCKim	Created.
 *
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <netat/sysvcompat.h>

#define TRUE 1
#define FALSE 0

static char *mypasswdfile = NULL;
static FILE *pfp = NULL;

typedef struct {
  char uname[9];		/* unix user name that name maps to */
  char passwd[9];		/* password to use */
  char name[40];		/* up 39 char user name */
}  aufspasswd;

rewaufs()
{
 if (pfp == NULL)
   if ((pfp=fopen(mypasswdfile, "r")) == NULL)
     return(FALSE);
 rewind(pfp);
 return(TRUE);
}

endaufspw()
{
  if (pfp != NULL)
    fclose(pfp);
}

static aufspasswd *
getaufspwent()
{
  static aufspasswd apd;
  char buf[128];
  int cnt;

  while (fgets(buf, 128, pfp) != NULL) {
    cnt = sscanf(buf, "USER: %s\tUNIXUSER: %s\tPASSWORD: %s",
		apd.name, apd.uname, apd.passwd);
    if (cnt != 3)
      continue;
    if (strlen(apd.name) > 39 || strlen(apd.uname) > 8 ||
	strlen(apd.passwd) > 8) {
      logit(0,"Bad password file entry: %s", buf);
      continue;
    }
    return(&apd);
  }
  return(NULL);
}

static aufspasswd *
getaufspwnam(nam)
char *nam;
{
  static aufspasswd *apd;

  if (apd && strcmp(apd->name, nam) == 0)
    return(apd);
  if (!rewaufs())
    return(NULL);
  while ((apd=getaufspwent()) != NULL)
    /* should be case insensitive */
    if (strcmp(nam, apd->name) == 0 && getpwnam(apd->uname) != NULL) { 
      endaufspw();
      return(apd);
    }
  endaufspw();
  apd = NULL;
  return(NULL);
}

init_aufs_passwd(pw)
char *pw;
{
  struct stat buf;

# ifndef NOLSTAT
  if (lstat(pw, &buf) < 0) {
    logit(0,"error: can't find aufs password file %s",pw);
    return(FALSE);
  }
# else
  if (stat(pw, &buf) < 0) {
    logit(0,"error: can't find aufs password file %s",pw);
    return(FALSE);
  }
#endif
  if ((buf.st_mode & S_IFMT) != S_IFREG) {
    logit(0,"error: aufs password file %s isn't a regular file", pw);
    return(FALSE);
  }
  if (buf.st_uid != 0) {
#ifdef DEBUG
    logit(0,"debug: aufs password file %s not owned by root", pw);
#else
    logit(0,"error: aufs password file %s not owned by root", pw);
    return(FALSE);
#endif
  }
  if (buf.st_mode & 002) {
    logit(0,"error: aufs password file %s is writable by world",pw);
    return(FALSE);
  }
  if (buf.st_mode & 020)
    logit(0,"warning: aufs password file %s is writable by group",pw);
  if (buf.st_mode & 004)
    logit(0,"warning: aufs password file %s is readable by world",pw);
  mypasswdfile = pw;
  return(TRUE);
}

int
is_aufs_user(nam)
char *nam;
{
  if (getaufspwnam(nam) == NULL)
    return(FALSE);
  return(TRUE);
}


char *
user_aufs_passwd(nam)
char *nam;
{
  aufspasswd *apd;
  
  if ((apd=getaufspwnam(nam)) == NULL)
    return(NULL);
  return(apd->passwd);
}

struct passwd *
aufs_unix_user(nam)
char *nam;
{
  aufspasswd *apd;
  
  if ((apd=getaufspwnam(nam)) == NULL)
    return(NULL);
  return(getpwnam(apd->uname));
}

