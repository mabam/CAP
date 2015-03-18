/*
 * $Author: djh $ $Date: 1995/06/12 06:58:18 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afposfi.c,v 2.7 1995/06/12 06:58:18 djh Rel djh $
 * $Revision: 2.7 $
*/

/*
 * afposfi.c - Appletalk Filing Protocol OS FNDR File Interface. 
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *  Jan   1988     CCKim        New finderinfo format
 *  December 1990  djh		tidy up for AFP 2.0
 *
 */

/* PATCH: PC.aufs/afposfi.c.diffs, djh@munnari.OZ.AU, 15/11/90 */
/* PATCH: Dan@lth.se/stat.cache.patches, djh@munnari.OZ.AU, 16/11/90 */
/* PATCH: Moy@Berkeley/afposfi.c.diff, djh@munnari.OZ.AU, 17/11/90 */


#include <stdio.h>
#include <errno.h>
extern int errno;
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>  /* for htons, htonl */
#include <netat/appletalk.h>
#include <netat/compat.h>
#include <netat/afp.h>
#include "afps.h"
#include "afpgc.h"
#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
#endif
#include "afpudb.h"

typedef struct FCacheEnt {
  struct FCacheEnt *next;	/* link for free queue */
  IDirP fe_pdir;		/* directory */
  int fe_okay;			/* last internal modify count */
  char  *fe_fnam;		/* file */
  long  fe_hash;		/* hash value of fe_fnam */
  /* should we use ctime instead perhaps? */
  time_t fe_mtime;		/* last modify time: file system */
  time_t fe_vtime;		/* last time validated */
  FileInfo fe_fi;		/* file info */
} FCacheEnt;
 
export GCHandle *fich;		/* handle on cache */
private FCacheEnt *fce, *lfce;	/* recycle purge-load */
private FCacheEnt *getfe;	/* last os_getfi */

private int fc_valid();
private int fc_compare();
private char *fc_load();
private void WriteFA();
private void fc_purge();
private void fc_flush();
private void fc_flush_start();
private void fc_flush_end();
private FileInfo *os_getfi();
private fc_readent();

#define FICacheSize 128
/* Each cache entry has a lifetime that it goes through before it must */
/* be revalidated before reuse: this prevents cache entries from getting */
/* "stale". */
/* The revalidation consists of seeing if the disk copy has changed */
#define FI_VALID_TIME (60*5)		/* 5 minutes */

InitOSFI()
{
  fich = GCNew(FICacheSize,fc_valid,fc_compare,fc_load,fc_purge,fc_flush);
  lfce = fce = (FCacheEnt *) 0; 
  getfe = (FCacheEnt *) 0;
}



/*
 * Our flush doesn't flush modified entries since our cache is
 * write through, but attempts instead to check validity of items in
 * our cache
 *
 * can bracket with fc_flush_start and fc_flush_end to reduce # of
 *  stats.  This depends on the .finderinfo directory modify time
 *  getting changed when a finderinfo entry is modified.  WriteFA
 *  guarantees this as best it can (can you say hack?)
 *
 *
*/
private time_t fcf_val_time = 0; /* validate time */
private IDirP fcf_val_dir = NILDIR; /* directory val time is for */

private void
fc_flush_start(pdir)
IDirP pdir;
{
  char path[MAXPATHLEN];
  struct stat stb;

  OSfname(path, pdir, "", F_FNDR); /* get directory */
  if (stat(path, &stb) < 0) {
#ifdef NOCASEMATCH
    noCaseFind(path);
    if (stat(path, &stb) < 0) {
      fcf_val_dir = NILDIR;
      fcf_val_time = 0;
      return;			/* nothing else we can do */
    }
#else NOCASEMATCH
    fcf_val_dir = NILDIR;
    fcf_val_time = 0;
    return;			/* nothing else we can do */
#endif NOCASEMATCH
  }
  fcf_val_dir = pdir;
  fcf_val_time = stb.st_mtime;	/* remember */
}

private void
fc_flush_end()
{
  fcf_val_time = 0;		/* mark none */
  fcf_val_dir = NILDIR;
}

