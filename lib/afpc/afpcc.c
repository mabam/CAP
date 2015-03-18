/*
 * $Author: djh $ $Date: 1996/04/25 01:18:16 $
 * $Header: /mac/src/cap60/lib/afpc/RCS/afpcc.c,v 2.5 1996/04/25 01:18:16 djh Rel djh $
 * $Revision: 2.5 $
*/

/*
 * afpcc.c - easy interface to AFP client calls
 *
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987 	CCKim		Created.
 *
 */

/* PATCH: Rutgers1/*, djh@munnari.OZ.AU, 19/11/90 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include <netat/afpc.h>
#ifdef	SUNOS4_FASTDES
#include <des_crypt.h>
#endif	SUNOS4_FASTDES

eFPAddAPPL(srn, dtr, dirid, fcreator, appltag, path, cr)
int srn;
word dtr;
dword dirid;
char fcreator[];
dword appltag;
byte *path;
dword *cr;
{
  AddAPPLPkt aa;

  aa.aap_cmd = AFPAddAPPL;
  aa.aap_zero = 0;
  aa.aap_dtrefnum = dtr;
  aa.aap_dirid = dirid;
  bcopy(fcreator, aa.aap_fcreator, 4);
  aa.aap_apptag = appltag;
  aa.aap_ptype = 2;
  pstrcpy(aa.aap_path, path);	/* copy in path */
  return(FPAddAPPL(srn, &aa, cr));
}

eFPAddComment(srn, dtr, dirid, path, comment, cr)
int srn;
word dtr;
dword dirid;
byte *path;
byte *comment;
dword *cr;
{
  AddCommentPkt ac;
  int len;

  ac.adc_cmd = AFPAddComment;
  ac.adc_zero = 0;
  ac.adc_dtrefnum = dtr;
  ac.adc_dirid = dirid;
  ac.adc_ptype = 2;
  pstrcpy(ac.adc_path, path);
  if ((len = pstrlen(comment)) > 199)
    len = 199;
  ac.adc_clen = len;
  pstrcpy(ac.adc_comment, comment);
  return(FPAddComment(srn, &ac, cr));
}

eFPAddIcon(srn, dtr, fcreator, ftype, icontype, icontag, icon, iconlen, cr)
int srn;
word dtr;
byte fcreator[]; 
byte ftype[]; 
byte icontype;
dword icontag;
byte *icon;
int iconlen;
dword *cr;
{
  AddIconPkt adi;
  adi.adi_cmd = AFPAddIcon;
  adi.adi_zero = 0;
  adi.adi_dtref = dtr;
  bcopy(fcreator, adi.adi_fcreator, 4);
  bcopy(ftype, adi.adi_ftype, 4);
  adi.adi_icontype = icontype;
  adi.adi_icontag = icontag;
  adi.adi_iconsize = iconlen;

  return(FPAddIcon(srn, &adi, icon, iconlen, cr));
}

eFPByteRangeLock(srn, oforkrefnum, offset, length, flags, rangestart, cr)
int srn;
word oforkrefnum;
dword offset, length;
word flags;
dword *rangestart;
dword *cr;
{
  ByteRangeLockPkt brl;

  brl.brl_cmd = AFPByteRangeLock;
  brl.brl_flg = flags;
  brl.brl_refnum = oforkrefnum;
  brl.brl_offset = offset;
  brl.brl_length = length;
  return(FPByteRangeLock(srn, &brl, rangestart, cr));
}

eFPCloseDir(srn, volid, dirid, cr)
int srn;
word volid;
dword dirid;
dword *cr;
{
  CloseDirPkt cd;
  cd.cdr_cmd = AFPCloseDir;
  cd.cdr_zero = 0;
  cd.cdr_volid = volid;
  cd.cdr_dirid = dirid;
  return(FPCloseDir(srn, &cd, cr));
}

eFPCloseDT(srn, dtrefnum, cr)
int srn;
word dtrefnum;
dword *cr;
{
  CloseDTPkt cdt;

  cdt.cdt_cmd = AFPCloseDT;
  cdt.cdt_zero = 0;
  cdt.cdt_dtrefnum = dtrefnum;

  return(FPCloseDT(srn, &cdt, cr));
}

eFPCloseFork(SRefNum, OForkRefnum, cr)
int SRefNum;
word OForkRefnum;     
dword *cr;
{
  CloseForkPkt cfp;

  cfp.cfk_cmd = AFPCloseFork;
  cfp.cfk_zero = 0;
  cfp.cfk_refnum = OForkRefnum;

  return(FPCloseFork(SRefNum, &cfp, cr));
}

