/*
 * $Author: djh $ $Date: 1996/06/19 04:29:14 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpdt.c,v 2.15 1996/06/19 04:29:14 djh Rel djh $
 * $Revision: 2.15 $
 *
 */

/*
 * afpdt.c - Appletalk Filing Protocol Desktop Interface
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
 *
 */

/*
 * Desktop management routines:
 *
 * FPOpenDT		- Open the icon desktop.
 * FPCloseDT		- Close the icon desktop.
 * FPAddIcon		- Add an icon bitmap to the desktop.
 * FPGetIcon		- Retrieve an icon bitmap from the desktop.
 * FPGetIconInfo 	- Retrieve icon info from the desktop.
 * FPAddAPPL		- Add application info to desktop.
 * FPRemoveAPPL		- Remove application info from desktop.
 * FPGetAPPL		- Retrieve application info from desktop.
 * FPAddComment		- Add comment info to the desktop.
 * FPRemoveComment	- Remove comment info from the desktop.
 * FPGetComment		- Retrieve comment info from the desktop.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#ifndef _TYPES
 /* assume included by param.h */
# include <sys/types.h>
#endif  _TYPES
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
#endif NEEDFCNTLDOTH
#ifdef SOLARIS
#include <unistd.h>
#endif SOLARIS
#ifdef linux
#include <unistd.h>
#endif linux
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include "afpntoh.h"
#include "afpgc.h"
#include "afps.h"
#include "afpdt.h"
#include "afpudb.h"


#ifdef DEBUG_AFP_CMD
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

import int errno;

private DeskTop *DeskTab[MAXVOLS]; /* table of open desktops by volume */

private APPLNode *dtBindFCtoAPPL();
private IconNode *dtBindFCtoIcon();
private APPLNode *dtFindAPPLList();
private IconNode *dtFindIconList();
private IconNode *dtIconInsertPoint();
private IconNode *dtIconInsert();
private APPLNode *dtAPPLInsertPoint();
private APPLNode *dtAPPLInsert();
private byte *IDFetch();
private void CacheAdd();

#define APPLFILEMODE 0644
#define ICONFILEMODE 0664

#define REMOVEAPPLIDB 0
#define REMOVEICONIDB 1

private ReadIDesk(), ReadADesk();

#ifdef AUFS_README
import char *aufsreadme;
import char *aufsreadmename;
#endif AUFS_README

/*
 * PrintINode(AVLUData *node)
 *
 * Print out information about the Icon node pointed to by node.
 *
 */

private void
PrintINode(in)
IconNode *in;
{
  char cbuff[5],tbuff[5];
  IconInfo *ii = &in->in_info;		/* handle on icon info */
  
  strncpy(cbuff,(char *)ii->i_FCreator,4);
  cbuff[4] = '\0';
  strncpy(tbuff,(char *)ii->i_FType,4);
  tbuff[4] = '\0';
  printf("IconNode Creator=%s Type=%s IType=%d iloc=%d bmsize=%d\n",
	 cbuff,tbuff,ii->i_IType,in->in_iloc,ii->i_bmsize);
}

PrintIconInfo(fc, ft)
byte fc[], ft[];
{
  printf("Icon Info: Creator = %c%c%c%c, Type = %c%c%c%c\n",
	 fc[0], fc[1], fc[2], fc[3], ft[0], ft[1], ft[2], ft[3]);
}

/*
 * PrintANode(AVLUData *node)
 *
 * Print out information about the APPL node pointed to by node.
 *
 */

private void
PrintANode(udata)
AVLUData *udata;
{
  char cbuff[5];
  APPLNode *an = (APPLNode *) udata;	/* cast to APPL record */
  APPLInfo *ai = &an->an_info;		/* handle on APPL info */

  strncpy(cbuff,(char *)ai->a_FCreator,4);
  cbuff[4] = '\0';
  printf("APPLNode node Creator=%s path=%s name=%s\n",
	 cbuff,pathstr(an->an_ipdir),an->an_fnam);
}

/*
 * int OpenDesk(DeskTop *dt, char *dtfn, int trunc, int mode, int *wrtok)
 *
 * Open or create a desktop as specified by the path in dt->dt_rootd
 * and the desktop file name, dtfn.
 *
 * First try opening in read/write mode.  If that fails because of
 * an access error then try opening read only.  If read/write fails 
 * because the file does not exist then try creating the file.
 *
 * Return the file handle or < 0, and set wrtok to TRUE if we obtained
 * write access on this file.
 *
 * Top level volume must have ".finderinfo" directory or we
 * won't create the files.
 *
*/
private int
OpenDesk(dt,dtfn,trunc,mode,wrtok)
DeskTop *dt;
char *dtfn;
int trunc;			/* if true, then open in truncate mode */
int *wrtok;				/* if file is writeable */
int mode;			/* mode to open file in */
{
  int dtfd;
  char path[MAXPATHLEN];
  int fmode;
#ifdef AUFS_README
  char path2[MAXPATHLEN];
#endif AUFS_README

  *wrtok = TRUE;			/* assume we can write */
  OSfname(path,dt->dt_rootd,dtfn,F_DATA); /* create file name */
  if (DBDSK)
    printf("OpenDesk: opening %s\n",path);
  
  fmode = O_RDWR;
#ifdef O_TRUNC
  if (trunc) {
    if (DBDSK)
      printf("OpenDesk: truncating %s\n",dtfn);
    fmode |= O_TRUNC;
  }
#endif
  if ((dtfd=open(path,fmode)) >= 0)
    return(dtfd);			/* success! */

  if (errno == EACCES || errno == EROFS) { /* access error? */
    *wrtok = FALSE;			/* indicate for caller no write */
    return(open(path,O_RDONLY));	/* just try reading */
  }
#ifdef AUFS_README
  if(aufsreadme && strcmp(dtfn, DESKTOP_APPL) == 0 &&
   access(aufsreadme, R_OK) == 0) {
    OSfname(path2,dt->dt_rootd,aufsreadmename,F_DATA);
    if(access(path2, F_OK) < 0 && errno == ENOENT)
      if(symlink(aufsreadme, path2) < 0) {
	if (DBDSK)
	  printf("OpenDesk: symbolic link %s failed\n",path2);
      } else {
	if (DBDSK)
	  printf("OpenDesk: linking %s\n",aufsreadme);
      }
  }
#endif AUFS_README

  if ((dt->dt_rootd->flags & DID_FINDERINFO) == 0)
    return(-1);

  dtfd = open(path,O_RDWR|O_CREAT, mode); /* try creating*/
  if (dtfd < 0)				/* did we open anything? */
    *wrtok = FALSE;			/* if not then can't write */
  return(dtfd);
}

/*
 * Intialize desktop manager for particular volume
 *
*/
private
DTInit(dt)
DeskTop *dt;
{
  int wok;				/* desktop is writeable flag */
  int err;

  dt->dt_avlroot = (AVLNode *) 0;
  dt->dt_avllast = (AVLNode *) 0; 
  dt->dt_ifd = -1;
  dt->dt_afd = -1;
  dt->dt_rootd = VolRootD(dt->dt_ivol);

#ifdef STAT_CACHE
  OScd(pathstr(dt->dt_rootd));
#endif STAT_CACHE

  dt->dt_afd = OpenDesk(dt,DESKTOP_APPL,FALSE,APPLFILEMODE,&wok); /* open or create */
  if (dt->dt_afd >= 0) {		/* success on open? */
    OSLockFileforRead(dt->dt_afd);
    err = ReadADesk(dt);	/* yes... read APPL database */
    OSUnlockFile(dt->dt_afd);
    (void)close(dt->dt_afd);		/* close APPL data base */
    if (err < 0) {
      DeskRemove(dt, REMOVEAPPLIDB);
      if (wok) {
	err = OpenDesk(dt,DESKTOP_APPL, TRUE, APPLFILEMODE, &wok);
	close(err);
      }
    }
    dt->dt_afd = -1;			/* no longer here... */
  }

  dt->dt_ifd = OpenDesk(dt,DESKTOP_ICON, FALSE,ICONFILEMODE,&wok);
  if (dt->dt_ifd >= 0) {		/* success on open? */
    OSLockFileforRead(dt->dt_ifd);
    err = ReadIDesk(dt);	/* yes... read Icon database  */
    OSUnlockFile(dt->dt_ifd);
    if (err < 0) {
      close(dt->dt_ifd);
      dt->dt_ifd = -1;
      DeskRemove(dt, REMOVEICONIDB);
      if (wok)
	dt->dt_ifd = OpenDesk(dt,DESKTOP_ICON, TRUE,ICONFILEMODE,&wok);
    }
#ifdef notdef
    /* yes, there is - geticon uses this fd */
    if (!wok) {				/* writeable? */
      (void)close(dt->dt_ifd);		/* no, so no reason to keep open */
      dt->dt_ifd = -1;
    }
#endif
  }
  unixiconalways(dt);
  return(noErr);
}