private void
fc_flush(fe, pdir)
FCacheEnt *fe;
IDirP pdir;
{
  char path[MAXPATHLEN];
  struct stat stb;

  /* if passed directory isn't nildir (flush all), then the file */
  /* will only be check if the particular item has the same directory */
  if (pdir != NILDIR && (fe->fe_pdir != pdir))
    return;
  if ((fe->fe_pdir->flags & DID_FINDERINFO)) { /* always okay */
    OSfname(path, fe->fe_pdir, fe->fe_fnam, F_FNDR);
    /* if we have a .finderinfo directory update time */
    /* and it is more recent than cache entry, then check the */
    /* cache entry */
    /* also add a check against vtime.  Makes it much more useful */
    /* since it is more than likely that fcf_val_time is later */
    /* than the mtime for the majority of the files */
    if (fcf_val_dir == fe->fe_pdir &&
	(fcf_val_time > fe->fe_mtime) &&
	(fcf_val_time > fe->fe_vtime)) {
#ifdef NOCASEMATCH
      if (stat(path, &stb) < 0) {
	noCaseFind(path);
        if (stat(path, &stb) < 0)
	  return;			/* nothing else we can do */
      }
#else NOCASEMATCH
      if (stat(path, &stb) < 0)
	return;				/* nothing else we can do */
#endif NOCASEMATCH
      if (stb.st_mtime != fe->fe_mtime)	/* reload entry */
	fe->fe_okay = FALSE;		/* make entry as bad */
    }
  }
  time(&fe->fe_vtime);			/* mark last validate time */
}

private
fc_valid(fe)
FCacheEnt *fe;
{
  char path[MAXPATHLEN];
  struct stat stb;
  time_t mtime;

  if (!fe->fe_okay)			/* if not okay, don't */
    return(fe->fe_okay);		/* bother with checks */
  time(&mtime);
  if (fe->fe_vtime + FI_VALID_TIME < mtime) {
    fe->fe_vtime = mtime;		/* mark as new validate time */
    if ((fe->fe_pdir->flags & DID_FINDERINFO) == 0) /* always okay */
      return(fe->fe_okay);
    OSfname(path, fe->fe_pdir, fe->fe_fnam, F_FNDR);
#ifdef NOCASEMATCH
    if (stat(path, &stb) < 0) {
      noCaseFind(path);
      if (stat(path, &stb) < 0)
        return(fe->fe_okay);		/* nothing else we can do */
    }
#else NOCASEMATCH
    if (stat(path, &stb) < 0)
      return(fe->fe_okay);		/* nothing else we can do */
#endif NOCASEMATCH
    if (stb.st_mtime != fe->fe_mtime)	/* reload entry */
      return(FALSE);			/* bad entry */
  }
  return(fe->fe_okay);
}

private void
fc_purge(fe)
FCacheEnt *fe;
{
  if (DBOSI)
    printf("fc_purge: %s\n",fe->fe_fnam);
  
  if (fe == getfe)			/* purging last get? */
    getfe = (FCacheEnt *) 0;		/* yes... then zero */
  
  free(fe->fe_fnam);			/* always free the name */
  fe->fe_fnam = (char *) 0;		/*  and zero... */
  fe->fe_hash = 0;			/* clear hash */
  fe->next = NULL;			/* trash it here since want in both */
  if (fce == (FCacheEnt *) 0)		/* check for recycled entry */
    lfce = fce = fe;			/* if none then save */
  else {
    lfce->next = fe;			/* put at end of free list */
    lfce = fe;				/* remember the end */
#ifdef notdef
    free(fe);				/* and the struct itself */
#endif
  }
}

private
fc_compare(fe,key)
FCacheEnt *fe,*key;
{
  if (fe->fe_pdir != key->fe_pdir)
    return(FALSE);
  if (fe->fe_hash != key->fe_hash)
    return(FALSE);
  return(strcmp(fe->fe_fnam,key->fe_fnam) == 0);
}

