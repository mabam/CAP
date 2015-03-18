/*
 * $Author: djh $ $Date: 1996/04/25 01:12:53 $
 * $Header: /mac/src/cap60/lib/afpc/RCS/afpc.c,v 2.6 1996/04/25 01:12:53 djh Rel djh $
 * $Revision: 2.6 $
 *
 */

/*
 * afpc.c - AFP client calls
 *
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987  CCKim		Created.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>		/* so ntohl, etc work on non-vax */
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include <netat/afpc.h>

sendspcmd(srn, sbuf, slen, cr)
byte *sbuf;
int slen;
dword *cr;
{
  int rlen, comp;

  SPCommand(srn, sbuf, slen, NULL, 0, cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  /* should we check rlen? */
  return(comp);
}


FPAddAPPL(srn, aa, cr)
int srn;
AddAPPLPkt *aa;
{
  char lbuf[sizeof(AddAPPLPkt)+1];
  extern PackEntry ProtoAAP[];
  int len;

  len = htonPackX(ProtoAAP, aa, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPAddComment(srn, ac, cr)
int srn;
AddCommentPkt *ac;
dword *cr;
{
  int len;
  char lbuf[sizeof(AddCommentPkt)+1];
  extern PackEntry ProtoACP[];

  len = htonPackX(ProtoACP, ac, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPAddIcon(srn, adi, icon, il, cr)
int srn;
AddIconPkt *adi;
byte *icon;
int il;
dword *cr;
{
  int len, rlen, wlen, comp;
  char lbuf[sizeof(AddIconPkt)+1];
  extern PackEntry ProtoAIP[];

  len = htonPackX(ProtoAIP, adi, lbuf);

  SPWrite(srn,lbuf,len, icon, il, NULL, 0, cr, &wlen, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)	/* keep trying if so */
    return(comp);
  return(noErr);
}


FPByteRangeLock(srn, brl, rangestart, cr)
int srn;
ByteRangeLockPkt *brl;
dword *rangestart;
dword *cr;
{
  dword reply;
  int rlen, len, comp;
  char lbuf[sizeof(ByteRangeLockPkt)+1];
  extern PackEntry ProtoBRL[];

  len = htonPackX(ProtoBRL, brl, lbuf);
  SPCommand(srn, lbuf, len, &reply, sizeof(reply), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  *rangestart = ntohl(reply);
  return(comp);
}


FPCloseDir(srn, cd, cr)
int srn;
CloseDirPkt *cd;
dword *cr;
{
  char lbuf[sizeof(CloseDirPkt)+1];
  extern PackEntry ProtoCDP[];
  int len;

  len = htonPackX(ProtoCDP, cd, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPCloseDT(srn, cdt, cr)
int srn;
CloseDTPkt *cdt;
dword *cr;
{
  extern PackEntry ProtoCDT[];
  int len;
  char lbuf[sizeof(CloseDTPkt)+1];

  len = htonPackX(ProtoCDT, cdt, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPCloseFork(SRefNum, cfp, cr)
int SRefNum;
CloseForkPkt *cfp;
dword *cr;
{
  char lbuf[sizeof(CloseForkPkt)+1];
  extern PackEntry ProtoCFkP[];
  int len;
  len = htonPackX(ProtoCFkP, cfp, lbuf);

  return(sendspcmd(SRefNum, lbuf, len, cr));
}


FPCloseVol(srn, cv, cr)
int srn;
CloseVolPkt *cv;
dword *cr;
{
  int len;
  extern PackEntry ProtoCVP[];
  char lbuf[sizeof(CloseVolPkt)+1];

  len = htonPackX(ProtoCVP, cv, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPCopyFile(srn, cf, cr)
int srn;
CopyFilePkt *cf;
dword *cr;
{
  char lbuf[sizeof(CopyFilePkt)];
  extern PackEntry ProtoCpFP[];
  int len;

  len = htonPackX(ProtoCpFP, cf, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPCreateDir(srn, cd, newdirid, cr)
int srn;
CreateDirPkt *cd;
dword *newdirid;
dword *cr;
{
  dword reply;
  int rlen, len, comp;
  char lbuf[sizeof(CreateDirPkt)+1];
  extern PackEntry ProtoCRDP[];

  len = htonPackX(ProtoCRDP, cd, lbuf);
  SPCommand(srn, lbuf, len, &reply, sizeof(reply), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  *newdirid = ntohl(reply);
  return(comp);
}


FPCreateFile(srn, cf, cr)
int srn;
CreateFilePkt *cf;
dword *cr;
{
  char lbuf[sizeof(CreateFilePkt)+1];
  int len;
  extern PackEntry ProtoCFP[];

  len = htonPackX(ProtoCFP, cf, lbuf);

  return(sendspcmd(srn, lbuf, len, cr));
}


FPDelete(srn, dp, cr)
int srn;
DeletePkt *dp;
dword *cr;
{
  char lbuf[sizeof(DeletePkt)+1];
  extern PackEntry ProtoDFP[];
  int len;

  len = htonPackX(ProtoDFP, dp, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPEnumerate(srn, ep, tbuf, tbufsiz, epar, eparcnt, cnt, cr)
int srn;
EnumeratePkt *ep;
byte *tbuf;
int tbufsiz;
FileDirParm *epar;
int eparcnt;
int *cnt;
dword *cr;
{
  char lbuf[sizeof(EnumeratePkt)];
  int rlen, comp, len, i;
  word bitmap;
  extern PackEntry ProtoEP[], ProtoEPR[], ProtoFileAttr[], ProtoDirAttr[];
  unsigned char *p;
  EnumerateReplyPkt epr;

  len = htonPackX(ProtoEP, ep, lbuf);  

  SPCommand(srn, lbuf, len, tbuf, tbufsiz, cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  len = ntohPackX(ProtoEPR, tbuf, rlen, &epr);
  *cnt = epr.enur_actcnt;
  for (i = 0, p = tbuf+len; i<(int)epr.enur_actcnt ; i++) {
    len = (int)p[0];
    epar->fdp_flg = p[1];
    bitmap = FDP_ISDIR(p[1]) ? epr.enur_dbitmap : epr.enur_fbitmap;
    ntohPackXbitmap(FDP_ISDIR(p[1]) ? ProtoDirAttr : ProtoFileAttr,
		    &p[2], len-2, epar, bitmap);
    epar++;
    p+=len;
  }
  return(noErr);
}


FPFlush(srn, fv, cr)
int srn;
FlushPkt *fv;
dword *cr;
{
  char lbuf[sizeof(FlushPkt)+1];
  int len;
  extern PackEntry ProtoCFP[];

  len = htonPackX(ProtoCFP, fv, lbuf);

  return(sendspcmd(srn, lbuf, len, cr));
}


FPFlushFork(srn, ff, cr)
int srn;
FlushForkPkt *ff;
dword *cr;
{
  char lbuf[sizeof(FlushForkPkt)+1];
  extern PackEntry ProtoFFP[];
  int len;

  len = htonPackX(ProtoFFP, ff, lbuf);

  return(sendspcmd(srn, lbuf, len, cr));
}


FPGetAPPL(srn, gap, gar, cr)
int srn;
GetAPPLPkt *gap;
GetAPPLReplyPkt *gar;
dword *cr;
{
  char lbuf[sizeof(GetAPPLPkt)+1];
  char buf[sizeof(GetAPPLReplyPkt)+1];
  int rlen, comp, len;
  extern PackEntry ProtoGAP[], ProtoGAPR[], ProtoFileAttr[];

  len = htonPackX(ProtoGAP, gap, lbuf);
  SPCommand(srn,lbuf,len,buf,sizeof(GetAPPLReplyPkt),cr,&rlen,-1,&comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  len = ntohPackX(ProtoGAPR, buf, rlen, gar);
  rlen -= len;
  ntohPackXbitmap(ProtoFileAttr, buf+len, rlen, &gar->fdp, gar->gapr_bitmap);
  return(noErr);
}


FPGetComment(srn, gc, gcr, cr)
int srn;
GetCommentPkt *gc;
GCRPPtr gcr;
dword *cr;
{
  char lbuf[sizeof(GetCommentPkt)+1];
  char buf[sizeof(GetCommentReplyPkt)+1];
  int rlen, comp, len;
  extern PackEntry ProtoGCP[];

  len = htonPackX(ProtoGCP, gc, lbuf);
  SPCommand(srn,lbuf,len,buf,sizeof(GetCommentReplyPkt),cr,&rlen,-1,&comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  len = (int)buf[0];		/* get return string len */
  if (len > 199)
    len = 199;			/* truncate if too long */
  bcopy(buf+1, gcr->gcmr_ctxt, len);
  gcr->gcmr_clen = len;
  return(noErr);
}


FPGetFileDirParms(srn, gfdp, epar, cr)
int srn;
GetFileDirParmsPkt *gfdp;
FileDirParm *epar;
dword *cr;
{
  byte lbuf[sizeof(GetFileDirParmsPkt)+1];
  byte buf[sizeof(FileDirParm)+10];
  int rlen, comp, len;
  extern PackEntry ProtoGFDPP[], ProtoFileAttr[], ProtoDirAttr[];
  word rfbitmap, rdbitmap;
  byte *p, isfiledir;

  len = htonPackX(ProtoGFDPP, gfdp, lbuf);

  SPCommand(srn,lbuf, len, buf, sizeof(FileDirParm)+10, cr, &rlen,-1,&comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0) 
    return(comp);
  p = buf;
  UnpackWord(&p, &rfbitmap);
  UnpackWord(&p, &rdbitmap);
  epar->fdp_flg = isfiledir = *p++;
  p++;				/* skip past zero entry */
  if (FDP_ISDIR(isfiledir))
    ntohPackXbitmap(ProtoDirAttr, p, rlen, epar, rdbitmap); /* directory */
  else
    ntohPackXbitmap(ProtoFileAttr, p, rlen, epar, rfbitmap); /* file */
  return(noErr);
}


FPGetForkParms(srn, gfp, epar, cr)
int srn;
GetForkParmsPkt *gfp;
FileDirParm *epar;
dword *cr;
{
  byte lbuf[sizeof(GetForkParmsPkt)+1];
  byte buf[sizeof(FileDirParm)+10];
  int rlen, comp, len;
  extern PackEntry ProtoGFkPP[],ProtoFileAttr[], ProtoDirAttr[];
  byte *p;
  word fbitmap;

  len = htonPackX(ProtoGFkPP, gfp, lbuf);

  SPCommand(srn,lbuf, len, buf, sizeof(buf), cr, &rlen,-1,&comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != noErr) 
    return(comp);
  p = buf;			/* copy pointer */
  UnpackWord(&p, &fbitmap);		/* unpack bitmap */
  ntohPackXbitmap(ProtoFileAttr, p, rlen, epar, fbitmap);
  return(noErr);
}


FPGetIcon(srn, gi, icon, iconlen, cr)
int srn;
GetIconPkt *gi;
byte *icon;
int iconlen;
dword *cr;
{
  byte lbuf[sizeof(GetIconPkt)+1];
  extern PackEntry ProtoGI[];
  int comp, rlen, len;

  len = htonPackX(ProtoGI, gi, lbuf);
  
  SPCommand(srn, lbuf, len, icon, iconlen, cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != noErr) 
    return(comp);
  return(noErr);
}


FPGetIconInfo(srn, gii, gicr, cr)
int srn;
GetIconInfoPkt *gii;
GetIconInfoReplyPkt *gicr;
dword *cr;
{
  byte buf[sizeof(GetIconInfoPkt)+1], lbuf[sizeof(GetIconInfoReplyPkt)+1];
  int len, rlen, comp;
  extern PackEntry ProtoGII[], ProtoGIIR[];

  len = htonPackX(ProtoGII, gii, lbuf);
  SPCommand(srn, lbuf, len, buf, sizeof(buf), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != noErr) 
    return(comp);
  ntohPackX(ProtoGIIR, buf, rlen, gicr);
  return(noErr);
}


FPGetSrvrMsg(srn, smp, smrp, cr)
int srn;
SrvrMsgPkt *smp;
SrvrMsgReplyPkt *smrp;
dword *cr;
{
  byte buf[sizeof(SrvrMsgPkt)+1], lbuf[sizeof(SrvrMsgReplyPkt)+1];
  int len, rlen, comp;
  extern PackEntry ProtoMsgP[], ProtoMsgRP[];

  len = htonPackX(ProtoMsgP, smp, lbuf);
  SPCommand(srn, lbuf, len, buf, sizeof(buf), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != noErr) 
    return(comp);
  ntohPackX(ProtoMsgRP, buf, rlen, smrp);
  return(noErr);
}


FPGetSrvrInfo(addr, sr)
AddrBlock *addr;
GetSrvrInfoReplyPkt *sr;
{
  byte buf[atpMaxData];
  int len, comp;
  extern PackEntry ProtoSRP[];
  int avolen, uamolen;
  byte *p;

  SPGetStatus(addr, buf, atpMaxData-1, &len, 1, -1, &comp) ;
  while (comp > 0)
    abSleep(4*9, TRUE);
  if (comp < 0)
    return(comp);

  ntohPackX(ProtoSRP, buf, len, sr);
  /* number of bytes for avo and uamo strings */
  avolen = IndStrLen(sr->sr_avo);
  uamolen = IndStrLen(sr->sr_uamo);
  if ((p = (byte *)malloc(avolen+uamolen)) == NULL)
    return(-1);
  /* copy the data */
  bcopy(sr->sr_avo, p, avolen);
  bcopy(sr->sr_uamo, p+avolen, uamolen);
  /* reset pointers */
  sr->sr_avo = p;
  sr->sr_uamo = p+avolen;
  return(noErr);
}


FPGetSrvrParms(srn, sp, cr)
int srn;
GSPRPPtr *sp;
dword *cr;
{
  byte cmd = AFPGetSrvrParms;
  byte buf[atpMaxData];
  int rlen, comp, i, nvols;
  extern PackEntry ProtoGSPRP[];
  GSPRPPtr rp;
  byte *p;
  
  SPCommand(srn, &cmd, 1, buf, sizeof(buf)-1, cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  nvols = (int)buf[4];		/* count of vols */
  if (nvols == 0)
    *sp = (GSPRPPtr)malloc(sizeof(GetSrvrParmsReplyPkt));
  else
    *sp = (GSPRPPtr)malloc(sizeof(GetSrvrParmsReplyPkt)+
			  (nvols-1)*sizeof(VolParm));
  if (*sp == NULL)
    return(-1);
  rp = *sp;
  ntohPackX(ProtoGSPRP, buf, rlen, rp);
  for (p = &buf[5], i = 0; i < nvols; i++) {
    rp->gspr_volp[i].volp_flag = *p;
    cpyp2cstr(rp->gspr_volp[i].volp_name, p+1);
    p += 2+(int)p[1];
  }
  return(noErr);
}


FPGetVolParms(srn, gvp, ve, cr)
int srn;
GetVolParmsPkt *gvp;
GetVolParmsReplyPkt *ve;
dword *cr;
{
  byte lbuf[sizeof(GetVolParmsPkt)+1];
  byte buf[sizeof(GetVolParmsReplyPkt)+10];
  int len, rlen, comp;
  extern PackEntry ProtoGVPP[];	/* getvolparms */
  extern PackEntry ProtoGVPRP[]; /* volume info */

  len = htonPackX(ProtoGVPP, gvp, lbuf);
  SPCommand(srn, lbuf, len, buf, sizeof(GetVolParmsReplyPkt)+10,
	    cr, &rlen, -1, &comp);
  while (comp > 0) abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  ntohPackX(ProtoGVPRP, buf, rlen, ve);
  return(noErr);
}


FPLogin(srn, lp, lir, cr)
int srn;
LoginPkt *lp;
LoginReplyPkt *lir;
dword *cr;
{
  byte lbuf[sizeof(LoginPkt)+8];
  int rlen, len, comp;
  extern PackEntry ProtoLP[], ProtoLRP[];
  char buf[sizeof(LoginReplyPkt)+1];

  len = htonPackXbitmap(ProtoLP, lp, lbuf, lp->log_flag);

  SPCommand(srn, lbuf, len, buf, sizeof(buf), cr, &rlen, -1, &comp);

  while (comp > 0)
    abSleep(4, TRUE);
  ntohPackXbitmap(ProtoLRP, buf, rlen, lir, lir->logr_flag);
  return(comp);
}


/* For now - assume no response - this is not necessarily true though */
FPLoginCont(srn, lgc, lir, cr)
int srn;
LoginContPkt *lgc;
LoginReplyPkt *lir;
dword *cr;
{
  extern PackEntry ProtoLCP[], ProtoLRP[];
  char lbuf[sizeof(LoginContPkt)];
  char buf[sizeof(LoginReplyPkt)+1];
  int rlen, len, comp;

  len = htonPackXbitmap(ProtoLCP, lgc, lbuf, lgc->lgc_flags);
  SPCommand(srn, lbuf, len, buf, sizeof(buf), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  ntohPackXbitmap(ProtoLRP, buf, rlen, lir, lir->logr_flag);
  return(comp);
}


FPLogout(srn, cr)
int srn;
dword *cr;
{
  byte cmd = AFPLogout;
  return(sendspcmd(srn, &cmd, 1, cr));
}


FPMapID(srn, mi,mapr, cr)
int srn;
MapIDPkt *mi;
MapIDReplyPkt *mapr;
dword *cr;
{
  byte buf[sizeof(MapIDReplyPkt)+1], lbuf[sizeof(MapIDPkt)+1];
  int len, rlen, comp;
  extern PackEntry ProtoMIP[];

  len = htonPackX(ProtoMIP, mi, lbuf);
  SPCommand(srn,lbuf,len,buf,sizeof(MapIDReplyPkt)+1, cr, &rlen, -1, &comp);
  while (comp > 0) abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  pstrcpy(mapr->mpir_name, buf); /* copy back name */
  return(noErr);
}


FPMapName(srn, mnp, id, cr)
int srn;
MapNamePkt *mnp;
dword *id;
dword *cr;
{
  byte lbuf[sizeof(MapNamePkt)+1], retid[sizeof(dword)], *p;
  int len, rlen, comp;
  extern PackEntry ProtoMNP[];

  len = htonPackX(ProtoMNP, mnp, lbuf);
  SPCommand(srn, lbuf, len, retid, sizeof(retid), cr, &rlen, -1, &comp);
  while (comp > 0) abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  p = retid;
  UnpackDWord(&p, id);
  return(comp);
}


FPMoveFile(srn, mf, cr)
int srn;
MovePkt *mf;
dword *cr;
{
  char lbuf[sizeof(MovePkt)];
  extern PackEntry ProtoMFP[];
  int len;

  len = htonPackX(ProtoMFP, mf, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPOpenDir(srn, od, retdirid, cr)
int srn;
OpenDirPkt *od;
dword *retdirid;
dword *cr;
{
  byte lbuf[sizeof(OpenDirPkt)+1], retid[sizeof(dword)], *p;
  int len, rlen, comp;
  extern PackEntry ProtoODP[];
  
  len = htonPackX(ProtoODP, od, lbuf);
  SPCommand(srn, lbuf, len, retid, sizeof(retid), cr, &rlen, -1, &comp);
  while (comp > 0) abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  p = retid;
  UnpackDWord(&p, retdirid);
  return(comp);
}


FPOpenDT(srn, odt, dtrefnum, cr)
int srn;
OpenDTPkt *odt;
word *dtrefnum;
dword *cr;
{
  extern PackEntry ProtoODT[];
  word reply;
  int rlen, len, comp;
  char lbuf[sizeof(OpenDTPkt)+1];

  len = htonPackX(ProtoODT, odt, lbuf);
  SPCommand(srn, lbuf, len, &reply, sizeof(reply), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)
    return(comp);
  *dtrefnum = ntohs(reply);
  return(comp);
}


/* if type  then rsrc else data */
/* should really optionally return bitmap data */


FPOpenFork(srn, of, epar, refnum, cr)
int srn;
OpenForkPkt *of;
FileDirParm *epar;
word *refnum;
dword *cr;
{
  extern PackEntry ProtoOFkP[], ProtoFileAttr[];
  byte lbuf[sizeof(OpenForkPkt)], buf[sizeof(FileDirParm)+20], *p;
  int rlen, comp, len;
  word bitmap;
  
  len = htonPackX(ProtoOFkP, of, lbuf);

  SPCommand(srn, lbuf, len, buf, sizeof(buf), cr, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0) 
    return(comp);
  p = buf;
  UnpackWord(&p, &bitmap);
  UnpackWord(&p, refnum);
  if (epar)
    ntohPackXbitmap(ProtoFileAttr, p, rlen, epar, bitmap);
#ifdef notdef
  printf("openfork CR = %X, %d\n",cr,-(*cr));
  printf("RLEN = %d\n",rlen);
  printf("Open file refnum = %d\n",*refnum);
#endif
  return(noErr);
}


FPOpenVol(srn, ov, op, cr)
int srn;
OpenVolPkt *ov;
GetVolParmsReplyPkt *op;
dword *cr;
{
  byte lbuf[sizeof(OpenVolPkt)+1];
  byte buf[100];
  int rlen, comp, len;
  extern PackEntry ProtoGVPRP[];
  extern PackEntry ProtoOVP[];

  len = htonPackX(ProtoOVP, ov, lbuf);

  SPCommand(srn, lbuf, len, buf, 100, cr, &rlen, -1, &comp);

  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0) 
    return(comp);

  ntohPackX(ProtoGVPRP, buf, rlen, op);
  return(noErr);
}


/*
 * reads from remote into buf for length at most buflen starting at offset
 * file must already be open
 * returns length read
 * sets eof if eof was returned 
*/


FPRead(srn, rp, buf, buflen, rlen, cr)
int srn;
ReadPkt *rp;
byte *buf;
int *rlen;
dword *cr;
{
  char lbuf[sizeof(ReadPkt)+1];
  extern PackEntry ProtoRP[];
  int comp, len;

  len = htonPackX(ProtoRP, rp, lbuf);

  SPCommand(srn, lbuf, len, buf, buflen, cr, rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  return(comp);
}


FPRemoveAPPL(srn, ra, cr)
int srn;
RemoveAPPLPkt *ra;
dword *cr;
{
  extern PackEntry ProtoRMA[];
  int len;
  byte lbuf[sizeof(RemoveAPPLPkt)+1];

  len = htonPackX(ProtoRMA, ra, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPRemoveComment(srn, rc, cr)
int srn;
RemoveCommentPkt *rc;
dword *cr;
{
  extern PackEntry ProtoRMC[];
  int len;
  byte lbuf[sizeof(RemoveCommentPkt)+1];

  len = htonPackX(ProtoRMC, rc, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPRename(srn, rn, cr)
int srn;
RenamePkt *rn;
dword *cr;
{
  extern PackEntry ProtoRFP[];
  byte lbuf[sizeof(RenamePkt)+1];
  int len;

  len = htonPackX(ProtoRFP, rn, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPSetDirParms(srn, sdp, fdp, cr)
int srn;
SetDirParmsPkt *sdp;
FileDirParm *fdp;
dword *cr;
{
  extern PackEntry ProtoSDPP[], ProtoDirAttr[];
  byte lbuf[sizeof(SetDirParmsPkt)+sizeof(FileDirParm)+1];
  int len;

  len = htonPackX(ProtoSDPP, sdp, lbuf);
  len += htonPackXbitmap(ProtoDirAttr, fdp, lbuf+len, sdp->sdp_bitmap);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPSetFileParms(srn, sfp, fdp, cr)
int srn;
SetFileParmsPkt *sfp;
FileDirParm *fdp;
dword *cr;
{
  extern PackEntry ProtoSFPP[], ProtoFileAttr[];
  byte lbuf[sizeof(SetFileParmsPkt)+sizeof(FileDirParm)+1];
  int len;

  len = htonPackX(ProtoSFPP, sfp, lbuf);
  len += htonPackXbitmap(ProtoFileAttr, fdp, lbuf+len, sfp->sfp_bitmap);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPSetFileDirParms(srn, sfdp, fdp, cr)
int srn;
SetFileDirParmsPkt *sfdp;
FileDirParm *fdp;
dword *cr;
{
  extern PackEntry ProtoSDPP[], ProtoFileDirAttr[];
  byte lbuf[sizeof(SetForkParmsPkt)+sizeof(FileDirParm)+1];
  int len;

  len = htonPackX(ProtoSDPP, sfdp, lbuf);
  len += htonPackXbitmap(ProtoFileDirAttr, fdp, lbuf+len, sfdp->scp_bitmap);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPSetForkParms(srn, sfp, cr)
int srn;
SetForkParmsPkt *sfp;
dword *cr;
{
  extern PackEntry ProtoSFkPP[];
  byte lbuf[sizeof(SetForkParmsPkt)+1];
  int len;

  len = htonPackX(ProtoSFkPP, sfp, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPSetVolParms(srn, svp, cr)
SetVolParmsPkt *svp;
dword *cr;
{
  extern PackEntry ProtoSVPP[];
  byte lbuf[sizeof(SetVolParmsPkt)+1];
  int len;

  len = htonPackX(ProtoSVPP, svp, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}


FPWrite(srn, wbuf, wlen, wp, actcnt, lastoffset_written, cr)
int srn;
char *wbuf;
int wlen;
WritePkt *wp;
dword *actcnt;			/* actual count written */
dword *lastoffset_written;	/* last offset written */
dword *cr;
{
  char lbuf[sizeof(WritePkt)+1];
  dword low;
  int rlen, comp, len;
  extern PackEntry ProtoWP[];

  len = htonPackX(ProtoWP, wp, lbuf);

  SPWrite(srn, lbuf, len, wbuf, wlen, &low, sizeof(low),
		cr, actcnt, &rlen, -1, &comp);
  while (comp > 0)
    abSleep(4, TRUE);
  if (comp < 0 || *cr != 0)	/* keep trying if so */
    return(comp);
  *lastoffset_written = htonl(low); /* ugh */
  return(noErr);
}


FPExchangeFiles(srn, mf, cr)
int srn;
ExchPkt *mf;
dword *cr;
{
  char lbuf[sizeof(ExchPkt)];
  extern PackEntry ProtoExP[];
  int len;

  len = htonPackX(ProtoExP, mf, lbuf);
  return(sendspcmd(srn, lbuf, len, cr));
}

