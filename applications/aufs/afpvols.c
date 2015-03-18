/*
 * $Author: djh $ $Date: 1996/06/19 04:16:30 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpvols.c,v 2.18 1996/06/19 04:16:30 djh Rel djh $
 * $Revision: 2.18 $
 *
 */

/*
 * afpvols.c - Appletalk Filing Protocol Volume Routines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986,1987,1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *
 */

/*
 * Non OS dependant support routines for:
 *
 *  FPGetVolParms()
 *  FPSetVolParms()
 *  FPOpenVol()
 *  FPCloseVol()
 *  FPFlush()
 *
 */

#include <stdio.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>		/* assume included by param.h */
#endif  _TYPES
#ifdef CREATE_AFPVOL
# include <sys/errno.h>
#endif CREATE_AFPVOL
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include "afps.h"
#include "afpntoh.h"
#include "afpvols.h"
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef DEBUG_AFP_CMD
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

private VolPtr VolTbl[MAXVOLS];		/* table of VolEntry records */
private int VolCnt = 0;			/* number of volumes */
private VolBitMap VolModBitMap = 0;	/* bitmap of modified volumes */

private PackEntry VolPackR[] = { 	/* Volume Parms Reply */
  PACK(VolPtr,P_WORD,v_bitmap),		/* bitmap specifies below items */
  PACK(VolPtr,P_BMAP,v_bitmap),		/* bitmap specifies below items */
  PAKB(VolPtr,P_WORD,v_attr,VP_ATTR),	/* attributes word */
  PAKB(VolPtr,P_WORD,v_sig,VP_SIG),	/* signature word */
  PAKB(VolPtr,P_TIME,v_cdate,VP_CDATE),	/* creation date */
  PAKB(VolPtr,P_TIME,v_mdate,VP_MDATE),	/* modification date */
  PAKB(VolPtr,P_TIME,v_bdate,VP_BDATE),	/* last back date */
  PAKB(VolPtr,P_WORD,v_volid,VP_VOLID),	/* volume id */
  PAKB(VolPtr,P_DWRD,v_free,VP_FREE),	/* free bytes */
  PAKB(VolPtr,P_DWRD,v_size,VP_SIZE),	/* size in bytes */
  PKSB(VolPtr,P_OSTR,v_name,VP_NAME),	/* name of volume */
  PKSB(VolPtr,P_BYTS,v_efree,VP_EFREE), /* extended free bytes */
  PKSB(VolPtr,P_BYTS,v_esize,VP_ESIZE), /* extended total bytes */
  PACKEND()
};

/*
 * void VNew(char *path, char *name, char *pwd)
 *
 * Given a path, volume name, and password string, create a new volume
 * in VolTbl.
 *
 */

void
VNew(path,name,pwd)
char *name;
char *path;
char *pwd;
{
  VolPtr vp;
  char *malloc();
  extern int sessvers;

  if ((strlen(name) > MAXVLEN) ||
      (strlen(path) > MAXDLEN) ||
      (strlen(pwd)  > MAXPLEN)) {
	logit(0,"VNew: path, name or password too long on path = %s",path);
	return;
      }
    
  if (DBVOL)
    printf("Adding vol '%s' on path '%s' pwd='%s'\n",name,path,pwd);

  /* create volume record */
  vp = (VolPtr) malloc(sizeof(VolEntry));
  strcpy(vp->v_name,name);		/* copy the name */
  strcpy(vp->v_path,path);		/*  the path */
  strcpy(vp->v_pwd,pwd);		/*  and the password */
  vp->v_mounted = FALSE;		/* not opened */
  vp->v_volid = VolCnt+1;		/* will be volcnt+1 */
  vp->v_sig = VOL_FIXED_DIRID;		/* signature word */
  vp->v_attr = 0;			/* clear attributes */
  if (sessvers >= AFPVersion2DOT1)
    vp->v_attr |= V_SUPPORTSFILEIDS;	/* for ExchangeFiles call */

  /* Now make sure entry is valid */
  if (OSVolInfo(path,vp,VP_ALL) != noErr) {	/* get volume info */
    free((char *) vp);			/* bad... release storage */
    printf("VNew: No volinfo for %s on path %s\n",name,path);
    return;
  }

  /* This is deferred because I'm not sure how to deallocate :-) */
  /* and isn't needed until we have validated the entry anyway */
  vp->v_rootd = Idirid(path);		/* create directory handle */
  if (vp->v_rootd != NILDIR)		/* avoid NULL handles */
    InitDIDVol(vp->v_rootd, VolCnt);	/* initialize volume info for list */
  
  /* Okay, stick it into the table */
  if (vp->v_rootd != NILDIR)		/* avoid NULL handles */
    VolTbl[VolCnt++] = vp;		/* set volume record */

  /* check for color volume icon */
  if (!icon_exists(path))
    icon_create(path);

  return;
}

