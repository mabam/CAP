/*
 * $Author: djh $ $Date: 1996/06/19 04:20:09 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpfile.c,v 2.11 1996/06/19 04:20:09 djh Rel djh $
 * $Revision: 2.11 $
 *
 */

/*
 * afpfile.c - Appletalk Filing Protocol File Level Routines
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
 *  FPExchangeFiles()
 *  FPSetFileParms()
 *  FPCreateFile()
 *  FPCopyFile()
 *  FPRename()
 *  FPDelete() 
 *  FPMove()
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
#include "afps.h"

#ifdef DEBUG_AFP_CMD
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

/*
 * OSErr FPSetFileParms(...)
 *
 */

/*ARGSUSED*/
OSErr
FPSetFileParms(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  SetFileParmsPkt sfp;
  FileDirParm fdp;
  IDirP idir,ipdir;
  char file[MAXUFLEN];
  int ivol,len,err;

  len = ntohPackX(PsSetFileParms,p,l,(byte *) &sfp);
  ntohPackXbitmap(ProtoFileAttr,p+len,l-len,(byte *) &fdp,sfp.sfp_bitmap);

  err = EtoIfile(file,&idir,&ipdir,&ivol,sfp.sfp_dirid,
		 sfp.sfp_volid,sfp.sfp_ptype,sfp.sfp_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    void dbg_print_parm();
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", sfp.sfp_volid);
    fprintf(dbg, "\tDirID: %08x\n", sfp.sfp_dirid);
    fprintf(dbg, "\tBtMap: %08x\t", sfp.sfp_bitmap);
    dbg_print_bmap(sfp.sfp_bitmap);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", sfp.sfp_ptype,
      (sfp.sfp_ptype == 1) ? "Short" : "Long");
    dbg_print_path(sfp.sfp_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    dbg_print_parm(sfp.sfp_bitmap, p+len, l-len, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  fdp.fdp_fbitmap = sfp.sfp_bitmap; 	/* set file bitmap */

  if (DBFIL) 
    printf("FPSetFileParms: setting bm=%d for %s %s\n",
	   fdp.fdp_fbitmap,pathstr(ipdir),file);

  return(OSSetFileParms(ipdir,file,&fdp));
}

/*
 * OSErr FPCreateFile(byte *p,byte *r,int *rl)
 *
 * This call is used to create a file.
 *
 */


/*ARGSUSED*/
OSErr 
FPCreateFile(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  CreateFilePkt crf;
  int ivol,delf,err;
  IDirP idir,ipdir;
  char file[MAXUFLEN];

  ntohPackX(PsCreateFile,p,l,(byte *) &crf);
  err = EtoIfile(file,&idir,&ipdir,&ivol,crf.crf_dirid,
		 crf.crf_volid,crf.crf_ptype,crf.crf_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", crf.crf_volid);
    fprintf(dbg, "\tDirID: %08x\n", crf.crf_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", crf.crf_ptype,
      (crf.crf_ptype == 1) ? "Short" : "Long");
    dbg_print_path(crf.crf_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (err != noErr)
    return(err);

  delf = (crf.crf_flg & CRF_HARD) != 0;

  if (DBFIL) 
    printf("FPCreateFile flg=%02x, delf=%d, path=%s, fn=%s\n",
	   crf.crf_flg,delf,pathstr(ipdir),file);

  err = OSCreateFile(ipdir,file,delf);
  if (err == noErr)			/* if success */
    VolModified(ivol);			/*  then volume modified */
  return(err);
}

#ifdef FIXED_DIRIDS
/*
 * OSErr FPCreateID(byte *p,byte *r,int *rl)
 *
 * This call is used to create a file ID.
 *
 */


/*ARGSUSED*/
OSErr 
FPCreateID(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  CreateIDPkt crid;
  int ivol,delf,err;
  IDirP idir,ipdir;
  char file[MAXUFLEN];
  sdword fileID;

  ntohPackX(PsCreateID,p,l,(byte *) &crid);

  err = EtoIfile(file,&idir,&ipdir,&ivol,crid.crid_dirid,
		 crid.crid_volid,crid.crid_ptype,crid.crid_path);

  if (err == noErr)
    fileID = aufsExtEFileId(ipdir->edirid, file);
#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", crid.crid_volid);
    fprintf(dbg, "\tDirID: %08x\n", crid.crid_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", crid.crid_ptype,
      (crid.crid_ptype == 1) ? "Short" : "Long");
    dbg_print_path(crid.crid_path);
    if (err == noErr)
      fprintf(dbg, "\tUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    else
      fprintf(dbg, "\tUPath: <EtoIfile returned %d>\n", err);
    fprintf(dbg, "\tFileID: %08x\n", fileID);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */


  if ((err != noErr) && (DBFIL)) 
     printf("error returned from EtoIfile\n");
  PackDWord(fileID,r);
  *rl = 4;
  return(noErr);
}

/*
 * OSErr FPResolveID(byte *p,byte *r,int *rl)
 *
 * This call is used to resolve a file ID.
 *
 */


/*ARGSUSED*/
OSErr 
FPResolveID(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  ResolveIDPkt rid;
  int ivol,delf,err;
  IDirP idir,ipdir;
  FileDirParm fdp;
  char file[MAXUFLEN];
  sdword fileID;
  char *path, *filep;
  extern char *strrchr();
  extern char *aufsExtPath();

  idir = NULL;

  ntohPackX(PsRslvID,p,l,(byte *) &rid);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_bmap();
    fprintf(dbg, "\tVolID: %04x\n", rid.rid_volid);
    fprintf(dbg, "\tFilID: %04x\n", rid.rid_fileid);
    fprintf(dbg, "\tFBMap: %04x\t", rid.rid_fbitmap);
    dbg_print_bmap(rid.rid_fbitmap, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * get the path from the database
   */
  if ((path = aufsExtPath(rid.rid_fileid)) == NULL)
	return(aeIDNotFound);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tFName: %s\n", path);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * devide into file and dir
   */
  filep = strrchr(path, '/');
  if (filep)
	*filep++ = '\0';

  /*
   * get the Volume ID
   */
  ivol = EtoIVolid(rid.rid_volid);		/* set internal volid */

  /*
   * get the directory ID
   */
  ipdir = Idirid(path);
  
  /*
   * call OSFileDirInfo
   */
  fdp.fdp_pdirid = ItoEdirid(ipdir,ivol);
  fdp.fdp_dbitmap = 0;
  fdp.fdp_fbitmap = rid.rid_fbitmap;
  fdp.fdp_zero = 0;
  err = OSFileDirInfo(ipdir,idir,filep,&fdp,ivol); /* fill in information */
  if (err != noErr) {
    if (DBFIL)
      printf("FPResolveID: OSFileDirInfo returns %d on %s/%s\n",
	     err,pathstr(ipdir),file);
    return(err);
  }
  PackWord(rid.rid_fbitmap,r);
  *rl = 2;
  *rl += htonPackX(FilePackR,(byte *) &fdp,r+(*rl));

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_parm();
    void dbg_print_bmap();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tFBMap: %04x\t", fdp.fdp_fbitmap);
    dbg_print_bmap(fdp.fdp_fbitmap, 0);
    fprintf(dbg, "\tFDFlg: %02x\t(%s)\n", fdp.fdp_flg,
      FDP_ISDIR(fdp.fdp_flg) ? "Directory" : "File");
    if (*rl == 2)
      fprintf(dbg, "\t<No Parameter List>\n");
    else
      dbg_print_parm(fdp.fdp_fbitmap, r+2, (*rl)-2, 0);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}


/*
 * OSErr FPDeleteID(byte *p,byte *r,int *rl)
 *
 * This call is used to delete a file ID.
 *
 */


/*ARGSUSED*/
OSErr 
FPDeleteID(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
	return(aeParamErr);
}
#endif	/* FIXED_DIRIDS */
  
/*
 * FPCopyFile(byte *p,byte *r,int *rl) [NOP]
 * 
 * Optional, may not be supported on all servers.
 *
 *
 */

/*ARGSUSED*/
OSErr 
FPCopyFile(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  CopyFilePkt cpf;
  char sfile[MAXUFLEN],dfile[MAXUFLEN],newname[MAXUFLEN];
  char *dfilep;
  IDirP sidir,sipdir,didir,dipdir;
  int sivol,divol;
  int err;

  ntohPackX(PsCopyFile,p,l,(byte *) &cpf);
  err = EtoIfile(sfile,&sidir,&sipdir,&sivol,cpf.cpf_sdirid,
		 cpf.cpf_svolid,cpf.cpf_sptype,cpf.cpf_spath);
  if (err != noErr)
    return(err);
  err = EtoIfile(dfile,&didir,&dipdir,&divol,cpf.cpf_ddirid,
		 cpf.cpf_dvolid,cpf.cpf_dptype,cpf.cpf_dpath);
  if (err != noErr)
    return(err);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tSVolID: %04x\n", cpf.cpf_svolid);
    fprintf(dbg, "\tSDirID: %08x\n", cpf.cpf_sdirid);
    fprintf(dbg, "\tDVolID: %04x\n", cpf.cpf_dvolid);
    fprintf(dbg, "\tDDirID: %08x\n", cpf.cpf_ddirid);
    fprintf(dbg, "\tSPType: %d\t(%s Names)\n", cpf.cpf_sptype,
      (cpf.cpf_sptype == 1) ? "Short" : "Long");
    dbg_print_path(cpf.cpf_spath);
    fprintf(dbg, "\tSUPath: \"%s/%s\"\n", pathstr(sipdir), sfile);
    fprintf(dbg, "\tDPType: %d\t(%s Names)\n", cpf.cpf_dptype,
      (cpf.cpf_dptype == 1) ? "Short" : "Long");
    dbg_print_path(cpf.cpf_dpath);
    fprintf(dbg, "\tDUPath: \"%s/%s\"\n", pathstr(dipdir), dfile);
    fprintf(dbg, "\tNPType: %d\t(%s Names)\n", cpf.cpf_newtype,
      (cpf.cpf_newtype == 1) ? "Short" : "Long");
    fprintf(dbg, "\tNMFile: \"%s\"\n", cpf.cpf_newname);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (didir == NILDIR)			/* destination directory */
    return(aeParamErr);			/* must be around */

#ifdef SHORT_NAMES
/* i don't have to worry about this if we are doing short names */
  if ((err = EtoIName(cpf.cpf_newname,newname)) != noErr)
    return(err);
#else SHORT_NAMES
  if ((err = EtoIName(cpf.cpf_newname,newname)) != noErr)
    return(err);
#endif SHORT_NAMES

  if (*newname != '\0')			/* if new name was specified */
    dfilep = newname;			/*  then use that */
  else					/* otherwise if no new name */
   dfilep = sfile;			/*  then use the source file name */

  if (DBFIL) {
    printf("FPCopyFile: srcvol=%d, srcdir=%s srcfil=%s\n",
	   sivol,pathstr(sipdir),sfile);
    printf("FPCopyFile: dstvol=%d, dstdir=%s, dstname=%s\n",
	   divol,pathstr(didir),dfilep);
  }
  
  err = OSCopyFile(sipdir,sfile,didir,dfilep);
  if (err == noErr)			/* if success */
    VolModified(divol);			/*  then dest volume modified */
  return(err);
}

/*
 * FPRename(byte *p,byte *r,int *rl)
 *
 * This call is used to rename either a file or directory.
 *
 */

/*ARGSUSED*/
OSErr 
FPRename(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  RenamePkt ren;
  IDirP idir,ipdir,nidir,nipdir;
  int ivol,err;
  char file[MAXUFLEN],nfile[MAXUFLEN];
 
  ntohPackX(PsRename,p,l,(byte *) &ren);
  
  err = EtoIfile(file,&idir,&ipdir,&ivol,ren.ren_dirid,
		 ren.ren_volid,ren.ren_ptype,ren.ren_path);
  if (err != noErr) 
    return(err);

  err = EtoIfile(nfile,&nidir,&nipdir,&ivol,ren.ren_dirid,
		 ren.ren_volid,ren.ren_ntype,ren.ren_npath);
  if (err != noErr)
    return(err);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", ren.ren_volid);
    fprintf(dbg, "\tDirID: %08x\n", ren.ren_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", ren.ren_ptype,
      (ren.ren_ptype == 1) ? "Short" : "Long");
    dbg_print_path(ren.ren_path);
    fprintf(dbg, "\tPPath: \"%s/%s\"\n", pathstr(ipdir), file);
    fprintf(dbg, "\tNType: %d\t(%s Names)\n", ren.ren_ntype,
      (ren.ren_ntype == 1) ? "Short" : "Long");
    dbg_print_path(ren.ren_npath);
    fprintf(dbg, "\tNPath: \"%s/%s\"\n", pathstr(nipdir), nfile);
    fprintf(dbg, "\tNwTyp: %d\t(%s Names)\n", ren.ren_ntype,
      (ren.ren_ntype == 1) ? "Short" : "Long");
    dbg_print_path(ren.ren_npath);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBFIL) {    
    printf("FPRename path=%s, name=%s,",pathstr(ipdir),file);
    printf("to path=%s, name=%s\n",pathstr(nipdir),nfile);
  }

  if (ipdir != nipdir) {
    printf("FPRename: different parent directory\n");
    return(aeParamErr);
  }
  err = OSRename(ipdir,file,nfile);
  if (err == noErr) {
    OFNFIXUP(ipdir, file, ipdir, nfile);
  }
  return(err);
}

  
/*
 * FPDelete(byte *p,byte *r,int *rl)
 *
 * This call is used to delete either a file or directory.
 *
 */

/*ARGSUSED*/
OSErr 
FPDelete(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  DeletePkt del;
  IDirP idir,ipdir;
  int ivol,err;
  char file[MAXUFLEN];

  ntohPackX(PsDelete,p,l,(byte *) &del);

  err = EtoIfile(file,&idir,&ipdir,&ivol,del.del_dirid,
		 del.del_volid,del.del_ptype,del.del_path);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tVolID: %04x\n", del.del_volid);
    fprintf(dbg, "\tDirID: %08x\n", del.del_dirid);
    fprintf(dbg, "\tPType: %d\t(%s Names)\n", del.del_ptype,
      (del.del_ptype == 1) ? "Short" : "Long");
    dbg_print_path(del.del_path);
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
    printf("FPDelete: path=%s file=%s\n",pathstr(ipdir),file);

  if (is_open_file(ipdir, file))
    return(aeFileBusy);

#ifdef STAT_CACHE
  OScd(pathstr(ipdir)); /* so we don't try and delete current directory */
#endif STAT_CACHE

  err = OSDelete(ipdir,idir,file);

  if (err == noErr)			/* if success */
    VolModified(ivol);			/*  then volume modified */

  return(err);
}

/*
 * FPMove(...)
 *
 * This call is used to move (not just copy) a directory or file
 * to another location on a single volume (source and destination
 * must be on the same volume).  An object cannot be moved from one
 * volume to another with this call, even though both volumes may be
 * managed by the server.  The destination of the move is specified
 * by providing a DirID and Pathname that indicates the object's new
 * Parent Directory.
 *
 */

/*ARGSUSED*/
OSErr
FPMove(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  MovePkt mov;
  IDirP idir,ipdir,nidir,nipdir;
  int ivol,err;
  char file[MAXUFLEN],nfile[MAXUFLEN];
  char *nf;
#ifdef SHORT_NAMES
  char tempfile[MAXUFLEN];
  int i;
#endif SHORT_NAMES
 
  ntohPackX(PsMove,p,l,(byte *) &mov);
  
  err = EtoIfile(file,&idir,&ipdir,&ivol,mov.mov_sdirid,
		 mov.mov_volid,mov.mov_sptype,mov.mov_spath);
  if (err != noErr)
    return(err);

  err = EtoIfile(nfile,&nidir,&nipdir,&ivol,mov.mov_ddirid,
		 mov.mov_volid,mov.mov_dptype,mov.mov_dpath);
  if (err != noErr)
    return(err);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tSVolID: %04x\n", mov.mov_volid);
    fprintf(dbg, "\tSDirID: %08x\n", mov.mov_sdirid);
    fprintf(dbg, "\tDVolID: %04x\n", mov.mov_volid);
    fprintf(dbg, "\tDDirID: %08x\n", mov.mov_ddirid);
    fprintf(dbg, "\tSPType: %d\t(%s Names)\n", mov.mov_sptype,
      (mov.mov_sptype == 1) ? "Short" : "Long");
    dbg_print_path(mov.mov_spath);
    fprintf(dbg, "\tSUPath: \"%s/%s\"\n", pathstr(ipdir), file);
    fprintf(dbg, "\tDPType: %d\t(%s Names)\n", mov.mov_dptype,
      (mov.mov_dptype == 1) ? "Short" : "Long");
    dbg_print_path(mov.mov_dpath);
    fprintf(dbg, "\tDUPath: \"%s/%s\"\n", pathstr(nipdir), nfile);
    fprintf(dbg, "\tNPType: %d\t(%s Names)\n", mov.mov_newtype,
      (mov.mov_newtype == 1) ? "Short" : "Long");
    dbg_print_path(mov.mov_newname);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /* nidir, nfile must be a directory! */
  if (nidir == NILDIR)
    return(aeParamErr);
  /* where should this be done if not here? */
#ifdef SHORT_NAMES
  if ((mov.mov_newtype != 0x2) && (mov.mov_newname[0]!= '\0')) {
    for (i = 1; i <= (int)mov.mov_newname[0]; i++)
       tempfile[i-1] = mov.mov_newname[i];	
    tempfile[mov.mov_newname[0]] = '\0';
    if (DBFIL)
       printf("tempfile %s\n",tempfile);
    EtoIName_Short(nidir,tempfile, nfile);
    nf = nfile;
  }else if (mov.mov_newname[0] != '\0'){
    EtoIName(mov.mov_newname, nfile);
    nf = nfile;
#else SHORT_NAMES
  if (mov.mov_newtype != 0x2)
    return(aeParamErr);
  if (mov.mov_newname[0] != '\0') {
    EtoIName(mov.mov_newname, nfile);
    nf = nfile;
#endif SHORT_NAMES
  } else nf = file;

  if (DBFIL) {    
    printf("FPMove path=%s, name=%s,",pathstr(ipdir),file);
    printf("to path=%s, name=%s\n",pathstr(nidir),nf);
  }

  err = OSMove(ipdir,file,nidir,nf);
  if (err == noErr) {			/* if success */
    OFNFIXUP(ipdir, file, nidir, nfile);
    VolModified(ivol);			/*  then volume modified */
  }
  return(err);
}

/*
 * FPExchangeFiles(...)
 *
 * Atomic exchange of two files.  Swaps the data and resource forks
 * but not the finder info.
 *
 */

/*ARGSUSED*/
OSErr
FPExchangeFiles(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  ExchPkt xch;
  IDirP aidir,aipdir,bidir,bipdir;
  int ivol,err;
  char afile[MAXUFLEN],bfile[MAXUFLEN];
  char *nf;
 
  ntohPackX(PsExchange,p,l,(byte *)&xch);
  
  err = EtoIfile(afile,&aidir,&aipdir,&ivol,xch.exc_adirid,
               xch.exc_volid,xch.exc_aptype,xch.exc_apath);
  if (err != noErr)
    return(err);

  err = EtoIfile(bfile,&bidir,&bipdir,&ivol,xch.exc_bdirid,
               xch.exc_volid,xch.exc_bptype,xch.exc_bpath);
  if (err != noErr)
    return(err);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_path();
    fprintf(dbg, "\tEVolID: %04x\n", xch.exc_volid);
    fprintf(dbg, "\tSDirID: %08x\n", xch.exc_adirid);
    fprintf(dbg, "\tDDirID: %08x\n", xch.exc_bdirid);
    fprintf(dbg, "\tSType: %d\t(%s Names)\n", xch.exc_aptype,
      (xch.exc_aptype == 1) ? "Short" : "Long");
    dbg_print_path(xch.exc_apath);
    fprintf(dbg, "\tDType: %d\t(%s Names)\n", xch.exc_bptype,
      (xch.exc_bptype == 1) ? "Short" : "Long");
    dbg_print_path(xch.exc_bpath);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * Neither a nor b should be directories.
   *
   */
  if (aidir != NILDIR || bidir != NILDIR)
    return(aeObjectTypeErr);

  if (DBFIL) {    
    printf("FPExchangeFiles path=%s, name=%s,\n",pathstr(aipdir),afile);
    printf("  with path=%s, name=%s\n",pathstr(bipdir),bfile);
  }

  err = OSExchangeFiles(aipdir,afile,bipdir,bfile);

  if (err == noErr) {	/* if success */
    VolModified(ivol);	/*  then volume modified */
#ifdef	FIXED_DIRIDS
    {
      char a_path[MAXPATHLEN], b_path[MAXPATHLEN];
      sprintf(a_path, "%s/%s", pathstr(aipdir),afile);
      sprintf(b_path, "%s/%s", pathstr(bipdir),bfile);
      aufsExtExchange(a_path, b_path);
    }
#endif	/* FIXED_DIRIDS */
  }

  return(err);
}
