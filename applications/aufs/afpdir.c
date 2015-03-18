/*
 * $Author: djh $ $Date: 1996/06/19 04:26:25 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpdir.c,v 2.8 1996/06/19 04:26:25 djh Rel djh $
 * $Revision: 2.8 $
 *
 */

/*
 * afpdir.c - Appletalk Filing Protocol Directory Level Routines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *  December 1990  djh		tidy up for AFP 2.0
 *
 */

/*
 * Non OS dependant support routines for:
 *
 * FPGetDirParms()
 * FPSetDirParms()
 * FPOpenDir()
 * FPCloseDir()
 * FPEnumerate()
 * FPCreateDir()
 * FPGetFileDirInfo()
 *
 */

#include <stdio.h>
#include <sys/param.h>
#ifndef _TYPES
 /* assume included by param.h */
# include <sys/types.h>
#endif
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include "afpntoh.h"
#include "afps.h"			/* common server header */

#ifdef DEBUG_AFP_CMD
#include <sys/time.h>
#include <ctype.h>
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

private int EnumPack();

/*
 * OSErr FPGetDirParms(byte *p,byte *r,int *rl)
 *
 * This call is used to retrieve parameters for a particular directory.
 *
 */

OSErr
FPGetFileDirParms(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  GetFileDirParmsPkt gfdp;
  FileDirParm fdp;
  IDirP idir,ipdir;
  char file[MAXUFLEN];
  int ivol,err;

  ntohPackX(PsGetFileDirParms,p,l,(byte *) &gfdp);

  err = EtoIfile(file,&idir,&ipdir,&ivol,gfdp.gdp_dirid,
		 gfdp.gdp_volid,gfdp.gdp_ptype,gfdp.gdp_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", gfdp.gdp_volid);
    fprintf(dbg, "\tDirID: %08x\n", gfdp.gdp_dirid);
    fprintf(dbg, "\tFBMap: %04x\t", gfdp.gdp_fbitmap);
    dbg_print_bmap(gfdp.gdp_fbitmap, 0);
    fprintf(dbg, "\tDBMap: %04x\t", gfdp.gdp_dbitmap);
    dbg_print_bmap(gfdp.gdp_dbitmap, 1);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", gfdp.gdp_ptype,
      (gfdp.gdp_ptype == 1) ? "Short" : "Long");
    dbg_print_path(gfdp.gdp_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr) {
    if (DBFIL || DBDIR)
      printf("FPGetFileDirParms: EtoIfile returns %d\n",err);
#ifdef SHORT_NAMES
    if (gfdp.gdp_dirid > 1000)
	return(aeObjectNotFound); 
#endif SHORT_NAMES
    return(err);
  }

  fdp.fdp_pdirid = ItoEdirid(ipdir,ivol);
  fdp.fdp_dbitmap = gfdp.gdp_dbitmap;
  fdp.fdp_fbitmap = gfdp.gdp_fbitmap;
  fdp.fdp_zero = 0;
  err = OSFileDirInfo(ipdir,idir,file,&fdp,ivol); /* fill in information */
  if (err != noErr) {
    if (DBFIL || DBDIR)
      printf("FPGetFileDirParms: OSFileDirInfo returns %d on %s/%s\n",
	     err,pathstr(ipdir),file);
    return(err);
  }
  if (FDP_ISDIR(fdp.fdp_flg)) {
    *rl = htonPackX(DirParmPackR,(byte *)&fdp,r);
    *rl += htonPackX(DirPackR,(byte *)&fdp,r+(*rl));
  }
  if (!FDP_ISDIR(fdp.fdp_flg)) {
    *rl = htonPackX(DirParmPackR,(byte *)&fdp,r);    
    *rl += htonPackX(FilePackR,(byte *)&fdp,r+(*rl));
  }

  /*
   * check for bogus bitmaps & truncate response
   * (DON'T return aeBitMapErr)
   *
   */
  if ((!FDP_ISDIR(fdp.fdp_flg) && gfdp.gdp_dbitmap && !gfdp.gdp_fbitmap)
   || (FDP_ISDIR(fdp.fdp_flg) && !gfdp.gdp_dbitmap && gfdp.gdp_fbitmap))
    *rl =6;

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_parm();
    void dbg_print_bmap();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tFBMap: %04x\t", fdp.fdp_fbitmap);
    dbg_print_bmap(fdp.fdp_fbitmap, 0);
    fprintf(dbg, "\tDBMap: %04x\t", fdp.fdp_dbitmap);
    dbg_print_bmap(fdp.fdp_dbitmap, 1);
    fprintf(dbg, "\tFDFlg: %02x\t(%s)\n", fdp.fdp_flg,
      FDP_ISDIR(fdp.fdp_flg) ? "Directory" : "File");
    if (*rl == 6)
      fprintf(dbg, "\t<No Parameter List>\n");
    else
      dbg_print_parm(FDP_ISDIR(fdp.fdp_flg) ? fdp.fdp_dbitmap:fdp.fdp_fbitmap, 
        r+6, (*rl)-6, FDP_ISDIR(fdp.fdp_flg) ? 1 : (idir) ? 1 : 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * OSErr FPEnumerate(...)
 *
 * This call is used to enumerate the contents of a directory.  The
 * reply is composed of a number of file and/or directory parameter
 * structures.
 *
 */

OSErr 
FPEnumerate(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  FileDirParm fdp;
  EnumeratePkt enup;
  EnumerateReplyPkt enpr;
  IDirP idir,ipdir;
  char file[MAXUFLEN];
  byte *cntptr;
  int ivol,stidx,idx,maxidx;
  word cnt;
  int reqcnt,maxreply,len,elen,err;
  extern int sqs;		/* maximum send qs */

  ntohPackX(PsEnumerate,p,l,(byte *) &enup);

  err = EtoIfile(file,&idir,&ipdir,&ivol,enup.enu_dirid,
		 enup.enu_volid,enup.enu_ptype,enup.enu_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", enup.enu_volid);
    fprintf(dbg, "\tDirID: %08x\n", enup.enu_dirid);
    fprintf(dbg, "\tFBMap: %04x\t", enup.enu_fbitmap);
    dbg_print_bmap(enup.enu_fbitmap, 0);
    fprintf(dbg, "\tDBMap: %04x\t", enup.enu_dbitmap);
    dbg_print_bmap(enup.enu_dbitmap, 1);
    fprintf(dbg, "\tRqCnt: %04x\n", enup.enu_reqcnt);
    fprintf(dbg, "\tStart: %04x\n", enup.enu_stidx);
    fprintf(dbg, "\tMaxRp: %04x\n", enup.enu_maxreply);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", enup.enu_ptype,
      (enup.enu_ptype == 1) ? "Short" : "Long");
    dbg_print_path(enup.enu_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  if (idir == NILDIR)
    return(aeDirNotFound);

  /* set the bitmaps for return message and packing */
  
  fdp.fdp_fbitmap = enpr.enur_fbitmap = enup.enu_fbitmap;
  fdp.fdp_dbitmap = enpr.enur_dbitmap = enup.enu_dbitmap;

  /* set the parent dirid to the enumerated directory */

  fdp.fdp_pdirid = ItoEdirid(idir,ivol); 

  /* fetch the max size of a reply packet, start index, request count */

  maxreply = enup.enu_maxreply;
  maxreply = min(sqs, maxreply);
  stidx = enup.enu_stidx;
  reqcnt = enup.enu_reqcnt;

  maxidx = OSEnumInit(idir);		/* init, and fetch count of entries */
  if (maxidx < 0)			/* error? */
    return(maxidx);			/* return error */

  if (stidx > maxidx) {			/* start index gt count of entries? */
    OSEnumDone(idir);
    return(aeObjectNotFound);		/* yes... object not found then */
  }

  cntptr = &r[ENUR_ACTCNT_OFF];		/* address of packed actcnt word */

  len = htonPackX(EnumPackR,(byte *)&enpr,r);

  /* starting with the file/directory at stidx, load upto reqcnt
   * entries into the reply buffer.  The size of the reply buffer must
   * not exceed maxreply bytes.  Do not include a file/directory if
   * the associated bitmap is zero.
   */

  for (idx=stidx,cnt=0; idx <= maxidx && cnt < (word)reqcnt; idx++) {
    elen = EnumPack(idir,&fdp,ivol,idx,&r[len]);
    if (elen > 0) {			/* something packed for this entry? */
      if (len+elen > maxreply)		/* yes... check if overflow */
	break;				/* yes.. break out */
      cnt++;				/* else include entry in count */
      len += elen;			/* include entry in len */
      if (len > maxreply-30)		/* if close to the limit */
	break;				/*  then break out now */
    }
  }

  OSEnumDone(idir);			/* finished with enumerate */

  if (cnt == 0)				/* filter tossed all */
    return(aeObjectNotFound);

  PackWord(cnt,cntptr);			/* pack the actual count */
  *rl = len;				/* length of packet for reply */

  if (DBDIR)
    printf("OSEnum: maxreply=%d, ourreply=%d, reqcnt=%d, ourcnt=%d\n",
	   maxreply,len,reqcnt,cnt);
  
#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    int i = cnt;
    byte *q = r + 6;
    void dbg_print_bmap();
    void dbg_print_parm();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tFBtMap: %04x\t", enpr.enur_fbitmap);
    dbg_print_bmap(enpr.enur_fbitmap, 0);
    fprintf(dbg, "\tDBtMap: %04x\t", enpr.enur_dbitmap);
    dbg_print_bmap(enpr.enur_dbitmap, 1);
    fprintf(dbg, "\tActCnt: %d\n", cnt);
    while (i-- > 0) {
      fprintf(dbg, "\t----------\n");
      fprintf(dbg, "\tStrcLn: %d\n", (int)*q);
      fprintf(dbg, "\tEnType: %02x\t(%s)\n", *(q+1),
	(*(q+1) & 0x80) ? "Directory" : "File");
      dbg_print_parm((*(q+1) & 0x80) ? enpr.enur_dbitmap : enpr.enur_fbitmap,
	q+2, (*q)-2, (*(q+1) & 0x80) ? 1 : 0);
      q += *q;
    }
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);			/* all ok */
}

/*
 * int EnumPack(IDirP ipdir, FileDirParm *fdp, int ivol, 
 *		int idx, byte *r);
 *
 * Pack a single enumeration entry as specified by the enumeration
 * idx and the enumeration direcotry ipdir.
 *
 * Fetch file/directory information for the entry as specified by
 * the parent directory (ipdir), the volume (ivol), and the name
 * from enumeration index (idx).
 *
 * If the file bitmap passed in fdp is zero and the entry is a file
 * then return 0.  If the directory bitmap passed in fdp is zero and
 * the entry is a directory then return 0.  
 *
 * In the normal case pack a length byte, directory flag byte, and
 * the file/directory information as specified by the fdp bitmaps
 * int r and return the length of of this entry.
 *
 */ 

private int
EnumPack(ipdir,fdp,ivol,idx,r)
IDirP ipdir;
FileDirParm *fdp;
int ivol,idx;
byte *r;
{
  char *fn;
  int len;
  int err;

  fn = (char *) OSEnumGet(ipdir,idx);	/* get the file name */
  /* and the info for this entry (nildir - no info on whether */
  /* directory or not) */
  err = OSFileDirInfo(ipdir,NILDIR,fn,fdp,ivol);
  if (err == aeAccessDenied)		/* if no access */
    return(0);				/*  then forget the entry */

  if (FDP_ISDIR(fdp->fdp_flg)) {	/* if a directory */
    if (fdp->fdp_dbitmap == 0)		/*  and dir bitmap is zero */
      return(0);			/*  then skip the entry */
    len = htonPackX(DirPackR,(byte *) fdp,r+2);	/* else pack */
  }  else {				/* else, if a file */
    if (fdp->fdp_fbitmap == 0)		/*  and file bitmap is zero  */
      return(0);			/*  then skip the entry */
    len = htonPackX(FilePackR,(byte *) fdp,r+2); /* else pack */
  }

  len += 2;				/* include size, flg into sum */
  if ((len % 2) != 0)			/* if odd number of bytes */
    r[len++] = 0;			/* then even out the length */
  r[0] = (byte) len;			/* store length of structure */
  r[1] = fdp->fdp_flg;			/* directory flag */
  return(len);				/* return length of this item */
}

/*ARGSUSED*/
OSErr
FPSetDirParms(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  SetDirParmsPkt sdp;
  FileDirParm fdp;
  IDirP ipdir,idir;
  int ivol,len,err;
  char file[MAXUFLEN];

  len = ntohPackX(PsSetDirParms,p,l,(byte *) &sdp);
  ntohPackXbitmap(ProtoDirAttr,p+len,l-len,
		  (byte *) &fdp,sdp.sdp_bitmap);

  err = EtoIfile(file,&idir,&ipdir,&ivol,sdp.sdp_dirid,
		 sdp.sdp_volid,sdp.sdp_ptype,sdp.sdp_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    char *pathstr();
    void dbg_print_bmap();
    void dbg_print_parm();
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", sdp.sdp_volid);
    fprintf(dbg, "\tDirID: %08x\n", sdp.sdp_dirid);
    fprintf(dbg, "\tFBMap: %04x\t", sdp.sdp_bitmap);
    dbg_print_bmap(sdp.sdp_bitmap, 1);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", sdp.sdp_ptype,
      (sdp.sdp_ptype == 1) ? "Short" : "Long");
    dbg_print_path(sdp.sdp_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    dbg_print_parm(sdp.sdp_bitmap, p+len, l-len, 1);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);  

  fdp.fdp_dbitmap = sdp.sdp_bitmap;	/* using this bitmap  */

  if (DBDIR) 
    printf("FPSetDirParms: path=%s, file=%s, bm=%d\n",
	   pathstr(ipdir),file,sdp.sdp_bitmap);

  err = OSSetDirParms(ipdir,file,&fdp);	/* set dirparms */
  if (err == noErr)
    VolModified(ivol);
  return(err);
}
  
#ifdef SHORT_NAMES
OSErr 
FPOpenDir(p,l,r,rl)
byte *p, *r;
int l;
int *rl;
{
 OpenDirPkt ODPkt;
 IDirP idir,ipdir;
 char file[MAXUFLEN];
 int ivol,err;

  ntohPackX(PsOpenDir,p,l,(byte*)&ODPkt);

  err = EtoIfile(file,&idir,&ipdir,&ivol,ODPkt.odr_dirid, ODPkt.odr_volid,
           ODPkt.odr_ptype,ODPkt.odr_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", ODPkt.odr_volid);
    fprintf(dbg, "\tDirID: %08x\n", ODPkt.odr_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", ODPkt.odr_ptype,
      (ODPkt.odr_ptype == 1) ? "Short" : "Long");
    dbg_print_path(ODPkt.odr_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((err != noErr) && (DBDIR)) 
     printf("error returned from EtoIfile\n");
   PackDWord(ItoEdirid(idir,ivol),r);
   *rl = 4;
  return(noErr);
}

#else SHORT_NAMES
OSErr 
FPOpenDir()
{
  return(aeParamErr);
}
#endif SHORT_NAMES

OSErr 
FPCloseDir()
{
  return(aeParamErr);
}


OSErr
FPCreateDir(p,l,r,rl)
byte *p,*r;
int l;
int *rl;
{
  CreateDirPkt crd;
  IDirP idir,ipdir;
  int ivol,err;
  char file[MAXUFLEN];

  ntohPackX(PsCreateDir,p,l,(byte *) &crd);

  err = EtoIfile(file,&idir,&ipdir,&ivol,crd.crd_dirid,
		 crd.crd_volid,crd.crd_ptype,crd.crd_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", crd.crd_volid);
    fprintf(dbg, "\tDirID: %08x\n", crd.crd_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", crd.crd_ptype,
      (crd.crd_ptype == 1) ? "Short" : "Long");
    dbg_print_path(crd.crd_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  if (DBFIL)
    printf("FPCreateDir: create path=%s name=%s\n",pathstr(ipdir),file);

  if ((err = OSCreateDir(ipdir,file, &idir)) == noErr) {
    if (DBFIL)
       printf("FPCreateDir: create path=%s name=%s edirID=%d\n",
					pathstr(ipdir),file,idir->edirid);
    PackDWord(ItoEdirid(idir,ivol),r);
    *rl = 4;
  }
  if (err == noErr)			/* if success  */
    VolModified(ivol);			/*  then volume modified */
  return(err);
}


/*
 * Preliminary version - just does files
 *
*/

/*ARGSUSED*/
OSErr
FPSetFileDirParms(p,l,r,rl)
byte *p, *r;
int l;
int *rl;
{
  SetFileDirParmsPkt scp;
  FileDirParm fdp;
  IDirP idir,ipdir;
  char file[MAXUFLEN];
  int ivol,len,err;

  len = ntohPackX(PsSetFileDirParms,p,l,(byte *) &scp);
  ntohPackXbitmap(ProtoFileDirAttr,p+len,l-len,(byte *) &fdp,scp.scp_bitmap);

  err = EtoIfile(file,&idir,&ipdir,&ivol,scp.scp_dirid,
		 scp.scp_volid,scp.scp_ptype,scp.scp_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    void dbg_print_parm();
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", scp.scp_volid);
    fprintf(dbg, "\tDirID: %08x\n", scp.scp_dirid);
    fprintf(dbg, "\tBtMap: %04x\t", scp.scp_bitmap);
    dbg_print_bmap(scp.scp_bitmap, 0);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", scp.scp_ptype,
      (scp.scp_ptype == 1) ? "Short" : "Long");
    dbg_print_path(scp.scp_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    dbg_print_parm(scp.scp_bitmap, p+len, l-len, (idir) ? 1 : 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  fdp.fdp_dbitmap = fdp.fdp_fbitmap = scp.scp_bitmap;

  if (DBFIL) 
    printf("FPSetFileDirParms: setting bm=%d for %s %s\n",
	   scp.scp_bitmap,pathstr(ipdir),file);

  return(OSSetFileDirParms(ipdir,idir,file,&fdp));
}

#ifdef DEBUG_AFP_CMD
/*
 * describe FPGetFileDirParms/FPGetFileInfo bitmaps
 *
 */

void
dbg_print_bmap(bmap, typ)
u_short bmap;
int typ;
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
	    fprintf(dbg, "Parent DID");
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
	    fprintf(dbg, "Findr Info");
	    j++;
	    break;
	  case 6:
	    fprintf(dbg, "Long Name");
	    j++;
	    break;
	  case 7:
	    fprintf(dbg, "Short Name");
	    j++;
	    break;
	  case 8:
	    if (typ)
	      fprintf(dbg, "Direct ID");
	    else
	      fprintf(dbg, "File Numbr");
	    j++;
	    break;
	  case 9:
	    if (typ)
	      fprintf(dbg, "OffSpr Cnt");
	    else
	      fprintf(dbg, "DFork Len");
	    j++;
	    break;
	  case 10:
	    if (typ)
	      fprintf(dbg, "Owner ID");
	    else
	      fprintf(dbg, "RFork Len");
	    j++;
	    break;
	  case 11:
	    if (typ)
	      fprintf(dbg, "Group ID");
	    j++;
	    break;
	  case 12:
	    if (typ) {
	      fprintf(dbg, "Acs Rights");
	      j++;
	    }
	    break;
	  case 13:
	    fprintf(dbg, "ProDos Inf");
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
 * dump parameters described by bitmap
 *
 */

#define get2(s)	(u_short)(((s)[0]<<8)|((s)[1]))
#define get4(s)	(u_int)(((s)[0]<<24)|((s)[1]<<16)|((s)[2]<<8)|((s)[3]))

void
dbg_print_parm(bmap, r, rl, typ)
u_short bmap;
byte *r;
int rl, typ;
{
  int i, j;
  byte *p, *q;
  short offset;
  void dbg_print_attr();
  void dbg_print_date();
  void dbg_print_accs();
  void dbg_print_fndr();

  p = r; /* parameters */

  if (dbg != NULL) {
    for (i = 0; i < 16 && rl > 0; i++) {
      if (bmap & (0x0001 << i)) {
	switch (i) {
	  case 0:
	    fprintf(dbg, "\tAttributes: ");
	    dbg_print_attr(get2(r), typ);
	    rl -= 2;
	    r += 2;
	    break;
	  case 1:
	    fprintf(dbg, "\tParent DID: %08x\n", get4(r));
	    rl -= 4;
	    r += 4;
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
	    fprintf(dbg, "\tFindr Info:\n");
	    dbg_print_fndr(r, typ);
	    rl -= 32;
	    r += 32;
	    break;
	  case 6:
	    fprintf(dbg, "\t Long Name: \"");
	    offset = get2(r);
	    q = p + offset;
	    for (j = 0; j < (int)*q; j++)
	      fprintf(dbg, "%c", *(q+j+1));
	    fprintf(dbg, "\"\n");
	    rl -= 2;
	    r += 2;
	    break;
	  case 7:
	    fprintf(dbg, "\tShort Name: ");
	    offset = get2(r);
	    q = p + offset;
	    for (j = 0; j < (int)*q; j++)
	      fprintf(dbg, "%c", *(q+j+1));
	    fprintf(dbg, "\"\n");
	    rl -= 2;
	    r += 2;
	    break;
	  case 8:
	    if (typ)
	      fprintf(dbg, "\t Direct ID: %08x\n", get4(r));
	    else
	      fprintf(dbg, "\tFile Numbr: %08x\n", get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 9:
	    if (typ) {
	      fprintf(dbg, "\tOffSpr Cnt: %d\n", get2(r));
	      rl -= 2;
	      r += 2;
	    } else {
	      fprintf(dbg, "\t DFork Len: %d\n", get4(r));
	      rl -= 4;
	      r += 4;
	    }
	    break;
	  case 10:
	    if (typ)
	      fprintf(dbg, "\t  Owner ID: %08x\n", get4(r));
	    else
	      fprintf(dbg, "\t RFork Len: %d\n", get4(r));
	    rl -= 4;
	    r += 4;
	    break;
	  case 11:
	    if (typ) {
	      fprintf(dbg, "\t  Group ID: %08x\n", get4(r));
	      rl -= 4;
	      r += 4;
	    }
	    break;
	  case 12:
	    if (typ) {
	      fprintf(dbg, "\tAcs Rights: ");
	      dbg_print_accs(r);
	      rl -= 4;
	      r += 4;
	    }
	    break;
	  case 13:
	    fprintf(dbg, "\tProDos Inf:\n");
	    rl -= 6;
	    r += 6;
	    break;
	  default:
	    fprintf(dbg, "\t<unknwn bit: %d>\n", i);
	    break;
	}
      }
    }
  }

  return;
}

/*
 * Print the 16-bit File/Dir attributes
 *
 */

void
dbg_print_attr(attr, typ)
u_int attr;
int typ;
{
  int i, j;

  if (dbg != NULL) {
    fprintf(dbg, "%04x (", attr);
    for (i = 0, j = 0; i < 16; i++) {
      if (attr & (0x0001 << i)) {
	attr &= ~(0x0001 << i);
	switch (i) {
	  case 0:
	    fprintf(dbg, "Invisible");
	    j++;
	    break;
	  case 1:
	    if (typ)
              fprintf(dbg, "IsExpFolder");
	    else
	      fprintf(dbg, "MultiUser");
	    j++;
	    break;
	  case 2:
	    fprintf(dbg, "System");
	    j++;
	    break;
	  case 3:
	    if (typ)
	      fprintf(dbg, "Mounted");
	    else
	      fprintf(dbg, "DAlrdyOpen");
	    j++;
	    break;
	  case 4:
	    if (typ)
	      fprintf(dbg, "InExpFolder");
	    else
	      fprintf(dbg, "RAlrdyOpen");
	    j++;
	    break;
	  case 5:
	    if (typ)
	      fprintf(dbg, "<unused %d>", i);
	    else
	      fprintf(dbg, "RDOnly/WrtInhib");
	    j++;
	    break;
	  case 6:
	    fprintf(dbg, "BackupNeeded");
	    j++;
	    break;
	  case 7:
	    fprintf(dbg, "RenameInhib");
	    j++;
	    break;
	  case 8:
	    fprintf(dbg, "DeleteInhib");
	    j++;
	    break;
	  case 9:
	    if (typ)
	      fprintf(dbg, "<unused %d>");
	    else
	      fprintf(dbg, "CopyProtect");
	    j++;
	    break;
	  case 15:
	    fprintf(dbg, "Set/Clear");
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

/*
 * print Finder Information
 *
 */

void
dbg_print_fndr(f, typ)
byte *f;
int typ;
{
  void dbg_print_type();

  /*
   * File Finder Info
   *
   */
  if (typ == 0) {
    dbg_print_type("\t    FilTyp:", f);
    f += 4;
    dbg_print_type("\t    Creatr:", f);
    f += 4;
    fprintf(dbg, "\t    FdrFlg: %04x\n", get2(f));
    f += 2;
    fprintf(dbg, "\t    Locatn: %04x %04x\n", get2(f), get2(f+2));
    f += 4;
    fprintf(dbg, "\t    Window: %04x\n", get2(f));
    f += 2;
    fprintf(dbg, "\t    IconID: %04x\n", get2(f));
    f += 2;
    f += 8; /* unused */
    fprintf(dbg, "\t    CommID: %04x\n", get2(f));
    f += 2;
    fprintf(dbg, "\t    HomeID: %08x\n", get4(f));

    return;
  }

  /*
   * Directory Finder Info
   *
   */
  if (typ == 1) {
    fprintf(dbg, "\t    DiRect: %04x %04x %04x %04x\n",
      get2(f), get2(f+2), get2(f+4), get2(f+6));
    f += 8;
    fprintf(dbg, "\t    FdrFlg: %04x\n", get2(f));
    f += 2;
    fprintf(dbg, "\t    Locatn: %04x %04x\n", get2(f), get2(f+2));
    f += 4;
    fprintf(dbg, "\t    FdView: %04x\n", get2(f));
    f += 2;
    fprintf(dbg, "\t    Scroll: %04x %04x\n", get2(f), get2(f+2));
    f += 4;
    fprintf(dbg, "\t    DChain: %08x\n", get4(f));
    f += 4;
    fprintf(dbg, "\t    Script: %02x\n", *f);
    fprintf(dbg, "\t    XFlags: %02x\n", *(f+1));
    f += 2;
    fprintf(dbg, "\t    CommID: %04x\n", get2(f));
    f += 2;
    fprintf(dbg, "\t    HomeID: %08x\n", get4(f));

    return;
  }

  return;
}

/*
 * print 4 byte access rights
 *
 */

void
dbg_print_accs(accs)
byte *accs;
{
  if (dbg != NULL) {
    if (accs[0] & 0x80)
      fprintf(dbg, "OWNER, ");
    if (accs[0] & 0x10)
      fprintf(dbg, "BLANK ACCESS, ");
    fprintf(dbg, "UARights %02x, World %02x, Group %02x, Owner %02x\n",
      accs[0] & 0x07, accs[1], accs[2], accs[3]);
  }

  return;
}

/*
 * print the AFP date
 *
 */

void
dbg_print_date(date)
time_t date;
{
  char *ctime();
  time_t offset;
  struct timeval tp;
  struct timezone tzp;

  if (dbg != NULL) {
    if (date == 0x80000000) {
      fprintf(dbg, "<AFP Zero Time>\n");
      return;
    }
#ifdef SOLARIS
    tzset();
    offset = timezone;
#else  /* SOLARIS */
    gettimeofday(&tp, &tzp);
    offset = (time_t)(tzp.tz_minuteswest*60);
#endif /* SOLARIS */
    date = date - (((30*365+7)*(-24)*3600L)) + offset;
    fprintf(dbg, "%s", ctime(&date));
  }

  return;
}

/*
 * print a path name (pascal string)
 *
 */

void
dbg_print_path(path)
char *path;
{
  int i;

  if (dbg != NULL) {
    fprintf(dbg, "\tMFile: \"");
    for (i = 0; i < *path; i++)
      if (isprint(*(path+i+1)))
        fprintf(dbg, "%c", *(path+i+1));
      else
	fprintf(dbg, "<0x%02x>", *(path+i+1));
    fprintf(dbg, "\"\n");
  }
}
#endif /* DEBUG_AFP_CMD */