private char *
spanspace(p)
char *p;
{
  while (*p == ' ' || *p == '\t' || *p == '\n')
    p++;
  return(p);
}

/*
 * char *vskip(char *p)
 *
 * vskip skips to the next field in a line containing volume information
 * as read from the VOLFILE.  The field seperator ":" or the end of line
 * character is set to NULL in order to terminate the field.
 *
 */

private char *
vskip(p)
char *p;
{
  while (*p != '\0' && *p != ':' && *p != '\n')
    p++;
  if (*p != '\0')		/* at end of string? */
    *p++ = '\0';		/* no tie off this field */
  return(p);			/* and return pointer */
}

/*
 * VRdVFile(FILE *fd)
 *
 * Reads a file containing volume information from the specified path.
 * The file descriptor is closed after reading the file
 *
 * VRdVFile is intended to read VOLFILE from the user's home directory
 * after FPLogin.  The information in this file is stored in the VolTbl.
 *
 * Format of VOLFILE:
 *  path:volume name[:optional password][:]
 *
 *  blank lines and lines starting with '#' are ignored.
 *
 */

VRdVFile(fd)
FILE *fd;
{
  char line[MAXLLEN+1];
  char *pathp,*namep,*pswdp,*p,*tilde();
  int origVolCnt;
#ifdef ISO_TRANSLATE
  void cISO2Mac();
#endif ISO_TRANSLATE

  origVolCnt = VolCnt;
  while ((p = fgets(line,MAXLLEN,fd)) != NULL) { /* read lines */
    if (DBVOL) printf("VRdVFile: Parse : %s\n",p);
    p = spanspace(p);			/* span spaces */
    if (DBVOL) printf("VRdVFile: After spanspace : %s\n",p);
    if (*p == '#' || *p == '\0')	/* comment or blank line? */
      continue;				/* yes, skip it */
    pathp = p; p = vskip(p);		/* save ptr to start of path */
    if (DBVOL) printf("VRdVFile: pathp : %s\n",pathp);
    namep = p; p = vskip(p);		/* start of name */
    if (DBVOL) printf("VRdVFile: namep : %s\n",namep);
#ifdef ISO_TRANSLATE
    cISO2Mac(namep);
#endif ISO_TRANSLATE
    pswdp = p; p = vskip(p);		/* save it */
    if (DBVOL) printf("VRdVFile: pswdp : %s\n",pswdp);
    pathp = tilde(pathp);		/* expand the path */
    if (pathp == NULL)			/* non existent user */
      continue;				/* skip it */
    if (DBVOL) printf("VRdVFile: pathp after tilde : %s\n",pathp);
    VNew(pathp,namep,pswdp);		/* add new entry */
  }
  fclose(fd);				/* close file */
  return(VolCnt != origVolCnt);	        /* return true if found any entries */
}

#ifdef REREAD_AFPVOLS
/*
 * VRRdVFile(FILE *fd)
 *
 * VRRdVFile() is called after a USR1 signal to the aufs process.
 * We delete contents of current VolTbl and call VRdVFile() again.
 *
 */

VRRdVFile(fd)
FILE *fd;
{
  int i;

  for (i = 0; i < VolCnt; i++)
    free(VolTbl[i]);

  VolCnt = 0;

  return(VRdVFile(fd));
}
#endif REREAD_AFPVOLS

/*
 * void VInit(char *usr,char *home)
 *
 * VInit adds entries to the table of volumes by reading the user's
 * VOLFILE or VOLFILE1.  If no VOLFILE exists on the path specified by
 * "home" then a default entry entry of path=home and name=usr is
 * setup.
 *
 */

