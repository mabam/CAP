/*
 * $Author: djh $ $Date: 1996/06/18 10:49:40 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afposenum.c,v 2.15 1996/06/18 10:49:40 djh Rel djh $
 * $Revision: 2.15 $
 *
 */

/*
 * afposenum.c - Appletalk Filing Protocol OS enumeration. 
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *
 */

#include <stdio.h>
#include <sys/param.h>

#ifndef _TYPES
 /* assume included by param.h */
# include <sys/types.h>
#endif  /* _TYPES */

#include <sys/stat.h>
#include <sys/time.h>
#include <netat/appletalk.h>
#include <netat/afp.h>

#ifdef USESTRINGDOTH
#include <string.h>
#else USESTRINGDOTH
#include <strings.h>
#endif USESTRINGDOTH
#ifdef SHORT_NAMES
#include <ctype.h>
#endif SHORT_NAMES
#ifdef NOCASEMATCH
#include <sys/file.h>
#include <ctype.h>
#endif NOCASEMATCH
#ifdef USEDIRENT
# include <dirent.h>
#else  /* USEDIRENT */
# ifdef xenix5
#  include <sys/ndir.h>
# else xenix5
#  include <sys/dir.h>
# endif xenix5
#endif /* USEDIRENT */
#ifdef SOLARIS
# include <unistd.h>
#endif /* SOLARIS */
#ifdef linux
# include <unistd.h>
#endif /* linux */
#ifdef DISTRIB_PASSWDS
#include <netat/afppass.h>
#endif /* DISTRIB_PASSWDS */

#include "afps.h"
#include "afpdt.h"
#include "afpgc.h"

#ifdef SHORT_NAMES
typedef struct {
	char s_name[MAXSFLEN+1];
}s_entry;

typedef struct {			/* Enumeration cache entry */
  IDirP ece_dirid;			/* enumerated directory id */
  int   ece_lru;
  int	ece_lock;			/* do not purge lock */
  int   ece_cnt;			/* count of entries */
#ifdef USEDIRENT
  struct dirent **ece_nlst;		/* direct info */
#else  USEDIRENT
  struct direct **ece_nlst;		/* direct info */
#endif USEDIRENT
  s_entry * s_namearray;
  time_t ece_ctime;			/* last directory write dt */
  int  ece_mmode;			/* last directory mode */
  int ece_mcnt;				/* last internal modify count */
  int ece_idx;				/* our index in the cache */
} EnumCE;
#else SHORT_NAMES
typedef struct {			/* Enumeration cache entry */
  IDirP ece_dirid;			/* enumerated directory id */
  int   ece_lru;
  int	ece_lock;			/* do not purge lock */
  int   ece_cnt;			/* count of entries */
#ifdef USEDIRENT
  struct dirent **ece_nlst;		/* direct info */
#else  USEDIRENT
  struct direct **ece_nlst;		/* direct info */
#endif USEDIRENT
  time_t ece_ctime;			/* last directory write dt */
  int  ece_mmode;			/* last directory mode */
  int ece_mcnt;				/* last internal modify count */
  int ece_idx;				/* our index in the cache */
} EnumCE;
#endif SHORT_NAMES

#define NILECE ((EnumCE *) 0)

private EnumCE *EnumCache[NECSIZE];
private int EC_Clock;			/* lru clock */

#ifdef USEDIRENT
#define NILNLST ((struct dirent **) 0)
#else  USEDIRENT
#define NILNLST ((struct direct **) 0)
#endif USEDIRENT

private EnumCE *EC_Min(),*EC_Find(),*EC_Fetch(),*EC_Load();
private boolean EC_Valid();
private void EC_Free();
private iselect();
#ifdef SHORT_NAMES
private name_toupper(), convert(),add_ast();
private int mysort();
#endif SHORT_NAMES

#ifdef NOCASEMATCH
private noCaseDir();
private searchDirectory();
#endif NOCASEMATCH

/*
 * int iselect(struct direct *d)
 * 
 * See if this directory entry should be included in our enumeration.
 * Special files are skipped ("." , ".." ".finderinfo", etc) and
 * a length check is made to see if the name is too long for AFP.
 *
 */