unixiconalways(dt)
DeskTop *dt;
{
  IconNode *in,*replaced;
  extern struct ufinderdb uf[];
  extern int uf_len;
  int i;
  
  in = dtBindFCtoIcon(dt, (byte *)DEFFCREATOR);
  for (i = 0 ; i < uf_len; i++) {
    if (uf[i].ufd_icon == NULL)
      continue;
    in = dtIconInsert(in, uf[i].ufd_ftype, (byte)1, &replaced);
    /* should only be called before desktop is inited */
    in->in_iloc = -1;		/* no file pos! */
    in->in_mloc = NOGCIDX;	/* no icon index */
    in->in_uloc = uf[i].ufd_icon; /* remember icon */
    in->in_info.i_bmsize = uf[i].ufd_iconsize; /* icon size */
    in->in_info.i_ITag[0] = 0x12;
  }
}

/*
 * ReadIDesk - Initialize Icon Desktop.
 *
 */
private
ReadIDesk(dt)
DeskTop *dt; 
{
  IconFileRecord ifr;
  IconNode *in, *replaced, *inhead;
  IconInfo *ii;
  int cnt,badcnt;
  off_t bmz,fpos;

  if (dt->dt_ifd < 0)
    return(0);

  (void)lseek(dt->dt_ifd, 0L, 0);		/* got to start */
  for (fpos = 0, badcnt = 0;;) { /* read length of record */
    cnt = read(dt->dt_ifd, (char *)&ifr,IFRLEN);
    if (cnt != IFRLEN)
      break;
    fpos += IFRLEN;
    if (ntohl(ifr.ifr_magic) != IFR_MAGIC) {
      if (badcnt > 10) {
	printf("ICON Desktop corrupt or out of revision\n");
	return(-1);
      }
      printf("ICON Desktop entry has bad magic number... skipping\n");
      badcnt++;
      return(-1);
    }
    /* find the head of our icon list */
    ii = &ifr.ifr_info;
    ii->i_bmsize = ntohl(ii->i_bmsize);
    inhead = dtBindFCtoIcon(dt, ii->i_FCreator);
    /* now go through list and find place to insert and insert if */
    /* we can */
    in = dtIconInsert(inhead, ii->i_FType, ii->i_IType, &replaced);
    /* what to do with replaced entry? (possibly place on free list?) */
    in->in_iloc = fpos;			/* index to find bm in file */
    in->in_mloc = NOGCIDX;		/* no cache index for icon */
    in->in_uloc = NULL;
    bcopy((char *)ii, (char *)&in->in_info, sizeof(IconInfo));
    bmz = in->in_info.i_bmsize;		/* this is the bitmap size */
    (void)lseek(dt->dt_ifd,bmz,L_INCR);	/* skip over bitmap */
    fpos += bmz;			/* increment past bml */
    if (DBDSK) {
      printf("ReadIDesk: Loading ");
      PrintINode(in);
    }
  }
  return(0);
}

private  
ReadADesk(dt)				/* read APPL database */
DeskTop *dt; 
{
  APPLFileRecord afr;			/* file format APPL info */
  APPLNode *an, *ahead, *replaced;
  char pdir[MAXPATHLEN];		/* create parent dir here */
  int len,cnt;
  off_t floc;
  IDirP ipdir;
  char *fnam;
  
  if (dt->dt_afd < 0)
    return(0);

  (void)lseek(dt->dt_afd, 0L, 0);		/* seek to home */
  /* directory string in APPLFileRecord is relative to volumes rootd */
  strcpy(pdir,pathstr(dt->dt_rootd));	/* copy volumes rootd */
  len = strlen(pdir);			/* fetch the length of this */

  while ((cnt = read(dt->dt_afd, (char *)&afr,AFRLEN)) == AFRLEN) {
    if (ntohl(afr.afr_magic) != AFR_MAGIC) {
      if (DBDSK) {
	printf("ReadADesk: APPL Desktop out of revision or bad\n");
	printf("ReadADesk: Skipping rest...\n");
      }
      return(-1);
    }
    /* remember where we are */
    floc = lseek(dt->dt_afd, 0L, 1) - ((off_t)sizeof(afr));
    afr.afr_pdirlen = ntohl(afr.afr_pdirlen);
    afr.afr_fnamlen = ntohl(afr.afr_fnamlen);

    if (afr.afr_pdirlen > 0)
      pdir[len] = '/';			/* deposit a directory term */
    else pdir[len] = '\0';
    /* Get directory component */
    cnt = read(dt->dt_afd, &pdir[len+1],afr.afr_pdirlen);
    if (cnt != afr.afr_pdirlen) {
      if (DBDSK)
	printf("ReadADesk: unable to read directory name\n");
      return(-1);
    }

    ipdir = Idirid(pdir);	/* find or create dirid  */

    fnam = malloc((unsigned)afr.afr_fnamlen+1);
    cnt = read(dt->dt_afd, fnam, afr.afr_fnamlen);
    if (cnt != afr.afr_fnamlen) {
      printf("ReadADesk: unable to read filename\n");
      free(fnam);
      return(-1);
    }
    if (ipdir == NILDIR) {
      free(fnam);
      continue;
    }
    if (DBDSK)
      printf("DeskARead: dir = %s, fnam = %s\n", pathstr(ipdir),fnam);

    ahead = dtBindFCtoAPPL(dt, afr.afr_info.a_FCreator);
    an = dtAPPLInsert(ahead, ipdir, fnam, &replaced);
    if (replaced) {
      /* should free space */
#ifdef notdef
      if (DBDSK)
	printf("DeskARead: warning: old undeleted entry: %s/%s\n",
	       pathstr(ipdir), fnam);
#endif
    }
    /* ipdir, fnam, and next already done */
    an->an_flags = 0;
    bcopy((char *)&afr.afr_info,(char *)&an->an_info,sizeof(APPLInfo));
    an->an_iloc = floc; /* remember file position */
  }
  return(0);
}
  
/*
 * OSErr FPAddComment(...)
 *
 * This call adds a comment for a file or directory to the desktop
 * database.
 *
 * Inputs:
 *   dtrefnum		desktop refnum.
 *   dirid		ancestor directory id.
 *   pathType		Long/short path indicator. 
 *   path		pathname to the file or directory.
 *   clen		Length of the comment.
 *   ctxt		text of the comment.
 *
 * Errors: 
 *   ParamErr		Unknown desktop refnum, bad pathname.
 *   ObjectNotFound	Input params do not point to an existing file.
 *   AccessDenied	User does not have access.
 *
 * If the comment is greater than 199 bytes then the comment will be
 * truncated to 199 bytes.
 *
 */
 