void
VInit(usr,home)
char *usr;
char *home;
{
  char vfn[MAXDLEN+20];			/* afpvols file name */
  FILE *fd;

  if (home == NULL)			/* no home, then nothing to do */
    return;

  strcpy(vfn,home);			/* copy directory */
  strcat(vfn,"/");			/*  terminator */
  strcat(vfn,VOLFILE);			/*  and then the volsfile name */

  if ((fd = fopen(vfn, "r")) == NULL) {
    strcpy(vfn,home);		/* copy directory */
    strcat(vfn,"/");		/*  terminator */
    strcat(vfn,VOLFILE1);	/*  and then the volsfile name */
    if ((fd = fopen(vfn, "r")) == NULL) {
#ifdef CREATE_AFPVOL
      if (aufsDirSetup(home, usr) >= 0)
        logit(0, "CREATE_AFPVOL: setting up %s for Mac access", home);
#else  CREATE_AFPVOL
      if (DBVOL)
	printf("VRdVFile: no afpvols or .afpvols in %s\n",home);
      VNew(home,usr,"");	/* if none, then setup home dir */
#endif CREATE_AFPVOL
      return;
    }
  }
  (void)VRdVFile(fd);		/* read vols file from user */
  if (VolCnt == 0)
    VNew(home, usr, "");
}

/*
 * return count of volumes
 *
*/
int
VCount()
{
  return(VolCnt);
}

/*
 * int VMakeVList(VolParm *vp,byte *vpl)
 *
 * Store into vp a number of VolParm entries, one for each volume
 * we know about.  Return in vpl the length of the information.
 * Return the count of volumes.
 * 
 * Used by GetSrvrParms.
 *
 */

int
VMakeVList(r)
byte *r;			/* ptr to place to store */
{
  VolParm vpp;
  int v, len;
  extern PackEntry ProtoGSPRPvol[];

  for (v=0,len=0; v < VolCnt; v++) {
    if (*VolTbl[v]->v_pwd != '\0')
      vpp.volp_flag = SRVRP_PASSWD;	/* is password protected */
    else
      vpp.volp_flag = 0;		/* no flags */
    cpyc2pstr(vpp.volp_name,VolTbl[v]->v_name);
    len += htonPackX(ProtoGSPRPvol, &vpp, r+len);
  }
  return(len);			/* return size used */
}

/*
 * OSErr FPGetVolParms(byte *p,int l,byte *r,int *rl);
 *
 * This call is used to retrieve paramters for a particular volume.
 * The volume is specified by its Volume ID as returned from the
 * FPOpenVol call.
 *
 *
 */

OSErr 
FPGetVolParms(p,l,r,rl)
byte *p,*r;
int *rl,l;
{
  GetVolParmsPkt gvp;			/* cast to packet type */
  int ivol,err;
  VolPtr vp;

  ntohPackX(PsGetVolParms,p,l,(byte *) &gvp); /* decode packet */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_vmap();
    fprintf(dbg, "\tVolID: %04x\n", gvp.gvp_volid);
    fprintf(dbg, "\tBtMap: %04x\t", gvp.gvp_bitmap);
    dbg_print_vmap(gvp.gvp_bitmap);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  ivol = EtoIVolid(gvp.gvp_volid); 	/* pick up volume id */
  if (ivol < 0)
    return(ivol);			/* unknown volume */
  if (!VolTbl[ivol]->v_mounted)		/* must have called FPOpenVol first */
    return(aeParamErr);			/* not opened */
  vp = VolTbl[ivol];			/* ptr to volume */
  err = OSVolInfo(pathstr(vp->v_rootd),vp,gvp.gvp_bitmap); /* get vol info */

  if (V_BITTST(VolModBitMap,ivol)) {	/* modified since last call? */
    V_BITCLR(VolModBitMap,ivol);	/* yes... clear the flag */
    vp->v_mdate = CurTime();		/* set modify time */
  }

  if (err != noErr)
    return(err);
  vp->v_bitmap = gvp.gvp_bitmap;
  *rl = htonPackX(VolPackR,(byte *) vp,r);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_vmap();
    void dbg_print_vprm();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tBtMap: %04x\t", vp->v_bitmap);
    dbg_print_vmap(vp->v_bitmap);
    dbg_print_vprm(vp->v_bitmap, r+2, (*rl)-2);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);			/* all ok */
}