private char *
fc_load(key)
FCacheEnt *key;
{
  FCacheEnt *fe;
  
  if (DBOSI)
    printf("fc_load: %s\n",key->fe_fnam);

  if (fce != (FCacheEnt *) 0) {		/* recycled fc avail? */
    fe = fce;				/* yes... use that */
    fce = fce->next;
  } else				/* else allocate */
    fe = (FCacheEnt *) malloc(sizeof(FCacheEnt));

  fe->fe_pdir = key->fe_pdir;
  fe->fe_fnam = (char *) malloc(strlen(key->fe_fnam)+1);
  fe->fe_okay = TRUE;
  fe->fe_mtime = 0;
  time(&fe->fe_vtime);			/* validate time stamp */
  strcpy(fe->fe_fnam,key->fe_fnam);
  fe->fe_hash = key->fe_hash;
  fc_readent(fe);
  return((char *) fe);
}



private
fc_readent(fe)
FCacheEnt *fe;
{
  struct stat stb;
  char path[MAXPATHLEN];
  int fd, ft, len, err;
  FileInfo *fi = &fe->fe_fi;
  FinderInfo *fndr;
  extern struct ufinderdb uf[];
  extern int sessvers;
  word newattr;
#ifdef USR_FILE_TYPES
  extern struct uft uft[];
#endif USR_FILE_TYPES
  
  bzero((char *) fi,sizeof(FileInfo)); /* make sure clear before */
  if (fe->fe_pdir->flags & DID_FINDERINFO) {
    OSfname(path,fe->fe_pdir,fe->fe_fnam,F_FNDR); 
    if (DBOSI)
      printf("fc_readent: reading %s\n",path);

#ifndef STAT_CACHE
    fd = open(path,O_RDONLY);
#else STAT_CACHE
    fd = cwd_open(path,O_RDONLY);
#endif STAT_CACHE
#ifdef NOCASEMATCH
    if (fd < 0) {
      noCaseFind(path);
#ifndef STAT_CACHE
      fd = open(path,O_RDONLY);
#else STAT_CACHE
      fd = cwd_open(path,O_RDONLY);
#endif STAT_CACHE
    }
#endif NOCASEMATCH
    if (fd >= 0) {
      OSLockFileforRead(fd);
      err = fstat(fd, &stb);
      len = read(fd,(char *) fi,sizeof(FileInfo));
      OSUnlockFile(fd);
      if (len < FI_BASELENGTH) {
	close(fd);
	if (len == 0)		/* length zero means creat'd */
	  goto dummy;
	if (DBOSI)
	  printf("fc_readent: finderinfo too short\n");
	goto nofileinfo;
      }
      if (fi->fi_magic1 != FI_MAGIC1) {
	OldFileInfo *ofi = (OldFileInfo *)fi;

	bcopy(ofi->fi_comnt, fi->fi_comnt, fi->fi_magic1);
	fi->fi_comln = fi->fi_magic1;
	newattr = (sessvers >= AFPVersion2DOT0) ? (FPA_RNI|FPA_DEI) : 0;
	if (ofi->fi_attr & FI_ATTR_SETCLEAR)
	  fi->fi_attr = ofi->fi_attr &
	    (FI_ATTR_READONLY|FI_ATTR_MUSER|FI_ATTR_INVISIBLE|newattr);
	else
	  fi->fi_attr = 0;
	fi->fi_magic1 = FI_MAGIC1;
	fi->fi_magic = FI_MAGIC;
	fi->fi_version = FI_VERSION;
	fi->fi_bitmap = FI_BM_MACINTOSHFILENAME;
#ifdef SHORT_NAMES
	ItoEName(fe->fe_fnam, fi->fi_macfilename);
	ItoEName_Short(fe->fe_pdir,fe->fe_fnam,fi->fi_shortfilename);
#else SHORT_NAMES
	ItoEName(fe->fe_fnam, fi->fi_macfilename);
#endif SHORT_NAMES
	/* make sure we update it */
	WriteFA(fe->fe_pdir, fe->fe_fnam, fi);
      } else {
	if (fi->fi_magic != FI_MAGIC || fi->fi_version != FI_VERSION) {
	  if (DBOSI)
	    printf("fc_readent: fileinfo check fail\n");
	  close(fd);
	  goto nofileinfo;
	}
	fi->fi_attr = ntohs(fi->fi_attr);
	if (err == 0)		/* stat okay */
	  fe->fe_mtime = stb.st_mtime;	/* remember mtime */
      }
      if (close(fd) != 0)
	printf("fc_readent: close error");
      return(noErr);
    }

    /* Open failed for .finderinfo file.  Use defaults finfo or zero if dir */

    if (DBOSI)
      printf("fc_readent: Open failed for %s\n",path);
  }

nofileinfo:

  /* convert name to internal name */
  OSfname(path,fe->fe_pdir,fe->fe_fnam,F_DATA); /* create plain name */
#ifdef NOCASEMATCH
#ifndef STAT_CACHE
  if (stat(path,&stb) != 0) {		/* check if it exists */
#else STAT_CACHE
  if (OSstat(path,&stb) != 0) {		/* check if it exists */
#endif STAT_CACHE
    noCaseFind(path);
#endif NOCASEMATCH
#ifndef STAT_CACHE
    if (stat(path,&stb) != 0)		/* check if it exists */
#else STAT_CACHE
    if (OSstat(path,&stb) != 0)		/* check if it exists */
#endif STAT_CACHE
      return(aeObjectNotFound);		/* no... */
#ifdef NOCASEMATCH
  }
#endif NOCASEMATCH
  if (S_ISDIR(stb.st_mode)) {
    fi->fi_comln = 0;
  } else {
    ft = os_getunixtype(path, &stb);
    fndr = (FinderInfo *)fi->fi_fndr; /* get pointer to finder info */
#ifdef USR_FILE_TYPES
    if (ft >= FNDR_UFT) {
      ft -= FNDR_UFT; /* ft is now the index into the UFT table */
      bcopy(uft[ft].uft_ftype, fndr->file.fdType, sizeof(uft[ft].uft_ftype));
      bcopy(uft[ft].uft_creat, fndr->file.fdCreator, sizeof(uft[ft].uft_creat));
      bcopy(uft[ft].uft_comment, fi->fi_comnt, uft[ft].uft_commentlen);
      fi->fi_comln = uft[ft].uft_commentlen;
    } else {
#endif USR_FILE_TYPES
      bcopy(uf[ft].ufd_ftype, fndr->file.fdType, sizeof(fndr->file.fdType));
      bcopy(uf[ft].ufd_creat,fndr->file.fdCreator,sizeof(fndr->file.fdCreator));
      bcopy(uf[ft].ufd_comment, fi->fi_comnt, uf[ft].ufd_commentlen);
      fi->fi_comln = uf[ft].ufd_commentlen;
#ifdef USR_FILE_TYPES
    }
#endif USR_FILE_TYPES
  }
  fi->fi_attr = DEFATTR;	/* set default attributes */
dummy:
  fi->fi_magic1 = FI_MAGIC1;
  fi->fi_version = FI_VERSION;
  fi->fi_magic = 0;		/* mark as "default" entry */
  fi->fi_bitmap = FI_BM_MACINTOSHFILENAME;
#ifdef SHORT_NAMES
  ItoEName(fe->fe_fnam, fi->fi_macfilename);  
  ItoEName_Short(fe->fe_pdir,fe->fe_fnam, fi->fi_shortfilename);  
#else SHORT_NAMES
  ItoEName(fe->fe_fnam, fi->fi_macfilename);  
#endif SHORT_NAMES
  return(noErr);
}

private void
WriteFA(ipdir,fn,fi)
IDirP ipdir;
char *fn;
FileInfo *fi;
{
  char path[MAXPATHLEN];
  int fd;
  int needu = 0;

  if ((ipdir->flags & DID_FINDERINFO) == 0) {
    if (DBOSI)
      printf("WriteFA skipped, no finderinfo directory\n");
    return;
  }
  OSfname(path,ipdir,fn,F_FNDR); 	/* convert to internal name */

  if (DBOSI)
    printf("WriteFA: writing %s\n",path);

  needu++;
  fd = open(path,O_WRONLY);
#ifdef NOCASEMATCH
  if(fd < 0) {
    noCaseFind(path);
    fd = open(path,O_WRONLY);
  }
#endif NOCASEMATCH
  if (fd < 0) {				/* open for write */
    if (errno != ENOENT) {
      printf("WriteFA: error opening %s errno=%d\n",path,errno);
      return;
    }
    if ((fd = open(path,O_WRONLY|O_CREAT,0666)) < 0) {
      if (DBOSI)
	printf("fc_flush: create failed for %s %d\n",path,errno);
      return;
    }
    if (DBOSI)
      printf("fc_flush: created %s\n",path);
  } else needu++;
  fi->fi_attr = htons(fi->fi_attr); /* swab!!!@ */
  fi->fi_magic = FI_MAGIC;
  OSLockFileforWrite(fd);
  (void)write(fd,(char *) fi,sizeof(FileInfo)); /* write stuff */
  OSUnlockFile(fd);
  if (close(fd) != 0)
    printf("WriteFA: close error");
  fi->fi_attr = htons(fi->fi_attr); /* then swab back!!!@ */
  /* horrible hack, but can't use utimes, because we must be owner then */
  if (needu) {			/* update directory modified time */
    OSfname(path,ipdir,FIDIRFN,F_FNDR);	/* pick bad name */
    if ((fd = open(path, O_WRONLY|O_CREAT,0666)) < 0)
      return;
    close(fd);
    unlink(path);
  }
/*  EModified(fe->fe_pdir);		/* and mark directory as modified */
}

private FileInfo *
os_getfi(pdir,fn)
IDirP pdir;
char *fn;
{
  FCacheEnt key;
  register long hash;
  register char *p;

  key.fe_pdir = pdir;
  p = key.fe_fnam = fn;
  
  hash = 0;
  while (*p) {
    hash <<= 2;
    hash += *p++;
  }
  key.fe_hash = hash;

  /* do the "quick" check first */
  if (getfe == 0 || !fc_compare(getfe,&key)) {
    /* nope, either find in cache or load from disk */
    getfe = (FCacheEnt *)GCLocate(fich,(char *)&key);
  }
  return(&getfe->fe_fi);		/* and return the FileInfo */
}

FModified(pdir, fn)
IDirP pdir;     
char *fn;
{
  FCacheEnt key;
  register long hash;
  register char *p;
  int idx;

  key.fe_pdir = pdir;
  p = key.fe_fnam = fn;

  hash = 0;
  while (*p) {
  hash <<= 2;
  hash += *p++;
  }
  key.fe_hash = hash;

  EModified(pdir);		/* make parent directory as modified */
  if (getfe == 0 || !fc_compare(getfe,&key)) {
    /* no kept entry */
    if (!GCScan(fich, &key, &idx)) /* not in cache, already flushed */
      return;
    getfe = (FCacheEnt *)GCidx2ent(fich, idx);

  }
  if (DBOSI)
    printf("Invalidating file cache entry %s/%s\n",pathstr(pdir),fn);
  getfe->fe_okay = FALSE;		/* invalidate entry */
}

/*
 * OSValidateFICacheforEnum(directory)
 *
 * validate a file cache for enumerate
 *
 *  make fc_flush only stat if .finderinfo directory modification time
 *  has changed -- writefa guarantees us that
 * 
*/
OSValFICacheForEnum(pdir)
IDirP pdir;
{
  fc_flush_start(pdir);
  GCFlush(fich, pdir);		/* make sure valid */
  fc_flush_end();
}

/*
 * OSSetFA(IDirP pdir, char *fn, word bm, FileDirParm *fdp)
 *
 * Set finder and attr for a file specified by ancestor directory
 * (pdir) and file name (fn).  The bitmap in bm specifies which
 * FP_FINFO (DP_FINFO) or FP_ATTR (DP_ATTR) are to be set.
 *
 * Update to handle AFP 2.0 as defined by "Inside AppleTalk", 2nd Ed, p13-21
 * djh@munnari.OZ.AU, 06/12/90
 *
 */
OSSetFA(pdir,fn,bm,fdp)
IDirP pdir;     
char *fn;
word bm;
FileDirParm *fdp;
{
  FileInfo *fi;
  word attr,newattr;
  extern int sessvers;

  fi = os_getfi(pdir,fn);
  if (bm & FP_ATTR) {
    attr = fdp->fdp_attr;
    /* limit: allowed alter some bits only & protocol vers. dependant */
    newattr = (sessvers >= AFPVersion2DOT0) ? (FPA_RNI|FPA_DEI) : 0;
    attr &= (FI_ATTR_READONLY|FI_ATTR_MUSER|FI_ATTR_INVISIBLE|newattr);
    if (sessvers == AFPVersion1DOT1 && (attr & FI_ATTR_READONLY))
      attr |= (FPA_RNI|FPA_DEI);
    if (fdp->fdp_attr & FI_ATTR_SETCLEAR)
      fi->fi_attr |= attr;
    else
      fi->fi_attr &= ~attr;
  }

#ifdef USE_MAC_DATES
  if (bm & FP_CDATE) {
    fi->fi_datemagic = FI_MAGIC;
    fi->fi_datevalid |= FI_CDATE;
    bcopy(&fdp->fdp_cdate,fi->fi_ctime,sizeof(fi->fi_ctime));
  }
  if (bm & FP_MDATE) {
    time_t when;
    time(&when);
    fi->fi_datemagic = FI_MAGIC;
    fi->fi_datevalid |= FI_MDATE;
    bcopy(&when,fi->fi_utime,sizeof(fi->fi_utime));
    bcopy(&fdp->fdp_mdate,fi->fi_mtime,sizeof(fi->fi_mtime));
  }
#endif USE_MAC_DATES

  if (bm & FP_FINFO) {
    bcopy(fdp->fdp_finfo,fi->fi_fndr,FINFOLEN);
    if (!(bm & FP_PDOS))	/* setting finder info BUT NOT PRODOS */
      mapFNDR2PDOS(fdp);	/* derive suitable File and Aux types */
  }

  if (bm & FP_PDOS)
    if (!(bm & FP_FINFO))	/* setting PRODOS info BUT NOT finder */
      mapPDOS2FNDR(fdp,fi);	/* derive suitable fdCreator & fdType */

#ifdef USE_MAC_DATES
  if (bm & (FP_ATTR|FP_FINFO|FP_PDOS|FP_CDATE|FP_MDATE))
#else  USE_MAC_DATES
  if (bm & (FP_ATTR|FP_FINFO|FP_PDOS))
#endif USE_MAC_DATES
    WriteFA(pdir,fn,fi);
  return(noErr);
}

OSGetAttr(pdir,fn,attr)
IDirP pdir;
char *fn;
word *attr;
{
  FileInfo *fi;
  extern int sessvers;

  fi = os_getfi(pdir,fn);
  *attr = fi->fi_attr;
  if (sessvers == AFPVersion1DOT1) {
    if (*attr & (FPA_RNI|FPA_DEI))
      *attr |= FI_ATTR_READONLY;
    *attr &= FPA_MASK1;	/* give only AFP 1.1 Attributes */
  }
  return(noErr);
}

OSSetAttr(pdir, fn, attr)
IDirP pdir;
char *fn;
word attr;
{
  FileInfo *fi;

  fi = os_getfi(pdir, fn);
  fi->fi_attr = attr;
  WriteFA(pdir, fn, fi);
  return(noErr);
}

OSGetFNDR(pdir,fn,finfo)
IDirP pdir;
char *fn;
byte *finfo;
{
  FileInfo *fi;

  fi = os_getfi(pdir,fn);
  bcopy(fi->fi_fndr,finfo,FINFOLEN);
  return(noErr);
}

OSSetComment(pdir,fn,cs,cl)
IDirP pdir;
char *fn;
byte *cs;
byte cl;
{
  FileInfo *fi;

  fi = os_getfi(pdir,fn);
  bcopy(cs,fi->fi_comnt,cl);
  fi->fi_comln = cl;
  WriteFA(pdir,fn,fi);
  return(noErr);
}

OSGetComment(pdir,fn,cs,cl)
IDirP pdir;
char *fn;
byte *cs;
byte *cl;
{
  FileInfo *fi;
  
  fi = os_getfi(pdir,fn);
  *cl = fi->fi_comln;
  if (*cl == 0)
    *cs = 0;
  bcopy(fi->fi_comnt,cs,*cl);
  return(noErr);
}

#ifdef USE_MAC_DATES
OSGetCDate(pdir,fn,cdate)
IDirP pdir;
char *fn;
time_t *cdate;
{
  FileInfo *fi;
  
  fi = os_getfi(pdir,fn);
  if (fi->fi_datemagic != FI_MAGIC || (!(fi->fi_datevalid & FI_CDATE)))
    return(-1);
  bcopy(fi->fi_ctime,cdate,sizeof(fi->fi_ctime));
  return(noErr);
}

OSGetMDate(pdir,fn,mdate,udate)
IDirP pdir;
char *fn;
time_t *mdate;
time_t *udate;
{
  FileInfo *fi;
  
  fi = os_getfi(pdir,fn);
  if (fi->fi_datemagic != FI_MAGIC || (!(fi->fi_datevalid & FI_MDATE)))
    return(-1);
  bcopy(fi->fi_mtime,mdate,sizeof(fi->fi_mtime));
  bcopy(fi->fi_utime,udate,sizeof(fi->fi_utime));
  return(noErr);
}
#endif USE_MAC_DATES

/*
 * Establish the mac file name after a os_move
 *
*/
OSSetMacFileName(pdir, fn)
IDirP pdir;
char *fn;
{
  FileInfo *fi;
  
  fi = os_getfi(pdir,fn);
#ifdef SHORT_NAMES
  ItoEName(fn, fi->fi_macfilename);
  ItoEName_Short(pdir,fn, fi->fi_shortfilename);
#else SHORT_NAMES
  ItoEName(fn, fi->fi_macfilename);
#endif SHORT_NAMES
  WriteFA(pdir, fn, fi);
  return(noErr);
}

/*
 * mapPDOS2FNDR
 *
 * convert ProDOS File Type and Aux Type to Finder Info
 * (even to have to contemplate doing this is disgusting)
 *
 */

#define CTLEN	4

mapPDOS2FNDR(fdp,fi)
FileDirParm *fdp;
FileInfo *fi;
{
  FinderInfo *fndr;
  byte prodos_at[4];
  byte prodos_ft[2];

  if (fdp->fdp_flg == FDP_DIRFLG)	/* directory, do nothing! */
    return;

  bcopy(&fdp->fdp_prodos_aux, prodos_at, sizeof(prodos_at));
  bcopy(&fdp->fdp_prodos_ft,  prodos_ft, sizeof(prodos_ft));
  fndr = (FinderInfo *) fi->fi_fndr;

  switch (prodos_ft[0]) {
    case 0x00:
      bcopy("BINA", fndr->file.fdType, CTLEN);
      break;
    case 0xb3:
      bcopy("PS16", fndr->file.fdType, CTLEN);
      break;
    case 0xff:
      bcopy("PSYS", fndr->file.fdType, CTLEN);
      break;
    case 0x04:
      if (prodos_at[0] == 0x00 && prodos_at[1] == 0x00) {
        bcopy("TEXT", fndr->file.fdType, CTLEN);
        break;
      } /* else fall thro' */
    default:
      /* some fdTypes will be unprintable  */
      fndr->file.fdType[0] = 'p';
      fndr->file.fdType[1] = prodos_ft[0];	/* file type */
      fndr->file.fdType[2] = prodos_at[1];	/* high byte */
      fndr->file.fdType[3] = prodos_at[0];	/* low byte  */
      break;
  }
  bcopy("pdos", fndr->file.fdCreator, CTLEN);
}

/*
 * mapFNDR2PDOS
 *
 * convert Finder Info to ProDOS File Type and Aux Type
 * (even to have to contemplate doing this is disgusting)
 *
 */

mapFNDR2PDOS(fdp)
FileDirParm *fdp;
{
  FinderInfo *fndr;
  byte prodos_at[4];
  byte prodos_ft[2];
  byte hex2num();

  fndr = (FinderInfo *) fdp->fdp_finfo;

  if (fdp->fdp_flg == FDP_DIRFLG) {
    /* set specific ProDOS directory information */
    prodos_ft[0] = 0x0f;
    prodos_ft[1] = 0x00;
    prodos_at[0] = 0x00;
    prodos_at[1] = 0x02;
    prodos_at[2] = 0x00;
    prodos_at[3] = 0x00;
    bcopy(prodos_ft, &fdp->fdp_prodos_ft,  sizeof(prodos_ft));
    bcopy(prodos_at, &fdp->fdp_prodos_aux, sizeof(prodos_at));
    return;
  }
  if (bcmp("TEXT", fndr->file.fdType, CTLEN) == 0) {
    prodos_ft[0] = 0x04;
    prodos_ft[1] = 0x00;
    bzero(&fdp->fdp_prodos_aux, sizeof(prodos_at));
    bcopy(prodos_ft, &fdp->fdp_prodos_ft, sizeof(prodos_ft));
    return;
  }
  if (bcmp("pdos", fndr->file.fdCreator, CTLEN) == 0) {
    if (bcmp("PSYS", fndr->file.fdType, CTLEN) == 0) {
      prodos_ft[0] = 0xff;
      prodos_ft[1] = 0x00;
      bcopy(prodos_ft, &fdp->fdp_prodos_ft, sizeof(prodos_ft));
      return;
    }
    if (bcmp("PS16", fndr->file.fdType, CTLEN) == 0) {
      prodos_ft[0] = 0xb3;
      prodos_ft[1] = 0x00;
      bcopy(prodos_ft, &fdp->fdp_prodos_ft, sizeof(prodos_ft));
      return;
    }
    if (bcmp("BINA", fndr->file.fdType, CTLEN) == 0) {
      prodos_ft[0] = 0x00;
      prodos_ft[1] = 0x00;
      bcopy(prodos_ft, &fdp->fdp_prodos_ft, sizeof(prodos_ft));
      return;
    }
    if (fndr->file.fdType[0] == 'p') {	/* bizarre */
      prodos_ft[0] = fndr->file.fdType[1];
      prodos_ft[1] = 0x00;
      prodos_at[0] = fndr->file.fdType[3];	/* low byte */
      prodos_at[1] = fndr->file.fdType[2];	/* high byte */
      prodos_at[2] = 0x00;
      prodos_at[3] = 0x00;
      bcopy(prodos_ft, &fdp->fdp_prodos_ft,  sizeof(prodos_ft));
      bcopy(prodos_at, &fdp->fdp_prodos_aux, sizeof(prodos_at));
      return;
    }
    if (fndr->file.fdType[2] == ' ' && fndr->file.fdType[3] == ' ') {
      short num;	/* much more bizarre */
      num  = hex2num(fndr->file.fdType[0]) << 4;
      num |= hex2num(fndr->file.fdType[1]) & 0xf;
      prodos_ft[0] = (byte) num & 0xff;
      prodos_ft[1] = 0x00;
      bcopy(prodos_ft, &fdp->fdp_prodos_ft,  sizeof(prodos_ft));
      return;
    }
  }
  /* otherwise */
  prodos_ft[0] = 0x00;
  prodos_ft[1] = 0x00;
  bzero(&fdp->fdp_prodos_aux, sizeof(prodos_at));
  bcopy(prodos_ft, &fdp->fdp_prodos_ft, sizeof(prodos_ft));
}

byte
hex2num(ch)
char ch;
{
  if (ch >= '0' && ch <= '9')
    return(ch - '0');
  if (ch >= 'a' && ch <= 'f')
    return(ch - 'a' + 10);
  if (ch >= 'A' && ch <= 'F')
    return(ch - 'A' + 10);
  return(0);
}