eFPCloseVol(srn, volid, cr)
int srn;
word volid;
dword *cr;
{
  CloseVolPkt cv;

  cv.cv_cmd = AFPCloseVol;
  cv.cv_zero = 0;
  cv.cv_volid = volid;
  return(FPCloseVol(srn, &cv, cr));
}

eFPCopyFile(srn, svolid, sdirid, spath, dvolid, ddirid, dpath, newname, cr)
int srn;
word svolid;
dword sdirid;
byte *spath;
word dvolid;
dword ddirid;
byte *dpath;
byte *newname;
dword *cr;
{
  CopyFilePkt cf;

  cf.cpf_cmd = AFPCopyFile;
  cf.cpf_zero = 0;
  cf.cpf_svolid = svolid;
  cf.cpf_sdirid = sdirid;
  cf.cpf_dvolid = dvolid;
  cf.cpf_ddirid = ddirid;
  cf.cpf_sptype = 2;
  pstrcpy(cf.cpf_spath, spath);
  cf.cpf_dptype = 2;
  pstrcpy(cf.cpf_dpath, dpath);
  cf.cpf_newtype = 2;
  pstrcpy(cf.cpf_newname, newname);
  return(FPCopyFile(srn, &cf, cr));
}

eFPCreateDir(srn, volid, dirid, path, newdirid, cr)
int srn;
word volid;
dword dirid;
byte *path;
dword *newdirid;
dword *cr;
{
  CreateDirPkt cd;

  cd.crd_cmd = AFPCreateDir;
  cd.crd_zero = 0;
  cd.crd_volid = volid;
  cd.crd_dirid = dirid;
  cd.crd_ptype = 2;
  pstrcpy(cd.crd_path, path);
  return(FPCreateDir(srn, &cd, newdirid, cr));
}

eFPCreateFile(srn, volid, dirid, createflag, pathname, cr)
int srn;
word volid;
dword dirid;
byte createflag;
byte *pathname;
dword *cr;
{
  CreateFilePkt cf;

  cf.crf_cmd = AFPCreateFile;
  cf.crf_flg = createflag ? CRF_HARD : 0;
  cf.crf_volid = volid;
  cf.crf_dirid = dirid;
  cf.crf_ptype = 0x2;		/* always long name */
  pstrcpy(cf.crf_path, pathname); /* file name */
  return(FPCreateFile(srn, &cf, cr));
}

eFPDelete(srn, volid, dirid, path, cr)
int srn;
word volid;
dword dirid;
byte *path;
dword *cr;
{
  DeletePkt dp;

  dp.del_cmd = AFPDelete;
  dp.del_zero = 0;
  dp.del_volid = volid;
  dp.del_dirid = dirid;
  dp.del_ptype = 0x2;		/* long name */
  pstrcpy(dp.del_path, path);
  return(FPDelete(srn, &dp, cr));
}

/* call with epars, returns filled in */
eFPEnumerate(srn, volid, dirid, path, idx, fbitmap, dbitmap,
	    epar, eparcnt, cnt, cr)
int srn;
word volid;
dword dirid;
byte *path;
int idx;
word fbitmap;
word dbitmap;
FileDirParm *epar;
int eparcnt;
int *cnt;
dword *cr;
{
  EnumeratePkt ep;
  byte buf[576*3-1];
  
  ep.enu_cmd = AFPEnumerate;
  ep.enu_zero = 0;
  ep.enu_volid = volid;
  ep.enu_dirid = dirid;		/* root */
  ep.enu_fbitmap = fbitmap;
  ep.enu_dbitmap = dbitmap;
  ep.enu_reqcnt = eparcnt;
  ep.enu_stidx = idx;
  ep.enu_maxreply = 576*3-1;
  ep.enu_ptype = 2;
  pstrcpy(ep.enu_path, path);
    
  return(FPEnumerate(srn, &ep, buf, 576*3-1, epar, eparcnt, cnt, cr));
}

eFPFlush(srn, volid, cr)
int srn;
word volid;
dword *cr;
{
  FlushPkt fv;

  fv.fls_cmd = AFPFlush;
  fv.fls_zero = 0;
  fv.fls_volid = volid;
  return(FPFlush(srn, &fv, cr));
}