/*
 * OSErr FPSetVolParms(byte *p,byte *r,int *rl) [NOOP]
 *
 * This call is used to set the parameters for a particular volume.  The
 * volume is specified by its VolumeID as returned from the FPOpenVol
 * call.
 *
 * In AFP Version 1.0 only the Backup Date field may be set.  Under
 * Unix this is a noop.
 *
 */

/*ARGSUSED*/  
OSErr
FPSetVolParms(p,l,r,rl)
byte *p,*r;
int *rl;
{
  SetVolParmsPkt svp;
  int ivol;

  ntohPackX(PsSetVolParms,p,l,(byte *) &svp); /* unpack */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_vmap();
    void dbg_print_date();
    fprintf(dbg, "\tVolID: %04x\n", svp.svp_volid);
    fprintf(dbg, "\tBtMap: %04x\t", svp.svp_bitmap);
    dbg_print_vmap(svp.svp_bitmap);
    dbg_print_date(svp.svp_backdata);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  ivol = EtoIVolid(svp.svp_volid);
  if (ivol < 0)
    return(aeParamErr);			/* unknown volume */

  if ((svp.svp_bitmap & ~VP_BDATE) != 0)
    return(aeBitMapErr);		/* trying to set unknown parms */

  if (svp.svp_bitmap & VP_BDATE)	/* want to set backup date? */
    VolTbl[ivol]->v_bdate = svp.svp_backdata; /* yes... */

  return(noErr);			/* return ok... */
}

/*
 * OSErr FPOpenVol(byte *p, byte *r, int *rl)
 *
 * This call is used to "mount" a volume.  It must be called before any
 * other call can be made to access objects on the volume.
 *
 */

OSErr 
FPOpenVol(p,l,r,rl)
byte *p;
int l;
byte *r;
int *rl;
{
  OpenVolPkt ovl;
  char pwd[MAXPASSWD+1];		/* null terminated password */
  int v;
  OSErr err;
  VolPtr vp;
  
  ovl.ovl_pass[0] = '\0';		/* zero optional password */
  ntohPackX(PsOpenVol,p,l,(byte *) &ovl); /* decode packet */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_vmap();
    fprintf(dbg, "\tBtMap: %04x\t", ovl.ovl_bitmap);
    dbg_print_vmap(ovl.ovl_bitmap);
    fprintf(dbg, "\tVolNm: \"%s\"\n", ovl.ovl_name);
    fprintf(dbg, "\tVolPw: \"%s\"\n", ovl.ovl_pass);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  strncpy(pwd,(char *) ovl.ovl_pass,MAXPASSWD);	/* copy optional pwd */
  pwd[MAXPASSWD] = '\0';		/* tie off with a null */

  if (DBVOL)
    printf("FPOpenVol: name=%s, pwd=%s, bm=%d\n",
	   ovl.ovl_name,pwd,ovl.ovl_bitmap);
  
  for (v=0; v < VolCnt; v++)		/* locate the volume */
    if (strcmp(VolTbl[v]->v_name,(char *) ovl.ovl_name) == 0)
      break;

  if (v >= VolCnt)			/* did we find a vol in the scan? */
    return(aeParamErr);			/* no... unknown volume name */

  vp = VolTbl[v];			/* dereference */
  if (*vp->v_pwd != '\0') 	/* password exists on volume? */
    if (strcmp(vp->v_pwd,pwd) != 0) /* yes, check for match */
      return(aeAccessDenied);		/* not the same... return failure */

  if (DBVOL)
    printf("FPOpenVol: name=%s volid=%d\n", vp->v_name,vp->v_volid);

  vp->v_mounted = TRUE;		/* now it is opened... */
  
  /* update volume info */
  if ((err = OSVolInfo(vp->v_path,vp,VP_ALL)) != noErr)
    return(err);

  vp->v_bitmap = ovl.ovl_bitmap;	/* bitmap for packing result */

  *rl = htonPackX(VolPackR,(byte *) vp,r);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_vmap();
    void dbg_print_vprm();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tBtMap: %04x\t", vp->v_bitmap);
    dbg_print_vmap(vp->v_bitmap);
    dbg_print_vprm(vp->v_bitmap, r+2, (*rl)-2);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);			/* return ok */
}


/*
 * OSErr FPCloseVol(byte *p,byte *r,int *rl)
 *
 * This call is used to "unmount" a volume.
 *
 */