/*ARGSUSED*/
OSErr
FPAddComment(p,l,r,rl)
byte *p,*r;
int *rl;
int l;
{
  AddCommentPkt adc;
  IDirP idir,ipdir;
  int err;
  char file[MAXUFLEN];
  int ivol;
  DeskTop *dt;

  if (DBDSK)
    printf("FPAddComment: ...\n");

  ntohPackX(PsAddComment,p,l,(byte *) &adc);
  if ((dt = VolGetDesk(adc.adc_dtrefnum)) == NILDT)
    return(aeParamErr);

  err = EtoIfile(file,&idir,&ipdir,&ivol,adc.adc_dirid,
		 dt->dt_evol,adc.adc_ptype,adc.adc_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    int i;
    void dbg_print_path();
    fprintf(dbg, "\tDTRef: %d\n", adc.adc_dtrefnum);
    fprintf(dbg, "\tDirID: %08x\n", adc.adc_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", adc.adc_ptype,
      (adc.adc_ptype == 1) ? "Short" : "Long");
    dbg_print_path(adc.adc_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fprintf(dbg, "\tComnt: (len %d) \"", *adc.adc_comment);
    for (i = 0; i < (int)*adc.adc_comment; i++)
      fprintf(dbg, "%c", *(adc.adc_comment+i+1));
    fprintf(dbg, "\"\n");
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  err = OSSetComment(ipdir,file,adc.adc_comment+1,adc.adc_comment[0]);
  return(err);
}

OSErr 
FPGetComment(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  GetCommentPkt gcm;
  IDirP idir,ipdir;
  int err,ivol;
  char file[MAXUFLEN];
  DeskTop *dt;
  
  ntohPackX(PsGetComment,p,l,(byte *) &gcm);
  if ((dt = VolGetDesk(gcm.gcm_dtrefnum)) == NILDT)
    return(aeParamErr);

  err = EtoIfile(file,&idir,&ipdir,&ivol,gcm.gcm_dirid,
		 dt->dt_evol,gcm.gcm_ptype,gcm.gcm_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tDTRef: %d\n", gcm.gcm_dtrefnum);
    fprintf(dbg, "\tDirID: %08x\n", gcm.gcm_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", gcm.gcm_ptype,
      (gcm.gcm_ptype == 1) ? "Short" : "Long");
    dbg_print_path(gcm.gcm_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  err = OSGetComment(ipdir,file,r+1,r);
  if (err != noErr)
    return(err);
  *rl = pstrlen(r)+1;
  if (DBDSK)
    printf("FPGetComment: returns comment of length %d\n", *rl);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_name();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tComnt: (len %d)", *rl);
    dbg_print_name("", r);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * OSErr FPAddAPPL(...)
 *
 * This call adds an APPL mapping to the desktop database.
 *
 * Inputs:
 *   dtrefnum		Desktop reference number.
 *   dirid		Ancestor directory id
 *   Creator		4 byte creator
 *   APPLTag		4 byte user tag, to be stored with the APPL
 *   pathType		Long/short path
 *   path		pathname to the application being added.
 *
 * Errors:
 *   ObjectNotFound	Input params do not point to an existing file.
 *   AccessDenied	User does not have access.
 *
 * An entry is added to the APPL desktop.  If an entry for the
 * application exists (same creator, file and directory) then it
 * is replaced.
 *
 */
/*ARGSUSED*/
OSErr
FPAddAPPL(p,l,r,rlen)
byte *p;
byte *r;
int l;
int *rlen;
{
  AddAPPLPkt aap;
  APPLNode *an,*replaced, *ahead;
  DeskTop *dt;
  int ivol,err;
  IDirP idir,ipdir;
  char file[MAXUFLEN], *fnam;

  ntohPackX(PsAddAPPL,p,l,(byte *) &aap);
  dt = VolGetDesk(aap.aap_dtrefnum);
  if (dt == NILDT)
    return(aeParamErr);

  err = EtoIfile(file,&idir,&ipdir,&ivol,aap.aap_dirid,
		 dt->dt_evol,aap.aap_ptype,aap.aap_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    void dbg_print_path();
    fprintf(dbg, "\tDTRef: %d\n", aap.aap_dtrefnum);
    fprintf(dbg, "\tDirID: %08x\n", aap.aap_dirid);
    dbg_print_type("\tCreat:", aap.aap_fcreator);
    fprintf(dbg, "\tApTag: %08x\n", aap.aap_apptag);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", aap.aap_ptype,
      (aap.aap_ptype == 1) ? "Short" : "Long");
    dbg_print_path(aap.aap_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  ahead = dtBindFCtoAPPL(dt, aap.aap_fcreator);
  fnam = malloc((unsigned)strlen(file)+1);
  /* assert(fnam != NULL) */
  strcpy(fnam, file);			/* copy it */
  an = dtAPPLInsert(ahead, ipdir, fnam, &replaced);
  bcopy((char *)aap.aap_fcreator,(char *)an->an_info.a_FCreator,4);
  bcopy((char *)&aap.aap_apptag,(char *)an->an_info.a_ATag,4);
  an->an_flags = AN_MOD;		/* modified, but not deleted */
#ifdef notdef
  /* already done */
  an->an_ipdir = ipdir;
  an->an_iloc = -1;
#endif

  if (DBDSK) {
    char fc[5];
    bcopy((char *)an->an_info.a_FCreator,(char *)fc,4);
    fc[4] = '\0';
    printf("FPAddAPPL: path=%s, file=%s, Creator=%s\n",
	   pathstr(an->an_ipdir),an->an_fnam,fc);
  }


  if (replaced != NULL) {		/* just inserted, naught else */
    /* proably should have better handling */
    free((char *)replaced->an_fnam);
    free((char *)replaced);
  }
  return(noErr);
}

/*
 * OSErr FPGetAPPL(...)
 *
 * This call is used to retrieve information about a particular 
 * application from the desktop database.
 *
 * Inputs:
 *   dtrefnum		desktop reference number.
 *   fcreator		4 bytes of file creator
 *   APPLIdx		index of the APPL entry to be retrieved.
 *   bitmap		bitmap of parms to be returned for file, same
 *			as FPGetFileDirParms call.
 *
 * Errors:
 *   ParamErr
 *   ObjectNotFound
 *   AccessDenied
 *
 *
 * Outputs:
 *   Bitmap
 *   APPLTag
 *   File Parameters
 *
 */
OSErr
FPGetAPPL(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  int aidx;
  GetAPPLPkt gap;
  APPLNode *ahead;
  DeskTop *dt;
  FileDirParm fdp;
  dword appltag;
  int err;

  ntohPackX(PsGetAPPL,p,l,(byte *) &gap);
  
  if ((dt = VolGetDesk(gap.gap_dtrefnum)) == NILDT)
    return(aeParamErr);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    void dbg_print_bmap();
    fprintf(dbg, "\tDTRef: %d\n", gap.gap_dtrefnum);
    dbg_print_type("\tCreat:", gap.gap_fcreator);
    fprintf(dbg, "\tAPidx: %d\n", gap.gap_applidx);
    fprintf(dbg, "\tBtMap: %04x\t", gap.gap_bitmap);
    dbg_print_bmap(gap.gap_bitmap, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBDSK) {
    char fc[5];
    bcopy((char *)gap.gap_fcreator,(char *)fc,4);
    fc[4] = '\0';

    printf("FPGetAPPL: FCreator=%s, Idx=%d\n",fc,gap.gap_applidx);
  }    

  if ((ahead = dtFindAPPLList(dt, gap.gap_fcreator)) == NULL)
    return(aeItemNotFound);
  /* "an APPL index of zero returns the first APPL mapping" */
  aidx = (gap.gap_applidx == 0) ? 1 : gap.gap_applidx;
  while ((ahead = ahead->an_next) != NULL)
    if ((ahead->an_flags & AN_DEL) == 0 && --aidx == 0)
      break;
  while (ahead != NULL)  {
    if (ahead->an_flags & AN_DEL)
      err = aeItemNotFound;
    else
      err = OSAccess(ahead->an_ipdir,ahead->an_fnam,OFK_MRD);
    if (err == noErr)
      break;				/* found readable entry */
    if (DBDSK)
      printf("AFPGetAPPL: No Access to %s %s error %s\n",
	     pathstr(ahead->an_ipdir),ahead->an_fnam,afperr(err));
    ahead = ahead->an_next;
  }
  if (ahead == NULL)
    return(aeItemNotFound);

  if (DBDSK)
    printf("FPGetAPPL: Found path=%s, file=%s\n",
	   pathstr(ahead->an_ipdir),ahead->an_fnam);

  fdp.fdp_pdirid = ItoEdirid(ahead->an_ipdir,dt->dt_ivol);
  fdp.fdp_fbitmap = gap.gap_bitmap;
  fdp.fdp_dbitmap = 0;

  err = OSFileDirInfo(ahead->an_ipdir,NILDIR,ahead->an_fnam,&fdp,dt->dt_ivol);

  if (err != noErr)
    return(aeItemNotFound);

  if (FDP_ISDIR(fdp.fdp_flg))		/* should not happen */
    return(aeItemNotFound);

  bcopy((char *)ahead->an_info.a_ATag, (char *)&appltag, 4);
  PackWord(fdp.fdp_fbitmap, r);
  PackDWord(appltag, r+2);
  *rl = 6 + htonPackX(FilePackR, (byte *)&fdp, r+6);

  if (DBDSK)
    printf("FPGetAPPL: ...\n");

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tBtMap: %04x\t", fdp.fdp_fbitmap);
    dbg_print_bmap(fdp.fdp_fbitmap, 0);
    fprintf(dbg, "\tAPTag: %08x\n", appltag);
    dbg_print_parm(fdp.fdp_fbitmap, r+6, (*rl)-6, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*ARGSUSED*/
OSErr
FPRmvAPPL(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  RemoveAPPLPkt rma;
  DeskTop *dt;
  APPLNode *ahead, *found;
  char file[MAXUFLEN];
  IDirP idir,ipdir;
  int ivol,err;

  ntohPackX(PsRmvAPPL,p,l,(byte *) &rma);

  if ((dt = VolGetDesk(rma.rma_refnum)) == NILDT)
    return(aeParamErr);

  err = EtoIfile(file,&idir,&ipdir,&ivol,rma.rma_dirid,
		 dt->dt_evol,rma.rma_ptype,rma.rma_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    void dbg_print_path();
    fprintf(dbg, "\tDTRef: %d\n", rma.rma_refnum);
    fprintf(dbg, "\tDirID: %08x\n", rma.rma_dirid);
    dbg_print_type("\tCreat:", rma.rma_fcreator);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", rma.rma_ptype,
      (rma.rma_ptype == 1) ? "Short" : "Long");
    dbg_print_path(rma.rma_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  ahead = dtFindAPPLList(dt, rma.rma_fcreator);
  if (ahead == NULL)
    return(aeItemNotFound);
  ahead = dtAPPLInsertPoint(ahead, ipdir, file, &found);
  if (found != (APPLNode *) 0) {		/* found something... */
    found->an_flags |= (AN_DEL|AN_MOD);	/* indicate it is deleted */
    if (DBDSK)
      printf("FPRmvAPPL: Removing %s\n",ahead->an_fnam);
    return(noErr);
  }
  if (DBDSK)
    printf("FPRmvAPPL: No name %s found for remove\n",file);
  return(aeObjectNotFound);
}


/*
 * OSErr FPOpenDT(...);
 *
 */

OSErr
FPOpenDT(p,l,r,rl) 
byte *p,*r;
int l;
int *rl;
{
  OpenDTPkt odt;
  DeskTop *dt;
  int ivol;
  OSErr dterr;

  ntohPackX(PsOpenDT,p,l,(byte *) &odt);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tVolID: %04x\n", odt.odt_volid);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  ivol = EtoIVolid(odt.odt_volid);
  if (ivol < 0)
    return(ivol);
    
  if (DBDSK)
    printf("FPOpenDT: ...\n");

  if ((dt = VolGetDesk(odt.odt_volid)) == NILDT) {
    dt = (DeskTop *) malloc(sizeof(DeskTop));
    dt->dt_ivol = ivol;
    dt->dt_evol = odt.odt_volid;
    if ((dterr = DTInit(dt)) != noErr) {	/* init desktop */
      free((char *)dt);
      return(dterr);
    }
  }
  VolSetDesk(odt.odt_volid,dt);	/* tell volume about desktop */
  PackWord(odt.odt_volid,r);		/* return volid as DTRefnum */
  *rl = sizeof(word);			/* length of result */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tDTRef: %d\n", odt.odt_volid);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);			/* all done */
}

/*
 * FPCloseDT(...)
 *
 * This call is used to disassociate a user from the volume's desktop
 * database.
 *
 * Inputs:
 *   dtrefnum		Desktop reference number.
 *
 * Errors:
 *   ParamErr		unknown desktop reference number.
 * 
 *
 */

/*ARGSUSED*/
OSErr
FPCloseDT(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  DeskTop *dt;
  CloseDTPkt cdt;

  ntohPackX(PsCloseDT,p,l,(byte *) &cdt);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tDTRef: %d\n", cdt.cdt_dtrefnum);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBDSK)
    printf("FPCloseDT: ...\n");

  if ((dt = VolGetDesk(cdt.cdt_dtrefnum)) == NILDT)	/* fetch desk */
    return(aeParamErr);

  VolClrDesk(cdt.cdt_dtrefnum);		/* release volume's handle */
					/* need to write out stuff */
  if (dt->dt_ifd >= 0) {		/* if open, then */
    (void)close(dt->dt_ifd);		/* close desktop file */
    dt->dt_ifd = -1;
  }
#ifdef notdef
  /* really need to map thorugh and release all storage */
  free((char *)dt);			/* release desktop storage */
#endif					/* need to release tree */
  return(noErr);
}

private
WriteAPPL(dt,an)
DeskTop *dt;
APPLNode *an;
{
  APPLFileRecord afr;
  char *pstr;
  int fd;
  off_t pos;
  int pdirlen, fnamlen;

  if (DBDSK)
    printf("WriteAPPL: writing APPL\n");
  
  if ((fd = dt->dt_afd) < 0) {
#ifdef notdef
    /* code to be used when rewritting entire file */
    fd = OpenDesk(dt, DESKTOP_APPL, FALSE, APPLFILEMODE, &wok);
    if (fd < 0) {
      if (DBDSK)
	printf("Can't open desktop\n");
      return;
    }
    if (!wok) {
      if (DBDSK)
	printf("Desktop is write protected\n");
      (void)close(fd);
      return;			/* can't */
    }
#else
    return;
#endif
  }
  /* copy APPL info */
  bcopy((char *)&an->an_info,(char* )&afr.afr_info, sizeof(APPLInfo));
  afr.afr_magic = htonl(AFR_MAGIC); /* insert the magic number */    
  pstr = (char *)ppathstr(dt->dt_rootd,an->an_ipdir);
  pdirlen = strlen(pstr)+1;
  afr.afr_pdirlen = htonl(pdirlen); /* length of path */
  fnamlen = strlen(an->an_fnam)+1;
  afr.afr_fnamlen = htonl(fnamlen); /* length of file */

  OSLockFileforWrite(fd);
  /* shouldn't keep going after an error! */
  if (an->an_iloc < 0) {
    if ((pos = lseek(fd, 0L, L_XTND)) < 0L) {
      printf("WriteAPPL failed\n");
      OSUnlockFile(fd);
      return;
    }
  } else {
    if ((pos=lseek(fd, an->an_iloc, L_SET)) < 0L) {
      printf("WriteAPPL failed\n");
      OSUnlockFile(fd);
      return;
    }
  }

  if (write(fd,(char *)&afr,AFRLEN) != AFRLEN) { /* write the file record */  
    printf("WriteAPPL failed on afr\n");
    OSUnlockFile(fd);
    return;
  }
  if (write(fd,pstr,pdirlen) != pdirlen) { /* write dir */
    printf("WriteAPPL failed on path\n");
    OSUnlockFile(fd);
    return;
  }
  /* write file name */
  if (write(fd,an->an_fnam,fnamlen) != fnamlen) {
    printf("WriteAPPL failed on file\n");
    OSUnlockFile(fd);
    return;
  }
  OSUnlockFile(fd);
  an->an_iloc = pos;
  an->an_flags &= ~(AN_MOD);	/* not modified anymore */
}

private
WriteIcon(fd,in,icon,ilen)
int fd;
IconNode *in;
byte *icon;
int ilen;
{
  off_t pos, start;
  IconFileRecord ifr;

  if (DBDSK)
    printf("WriteIcon: writing icon\n");

  in->in_info.i_bmsize = ilen;		/* set bitmap size */
  if (fd < 0)
    return;

  OSLockFileforWrite(fd);
  if (in->in_iloc < 0) {
    if ((pos = lseek(fd, 0L, L_XTND)) < 0L) {		/* seek to end */
      in->in_iloc = -1;			/* mean no disk icon (default) */
      OSUnlockFile(fd);
      return;
    }
  } else {
    start = in->in_iloc - ((off_t)IFRLEN);
    if ((pos = lseek(fd, start, L_SET)) < 0L) { /* seek to pos */
      in->in_iloc = -1;			/* mean no disk icon (default) */
      OSUnlockFile(fd);
      return;
    }
  }
  /* copy the icon information to file record */
  bcopy((char *)&in->in_info, (char *)&ifr.ifr_info, sizeof(IconInfo));
  ifr.ifr_magic = htonl(IFR_MAGIC); /* for checking on read-in */
  ifr.ifr_info.i_bmsize = htonl(ifr.ifr_info.i_bmsize);
  if (write(fd, (char *)&ifr, IFRLEN) != IFRLEN) { /* write the file record */
    in->in_iloc = -1;			/* mean no disk icon (default) */
    printf("WriteIcon failed\n");
    OSUnlockFile(fd);
    return;
  }
 /* write out the icon */
  if (write(fd,(char *)icon,ilen) != ilen) {
    in->in_iloc = -1;			/* mean no disk icon (default) */
    printf("WriteIcon icon failed\n");
    OSUnlockFile(fd);
    return;
  }
  in->in_iloc = pos+IFRLEN;		/* location of icon */
  OSUnlockFile(fd);
}

/*
 * OSErr FPAddIcon(...)
 * 
 * This call is used to add an icon to the desktop database.
 *
 * Inputs:
 *   dtrefnum		desktop reference number.
 *   fcreator		4 bytes of file creator
 *   ftype		4 bytes of file type
 *   icontype		type of icon being added.
 *   icontag		4 bytes of user information associated with icon.
 *   bitmapsize		size of the bitmap for this icon.
 *
 * Errors: 
 *   ParamErr		unknown desktop refnum.
 *   IconTypeErr	new icon size is different from existing icon size.
 *   AccessDenied	User does not have access.
 *
 * A new icon is added to the icon database with the specified file type
 * and creator.  If an icon with the same file type, creator and icontype
 * already exists then that icon is replaced.  If the new icons size is
 * different from the old icon size then IconTypeErr is returned.
 *
 * The bitmap is sent to the server in a subsequent exchange of Session
 * Protocol packets.
 *
 */

/*ARGSUSED*/
OSErr
FPAddIcon(p,l,r,rl,cno,reqref)
byte *p,*r;
int l;
int *rl;
int cno;
ReqRefNumType reqref;
{
  DeskTop *dt;
  AddIconPkt adi;
  IconInfo *ii;
  IconNode *in,*ihead, *replaced;
  byte *icon, *oldicon;
  int rcvlen,comp,err;
  
  ntohPackX(PsAddIcon,p,l,(byte *) &adi);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    fprintf(dbg, "\tDTRef: %d\n", adi.adi_dtref);
    dbg_print_type("\tCreat:", adi.adi_fcreator);
    dbg_print_type("\tFlTyp:", adi.adi_ftype);
    fprintf(dbg, "\tIcTyp: %d\n", adi.adi_icontype);
    fprintf(dbg, "\tIcTag: %d\n", adi.adi_icontag);
    fprintf(dbg, "\tBMSiz: %d\n", adi.adi_iconsize);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((dt = VolGetDesk(adi.adi_dtref)) == NULL)
    return(aeParamErr);
  
  ihead = dtBindFCtoIcon(dt, adi.adi_fcreator);
  /* assert ihead != NULL */
  icon = (byte *) malloc(adi.adi_iconsize);

  if (DBDSK) {
    printf("FPAddIcon: for icon size %d ", adi.adi_iconsize);
    PrintIconInfo(adi.adi_fcreator,adi.adi_ftype);
  }

  err = dsiWrtContinue(cno,reqref,icon,adi.adi_iconsize,&rcvlen,-1,&comp);
  if (err != noErr) {
    free((char *)icon);
    return(err);
  }
  do { abSleep(4,TRUE); } while (comp > 0);    
  if (comp < 0) {
    free((char *)icon);
    return(comp);
  }

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_icon();
    if (adi.adi_icontype == 1) {
      fprintf(dbg, "\tAIcon:\n");
      dbg_print_icon(icon, adi.adi_iconsize);
      fprintf(dbg, "\n");
    }
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /* Insert the new entry */
  in = dtIconInsert(ihead, adi.adi_ftype, adi.adi_icontype, &replaced);
  /* assert in != NULL */
  in->in_mloc = NOGCIDX;	/* no cache yet */
  in->in_iloc = -1;		/* initially none */
  in->in_uloc = NULL;
  ii = &in->in_info;
  /* redunancy */
  bcopy((char *)adi.adi_fcreator,(char *)ii->i_FCreator,FCreatorSize);
  /* bcopy(adi.adi_ftype,ii->i_FType,FTypeSize); - already done */
  /* ii->i_IType = adi.adi_icontype; - already done */
  bcopy((char *)&adi.adi_icontag,(char *)ii->i_ITag,ITagSize);
  ii->i_bmsize = adi.adi_iconsize;

  if (replaced != 0) {
    if (DBDSK)
      printf("FPAddIcon: trying to reuse old ICON entry\n");
    if (in->in_info.i_bmsize == replaced->in_info.i_bmsize) {
      oldicon = IDFetch(dt->dt_ifd, replaced);
      if (oldicon && bcmp((char *)oldicon, (char *)icon,
	in->in_info.i_bmsize) == 0 &&
	  bcmp((char *)replaced->in_info.i_ITag,(char *)in->in_info.i_ITag,
	       ITagSize) == 0) {
	if (DBDSK)
	  printf("New icon matches an old one!\n");
	/* was no different! */
	in->in_iloc = replaced->in_iloc;
	in->in_mloc = replaced->in_mloc;
	in->in_uloc = replaced->in_uloc;
	CacheAdd(in, (byte *)NULL); /* just mark as replacement */
	free((char *)icon);		/* unused */
	free((char *)replaced);		/* no longer wanted or needed */
	return(noErr);
      }
      /* mark be rewritten on disk */
      if (DBDSK)
	printf("New icon data or user tags different, reusing space\n");
      /* must be rewritten to disk */
      in->in_iloc = replaced->in_iloc;
      free((char *)replaced);		/* taken care of */
    } else {
      /* should mark as "bad" in file */
      /* what to do with replaced? */
    }
  }
  WriteIcon(dt->dt_ifd,in,icon,rcvlen);	/* store in file */
  CacheAdd(in,icon);		/* add this to the icon cache */
  return(noErr);
}

/*
 * OSErr FPGetIcon(...)
 *
 */
OSErr
FPGetIcon(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  IconNode *ihead, *tofind;
  GetIconPkt gic;
  DeskTop *dt;
  byte *ip;
  int len;

  ntohPackX(PsGetIcon,p,l,(byte *) &gic);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    fprintf(dbg, "\tDTRef: %d\n", gic.gic_dtrefnum);
    dbg_print_type("\tCreat:", gic.gic_fcreator);
    dbg_print_type("\tFlTyp:", gic.gic_ftype);
    fprintf(dbg, "\tIcTyp: %d\n", gic.gic_itype);
    fprintf(dbg, "\tIcLen: %d\n", gic.gic_length);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((dt = VolGetDesk(gic.gic_dtrefnum)) == NILDT)
    return(aeParamErr);
  
  if (DBDSK) {
    printf("FPGetIcon: Get - ");
    PrintIconInfo(gic.gic_fcreator, gic.gic_ftype);
  }
  ihead = dtFindIconList(dt, gic.gic_fcreator);
  if (ihead == NULL)
    return(aeItemNotFound);
  (void)dtIconInsertPoint(ihead, gic.gic_ftype, gic.gic_itype, &tofind);
  if (tofind == NULL)
    return(aeItemNotFound);

  if (DBDSK) {
    printf("FPGetIcon: Got - ");
    PrintINode(tofind);
  }
  
  len = min(tofind->in_info.i_bmsize,gic.gic_length);

  if ((ip = IDFetch(dt->dt_ifd, tofind)) != NULL)
    bcopy((char *)ip,(char *)r,len);
  else {
    if (tofind->in_uloc != NULL) {
      len = min(gic.gic_length, tofind->in_info.i_bmsize);
      bcopy((char *)tofind->in_uloc, (char *)r, len);
    } else
      return(aeItemNotFound);
  }
  *rl = len;

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_icon();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tIcLen: %d\n", *rl);
    dbg_print_icon(r, *rl);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * OSErr FPRmvComment(...)
 *
 */

/*ARGSUSED*/
OSErr
FPRmvComment(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  RemoveCommentPkt rmc;
  IDirP idir,ipdir;
  int err;
  char file[MAXUFLEN];
  int ivol;
  DeskTop *dt;
  
  if (DBDSK)
    printf("FPRmvComment: ...\n");
  ntohPackX(PsRmvComment, p, l, (byte *)&rmc);
  if ((dt = VolGetDesk(rmc.rmc_dtrefnum)) == NILDT)
    return(aeParamErr);

  err = EtoIfile(file,&idir,&ipdir,&ivol,rmc.rmc_dirid,
		 dt->dt_evol,rmc.rmc_ptype,rmc.rmc_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tDTRef: %d\n", rmc.rmc_dtrefnum);
    fprintf(dbg, "\tDirID: %08x\n", rmc.rmc_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", rmc.rmc_ptype,
      (rmc.rmc_ptype == 1) ? "Short" : "Long");
    dbg_print_path(rmc.rmc_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  /* set comment to empty */
  err = OSSetComment(ipdir,file,(byte *)"\0",0);

  return(err);
}


OSErr
FPGetIconInfo(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  GetIconInfoPkt gii;
  GetIconInfoReplyPkt giir;
  DeskTop *dt;
  IconNode *ihead;
  int idx;

  ntohPackX(PsGetIconInfo,p,l,(byte *) &gii);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    fprintf(dbg, "\tDTRef: %d\n", gii.gii_dtrefnum);
    dbg_print_type("\tCreat:", gii.gii_fcreator);
    fprintf(dbg, "\tIcIdx: %d\n", gii.gii_iidx);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((dt = VolGetDesk(gii.gii_dtrefnum)) == NILDT)
    return(aeParamErr);

  if (DBDSK) {
    printf("FPGetIconInfo: Nth = %d ",gii.gii_iidx);
    PrintIconInfo(gii.gii_fcreator, (byte *)"none");
  }    
  ihead = dtFindIconList(dt, gii.gii_fcreator);
  if (ihead == NULL)
    return(aeItemNotFound);
  idx = gii.gii_iidx;
  while ((ihead = ihead->in_next) != NULL && --idx > 0)
    /* NULL */;
  if (ihead == NULL)
    return(aeItemNotFound);

  /* return this info */
  if (DBDSK) {
    printf("FPGetIconInfo: Got - ");
    PrintINode(ihead);
  }
  bcopy((char *)ihead->in_info.i_ITag,(char *)&giir.giir_itag, 4);
  bcopy((char *)ihead->in_info.i_FType,(char *)giir.giir_ftype, 4);
  giir.giir_itype = ihead->in_info.i_IType;
  giir.giir_zero = 0;
  giir.giir_size = ihead->in_info.i_bmsize;
  *rl = htonPackX(PsGetIconInfoReply, (byte *)&giir, r);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_type();
    fprintf(dbg, "\tIcTag: %d\n", giir.giir_itag);
    dbg_print_type("\tFlTyp:", giir.giir_ftype);
    fprintf(dbg, "\tIcTyp: %d\n", giir.giir_itype);
    fprintf(dbg, "\tIcLen: %d\n", giir.giir_size);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}


private DeskTop *
VolGetDesk(volid)
word volid;
{
  int ivol = EtoIVolid(volid);
  if (ivol < 0)
    return(NILDT);
  return(DeskTab[ivol]);
}

private void
VolSetDesk(volid,dt)
word volid;
DeskTop *dt;
{
  int ivol = EtoIVolid(volid);
  if (ivol < 0)
    return;
  dt->dt_opened = TRUE;
  DeskTab[ivol] = dt;
} 

private void
VolClrDesk(volid)
word volid;
{
  int ivol = EtoIVolid(volid);
  if (ivol < 0) 
    return;
  if (DeskTab[ivol])
    DeskTab[ivol]->dt_opened = FALSE;
}


/*
 * The following routines manage a tree of "File Creators".  The bind routines
 * simply return the "correct instance" a File Creator, inserting into the
 * tree if necessary.  The look routines do not insert.
 * 
*/

typedef struct {
  byte adt_FCreator[FCreatorSize];
  APPLNode adt_ahead;
  IconNode adt_ihead;
} AVLdt;


/*
 * Compare two 4 byte entities and return:
 *   < 0 if a < b
 *   = 0 if a = b
 *   > 0 if a > b
 *  Assumes args are points to the two entities. 
 *
*/ 
private int
FCompare(a, b)
byte *a;
byte *b;
{
  int i;

  /* don't use strncmp, because it will terminate on NULL */
  for (i = 0; i < 4; i++, a++, b++) {
    if (*a == *b)
      continue;
    if (*a < *b)
      return(-1);
    return(1);
  }
  return(0);
}
/*
 * Uses above to compare when first input argument is a pointer to
 * byte[4] and the second is of type AVLdt.  Only valid for File Creators.
 *
*/
private int
FCompare1(aa, b)
byte *aa;
AVLdt *b;
{
  return(FCompare(aa, b->adt_FCreator));
}

/*
 * Given a file creator, give the binding in the ordered tree
 * If the binding does not exist, then create it.
 *
*/
private AVLdt *
dtBindFC(dt, FCreator)
DeskTop *dt;
byte FCreator[];
{
  AVLdt *newnode = NULL;
  AVLNode *fcn;

  fcn = AVLInsert(&dt->dt_avlroot, FCreator, FCompare1);
  /* assert fcn != NULL */
  if ((byte *)fcn->b_udata != FCreator)
    return((AVLdt *)fcn->b_udata);
  newnode = (AVLdt *)malloc(sizeof(AVLdt));
  bcopy((char *)FCreator, (char *)newnode->adt_FCreator, FCreatorSize);
  newnode->adt_ahead.an_next = NULL;
  newnode->adt_ihead.in_next = NULL;
  fcn->b_udata = (AVLUData *)newnode;
  return(newnode);
}

/*
 * Find (and possilby bind) the APPL node chain
 *
*/
private APPLNode *
dtBindFCtoAPPL(dt, FCreator)
DeskTop *dt;
byte FCreator[];
{
  AVLdt *p;

  p = dtBindFC(dt, FCreator);
  /* assert (p != NULL) */
  /* probably should do insertion sort at this point */
  return(&p->adt_ahead);
}

/*
 * Find (and possibly bind) the Icon node chain
 *
*/
private IconNode *
dtBindFCtoIcon(dt, FCreator)
DeskTop *dt;
byte FCreator[];
{
  AVLdt *p;

  p = dtBindFC(dt, FCreator);
  /* assert (p!=NULL) */
  /* probably should do insertion sort at this point */
  return(&p->adt_ihead);
}

/*
 * Given a file creator, give the binding in the ordered tree
 * Cache - high probability that lookus on same File Creator will occur.
 *
*/
private AVLdt *
dtLookFC(dt, FCreator)
DeskTop *dt;
byte FCreator[];
{
  if (dt->dt_avllast == NULL || FCompare(FCreator, dt->dt_oFCreator) != 0) { 
    bcopy((char *)FCreator, (char *)dt->dt_oFCreator, FCreatorSize);
    dt->dt_avllast = AVLLookup(dt->dt_avlroot, FCreator, FCompare1);
  }
  if (dt->dt_avllast)
    return((AVLdt *)dt->dt_avllast->b_udata);
  return(NULL);
}


/*
 * Find the APPL node chain
 *
*/
private APPLNode *
dtFindAPPLList(dt, FCreator)
DeskTop *dt;
byte FCreator[];
{
  AVLdt *p;

  p = dtLookFC(dt, FCreator);
  if (p)
    return(&p->adt_ahead);
  return(NULL);
}


/*
 * Find the ICon node chain
 *
*/
private IconNode *
dtFindIconList(dt, FCreator)
DeskTop *dt;
byte FCreator[];
{
  AVLdt *p;

  p = dtLookFC(dt, FCreator);
  if (p)
    return(&p->adt_ihead);
  return(NULL);
}


/*
 * rewrite a list of APPLnodes
 * done if:
 *   (a) entry modified
 *   (b) parent directory of directory APPL is in has changed (e.g. APPL
 *         moved)
 *
*/
private void
FlushAPPL(adt,dt)
AVLdt *adt;
DeskTop *dt;
{
  register APPLNode *an;
  register IDirP appdir, rppdir; /* supposed and real parent directory */
				/* of parent of entry */

  an = &adt->adt_ahead;	/* get first entry */
  while ((an  = an->an_next) != NULL) {
    if (DBDSK)
      printf("FlushAPPL:  parpath=%s, file=%s, deleted %s, modified %s\n",
	     ppathstr(dt->dt_rootd,an->an_ipdir),an->an_fnam,
	     (an->an_flags & AN_DEL) ? "yes" : "no", 
	     (an->an_flags & AN_MOD) ? "yes" : "no");
    appdir = an->an_ippdir;	/* get remember parent */
    rppdir = Ipdirid(an->an_ipdir); /* get real */
    if (an->an_flags & AN_MOD || appdir != rppdir)
      WriteAPPL(dt,an);
    if (appdir != rppdir)
      an->an_ippdir = rppdir;	/* reset to real parent */
  }
}

/*
 * Flush the desk top for the specified volume - e.g. flush the various
 * cache to disk.  Whenever called will (a) try to open for write and 
 * if it can (b) will run through all APPL mappings, rewritting them.
 *
*/
FlushDeskTop(volid)
int volid;
{
  DeskTop *dt;
  int wok;

  dt = VolGetDesk(ItoEVolid(volid));
  if (dt == NILDT) {			/* happens when vol mounted */
    if (DBDSK)				/*  from application */
      printf("FlushDeskTop: Unknown volid %d\n",volid);
    return;
  }

  /* 
   * Rewrite the entire APPL desktop on a flush when entries have
   * been modified.  Will be a nop is can't open desktop for write 
   * This is necessary even though we have a write through cache
   * because old entries stay around (will have to figure something
   * better out - maybe keep track of file position and write (invalid)
   * flag or something.
   * (Actually, just rewrite modified entries)
   */
  if (dt->dt_afd < 0) {
    dt->dt_afd = OpenDesk(dt, DESKTOP_APPL, FALSE, APPLFILEMODE, &wok);
    if (dt->dt_afd < 0)
      return;
    if (!wok) {
      (void)close(dt->dt_afd);
      dt->dt_afd = -1;
      return;
    }
  }
  /* yes... so flush APPL records */
  OSLockFileforWrite(dt->dt_afd); /* lock it for write */
  AVLMapTree(dt->dt_avlroot, FlushAPPL,(char *) dt);
  OSUnlockFile(dt->dt_afd);
  (void)close(dt->dt_afd);			/* close desktop */
  dt->dt_afd = -1;
}


/*
 * Finds the insertion point for a new icon node.  Returns the node
 * whose "next" pointer should be replaced with the new node.
 * "replaced" is set to the next node iff that node should be "replaced"
 * in the list because it is a duplicate.
 *
 * FType is the primary key - multiple instanaces of this can occur
 * IType is the secondary key - only one instance (per FType) may exist
 *   in the list.
*/
private IconNode * 
dtIconInsertPoint(h, FType, IType, replaced)
IconNode *h;
byte FType[];
byte IType;
IconNode **replaced;
{
  IconNode *p, *c;		/* previous and current respectively */
  int cmp;

  *replaced = NULL;
  /* Scan until we find FType == node's FType */
  for (p = h, c = h->in_next; c != NULL; p = c, c = c->in_next)
    if ((cmp = FCompare(FType, c->in_info.i_FType)) >= 0)
      break;
  /* assert(FCompare(FType, p->in_info.i_FType) < 0) */
  /* if c isn't null then we may also assert that: */
  /* assert(FCompare(FType, c->in_info.i_FType) >= 0) */

  /* If previous assert is "=" then we check down secondary key */
  /* (if only FType, then replace we replace in this case) */
  /* If previous assert is ">", then insert after previous */
  
  /* Thus p points to node to link after if only FTypes were considered */ 
  /* c points to node >= new node in terms of FType */
  if (cmp == 0 && c != NULL) {	/* previous FType was an exact match? */
    do {
      if (IType >= c->in_info.i_IType)
	break;
      p = c;
      if ((c = c->in_next) == NULL)
	break;
    } while ((cmp = FCompare(FType, c->in_info.i_FType)) == 0);
    /* at this point we can assert (unless c is null) */
    /* FCompare(FType, p->in_info.i_FType) = 0 */
    /* FCompare(FType, c->in_info.i_FType) >= 0 */
    /* IType >= c->in_info.i_IType && IType < p->in_info.i_IType */

    if (cmp == 0 && c != NULL)
      /* Assert FType == c->in_info.i_FType */
      /* IF Itype == c->in_info.i_IType, then replace c */
      if (IType == c->in_info.i_IType) {
	*replaced = c;
      } /* else iType > c->in_info and we want to insert after p */
  }
  return(p);
}
/*
 * Insert a new Icon node into a list.  Old instances get "deleted"
 * (not replaced - this allows management of the "deleted" space)
 * Base key information is inserted into the node and caller is responsible
 * for filling in the rest.  Replaced icon entry is returned for
 * recovery.
*/
private IconNode *
dtIconInsert(h, FType, IType, replaced)
IconNode *h;
byte FType[];
byte IType;
IconNode **replaced;
{
  IconNode *p;
  IconNode *new;		/* new entry */

  p = dtIconInsertPoint(h, FType, IType, replaced);
  if (*replaced)
    p->in_next = (*replaced)->in_next;	/* unlink node */
  new = (IconNode *)malloc(sizeof(IconNode));
  new->in_next = p->in_next;
  p->in_next = new;
  /* make sure new node has key info (at least) */
  bcopy((char *)FType, (char *)new->in_info.i_FType, FTypeSize);
  new->in_info.i_IType = IType;
  return(new);
}

/*
 * Insert APPLNode to chain - append to end if not in list
 * otherwise replace entry.
 * 
 * scan list and check for match, otherwise just insert at end 
 * if deleted (and same) unmark deleted bit want to keep deleted
 * entries incore so we can reuse the space  
*/
private APPLNode *
dtAPPLInsertPoint(h, ipdir, fnam, replaced)
APPLNode *h;
IDirP ipdir;
char *fnam;
APPLNode **replaced;
{
  APPLNode *p, *np;

  *replaced = NULL;
  for ( p = h, np = h->an_next ; np != NULL ; p = np, np = np->an_next) {
    /* strcmp should be case insenstive here, but other considerations.. */
    if (np->an_ipdir == ipdir && strcmp(np->an_fnam, fnam) == 0) {
      *replaced = np;
      break;
    }
  }
  return(p);
}

private APPLNode *
dtAPPLInsert(h, ipdir, fnam, replaced)
APPLNode *h;
IDirP ipdir;
char *fnam;
APPLNode **replaced;
{
  APPLNode *p, *np;

  p = dtAPPLInsertPoint(h, ipdir, fnam, replaced);
  if (*replaced)
    p->an_next = (*replaced)->an_next;
  /* insert at this point */
  if ((np = (APPLNode *)malloc(sizeof(APPLNode))) == NULL)
    logit(0,"internal error: Memory allocation failure: dtAPPLInsert\n");
  /* hopefully will coredump on next */
  np->an_ipdir = ipdir;
  np->an_ippdir = Ipdirid(ipdir);	/* remember parent too */
  np->an_fnam = fnam;
  np->an_iloc = -1;			/* no iloc */
  np->an_next = p->an_next;		/* should be null */
  p->an_next = np;			/* link in */
  return(np);
}


/*
 * Cache handling routines
*/

private GCHandle *gci;			/* general cache for icon data */
private IconCacheEntry *fice;		/* maybe free cache entry */
/*
 * int gci_compare(IconCacheEntry *ice, IconCacheEntry *key)
 *
 * General cache compare routine for icon bitmap data.
 *
 */

private int 
gci_compare(ice,key)
IconCacheEntry *ice,*key;
{
  return(ice->ib_node == key->ib_node);		/* compare owners */
}

/*
 * void gci_purge(IconCacheEntry *ice)
 *
 * general cache purge routine for icon bitmap data.
 *
 */

private void 
gci_purge(ice)
IconCacheEntry *ice;
{
  if (DBDSK)
    printf("gci_purge: Purging icon at %d\n",ice->ib_node);

  (ice->ib_node)->in_mloc = NOGCIDX;	/* tell iconnode no longer cached */
  if (fice) {
    if (ice->ib_size > fice->ib_size) {
      fice->ib_data = ice->ib_data;	/* use larger */
      fice->ib_size = ice->ib_size;
    } else 
      if (ice->ib_data)
	free((char *)ice->ib_data); /* release the data */
    free((char *) ice);			/* free entry itself */
  } else
    fice = ice;
}

private void gci_flush() {}				/* noop */
private GCUData *gci_load() { return((GCUData *)0); }	/* noop */
private int gci_valid() { return(TRUE); }		/* noop */

/*
 * InitIconCache()
 *
 * Create a general purpose cache for storing icon bitmaps.  The
 * IconNode record contains a cache index when the icon is in memory.
 *
 */

void
InitIconCache()
{
  gci = GCNew(ICSize,gci_valid,gci_compare,gci_load,gci_purge,gci_flush);
}

/*
 * void CacheAdd(IconNode *in, char *icon)
 *
 * Add the icon bitmap (icon) associated with the IconNode in
 * to the cache and store the cache index in IconNode in for
 * quick reference.  If cache index is valid, be nice about it.
 * If no icon data and cache index is valid, then we have "replaced"
 * the icon node and should simply rechain things.
 *
 * Used for adding icons not read from disk. (e.g. sent by remote)
 *
 */
private void
CacheAdd(in,icon)
IconNode *in;
byte *icon;
{
  IconCacheEntry *ice;

  if (DBDSK)
    printf("CacheAdd: Adding icon at %d\n",in);

  if (in->in_mloc != NOGCIDX) {
    ice = (IconCacheEntry *)GCGet(gci, in->in_mloc); /* get cache entry */
    /* if new icon data and free entry, then maximize space in free entry */
    if (icon) {
      if (fice && (ice->ib_size > fice->ib_size)) {
	if (fice->ib_data)		/* Get rid of any left */
	  free((char *)fice->ib_data);
	fice->ib_data = ice->ib_data;	/* use larger */
	fice->ib_size = ice->ib_size;
      } else 
	if (ice->ib_data)
	  free((char *)ice->ib_data); /* release the data */
    }
  } else {
    if (icon == NULL)			/* no new icon data */
      return;			/* nothing means nop here */
    if (fice) {				/* free entry? */
      ice = fice;
      if (fice->ib_data)
	free((char *)fice->ib_data); /* data no good */
      fice = NULL;
    } else {				/* nope, must allocate */
      ice = (IconCacheEntry *) malloc(sizeof(IconCacheEntry));
    }
  }
  if (icon)
    ice->ib_data = icon;		/* here is the bitmap */
  ice->ib_node = in;			/* remember parent */
  ice->ib_size = in->in_info.i_bmsize;	/* remember size */
  if (in->in_mloc == NOGCIDX)
    in->in_mloc = GCAdd(gci,(GCUData *) ice); /* set node to be cache idx */
}


/*
 * char *IDFetch( int fd,IconNode *in)
 *
 * The icon is either cached in memory or in the file fd.
 *
 * Check IconNode in to see if the icon is at a cache index, if so
 * return the cached icon.  If the icon is not in memory then read
 * in from file fd and add to the cache.
 *
 */
private byte *
IDFetch(fd,in)
int fd;
IconNode *in;
{
  IconCacheEntry *ice;

  if (in->in_mloc != NOGCIDX) {		/* data in memory? */
    /* yes... get cached */
    ice = (IconCacheEntry *)GCGet(gci,in->in_mloc); /*  icon data at index */
    if (DBDSK)
      printf("IDFetch: hit on icon at %d\n",in);
    return(ice->ib_data);		/* return bitmap data */
  }

  if (fd < 0)				/* no fp? */
    return(NULL);			/* then can't get icon */

  if (fice) {				/* use last freed if there */
    ice = fice;
    fice = NULL;
  } else {
    ice = (IconCacheEntry *) malloc(sizeof(IconCacheEntry));
    if (ice == NULL) {
      printf("Memory allocation error");
    }
    ice->ib_data = NULL;
    ice->ib_size = -1;
  }
  if (ice->ib_size < in->in_info.i_bmsize) {
    if (ice->ib_data)
      free((char *)ice->ib_data); /* get rid of too small region */
    /* allocate */
    ice->ib_data = (byte *) malloc((unsigned)in->in_info.i_bmsize);
    if (ice->ib_data == NULL) {
      printf("Memory allocation error");
    }
  }

  OSLockFileforRead(fd);
  if (lseek(fd,in->in_iloc,L_SET) < 0L) { /* offset in file to data */
    fice = ice;
    OSUnlockFile(fd);
    return(NULL);
  }
  if (read(fd,(char *)ice->ib_data,in->in_info.i_bmsize) < 0) {
    fice = ice;
    OSUnlockFile(fd);
    return(NULL);
  }
  OSUnlockFile(fd);
  ice->ib_node = in;
  in->in_mloc = GCAdd(gci, (GCUData *) ice); /* add to cache */
  return(ice->ib_data);			/* return buffer pointer */
}


RemoveAPPLIDB(adt, dt)
AVLdt *adt;
DeskTop *dt;
{
  APPLNode *an, *ant;
  
  for (an = adt->adt_ahead.an_next; an ; ) {
    ant = an;
    an = an->an_next;
    free(ant->an_fnam);
    free(ant);
  }
  adt->adt_ahead.an_next = NULL;
}

RemoveICONIDB(adt, dt)
AVLdt *adt;
DeskTop *dt;
{
  IconNode *in, *intemp;
  
  for (in = adt->adt_ihead.in_next; in ; ) {
    intemp = in;
    in = in->in_next;
    if (intemp->in_uloc)
      free(intemp->in_uloc);
    free(intemp);
  }
  adt->adt_ihead.in_next = NULL;
}

DeskRemove(dt, wh)
DeskTop *dt;
int wh;
{
  AVLMapTree(dt->dt_avlroot,
	     wh == REMOVEAPPLIDB ? RemoveAPPLIDB : RemoveICONIDB, (char *) dt);
}

#ifdef DEBUG_AFP_CMD
/*
 * print a 4 byte Creator or File Type
 *
 */

void
dbg_print_type(str, typ)
char *str;
byte *typ;
{
  int i;

  if (dbg != NULL) {
    fprintf(dbg, "%s '", str);
    for (i= 0; i < 4; i++)
      fprintf(dbg, "%c", (isprint(typ[i])) ? typ[i] : '?');
    fprintf(dbg, "'\n");
  }

  return;
}

/*
 * print out a representation of the ICN#
 *
 */

#define get2(s)	(u_short)(((s)[0]<<8)|((s)[1]))
#define get4(s)	(u_int)(((s)[0]<<24)|((s)[1]<<16)|((s)[2]<<8)|((s)[3]))

void
dbg_print_icon(icn, len)
byte *icn;
int len;
{
  u_char *p;
  int i, j, k;
  u_long data1, data2, mask;

  if (len != 256)
    return;

  if (dbg != NULL) {
    for (i = 0, p = icn; i < 32; i += 2, p += 8) {
      for (j = 0; j < 2; j++) {
        if (j == 1)
          fprintf(dbg, "\t    ");
	data1 = get4(p+(j*128));
	data2 = get4(p+4+(j*128));
        for (k = 0; k < 32; k++) {
          mask = ((u_long)0x80000000 >> k);
          if (data1 & mask && data2 & mask)
            fprintf(dbg, "8");
          else
            if (data1 & mask)
              fprintf(dbg, "\"");
            else
              if (data2 & mask)
                fprintf(dbg, "o");
              else
                fprintf(dbg, " ");
        }
      }
      fprintf(dbg, "\n");
    }
  }

  return;
}
#endif /* DEBUG_AFP_CMD */