eFPFlushFork(srn, oforkrefnum, cr)
int srn;
word oforkrefnum;
dword *cr;
{
  FlushForkPkt ff;

  ff.flf_cmd = AFPFlushFork;
  ff.flf_zero = 0;
  ff.flf_refnum = oforkrefnum;
  return(FPFlushFork(srn, &ff, cr));
}

eFPGetAPPL(srn, dtr, fcreator, idx, bitmap, gar, cr)
int srn;
word dtr;
byte fcreator[];
word idx;
word bitmap;
GetAPPLReplyPkt *gar;
dword *cr;
{
  GetAPPLPkt gap;

  gap.gap_cmd = AFPGetAPPL;
  gap.gap_zero = 0;
  gap.gap_dtrefnum = dtr;
  bcopy(fcreator, gap.gap_fcreator, 4);
  gap.gap_applidx = idx;
  gap.gap_bitmap = bitmap;

  return(FPGetAPPL(srn, &gap, gar, cr));
}

eFPGetComment(srn, dtr, dirid, path, gcr, cr)
int srn;
word dtr;
dword dirid;
byte *path;
GCRPPtr gcr;
dword *cr;
{
  GetCommentPkt gc;

  gc.gcm_cmd = AFPGetComment;
  gc.gcm_zero = 0;
  gc.gcm_dtrefnum = dtr;
  gc.gcm_dirid = dirid;
  gc.gcm_ptype = 2;		/* long name */
  pstrcpy(gc.gcm_path, path);
  return(FPGetComment(srn, &gc, gcr, cr));
}

eFPGetFileDirParms(srn, volid, dirid, fbitmap, dbitmap, path, epar, cr)
int srn;
word volid;
dword dirid;
word fbitmap;
word dbitmap;
byte *path;
FileDirParm *epar;
dword *cr;
{
  GetFileDirParmsPkt gfdp;

  gfdp.gdp_cmd = AFPGetFileDirParms;
  gfdp.gdp_zero = 0;
  gfdp.gdp_volid = volid;
  gfdp.gdp_dirid = dirid;	/* root */
  gfdp.gdp_fbitmap = fbitmap;
  gfdp.gdp_dbitmap = dbitmap;
  gfdp.gdp_ptype = 0x2;		/* long path type */
  pstrcpy(gfdp.gdp_path,path);
  return(FPGetFileDirParms(srn, &gfdp, epar, cr));
}

eFPGetForkParms(srn, fref, bitmap, epar, cr)
int srn;
word fref;
word bitmap;
FileDirParm *epar;
dword *cr;
{
  GetForkParmsPkt gfp;

  gfp.gfp_cmd = AFPGetForkParms;
  gfp.gfp_zero = 0;
  gfp.gfp_refnum = fref;
  gfp.gfp_bitmap = bitmap;
  return(FPGetForkParms(srn, &gfp, epar, cr));
}

eFPGetIcon(srn, dtr, fcreator, ftype, icontype, icon, iconlen, cr)
int srn;
word dtr;
byte fcreator[];
byte ftype[];
byte icontype;
byte *icon;
int iconlen;
dword *cr;
{
  GetIconPkt gic;

  gic.gic_cmd = AFPGetIcon;
  gic.gic_zero = 0;
  gic.gic_dtrefnum = dtr;
  bcopy(fcreator, gic.gic_fcreator, 4);
  bcopy(ftype,gic.gic_ftype, 4);
  gic.gic_itype = icontype;
  gic.gic_zero2 = 0;
  gic.gic_length = iconlen;
  return(FPGetIcon(srn, &gic, icon, iconlen, cr));
}

eFPGetIconInfo(srn, dtr, fcreator, iconidx, gicr, cr)
int srn;
word dtr;
byte fcreator[];
word iconidx;
GetIconInfoReplyPkt *gicr;
dword *cr;
{
  GetIconInfoPkt gii;

  gii.gii_cmd = AFPGetIconInfo;
  gii.gii_zero = 0;
  gii.gii_dtrefnum = dtr;
  bcopy(fcreator, gii.gii_fcreator, 4);
  gii.gii_iidx = iconidx;

  return(FPGetIconInfo(srn, &gii, gicr, cr));
}

/* maybe make this do a lookup first someday? */
eFPGetSrvrInfo(addr, sr)
AddrBlock *addr;
GetSrvrInfoReplyPkt *sr;
{
  return(FPGetSrvrInfo(addr, sr));
}

eFPGetSrvrParms(srn, sp, cr)
int srn;
GSPRPPtr *sp;
dword *cr;
{
  return(FPGetSrvrParms(srn, sp, cr));
}