/*ARGSUSED*/
OSErr 
FPCloseVol(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  CloseVolPkt cv;
  int ivol;

  ntohPackX(PsCloseVol,p,l,(byte *) &cv);
  
#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tVolID: %04x\n", cv.cv_volid);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  ivol = EtoIVolid(cv.cv_volid); /* convert to internal format */
  if (ivol < 0)			/* error code */
    return(ivol);

  if (DBVOL)
    printf("FPCloseVol %d=%s\n",ivol,VolTbl[ivol]->v_name);

  if (!VolTbl[ivol]->v_mounted)		/* was it mounted? */
    return(aeMiscErr);			/* no... not mounted */

  VolTbl[ivol]->v_mounted = FALSE;	/* indicate no longer mounted */
  return(noErr);			/* and return ok */
}


/*
 * OSErr FPFlush(byte *p,byte *r, int *rl)
 *
 * This call is used to flush to disk any data relating to the specified
 * volume that has been modified by the user.
 *
 */

/*ARGSUSED*/
OSErr
FPFlush(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  FlushPkt fls;
  int ivol;

  ntohPackX(PsFlush,p,l,(byte *) &fls);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tVolID: %04x\n", fls.fls_volid);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBVOL)
    printf("FPFLush: ...\n");

  ivol = EtoIVolid(fls.fls_volid);
  if (ivol < 0)
    return(ivol);

  return(OSFlush(ivol));
}

/*
 * IDirP VolRootD(int volid)
 *
 * Return the internal directory pointer for this volumes root directory.
 * Root directory is the volumes mount point, or initial path.
 *
 */

IDirP
VolRootD(volid)
int volid;
{
  if (volid > VolCnt)
    return(NILDIR);
  return(VolTbl[volid]->v_rootd);
}

/*
 * void VolModified(VolBitMap volbm)
 *
 * Indicate that the volumes specified in volbm have been modified.
 *
 */

void
VolModified(volbm)
VolBitMap volbm;
{
  V_BITOR(VolModBitMap,			/* result */
	  VolModBitMap,volbm);		/* is OR of these two */
}


char *
VolName(volid)
{
  if (volid > VolCnt)
    return("");
  return(VolTbl[volid]->v_name);
}

#ifdef SHORT_NAMES
char *
VolSName(volid)
{
  static char temp[9];

  if (volid > VolCnt)
    return("");
  strncpy(temp,VolTbl[volid]->v_name,8);
  temp[8] = '\0';
  if (DBVOL)
    printf("VolSname %s\n",temp);
  return(temp);
}
#endif SHORT_NAMES

word 
ItoEVolid(iv)
int iv;
{
  return(VolTbl[iv]->v_volid);		/* return volume id */
}

int
EtoIVolid(ev)
word ev;
{
  int iv;
  for (iv=0; iv < VolCnt; iv++)
    if (VolTbl[iv]->v_volid == ev)
      return(iv);
  if (DBVOL)
    printf("EtoIVolid: Bad Volid %d\n",ev);
  return(aeParamErr);
}

#ifdef CREATE_AFPVOL
/*
 * int aufsDirSetup(char *home, char *usr)
 *
 * Added by Heather Ebey, UC San Diego <hebey@ucsd.edu>
 *
 * Sets up home directory of UNIX user needing aufs(1) for file sharing.
 * Will create .afpvols and appropriate directory/subdirectories.
 *
 * CREATE_AFPVOL is defined as the name of the AUFS Mac volume
 *
 */

