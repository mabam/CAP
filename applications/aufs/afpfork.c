/*
 * $Author: djh $ $Date: 1995/06/26 05:49:55 $
 * $Header: /local/mulga/mac/src/cap60/applications/aufs/RCS/afpfork.c,v 2.8 1995/06/26 05:49:55 djh Rel djh $
 * $Revision: 2.8 $
*/

/*
 * afpfork.c - Appletalk Filing Protocol Fork Level Routines
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

/*
 * Non OS dependant support routines for:
 *
 *  FPGetForkParms()
 *  FPSetForkParms()
 *  FPOpenFork()
 *  FPCloseFork()
 *  FPRead()
 *  FPWrite() 
 *  FPFlushFork()
 *  FPByteRangeLock()
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
#include "afps.h"
#include "afpntoh.h"
#include "afposncs.h"
 
#ifdef DEBUG_AFP_CMD
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

typedef struct {
  int ofn_fd;			/* file descriptor */
  sdword ofn_pos;		/* file position */
  IDirP ofn_ipdir;
  int ofn_ivol;
  int ofn_efn;			/* external file number */
  char ofn_fnam[MAXUFLEN];	/* file name */
  int ofn_mode;
  int ofn_type;			/* data or rsrc */
  int ofn_flgs;			/* file flags */
#define OF_EOF  0x020		/*  eof on file */
#define OF_NOTOPEN   0x02	/* to support non-existent files */
#define OF_INUSE 0x04		/* OFN is in use */
  int ofn_trans_table;		/* Translation table if any */
} OFN;

#define MaxOFN 30

private OFN OFNTBL[MaxOFN];

private int oinit = FALSE;

private
iniofn()
{
  int i;

  for (i=0; i < MaxOFN; i++) {
    OFNTBL[i].ofn_flgs = 0;
    OFNTBL[i].ofn_pos = -1;
    OFNTBL[i].ofn_trans_table = -1; /* no index */
    OFNTBL[i].ofn_efn = i+1;
  }
  oinit = TRUE;
}

private word
ItoEOFN(o)
OFN *o;
{
  return(o->ofn_efn);
}

private OFN *
EtoIOFN(i)
int i;
{
  i = i-1;
  if (i > MaxOFN || i < 0) {
    printf("EtoIOFN: bad file number\n");
    return(NULL);
  }
  if (!oinit || (OFNTBL[i].ofn_flgs & OF_INUSE) == 0)	/* ofn assigned? */
    return(NULL);		/* no, mark as bad */
  return(&OFNTBL[i]);
}

private void
relofn(ofn)
OFN *ofn;
{
  ofn->ofn_flgs = 0;
  ofn->ofn_pos = -1;
  ofn->ofn_trans_table = -1;	/* delete */
}

private OFN *
asnofn()
{
  int i;
  if (!oinit)
    iniofn();
  for (i=0; i < MaxOFN; i++)
    if ((OFNTBL[i].ofn_flgs & OF_INUSE) == 0) {
      OFNTBL[i].ofn_trans_table = -1; /* paranoia: no index */
      OFNTBL[i].ofn_flgs = OF_INUSE;
      return(&OFNTBL[i]);
    }
  return(0);
}

/*
 * This is a horrible hack that is necessary to ensure that renames
 * get reflected back.  Really, really, points to the need for internal
 * directory of names....  Can't use fstat because file name is required
 * to return some of the information (from finderinfo).  Sigh...
 *
*/
OFNFIXUP(oipdir, ofile, nipdir, nfile)
IDirP oipdir;
char *ofile;
IDirP nipdir;
char *nfile;
{
  OFN *ofn;
  if (!oinit)
    return(-1);

  for (ofn = OFNTBL; ofn < OFNTBL+MaxOFN; ofn++)
    if (ofn->ofn_flgs & OF_INUSE) {
      if (ofn->ofn_ipdir == oipdir && strcmp(ofn->ofn_fnam, ofile) == 0) {
	ofn->ofn_ipdir = nipdir; /* remember new directory */
	strcpy(ofn->ofn_fnam, nfile); /* remember new file */
      }
    }
  return(0);
}

/*
 * determine if file is already open
 *
 */