eFPGetVolParms(srn, volid, vbitmap, ve, cr)
int srn;
word volid;
word vbitmap;
GetVolParmsReplyPkt *ve;
dword *cr;
{
  GetVolParmsPkt gvp;

  gvp.gvp_cmd = AFPGetVolParms;
  gvp.gvp_zero = 0;
  gvp.gvp_volid = volid;
  gvp.gvp_bitmap = vbitmap;

  return(FPGetVolParms(srn, &gvp, ve, cr));
}

/* Order matters */
static char *uam_which[3] = {
  "No User Authent",
  "Cleartxt passwrd",
  "Randnum exchange"
};

eFPLogin(srn, user, passwd, uam, cr)
int srn;
byte *user, *passwd;
int uam;
dword *cr;
{
  static byte uam_flags[3] = {
    0,				/* no special parms for ANON */
    UAMP_USER|UAMP_PASS|UAMP_ZERO, /* passwd + user... */
    UAMP_USER,			/* just pass user for RANDNUM initially */
    };
  LoginPkt lp;
  LoginReplyPkt lrp;
  LoginContPkt lcp;
  byte flgs;
  int comp;

  if (desinit(0) < 0)
    if (uam == UAM_RANDNUM)  {
      *cr = aeBadUAM;
      return(-1);
    }
  lp.log_cmd = AFPLogin;
  strcpy(lp.log_ver, "AFPVersion 1.1");
  strcpy(lp.log_uam, uam_which[uam]);
  flgs = lp.log_flag = uam_flags[uam];
  /* Have to do this - we are going to say this don't have to valid */
  /* if not needed for uam */
  if (flgs & UAMP_USER)
    strcpy(lp.log_user,user);
  if (flgs & UAMP_PASS)
    bcopy(passwd, lp.log_passwd, 8); /* copy in password */
  if (uam == UAM_RANDNUM)
    lrp.logr_flag = UAMP_RAND|UAMP_INUM; /* expect these back */
  comp = FPLogin(srn, &lp, &lrp, cr);
  if (comp < 0)
    return(comp);
  if ((((dword)*cr) != aeAuthContinue) && (uam != UAM_RANDNUM))
    return(comp);
  if (desinit(0) < 0) {
    /* here handle randnum exchange */
    *cr = aeBadUAM;
    return(-1);
  }
#ifdef SUNOS4_FASTDES
  des_setparity(passwd);
  bcopy(lrp.logr_randnum, lcp.lgc_encrypted, sizeof(lcp.lgc_encrypted));
  ecb_crypt(passwd,lcp.lgc_encrypted,64,DES_ENCRYPT|DES_HW);
#else  SUNOS4_FASTDES
  dessetkey(passwd);
  bcopy(lrp.logr_randnum, lcp.lgc_encrypted, sizeof(lcp.lgc_encrypted));
  endes(lcp.lgc_encrypted);
  desdone();			/* clean up (not used except by login) */
#endif SUNOS4_FASTDES
  lcp.lgc_cmd = AFPLoginCont;
  lcp.lgc_zero = 0;
  lcp.lgc_idno = lrp.logr_idnum;
  lcp.lgc_flags = UAMP_INUM|UAMP_ENCR;
  return(FPLoginCont(srn, &lcp, &lrp, cr));
}

eFPMapID(srn, fnc, ugid, mapr, cr)
int srn;
byte fnc;
dword ugid;
MapIDReplyPkt *mapr;
dword *cr;
{
  MapIDPkt mi;

  mi.mpi_cmd = AFPMapID;
  mi.mpi_fcn = fnc;
  mi.mpi_id = ugid;

  return(FPMapID(srn, &mi, mapr, cr));
}

eFPMapName(srn, fnc, name, id, cr)
int srn;
byte fnc;
byte *name;
dword *id;
dword *cr;
{
  MapNamePkt mnp;

  mnp.mpn_cmd = AFPMapName;
  mnp.mpn_fcn = fnc;
  pstrcpy(mnp.mpn_name, name);
  return(FPMapName(srn, &mnp, id, cr));
}