int
aufsDirSetup(home, usr)
char *home, *usr;
{
  char volfname[MAXDLEN+20];
  char dirname[MAXDLEN+20];
  char subdir[MAXDLEN+20];
  extern int errno;
  FILE *fp;

  strcpy(volfname, home);
  strcat(volfname, "/");
  strcat(volfname, VOLFILE1); /* .afpvols */
  if ((fp = fopen(volfname, "w")) == NULL) {
    logit(0, "CREATE_AFPVOL: can't open %s for writing", volfname);
    return(-1);
  }
#ifdef CREAT_AFPVOL_NAM
  { char *makeVolName(), *cp;
    char host[64], name[128];
    gethostname(host, sizeof(host));
    host[sizeof(host)-1] = '\0';
    if ((cp = index(host, '.')) != NULL)
      *cp = '\0'; /* remove domain name */
    sprintf(name, makeVolName(CREAT_AFPVOL_NAM,usr,host,CREATE_AFPVOL,home));
    if (fprintf(fp, "~%s/%s:%s\n", usr, CREATE_AFPVOL, name) < 0) {
      logit(0, "CREATE_AFPVOL: error in fprintf()");
      (void)fclose(fp);
      return(-1);
    }
  }
#else  CREAT_AFPVOL_NAM
  if (fprintf(fp, "~%s/%s:%s\n", usr, CREATE_AFPVOL, CREATE_AFPVOL) < 0) {
    logit(0, "CREATE_AFPVOL: error in fprintf()");
    (void)fclose(fp);
    return(-1);
  }
#endif CREAT_AFPVOL_NAM
  (void)fclose(fp);
  sprintf(subdir, "%s/%s", home, FIDIRFN);
  if (mkdir(subdir, 0700) < 0) {
    if (errno != EEXIST) {
      logit(0, "CREATE_AFPVOL: unable to create %s", subdir);
      return(-1);
    }
  }
  sprintf(subdir, "%s/%s", home, RFDIRFN);
  if (mkdir(subdir, 0700) < 0) {
    if (errno != EEXIST) {
      logit(0, "CREATE_AFPVOL: unable to create %s", subdir);
      return(-1);
    }
  }
  sprintf(dirname, "%s/%s", home, CREATE_AFPVOL);
  if (mkdir(dirname, 0700) < 0) {
    if (errno != EEXIST) {
      logit(0, "CREATE_AFPVOL: unable to create %s", dirname);
      return(-1);
    }
  }
  sprintf(subdir, "%s/%s", dirname, FIDIRFN);
  if (mkdir(subdir, 0700) < 0) {
    if (errno != EEXIST) {
      logit(0, "CREATE_AFPVOL: unable to create %s", subdir);
      return(-1);
    }
  }
  sprintf(subdir, "%s/%s", dirname, RFDIRFN);
  if (mkdir(subdir, 0700) < 0) {
    if (errno != EEXIST) {
      logit(0, "CREATE_AFPVOL: unable to create %s", subdir);
      return(-1);
    }
  }
  if ((fp = fopen(volfname, "r")) == NULL) {
    logit(0, "CREATE_AFPVOL: can't open %s for reading", volfname);
    return(-1);
  }
  (void)VRdVFile(fp);	/* read vols file from user */
  return(0);
}

#ifdef CREAT_AFPVOL_NAM
/*
 * build a Mac volume name out of specified args in fmt
 *	%U username
 *	%H hostname
 *	%V volumename
 *	%D home directory
 *
 */

char *
makeVolName(fmt, user, host, volm, home)
char *fmt, *user, *host, *volm, *home;
{
  char *p, *q;
  static char string[128];

  p = fmt;
  q = string;
  string[0] = '\0';

  while (*p != '\0') {
    if (*p == '%' && *(p+1) != '\0') {
      switch (*(p+1)) {
	case 'U':
	  strcat(string, user);
	  q += strlen(user);
	  break;
	case 'H':
	  strcat(string, host);
	  q += strlen(host);
	  break;
	case 'V':
	  strcat(string, volm);
	  q += strlen(volm);
	  break;
	case 'D':
	  strcat(string, home);
	  q += strlen(home);
	  break;
      }
      p += 2;
      continue;
    }
    *q++ = *p++;
    *q = '\0';
  }

  return(string);
}
#endif CREAT_AFPVOL_NAM
#endif CREATE_AFPVOL

#ifdef DEBUG_AFP_CMD
/*
 * print bitmap for Volume Parameters
 *
 */