int
is_open_file(ipdir, file)
IDirP ipdir;
char *file;
{
  OFN *ofn;

  if (!oinit)
    return(0);

  for (ofn = OFNTBL; ofn < OFNTBL+MaxOFN; ofn++)
    if (ofn->ofn_flgs & OF_INUSE)
      if (ofn->ofn_ipdir == ipdir
       && strcmp(ofn->ofn_fnam, file) == 0)
	return(1);

  return(0);
}

/*
 * OSErr FPGetForkParms(...)
 *
 * This call is used to retrieve parameters for a file associated with
 * a particular open fork.
 *
 * Inputs:
 *   refnum	Open fork refnum
 *   bitmap	Bitmap describing which parameters are to be retrieved.
 *		This field is the same as the FPGetFileDirParms call.
 *
 * Outputs:
 *   Same as FPGetFileDirParms.
 *
 * Errors:
 *   ParamErr, BitMapErr, AccessDenied.
 *
 * The fork must be open for read.
 *
 */

OSErr 
FPGetForkParms(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  GetForkParmsPkt gfp;
  FileDirParm fdp;
  int err;
  OFN *ofn;

  ntohPackX(PsGetForkParms,p,l,(byte *) &gfp);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    fprintf(dbg, "\tRefNum: %d\n", gfp.gfp_refnum);
    fprintf(dbg, "\tBitMap: %04x\t", gfp.gfp_bitmap);
    dbg_print_bmap(gfp.gfp_bitmap, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((ofn = EtoIOFN(gfp.gfp_refnum)) == NULL)
    return(aeParamErr);
  
  fdp.fdp_pdirid = ItoEdirid(ofn->ofn_ipdir,ofn->ofn_ivol);
  fdp.fdp_dbitmap = 0;
  fdp.fdp_fbitmap = gfp.gfp_bitmap;
  fdp.fdp_zero = 0;
  err = OSFileDirInfo(ofn->ofn_ipdir,NILDIR,ofn->ofn_fnam,&fdp,ofn->ofn_ivol);
  if (err != noErr)
    return(err);
  PackWord(gfp.gfp_bitmap,r);
  *rl = 2;
  *rl += htonPackX(FilePackR,(byte *) &fdp,r+(*rl));

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_parm();
    fprintf(dbg, "    Return Parameters:\n");
    dbg_print_parm(fdp.fdp_fbitmap, r+2, (*rl)-2, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * OSErr FPSetForkParms(...)
 *
 * Inputs:
 *
 *   refnum	Open fork refnum.
 *   bitmap	Bitmap describing which params are to be set.
 *
 * Output:
 *   Function Result.
 *
 * Errors:
 *   ParamErr, BitMapErr, DiskFull, LockErr, AccessDenied.
 *
 * The bitmap is the same as FPSetFileDirParms, however in AFP 1.1
 * only the fork length may be set.
 *
 * The fork must be opened for write.
 *
 */ 

/*ARGSUSED*/
OSErr 
FPSetForkParms(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  SetForkParmsPkt sfkp;
  OFN *ofn;
  sdword len;

  ntohPackX(PsSetForkParms,p,l,(byte *) &sfkp);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    fprintf(dbg, "\tRefNum: %d\n", sfkp.sfkp_refnum);
    fprintf(dbg, "\tBitMap: %04x\t", sfkp.sfkp_bitmap);
    dbg_print_bmap(sfkp.sfkp_bitmap, 0);
    fprintf(dbg, "\tForkLn: %d\n",
      (sfkp.sfkp_bitmap & FP_DFLEN) ? sfkp.sfkp_dflen  : sfkp.sfkp_rflen);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((ofn = EtoIOFN(sfkp.sfkp_refnum)) == NULL)
    return(aeParamErr);
  
  if (DBFRK)
    printf("FPSetForkParms: ofn=%d\n",sfkp.sfkp_refnum);

  if ((sfkp.sfkp_bitmap & ~(FP_DFLEN|FP_RFLEN)) != 0 || sfkp.sfkp_bitmap==0) {
    printf("FPSetForkParms: bad bitmap %x\n",sfkp.sfkp_bitmap);
    return(aeBitMapErr);
  }

  len = (sfkp.sfkp_bitmap & FP_DFLEN) ? sfkp.sfkp_dflen  : sfkp.sfkp_rflen;
  /* can only set fork length for now */
  return((ofn->ofn_flgs&OF_NOTOPEN) ? noErr : OSSetForklen(ofn->ofn_fd, len));
}


OSErr 
FPOpenFork(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  OpenForkPkt ofk;
  OpenForkReplyPkt ofr;
  FileDirParm fdp;
  int ivol,err,err1,flg,fhdl,len, ttidx;
  IDirP idir,ipdir;
  char file[MAXUFLEN];
  byte finderinfo[FINFOLEN];
  OFN *ofn;
  extern PackEntry ProtoOFkRP[];

  ntohPackX(PsOpenFork,p,l,(byte *) &ofk);

  err = EtoIfile(file,&idir,&ipdir,&ivol,ofk.ofk_dirid,
		 ofk.ofk_volid,ofk.ofk_ptype,ofk.ofk_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_mode();
    void dbg_print_bmap();
    void dbg_print_path();
    fprintf(dbg, "\tOFork: %s\n", (ofk.ofk_rdflg) ? "Resource" : "Data");
    fprintf(dbg, "\tVolID: %04x\n", ofk.ofk_volid);
    fprintf(dbg, "\tDirID: %08x\n", ofk.ofk_dirid);
    fprintf(dbg, "\tBtMap: %04x\t", ofk.ofk_bitmap);
    dbg_print_bmap(ofk.ofk_bitmap, 0);
    fprintf(dbg, "\tAMode: %04x\t", ofk.ofk_mode);
    dbg_print_mode(ofk.ofk_mode);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", ofk.ofk_ptype,
      (ofk.ofk_ptype == 1) ? "Short" : "Long");
    dbg_print_path(ofk.ofk_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  if (DBFRK) 
    printf("OpenFork: rdflg=%d, mode=%d, path=%s file=%s\n",
	   ofk.ofk_rdflg,ofk.ofk_mode,pathstr(ipdir),file);

  flg = (ofk.ofk_rdflg == 0) ? F_DATA : F_RSRC;

  err = OSOpenFork(ipdir,file,ofk.ofk_mode,flg,&fhdl);

  if (err != noErr
   && err != aeDenyConflict
   && !(flg == F_RSRC && err == aeObjectNotFound))
    return(err);
  
  /*
   * now, if data fork exists and resource
   * open failed, allow go ahead
   *
   */
  if (err == aeObjectNotFound) {
#ifdef notdef
    /* why did I do this? */
    if (ofk.ofk_mode & OFK_MWR)	/* don't allow write access */
      return(aeAccessDenied);
#endif
    if (OSFileExists(ipdir, file, F_DATA) != noErr)
      return(aeObjectNotFound);
    else {
      if (DBFRK)
	printf("Allowing fake access to resource fork\n");
    }
  }

  if ((ofn = asnofn()) == 0) {
    if (err == noErr)
      OSClose(fhdl);
    return(aeTooManyFilesOpen);
  }
  ofn->ofn_fd = fhdl;
  ofn->ofn_mode = ofk.ofk_mode;
  ofn->ofn_type = flg;
  ofn->ofn_ipdir = ipdir;
  ofn->ofn_ivol = ivol;

  if (err == aeObjectNotFound
   || err == aeDenyConflict)
    ofn->ofn_flgs |= OF_NOTOPEN;

  if (flg == F_DATA) {
    /* Never translate resource fork */
    OSGetFNDR(ipdir, file, finderinfo); /* check file type */
#ifdef SHORT_NAMES
    if (DBFRK)
      printf("afpfork, ofk.ofk_path %d\n",ofk.ofk_ptype);
    if ((ttidx = ncs_istrans(finderinfo, file, ofk.ofk_ptype)) >= 0) {
#else SHORT_NAMES
    if ((ttidx = ncs_istrans(finderinfo, file)) >= 0) {
#endif SHORT_NAMES
      ofn->ofn_trans_table = ttidx;
      if (DBFRK)
	printf("Open fork: translation table: %s\n", ncs_tt_name(ttidx));
    }
  }

  strcpy(ofn->ofn_fnam,file);	/* remember file name (UGH!!!!) */

  fdp.fdp_fbitmap = ofk.ofk_bitmap;
  fdp.fdp_dbitmap = 0;
  if ((err1 = OSFileDirInfo(ipdir,NILDIR,file,&fdp,ivol)) != noErr) {
    if (err != noErr)
      OSClose(fhdl);
    relofn(ofn);
    return(err1);
  }

  ofr.ofkr_bitmap = ofk.ofk_bitmap;
  ofr.ofkr_refnum = (err == aeDenyConflict) ? 0x0000 : ItoEOFN(ofn);
  len = htonPackX(ProtoOFkRP, &ofr, r);	/* pack away */
  *rl = len + htonPackX(FilePackR,(byte *) &fdp,r+len);

  if (err == aeDenyConflict) {
    relofn(ofn);
    return(err);
  }

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_parm();
    void dbg_print_bmap();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tFDescr: %d\n", ofn->ofn_fd);
    fprintf(dbg, "\tBitMap: %04x\t", ofr.ofkr_bitmap);
    dbg_print_bmap(ofk.ofk_bitmap, 0);
    fprintf(dbg, "\tRefNum: %d\n", ofr.ofkr_refnum);
    dbg_print_parm(ofr.ofkr_bitmap, r+len, *rl, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*ARGSUSED*/
OSErr 
FPCloseFork(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  CloseForkPkt cfk;
  word attr;
  OFN *ofn;
  int err;

  ntohPackX(PsCloseFork,p,l,(byte *) &cfk);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tRefNum: %d\n", cfk.cfk_refnum);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBFRK)
    printf("CloseFork: refnum=%d\n",cfk.cfk_refnum);
  if ((ofn = EtoIOFN(cfk.cfk_refnum)) == NULL)
    return(aeParamErr);

  if ((ofn->ofn_mode & OFK_MWR) != 0)	/* if opened for write */
    VolModified(ofn->ofn_ivol);		/*  then set volume modification dt */
  
  err = (ofn->ofn_flgs & OF_NOTOPEN) ? noErr : OSClose(ofn->ofn_fd);

  if (err == noErr) {
    OSGetAttr(ofn->ofn_ipdir, ofn->ofn_fnam, &attr);
    attr &= ~((ofn->ofn_type == F_DATA) ? FPA_DAO : FPA_RAO);
    OSSetAttr(ofn->ofn_ipdir, ofn->ofn_fnam, attr);
  }

  relofn(ofn);
  return(err);
}


/*
 * 
 * FPRead(...)
 *
 * The fork is read starting offset bytes from the beginning of the fork
 * and terminating at the first newline-char encountered.
 *
 */
 
OSErr 
FPRead(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  ReadPkt rp;
  OFN *ofn;
  extern int sqs;		/* get max send quantum size */

  ntohPackX(PsRead,p,l,(byte *) &rp);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tRefNum: %d\n", rp.rdf_refnum);
    fprintf(dbg, "\tOffSet: %d\n", rp.rdf_offset);
    fprintf(dbg, "\tReqCnt: %d\n", rp.rdf_reqcnt);
    fprintf(dbg, "\tNLMask: %d\n", rp.rdf_flag);
    fprintf(dbg, "\tNLChar: 0x%02x\n", rp.rdf_nlchar);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBFRK)
    printf("Read: ofn=%d, offset=%d, reqcnt=%d, flg=%d, nlc=%02x\n",
	   rp.rdf_refnum,rp.rdf_offset,rp.rdf_reqcnt,
	   rp.rdf_flag,rp.rdf_nlchar);

  if ((ofn = EtoIOFN(rp.rdf_refnum)) == NULL)
    return(aeParamErr);

  if (ofn->ofn_flgs & OF_NOTOPEN) {
    *rl = 0;
    return(aeEOFErr);
  }

  return(OSRead(ofn->ofn_fd,rp.rdf_offset,
		min(rp.rdf_reqcnt,sqs),
		rp.rdf_flag,rp.rdf_nlchar,r,rl,&ofn->ofn_pos,
		ofn->ofn_trans_table));
}

OSErr 
FPWrite(p,l,r,rl,cno,reqref)
byte *p,*r;
int l,*rl;
int cno;
ReqRefNumType reqref;
{
  WritePkt wrt;
  int rcvlen;
  int comp,err;
  OFN *ofn;
  extern int n_rrpkts;

  ntohPackX(PsWrite,p,l,(byte *) &wrt);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tS/EFlg: %02x\n", wrt.wrt_flag);
    fprintf(dbg, "\tRefNum: %d\n", wrt.wrt_refnum);
    fprintf(dbg, "\tOffset: %d\n", wrt.wrt_offset);
    fprintf(dbg, "\tReqCnt: %d\n", wrt.wrt_reqcnt);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((ofn = EtoIOFN(wrt.wrt_refnum)) == NULL)
    return(aeParamErr);
  if (DBFRK)
    printf("FPWrite: ofn=%d, flg=%d, offs=%d, req=%d\n",
	   wrt.wrt_refnum,wrt.wrt_flag,wrt.wrt_offset,wrt.wrt_reqcnt);
  if (ofn->ofn_flgs & OF_NOTOPEN) {
    if (wrt.wrt_reqcnt != 0)	/* allow zero length requests */
      return(aeAccessDenied);
  }

  err = dsiWrtContinue(cno,reqref,r,n_rrpkts*atpMaxData,&rcvlen,-1,&comp);
  if (err != noErr)
    return(err);
  do { abSleep(4,TRUE); } while (comp > 0);    
  if (comp < 0)
    return(comp);
  err = OSWrite(ofn->ofn_fd,r,(sdword) rcvlen,
		wrt.wrt_offset,wrt.wrt_flag,&ofn->ofn_pos,
		ofn->ofn_trans_table);
  if (err != noErr)		
    return(err);
  PackDWord(ofn->ofn_pos,r);
  *rl = sizeof(ofn->ofn_pos);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tWrittn: %d\n", ofn->ofn_pos);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
} 

/*ARGSUSED*/
OSErr 
FPFlushFork(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  OFN *ofn;
  FlushForkPkt flf;

  ntohPackX(PsFlush, p, l, (byte *)&flf);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL ) {
    fprintf(dbg, "\tRefNum: %d\n", flf.flf_refnum);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((ofn = EtoIOFN(flf.flf_refnum)) == NULL)
    return(aeParamErr);
  if (DBFRK)
    printf("FLFlush: ofn=%d\n", flf.flf_refnum);
  return((ofn->ofn_flgs & OF_NOTOPEN) ? noErr : OSFlushFork(ofn->ofn_fd));
}

/*ARGSUSED*/
OSErr 
FPByteRangeLock(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  OFN *ofn;
  ByteRangeLockPkt brl;
  OSErr err;

  ntohPackX(PsByteRangeLock, p, l, (byte *)&brl);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tULFlag: %s\n", (brl.brl_flg & 0x01) ? "UNLOCK" : "LOCK");
    fprintf(dbg, "\tSEFlag: %s\n", (brl.brl_flg & 0x80) ? "END" : "START");
    fprintf(dbg, "\tRefNum: %d\n", brl.brl_refnum);
    fprintf(dbg, "\tOffset: %d\n", brl.brl_offset);
    fprintf(dbg, "\tLength: %d\n", brl.brl_length);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((ofn = EtoIOFN(brl.brl_refnum)) == NULL)
    return(aeParamErr);
  if (DBFRK)
    printf("Byte Range Lock: ofn=%d (file %s)\n",ofn->ofn_fd,ofn->ofn_fnam);
  if (ofn->ofn_flgs & OF_NOTOPEN)
    ofn->ofn_pos = brl.brl_offset;
  else {
    err = OSByteRangeLock(ofn->ofn_fd, brl.brl_offset, brl.brl_length,
			  brl.brl_flg, &ofn->ofn_pos);
    if (err != noErr)
      return(err);
  }
  PackDWord(ofn->ofn_pos,r);
  *rl = sizeof(ofn->ofn_pos);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tRStart: %d\n", ofn->ofn_pos);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

#ifdef DEBUG_AFP_CMD
void
dbg_print_mode(mode)
u_short mode;
{
  if (dbg != NULL) {
    fprintf(dbg, "(");
    if (mode & 0x0001)
      fprintf(dbg, "Read ");
    if (mode & 0x0002)
      fprintf(dbg, "Write ");
    if (mode & 0x0004)
      fprintf(dbg, "DenyRead ");
    if (mode & 0x0008)
      fprintf(dbg, "DenyWrite ");
    fprintf(dbg, ")\n");
  }
}
#endif /* DEBUG_AFP_CMD */