private
iselect(d)
#ifdef USEDIRENT
struct dirent *d;
#else  USEDIRENT
struct direct *d;
#endif USEDIRENT
{
  if (d == NULL)
    return(FALSE);
  if (d->d_name == NULL)
    return(FALSE);
  if (ENameLen(d->d_name) > MAXLFLEN)	/* external name too long? */
    return(FALSE);			/* sorry this name is too long */
  if (d->d_name[0] != '.')		/* all special dirs begin with dot */
    return(TRUE);
  if (d->d_name[1] == '\0')			/* check for dot */
    return(FALSE);
  if (d->d_name[1] == '.' &&		/* check for dot dot */
      d->d_name[2] == '\0')
    return(FALSE);
  if (strcmp(d->d_name,RFDIRFN) == 0)	/* resource fork directory? */
    return(FALSE);			/* yes... don't include */
  if (strcmp(d->d_name,FIDIRFN) == 0)	/* finder info directory? */
    return(FALSE);			/* yes... don't include */
  if (strcmp(d->d_name,DESKTOP_ICON) == 0) /* check for desktop files */
    return(FALSE);
  if (strcmp(d->d_name,DESKTOP_APPL) == 0)
    return(FALSE);
#ifdef DISTRIB_PASSWDS
  if (strcmp(d->d_name,AFP_DISTPW_USER) == 0) /* local password file */
    return(FALSE);
#endif /* DISTRIB_PASSWDS */
  return(TRUE);
}

/*
 * void ECacheInit()
 *
 * Initialize the cache table.
 *
 */

void
ECacheInit()
{
  int i;

  if (DBENU)
    printf("ECacheInit: Cache size is %d\n",NECSIZE);

  EC_Clock = 1;				/* init clock */
  for (i=0; i < NECSIZE; i++) {
    EnumCache[i] = (EnumCE *) malloc(sizeof(EnumCE));
    EnumCache[i]->ece_dirid = NILDIR;
    EnumCache[i]->ece_idx = i;
  }
}

private EnumCE *
EC_Min()
{
  int i,mi = -1;
  int mlru = EC_Clock+1;

  for (i=0; i < NECSIZE; i++) {
    if (EnumCache[i]->ece_dirid == NILDIR)
      return(EnumCache[i]);		/* found unused slot */
    if (!EnumCache[i]->ece_lock &&	/* if not locked and... */
	EnumCache[i]->ece_lru < mlru) {	/* is older than current */
	  mlru = EnumCache[i]->ece_lru;	/* remember new clock */
	  mi = i;			/* and new index */
	}
  }

  if (mi == -1)				/* all locked? big problem */
    logit(0,"EC_Min: no entry found\n");/* at least we will know */

  return(EnumCache[mi]);
}

/*
 * void EC_Free(EnumCE *ec)
 *
 * Free storage associate with a cache entry and set the entry to be
 * unused by making ece_dirid = NILDIR.
 *
 */

private void 
EC_Free(ec)
EnumCE *ec;
{
  int i;

  if (ec->ece_dirid == NILDIR)		/* unused entry? */
    return;				/* yes, just return */

  if (DBENU)
    printf("EC_Free: releasing %s\n",pathstr(ec->ece_dirid));

  for (i=0; i < ec->ece_cnt; i++)
    free((char *) ec->ece_nlst[i]);

  free((char *) ec->ece_nlst);
  (ec->ece_dirid)->eceidx = NOECIDX;	/* not in cache */
  ec->ece_dirid = NILDIR;		/* indicate free entry */
}

/*
 * boolean EC_Valid(EnumCE *ec, struct stat *stb)
 *
 * Check if the cache entry is valid by comparing information in
 * the cache entry with information provided from the stat.
 *
 * If the cache entry is empty, the internal modification counters
 * are different, or the directory modification time_t differs from 
 * the time_t the entry was created, then return FALSE.
 *
 */

private boolean
EC_Valid(ec,stb)
EnumCE *ec;
struct stat *stb;
{
  IDirP dirid = ec->ece_dirid;

  if (ec->ece_dirid == NILDIR)		/* no entry? */
    return(FALSE);			/* should not happen */

  /* See if the directory has been modified and needs to be reread */
  /* Check both the internal modified counter and the os modify times. */
  /* The internal mod counts are to prevent a race condition with the */
  /* file's mod times not getting set/read correctly */

  if (ec->ece_dirid->modified != ec->ece_mcnt) { /* modified by us? */
    if (DBENU)
      printf("EC_Valid: internal modify invalidates %s\n",pathstr(dirid));
    return(FALSE);			/* yes... */
  }