eFPMoveFile(srn, volid, sdirid, spath, ddirid, dpath, newname, cr)
int srn;
word volid;
dword sdirid;
byte *spath;
dword ddirid;
byte *dpath;
byte *newname;
dword *cr;
{
  MovePkt mf;

  mf.mov_cmd = AFPMove;
  mf.mov_zero = 0;
  mf.mov_volid = volid;
  mf.mov_sdirid = sdirid;
  mf.mov_ddirid = ddirid;
  mf.mov_sptype = 2;
  pstrcpy(mf.mov_spath, spath);
  mf.mov_dptype = 2;
  pstrcpy(mf.mov_dpath, dpath);
  mf.mov_newtype = 2;
  pstrcpy(mf.mov_newname, newname);
  return(FPMoveFile(srn, &mf, cr));
}

eFPOpenDir(srn, volid, dirid, path, retdirid, cr)
int srn;
word volid;
dword dirid;
byte *path;
dword *retdirid;
dword *cr;
{
  OpenDirPkt od;

  od.odr_cmd = AFPOpenDir;
  od.odr_zero = 0;
  od.odr_dirid = dirid;
  od.odr_volid = volid;
  od.odr_ptype = 2;
  pstrcpy(od.odr_path, path);
  return(FPOpenDir(srn, &od, retdirid, cr));
}

eFPOpenDT(srn, volid, dtrefnum, cr)
int srn;
word volid;
word *dtrefnum;
dword *cr;
{
  OpenDTPkt odt;

  odt.odt_cmd = AFPOpenDT;
  odt.odt_zero = 0;
  odt.odt_volid = volid;
  return(FPOpenDT(srn, &odt, dtrefnum, cr));
}

eFPOpenFork(srn, volid, dirid, mode, path, type, bitmap, epar, refnum, cr)
int srn;
word volid;
dword dirid;
word mode;
char *path;
byte type;
word bitmap;
FileDirParm *epar;
word *refnum;
dword *cr;
{
  OpenForkPkt of;

  of.ofk_cmd = AFPOpenFork;
  of.ofk_rdflg = type ? OFK_RSRC : 0;
  of.ofk_volid = volid;
  of.ofk_dirid = dirid;		/* root */
  of.ofk_bitmap = bitmap;		/* no info */
  of.ofk_mode = mode;		/* read/write */
  of.ofk_ptype = 0x2;		/* long name */
  pstrcpy(of.ofk_path, path); /* file name */  
  return(FPOpenFork(srn, &of, epar, refnum, cr));
}

eFPOpenVol(srn, bitmap, dovol, passwd, op, cr)
int srn;
word bitmap;
char *dovol;
byte *passwd;
GetVolParmsReplyPkt *op;
dword *cr;
{
  OpenVolPkt ov;

  ov.ovl_cmd = AFPOpenVol;
  ov.ovl_zero = 0;
  ov.ovl_bitmap = bitmap;
  strcpy(ov.ovl_name, dovol);
  if (passwd) 
    pstrcpy(ov.ovl_pass, passwd);
  else
    ov.ovl_pass[0] = 0;
  return(FPOpenVol(srn, &ov, op, cr));
}

eFPRead(srn, fd, buf, buflen, toget, offset, cr)
int srn;
word fd;
char *buf;
int buflen;
int toget;
dword offset;
dword *cr;
{
  ReadPkt rp;
  int err;
  int rlen;

  rp.rdf_cmd = AFPRead;
  rp.rdf_zero = 0;
  rp.rdf_refnum = fd;
  rp.rdf_offset = offset;
  rp.rdf_reqcnt = toget;
  rp.rdf_flag = 0;
  rp.rdf_nlchar = 0;
  err = FPRead(srn, &rp, buf, buflen, &rlen, cr);
  if (err < 0)
    return(err);
  if (*cr && ((dword)*cr) != aeEOFErr)
    return(0);
  return(rlen);
}

eFPRemoveAPPL(srn, dtr, dirid, fcreator, path, cr)
int srn;
word dtr;
dword dirid;
byte fcreator[];
byte *path;
dword *cr;
{
  RemoveAPPLPkt rma;

  rma.rma_cmd = AFPRmvAPPL;
  rma.rma_zero = 0;
  rma.rma_refnum = dtr;
  rma.rma_dirid = dirid;
  bcopy(fcreator, rma.rma_fcreator, 4);
  rma.rma_ptype = 0x2;
  pstrcpy(rma.rma_path, path);
  return(FPRemoveAPPL(srn, &rma, cr));
}

eFPRemoveComment(srn, dtr, dirid, path, cr)
int srn;
word dtr;
dword dirid;
byte *path;
dword *cr;
{
  RemoveCommentPkt rmc;

  rmc.rmc_cmd = AFPRmvComment;
  rmc.rmc_zero = 0;
  rmc.rmc_dtrefnum = dtr;
  rmc.rmc_dirid = dirid;
  rmc.rmc_ptype = 0x2;
  pstrcpy(rmc.rmc_path, path);
  return(FPRemoveComment(srn, &rmc, cr));
}