void
dbg_print_vmap(bmap)
u_short bmap;
{
  int i, j;

  if (dbg != NULL) {
    fprintf(dbg, "(");
    for (i = 0, j = 0; i < 16; i++) {
      if (bmap & (0x0001 << i)) {
	bmap &= ~(0x0001 << i);
	switch (i) {
	  case 0:
	    fprintf(dbg, "Attributes");
	    j++;
	    break;
	  case 1:
	    fprintf(dbg, "Signature");
	    j++;
	    break;
	  case 2:
	    fprintf(dbg, "Creat Date");
	    j++;
	    break;
	  case 3:
	    fprintf(dbg, "Modif Date");
	    j++;
	    break;
	  case 4:
	    fprintf(dbg, "Bakup Date");
	    j++;
	    break;
	  case 5:
	    fprintf(dbg, "Volume ID");
	    j++;
	    break;
	  case 6:
	    fprintf(dbg, "Bytes Free");
	    j++;
	    break;
	  case 7:
	    fprintf(dbg, "Bytes Total");
	    j++;
	    break;
	  case 8:
	    fprintf(dbg, "Volume Name");
	    j++;
	    break;
	  default:
	    fprintf(dbg, "Unknwn Bit");
	    j++;
	    break;
	}
	if (bmap)
	  fprintf(dbg, ", ");
	if (bmap && (j % 4) == 0)
	  fprintf(dbg, "\n\t\t\t");
      }
    }
    fprintf(dbg, ")\n");
  }

  return;
}

/*
 * dump Volume parameters described by bitmap
 *
 */

#define get2(s)	(u_short)(((s)[0]<<8)|((s)[1]))
#define get4(s)	(u_int)(((s)[0]<<24)|((s)[1]<<16)|((s)[2]<<8)|((s)[3]))

void
dbg_print_vprm(bmap, r, rl)
u_short bmap;
byte *r;
int rl;
{
  int i, j;
  byte *p, *q;
  short offset;
  void dbg_print_date();
  void dbg_print_vatr();

  p = r; /* parameters */

  if (dbg != NULL) {
    for (i = 0; i < 16 && rl > 0; i++) {
      if (bmap & (0x0001 << i)) {
	switch (i) {
	  case 0:
	    fprintf(dbg, "\tAttributes: ");
	    dbg_print_vatr(get2(r));
	    rl -= 2;
	    r += 2;
	    break;
	  case 1:
	    fprintf(dbg, "\tVSignature: %04x\n", get2(r));
	    rl -= 2;
	    r += 2;
	    break;
	  case 2:
	    fprintf(dbg, "\tCreat Date: ");
	    dbg_print_date(get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 3:
	    fprintf(dbg, "\tModif Date: ");
	    dbg_print_date(get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 4:
	    fprintf(dbg, "\tBakup Date: ");
	    dbg_print_date(get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 5:
	    fprintf(dbg, "\t Volume ID: %04x\n", get2(r));
	    rl -= 2;
	    r += 2;
	    break;
	  case 6:
	    fprintf(dbg, "\tBytes Free: %d\n", get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 7:
	    fprintf(dbg, "\tBytes Totl: %d\n", get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 8:
	    fprintf(dbg, "\tVolume Nam: \"");
	    offset = get2(r);
	    q = p + offset;
	    for (j = 0; j < (int)*q; j++)
	      fprintf(dbg, "%c", *(q+j+1));
	    fprintf(dbg, "\"\n");
	    rl -= 2;
	    r += 2;
	    break;
	  default:
	    fprintf(dbg, "\tUnknwn Bit: %d\n", i);
	    break;
	}
      }
    }
  }

  return;
}

/*
 * print volume attributes
 *
 */

void
dbg_print_vatr(attr)
u_short attr;
{
  int i, j;

  if (dbg != NULL) {
    fprintf(dbg, "%04x (", attr);
    for (i = 0, j = 0; i < 16; i++) {
      if (attr & (0x0001 << i)) {
	attr &= ~(0x0001 << i);
	switch (i) {
	  case 0:
	    fprintf(dbg, "ReadOnlyVol");
	    j++;
	    break;
	  case 1:
            fprintf(dbg, "HasVolPaswd");
	    j++;
	    break;
	  case 2:
	    fprintf(dbg, "SuppFileIDs");
	    j++;
	    break;
	  case 3:
	    fprintf(dbg, "SuppCatSrch");
	    j++;
	    break;
	  case 4:
	    fprintf(dbg, "SuppBlnkAcs");
	    j++;
	    break;
	  default:
	    fprintf(dbg, "<unknown %d>", i);
	    j++;
	    break;
	}
	if (attr)
	  fprintf(dbg, ", ");
	if (attr && (j % 4) == 0)
	  fprintf(dbg, "\n\t\t\t");
      }
    }
    fprintf(dbg, ")\n");
  }

  return;
}
#endif /* DEBUG_AFP_CMD */