  if ((stb->st_ctime != ec->ece_ctime) || (stb->st_mode != ec->ece_mmode)) {
    if (DBENU)
      printf("EC_Valid: external modify invalidates %s\n",pathstr(dirid));
    return(FALSE);
  }
  return(TRUE);				/* not modified */
}

/*
 * EnumCE * EC_Find(IDirP dirid)
 *
 * Given a dirid return the cached Enum entry or NILECE.
 *
 */

private EnumCE *
EC_Find(dirid)
IDirP dirid;
{
  int cidx = dirid->eceidx;		/* cache index */

  if (cidx == NOECIDX)			/* check dir for cache entry */
    return(NILECE);			/* no cache entry */
  EnumCache[cidx]->ece_lru = EC_Clock++; /* this is a reference */
  return(EnumCache[cidx]);		/* return the cache */
}

/*
 * EnumCE * EC_Fetch(IDirP dirid)
 *
 * Get the EnumCE entry for this dirid.  If the entry is in the
 * cache then validate it by calling EC_Valid.  If not valid
 * (changed) then reload the information.  If the directory info
 * is not in the cache then recycle the min cache entry and load
 * a new entry.
 * 
 * Returns NILECE on failure, ece on success.
 *
 */

private EnumCE *
EC_Fetch(dirid)
IDirP dirid;
{
  EnumCE *ec;
  struct stat stb;

  if (DBENU)
    printf("EC_Fetch: request for %s\n",pathstr(dirid));

  ec = EC_Find(dirid);			/* see if entry is cached */
  if (ec != NILECE && ec->ece_lock)	/* found and locked? */
    return(ec);				/* then being enumerated, return */

#ifndef STAT_CACHE
  if (stat(pathstr(dirid),&stb) != 0) {	/* else do a stat for checks */
#else STAT_CACHE
  if(OSstat(pathstr(dirid),&stb)!= 0) {	/* else do a stat for checks */
#endif STAT_CACHE
    Idrdirid(Ipdirid(dirid),dirid);	/* unlink from directory tree */
    return(NILECE);			/* nothing there... */
  }

  if (ec != NILECE) {			/* find anything? */
    if (EC_Valid(ec,&stb))		/* yes... do validity check */
      return(ec);			/* seems ok, return it */
  } else
    ec = EC_Min();			/* else get a current min */

  EC_Free(ec);				/* free entry or min */

  return(EC_Load(dirid,ec,&stb));	/* load dir and return */
}

#ifdef SHORT_NAMES

/*
 * table to map Mac character set represented with :aa into something
 * "approximately similar" for MSDOS, any bad characters map to '#'
 * (valid character list from "MSDOS User's Guide").
 *
 * this is horrible!, but the results look a little nicer on the PC
 *
 * Modified 91/12/14 by Paul Buddington to improve translation based
 * on MSDOS 5.0 User's Guide
 *
 */

char mapMacChar[] = {
'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#',
'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#',
'-', '!', '#', '#', '$', '%', '&', '\'','(', ')', '#', '#', '#', '-', '#', '#',
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '#', '#', '#', '#', '#', '#',
'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '#', '#', '#', '^', '_',
'`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '#', '}', '~', '#',
'A', 'A', 'C', 'E', 'N', 'O', 'U', 'A', 'A', 'A', 'A', 'A', 'A', 'C', 'E', 'E',
'E', 'E', 'I', 'I', 'I', 'I', 'N', 'O', 'O', 'O', 'O', 'O', 'U', 'U', 'U', 'U',
'#', '#', 'C', '#', '#', '#', '#', 'B', 'R', 'C', 'T', '#', '#', '#', 'A', 'O',
'#', '#', '#', '#', '#', 'U', '#', '#', '#', '#', '#', 'A', '#', '#', 'A', 'O',
'#', '#', '#', '#', 'F', '#', '#', '#', '#', '#', '#', 'A', 'A', 'O', 'O', 'O',
'#', '#', '#', '#', '#', '#', '#', '#', 'Y', '#', '#', '#', '#', '#', '#', '#',
'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#',
'#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'
};

/*
 * Map a long name into a short, uppercase DOS compatible name.
 * Nothing specifies exactly how to do this, so try and make it
 * resemble the behaviour of a Mac AppleShare server.
 */

private
name_toupper(nlen,name,s_name)
int nlen;
char *name, *s_name;
{
	int c;
        int period = 0;
	int altered = 0;
	int i=0,j=0,k=0;
	private byte hex2nibble();
	char sname[MAXSFLEN+4]; /* slop */
        
	if (DBENU)
	   printf("nlen %d, MAXSFLEN %d name %s \n",nlen,MAXSFLEN,name);

	while (name[i] != '\0' && i < nlen && j < MAXSFLEN && period < 5) {
	/* if the name starts with a . ignore for now, mark altered */
            if ((i == 0) && (name[i] == '.')) {
                altered++;
                i++;
	/* when you hit the first period, save it and count it */
	    } else if ((period == 0) && (name[i] == '.')) {
	    	     sname[j++] = '.';
	             period++;
	    	     i++;
        /* any weird characters, just say no !!!! */
	/* if we hit another period, skip over it */
	    } else if ((name[i] == '*') || (name[i] == '+') || 
                       (name[i] == '|') || (name[i] == '?') ||
                       (name[i] == '<') || (name[i] == '>') || 
                       (name[i] == '[') || (name[i] == ']') || 
                       (name[i] == '"') || (name[i] == '=') ||
                       (name[i] == '`') || (name[i] == '.') ||
                       (name[i] == '/') || (name[i] == '\\')|| 
                       (name[i] == ';') || (name[i] == ',')) {
		     altered++;
                     i++;
	/* if we see a space, ignore it, but don't mark as altered */
	    } else if (name[i] == ' ') {
		     i++;
	/* check for aufs :aa special chars, map down into sim. ascii char */
	    } else if (name[i] == ':' && i < (nlen-2)) {
		     c = hex2nibble(name[++i]);
		     c = c << 4;
		     c |= hex2nibble(name[++i]);
		     if ((sname[j] = mapMacChar[c & 0xff]) != '#')
		       j++;
		     altered++;
		     i++;
        /* otherwise, just convert to upper */
	    } else {
		if (isalpha(name[i]) && !(isupper(name[i])))
                  sname[j++] = toupper(name[i]);
		else
		  sname[j++] = name[i];
	        i++;
	    }
	    if (period)
	      period++; /* keep counting to get 3 more chars for DOS suffix */
        }
        sname[j] = '\0';
	/* more characters to go ? */
	if (name[i] != '\0')
	  altered++;
	j = k = 0;
	/* copy to s_name, mark start of name with ! if altered */
	while(sname[j] != '\0' && j < MAXSFLEN && k < MAXSFLEN) {
	  if (altered && k == 0)
	    s_name[k++] = '!';
	  if (!period && k == 8)
	    s_name[k++] = '.';
	  s_name[k++] = sname[j++];
	}
	s_name[k] = '\0';
}

private byte
hex2nibble(a)
char a;
{
  if (a >= '0' && a <= '9')
    return(a - '0');
  if (a >= 'a' && a <= 'f')
    return(a - 'a' + 10);
  if (a >= 'A' && a <= 'F')
    return(a - 'A' + 10);
  return(0);
}

private add_ast (ec,pos)
EnumCE *ec;
int pos;
{
     	int i = 1;
        char c,c1;
        
        c = ec->s_namearray[pos].s_name[0];
        ec->s_namearray[pos].s_name[0] = '*';	
        while ((c != '\0') && (i<MAXSFLEN)) {	
                if (i == 8) {
		   ec->s_namearray[pos].s_name[i] = '.';
	           c1 = ec->s_namearray[pos].s_name[i+1];
                   ec->s_namearray[pos].s_name[i+1] = c;
                   i+=2;
                } else {
                   c1 = ec->s_namearray[pos].s_name[i];
		   ec->s_namearray[pos].s_name[i] = c;
                   i++;
                }
                c = c1; 
        }
        ec->s_namearray[pos].s_name[i] = '\0';
}

private
convert(ec)
EnumCE *ec;
{
	int ast,i;
	
	ec->s_namearray = (s_entry*)malloc (ec->ece_cnt*sizeof(s_entry));
        ast = FALSE;
	for (i = 0; i<ec->ece_cnt; i++) {
#ifdef USEDIRENT
           name_toupper(strlen(ec->ece_nlst[i]->d_name),ec->ece_nlst[i]->d_name,
#else  /* USEDIRENT */
           name_toupper(ec->ece_nlst[i]->d_namlen, ec->ece_nlst[i]->d_name,
#endif /* USEDIRENT */
                            ec->s_namearray[i].s_name); 
	   if (i>0) { 
             if ((strcmp(ec->s_namearray[i].s_name,
                         ec->s_namearray[i-1].s_name)) == 0){
		add_ast(ec,i-1);
		ast = TRUE;
              } else if (ast) {
                 add_ast(ec,i-1);
                 ast = FALSE;
              }
            }
	 }  
         if (ast) 
           add_ast(ec,(ec->ece_cnt)-1);
}
private int
mysort(x,y)
#ifdef USEDIRENT
struct dirent **x,**y;
#else  USEDIRENT
struct direct **x,**y;
#endif USEDIRENT
{
	char tempx[MAXNAMLEN],tempy[MAXNAMLEN];
	int i = 0;
	
#ifdef USEDIRENT
	   while (i < strlen((*x)->d_name))  {
#else  /* USEDIRENT */
	   while (i < (*x)->d_namlen)  {
#endif /* USEDIRENT */
	     tempx[i] = toupper((*x)->d_name[i]);
	     i++;
 	   }
	   tempx[i] = '\0';
	   i = 0;
#ifdef USEDIRENT
	   while (i < strlen((*y)->d_name))  {
#else  /* USEDIRENT */
	   while (i < (*y)->d_namlen)  {
#endif /* USEDIRENT */
	     tempy[i] = toupper((*y)->d_name[i]);
	     i++;
	   }
	   tempy[i] = '\0';
	   return(strcmp(tempx,tempy));
}
#endif SHORT_NAMES
/*
 * EnumCE *EC_Load(IDirP dirid, EnumCE *ec, struct stat *stb)
 *
 * Read the directory dirid into ec, filling in information from 
 * the stat structure stb.
 *
 * Return ec on success, or NILECE if failure.
 *
 */

private EnumCE *
EC_Load(dirid,ec,stb)
IDirP dirid;
EnumCE *ec;
struct stat *stb;
{
  char *path = pathstr(dirid);
#ifdef STAT_CACHE
  extern char *cwd_path();
#endif STAT_CACHE
  int i;

  if (DBENU)
    printf("EC_Load: adding %s\n",path);
#ifdef STAT_CACHE
  path = cwd_path(path);
#endif STAT_CACHE

#ifdef SHORT_NAMES 
  ec->ece_cnt = scandir(path,&ec->ece_nlst,iselect,mysort);
#else SHORT_NAMES
  ec->ece_cnt = scandir(path,&ec->ece_nlst,iselect,0);
#endif SHORT_NAMES
  if (ec->ece_cnt < 0) {
    if (DBENU)
      printf("EC_Load: scandir failed for %s\n",path);
    return(NILECE);
  }
#ifdef SHORT_NAMES
  convert(ec);
#endif SHORT_NAMES
  ec->ece_ctime = stb->st_ctime;	/* copy last modify time */
  ec->ece_mmode = stb->st_mode;		/* copy the last mode */
  ec->ece_dirid = dirid;		/* copy directory id */
  ec->ece_mcnt = dirid->modified;	/* copy modify count */
  ec->ece_lru = EC_Clock++;		/* set LRU from clock */
  dirid->eceidx = ec->ece_idx;		/* add new entry */
  return(ec);				/* and return table */
}    

/*
 * char * OSEnumGet(dirid,idx)
 *
 * Fetch the idx'th file/dir name from an enumerated directory.  
 * Index starts at 1.  
 *
 * You must have first called OSEnumIni() to initialize and
 * discover the maximun count of entries in the directory.
 *
 */

char *
OSEnumGet(dirid,idx)
IDirP dirid;
int idx;				/* ranges from 1..count */
{
  EnumCE *ec;

  ec = EC_Find(dirid);			/* find enum entry given dirid */
  if (ec == NILECE) {			/* was locked could not be purged */
    logit(0,"OSEnumGet: bad enum!\n");
    return("");
  }

  if (idx > ec->ece_cnt) {		/* check for bad idx */
    logit(0,"OSEnumGet: Bad idx!\n");
    return("");
  }

  if (!ec->ece_lock)			/* check for bad lock */
    logit(0,"OSEnumGet: Handle not locked!\n");
  
  if (DBENU)
    printf("OSEnumGet: %d:%s\n",idx,ec->ece_nlst[idx-1]->d_name);

  return(ec->ece_nlst[idx-1]->d_name);
}

/*
 * int OSEnumInit(IDirP idir)
 *
 * Call OSEnumInit to initialize enumeration information.  This
 * procedure must be called before OSEnumGet which returns the
 * nth entry for a directory.
 *
 * OSEnumDone must be called when finished to release enumeration
 * information for a directory.
 *
 * Returns the count of entries in the directory or a negative error
 * code.
 *
 */

int
OSEnumInit(idir)
IDirP idir;
{
  EnumCE *ec;

  if (DBENU)
    printf("OSEnumInit: %s\n",pathstr(idir));

#ifdef STAT_CACHE
  OScd(pathstr(idir));
#endif STAT_CACHE

  OSValFICacheForEnum(idir);	/* make sure fi cache for directory is valid */
  ec = EC_Fetch(idir);		/* get directory info */
  if (ec == NILECE)		/* access problem? */
    return(aeAccessDenied);	/* yes... */

  if (ec->ece_lock == TRUE)
    logit(0,"OSEnumInit: directory already locked!\n");

  ec->ece_lock = TRUE;		/* entry is locked, no purge */
  return(ec->ece_cnt);		/* return count */
}

/*
 * void OSEnumDone(dirid)
 *
 * Call this routine to say that enumeration is no longer active for
 * a directory.
 *
 * OSEnumDone unlocks the directory entry and allows it to be removed
 * from the cache.
 *
 */

void
OSEnumDone(dirid)
IDirP dirid;
{
  EnumCE *ec;

  ec = EC_Find(dirid);			/* get cached entry */
  if (ec == NILECE)			/* should not happen */
    logit(0,"OSEnumDone: no cache for dirid\n");
  else
    ec->ece_lock = FALSE;		/* unlock */
}

/*
 * int OSEnumCount(dirid)
 * 
 * Return the count of entries in a directory.
 *
 */

int OSEnumCount(dirid)
IDirP dirid;
{
  EnumCE *ec;

  ec = EC_Fetch(dirid);			/* get directory info */
  if (ec == NILECE)			/* access problem? */
    return(aeAccessDenied);		/* yes... */

  if (DBENU)
    printf("OSEnumCount: %s = %d\n",pathstr(ec->ece_dirid),ec->ece_cnt);

  return(ec->ece_cnt);			/* return the count */
}

OSfname(r,idir,fn,typ)
IDirP idir;
register char *fn,*r;
register int typ;
{
  register char *p;

  for (p = pathstr(idir); *p != '\0';)		/* copy the directory */
    *r++ = *p++;

  if (typ == F_RSRC || typ == F_FNDR) /* append directory names */
    for (p = ((typ == F_RSRC) ? RFDIR : FIDIR); *p != '\0'; )
      *r++ = *p++;

  if (*fn != '\0') {		/* add slash */
    *r++ = '/';			/*  if not null file */
    while (*fn != '\0')
      *r++ = *fn++;
  }
  *r = '\0';
}

toResFork(str, fn)
register char *str;
char *fn;
{
  register char *fp, *tp;
  if(*fn) {			/* a real file */
    if(fp = rindex(str, '/'))	/* skip over last slash */
      fp++;
    else
      fp = str;
    str = fp;
    fp = str + strlen(str);
    tp = fp + DIRRFLEN;
    *tp = 0;
    while(fp > str)	/* move filename, leaving space for .resource */
      *--tp = *--fp;
    fp = DIRRF;
    while(*fp)
      *str++ = *fp++;
  } else			/* a directory */
    strcat(str,RFDIR);		/* just append .resource */
}

toFinderInfo(str, fn)
register char *str;
char *fn;
{
  register char *fp, *tp;

  if(*fn) {			/* a real file */
    if(fp = rindex(str,'/'))	/* skip over last slash */
      fp++;
    else
      fp = str;
    str = fp;
    fp = str + strlen(str);
    tp = fp + DIRFILEN;
    *tp = 0;
    while(fp > str)	/* move filename, leaving space for .finderinfo */
      *--tp = *--fp;
    fp = DIRFI;
    while(*fp)
      *str++ = *fp++;
  } else			/* a directory */
    strcat(str,FIDIR);		/* just append .finderinfo */
}

#ifdef NOCASEMATCH
/*
 * searchDirectory(dir, name)
 *	Do a case insensitive search for name in dir, and write the true name
 *	of the file in name.
 */

private
searchDirectory(dir, name)
char *dir, *name;
{
	register char *fp, *tp;
	register DIR *dp;
#ifdef USEDIRENT
	register struct dirent *d;
#else  USEDIRENT
	register struct direct *d;
#endif USEDIRENT
	register int len;
	char lname[BUFSIZ], dname[BUFSIZ];

	if((dp = opendir(dir)) == NULL)
		return(0);
	len = 0;
	for(fp = name, tp = lname ; *fp ; fp++) {
		*tp++ = isupper(*fp) ? tolower(*fp) : *fp;
		len++;
	}
	*tp = 0;
	while(d = readdir(dp)) {
#ifdef USEDIRENT
		if (strlen(d->d_name) != len)
#else  /* USEDIRENT */
		if (d->d_namlen != len)
#endif /* USEDIRENT */
			continue;
		for(fp = d->d_name, tp = dname ; *fp ; fp++)
			*tp++ = isupper(*fp) ? tolower(*fp) : *fp;
		*tp = 0;
		if(strcmp(dname, lname) == 0) {
			strcpy(name, d->d_name);
			closedir(dp);
			return(1);
		}
	}
	closedir(dp);
	return(0);
}

/*
 * noCaseDir(path)
 *	Recursively verify the components of path.
 */

private
noCaseDir(path)
register char *path;
{
	register char *last;
	register int status;

	if(access(path, F_OK) >= 0)
		return(1);
	if(last = rindex(path, '/')) {
		if(last == path)
			return(searchDirectory("/", last + 1));
		else {
			*last++ = 0;
			status = 0;
			if(noCaseDir(path))
				status = searchDirectory(path, last);
			*--last = '/';
			return(status);
		}
	}
	return(searchDirectory(".", path));
}

/*
 * noCaseFind(path)
 *	noCaseFind() calls noCaseDir() and searchDirectory() recursively to
 *	take path (case insensitive) and converts it to (case sensitive) newpath
 *	corresponding to the true Unix filename.  This is mainly to fix
 *	HyperCard doing funny things to stack names.
 */

void
noCaseFind(path)
register char *path;
{
	register char *last;

	if(last = rindex(path, '/')) {
		if(last == path)
			searchDirectory("/", last + 1);
		else {
			*last++ = 0;
			if(noCaseDir(path))
				searchDirectory(path, last);
			*--last = '/';
		}
	} else
		searchDirectory(".", path);
}

/*
 * noCaseMatch(path)
 *	noCaseMatch() tests path first and will call noCaseFind() is path
 *	doesn't exist.
 */

void
noCaseMatch(path)
register char *path;
{
	if(access(path, F_OK) >= 0)
		return;
	noCaseFind(path);
}
#endif NOCASEMATCH

#ifdef SHORT_NAMES
private int
searchn (name,ec,type)
char *name;
EnumCE *ec;
{
	int i = 0;

	if (ec == NILECE)	/* hmmmm ... */
		return(-1);

	if (type != 1)  
	  while ((i<ec->ece_cnt) 
              && (strcmp (name, ec->ece_nlst[i]->d_name) != 0)) {
		i++;
          }
	else
	  while ((i<ec->ece_cnt) 
              && (strcmp (name, ec->s_namearray[i].s_name) != 0)) {
                i++;
          }
	if (i == ec->ece_cnt)
 	   return(-1);
	else
	   return(i);
}

char*
Get_name(dirid,name,type,outname)
IDirP dirid;
char *name,*outname;
int type;
{
   EnumCE *ec;
   int i, idx;
	ec = EC_Fetch(dirid);
	if (ec == NILECE)
	  idx = -1;
	else
	  idx = searchn(name,ec,type);
        if (idx == -1) {
          if (strcmp (name, dirid->name) == 0)  /*then it is a directory*/
        	printf("wasn't found and it is a directory\n");
          if (DBENU)
	     printf("name wasn't found!!!!!Index == -1\n");
	  sprintf(outname,"%s",name);
         }
	else if (type != 1) /*return longname*/
 	    sprintf(outname,"%s",ec->s_namearray[idx].s_name);
	else  /* return short name */
 	    sprintf(outname,"%s",ec->ece_nlst[idx]->d_name);
}
#endif SHORT_NAMES