eFPRename(srn, volid, dirid, path, newname, cr)
int srn;
word volid;
dword dirid;
byte *path, *newname;
dword *cr;
{
  RenamePkt rn;
  rn.ren_cmd = AFPRename;
  rn.ren_zero = 0;
  rn.ren_volid = volid;
  rn.ren_dirid = dirid;
  rn.ren_ptype = 0x2;
  pstrcpy(rn.ren_path, path);
  rn.ren_ntype = 0x2;
  pstrcpy(rn.ren_npath, newname);
  return(FPRename(srn, &rn, cr));
}

eFPSetDirParms(srn, volid, dirid, bitmap, path, di, cr)
int srn;
word volid;
dword dirid;
word bitmap;
byte *path;
FileDirParm *di;
dword *cr;
{
  SetDirParmsPkt sdp;
  sdp.sdp_cmd = AFPSetDirParms;
  sdp.sdp_zero = 0;
  sdp.sdp_volid = volid;
  sdp.sdp_dirid = dirid;
  sdp.sdp_bitmap = bitmap;
  sdp.sdp_ptype = 0x2;
  pstrcpy(sdp.sdp_path, path);
  return(FPSetDirParms(srn, &sdp, di, cr));
}

eFPSetFileParms(srn, volid, dirid, bitmap, path, fi, cr)
int srn;
word volid;
dword dirid;
word bitmap;
byte *path;
FileDirParm *fi;
dword *cr;
{
  SetFileParmsPkt sfp;

  sfp.sfp_cmd = AFPSetFileParms;
  sfp.sfp_zero = 0;
  sfp.sfp_volid = volid;
  sfp.sfp_dirid = dirid;
  sfp.sfp_bitmap = bitmap;
  sfp.sfp_ptype = 0x2;
  pstrcpy(sfp.sfp_path, path);
  return(FPSetFileParms(srn, &sfp, fi, cr));
}

eFPSetFileDirParms(srn, volid, dirid, bitmap, path, fdi, cr)
int srn;
word volid;
dword dirid;
word bitmap;
byte *path;
FileDirParm *fdi;
dword *cr;
{
  SetFileDirParmsPkt scp;

  scp.scp_cmd = AFPSetFileParms;
  scp.scp_zero = 0;
  scp.scp_volid = volid;
  scp.scp_dirid = dirid;
  scp.scp_bitmap = bitmap;
  scp.scp_ptype = 0x2;
  pstrcpy(scp.scp_path, path);
  return(FPSetFileDirParms(srn, &scp, fdi, cr));
}

eFPSetForkParms(srn, fd, bitmap, fi, cr)
int srn;
word fd;
word bitmap;
FileParm *fi;
dword *cr;
{
  SetForkParmsPkt sfp;
  sfp.sfkp_cmd = AFPSetForkParms;
  sfp.sfkp_zero = 0;
  sfp.sfkp_refnum  = fd;
  sfp.sfkp_bitmap = bitmap;
  sfp.sfkp_rflen = fi->fp_rflen;
  sfp.sfkp_dflen = fi->fp_dflen;
  return(FPSetForkParms(srn, &sfp, cr));
}

eFPSetVolParms(srn, volid, backupdate, cr)
int srn;
word volid;
dword backupdate;
dword *cr;
{
  SetVolParmsPkt svp;

  svp.svp_cmd = AFPSetVolParms;
  svp.svp_zero = 0;
  svp.svp_volid = volid;
  svp.svp_bitmap = VP_BDATE;
  svp.svp_backdata = backupdate;
  return(FPSetVolParms(srn, &svp, cr));
}

eFPWrite(srn, fd, wbuf, wlen, towrite, actcnt, offset, cr)
int srn, fd;
char *wbuf;
int towrite;
dword *actcnt;
int *offset;
dword *cr;
{
  WritePkt wp;
  dword pactcnt, pwritten;
  int comp;

  *actcnt = 0;
  wp.wrt_cmd = AFPWrite;
  wp.wrt_flag = 0;
  wp.wrt_refnum = fd;
  wp.wrt_offset = *offset;
  wp.wrt_reqcnt = towrite;

  return(FPWrite(srn, wbuf, wlen, &wp, actcnt, offset, cr));
}
