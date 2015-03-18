/*
 * $Author: djh $ $Date: 1996/04/25 03:26:19 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afppacks.c,v 2.5 1996/04/25 03:26:19 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * afppacks.c - Packing and unpacking templates
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  march 23, 1987	CCKim    Created from afpcmd.c
 *
 */

#include <sys/types.h>
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>

#ifndef NULL
#define NULL 0
#endif  NULL

PackEntry EnumPackR[] = {
  PACK(ERPPtr,P_WORD,enur_fbitmap),
  PACK(ERPPtr,P_WORD,enur_dbitmap),
  PACK(ERPPtr,P_WORD,enur_actcnt),
  PACKEND()
  };

PackEntry DirParmPackR[] = {
  PACK(FDParmPtr,P_WORD,fdp_fbitmap),
  PACK(FDParmPtr,P_WORD,fdp_dbitmap),    
  PACK(FDParmPtr,P_BYTE,fdp_flg),
  PACK(FDParmPtr,P_BYTE,fdp_zero),
  PACKEND()
  };


PackEntry FilePackR[] = {
  PACK(FDParmPtr,P_BMAP,fdp_fbitmap), /* start of bitmap parms */
  PAKB(FDParmPtr,P_WORD,fdp_attr,FP_ATTR),
  PAKB(FDParmPtr,P_DWRD,fdp_pdirid,FP_PDIR),
  PAKB(FDParmPtr,P_TIME,fdp_cdate,FP_CDATE),
  PAKB(FDParmPtr,P_TIME,fdp_mdate,FP_MDATE),
  PAKB(FDParmPtr,P_TIME,fdp_bdate,FP_BDATE),  
  PKSB(FDParmPtr,P_BYTS,fdp_finfo,FP_FINFO),  
  PKSB(FDParmPtr,P_OSTR,fdp_lname,FP_LNAME),  
  PKSB(FDParmPtr,P_OSTR,fdp_sname,FP_SNAME),  
  PAKB(FDParmPtr,P_DWRD,fdp_parms.fp_parms.fp_fileno,FP_FILNO),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.fp_parms.fp_dflen,FP_DFLEN),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.fp_parms.fp_rflen,FP_RFLEN),
  PAKB(FDParmPtr,P_WORD,fdp_prodos_ft,FP_PDOS),
  PAKB(FDParmPtr,P_DWRD,fdp_prodos_aux,FP_PDOS),
  PACKEND()
};


PackEntry DirPackR[] = {
  PACK(FDParmPtr,P_BMAP,fdp_dbitmap), /* start of bitmap parms */
  PAKB(FDParmPtr,P_WORD,fdp_attr,DP_ATTR),
  PAKB(FDParmPtr,P_DWRD,fdp_pdirid,DP_PDIR),
  PAKB(FDParmPtr,P_TIME,fdp_cdate,DP_CDATE),
  PAKB(FDParmPtr,P_TIME,fdp_mdate,DP_MDATE),
  PAKB(FDParmPtr,P_TIME,fdp_bdate,DP_BDATE),  
  PKSB(FDParmPtr,P_BYTS,fdp_finfo,DP_FINFO),  
  PKSB(FDParmPtr,P_OSTR,fdp_lname,DP_LNAME),  
  PKSB(FDParmPtr,P_OSTR,fdp_sname,DP_SNAME),  
  PAKB(FDParmPtr,P_DWRD,fdp_parms.dp_parms.dp_dirid,DP_DIRID),
  PAKB(FDParmPtr,P_WORD,fdp_parms.dp_parms.dp_nchild,DP_CHILD),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.dp_parms.dp_ownerid,DP_CRTID),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.dp_parms.dp_groupid,DP_GRPID),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.dp_parms.dp_accright,DP_ACCES),
  PAKB(FDParmPtr,P_WORD,fdp_prodos_ft,DP_PDOS),
  PAKB(FDParmPtr,P_DWRD,fdp_prodos_aux,DP_PDOS),
  PACKEND()
  };

PackEntry ProtoGVPP[] = {		/* FPGetVolParms */
  PACK(GVPPPtr,P_BYTE,gvp_cmd),		/* command */
  PACK(GVPPPtr,P_ZERO,gvp_zero),	/* always zero */
  PACK(GVPPPtr,P_WORD,gvp_volid),	/* volume id */
  PACK(GVPPPtr,P_WORD,gvp_bitmap),	/* request bitmap */
  PACKEND()
  };


PackEntry ProtoSVPP[] = {		/* FPSetVolParms */
  PACK(SVPPPtr,P_BYTE,svp_cmd),		/* command */
  PACK(SVPPPtr,P_ZERO,svp_zero),	/* always zero */
  PACK(SVPPPtr,P_WORD,svp_volid),	/* volume id */
  PACK(SVPPPtr,P_WORD,svp_bitmap),	/* set bitmap */
  PACK(SVPPPtr,P_DWRD,svp_backdata),	/* backup data to set */
  PACKEND()
  };

PackEntry ProtoOVP[] = {		/* FPOpenVol */
  PACK(OVPPtr,P_BYTE,ovl_cmd),		/* command */
  PACK(OVPPtr,P_ZERO,ovl_zero),		/* always zero */
  PACK(OVPPtr,P_WORD,ovl_bitmap),	/* request bitmap */
  PAKS(OVPPtr,P_PSTR,ovl_name),		/* volume name packed */
  PACKEVEN(),				/* even out if necessary */
  PAKS(OVPPtr,P_BYTS,ovl_pass),		/* password packed */
  PACKEND()
  };

PackEntry ProtoCVP[] = {		/* FPCloseVol */
  PACK(CVPPtr,P_BYTE,cv_cmd),		/* command */
  PACK(CVPPtr,P_ZERO,cv_zero),		/* always zero */
  PACK(CVPPtr,P_WORD,cv_volid),		/* volume ID */
  PACKEND()
  };

PackEntry ProtoFVP[] = {		/* FPFlushVol */
  PACK(FPPtr,P_BYTE,fls_cmd),		/* command */
  PACK(FPPtr,P_ZERO,fls_zero),		/* always zero */
  PACK(FPPtr,P_WORD,fls_volid), /* volume ID */
  PACKEND()
  };

PackEntry ProtoMNP[] = {		/* FPMapName */
  PACK(MNPPtr,P_BYTE,mpn_cmd),		/* MapName command */
  PACK(MNPPtr,P_BYTE,mpn_fcn),		/* function */
  PAKS(MNPPtr,P_PATH,mpn_name),		/* name */
  PACKEND()
  };

PackEntry ProtoMIP[] = {		/* FPMapID */
  PACK(MIPPtr,P_BYTE,mpi_cmd),		/* MapID command */
  PACK(MIPPtr,P_BYTE,mpi_fcn),		/* function */
  PACK(MIPPtr,P_DWRD,mpi_id),		/* ID to map */
  PACKEND()
  };				
  

PackEntry ProtoGFkPP[] = {		/* FPGetForkParms */
  PACK(GFkPPPtr,P_BYTE,gfp_cmd),	/* command */
  PACK(GFkPPPtr,P_ZERO,gfp_zero),	/* zero word */
  PACK(GFkPPPtr,P_WORD,gfp_refnum),	/* reference number */
  PACK(GFkPPPtr,P_WORD,gfp_bitmap),	/* bitmap */
  PACKEND()
  };

PackEntry ProtoSFkPP[] = {		/* FPSetForkParms */
  PACK(SFkPPPtr,P_BYTE,sfkp_cmd),	/* command */
  PACK(SFkPPPtr,P_ZERO,sfkp_zero),	/* zero word */
  PACK(SFkPPPtr,P_WORD,sfkp_refnum),	/* reference number */
  PACK(SFkPPPtr,P_WORD,sfkp_bitmap),	/* bitmap */
  PACK(SFkPPPtr,P_BMAP,sfkp_bitmap),	/* for attributes */
  PAKB(SFkPPPtr,P_DWRD,sfkp_dflen,FP_DFLEN), /* fork length */
  PAKB(SFkPPPtr,P_DWRD,sfkp_rflen,FP_RFLEN), /* fork length */
  PACKEND()
  };

PackEntry ProtoOFkP[] = {		/* FPOpenFork */
  PACK(OFkPPtr,P_BYTE,ofk_cmd),		/* command */
  PACK(OFkPPtr,P_BYTE,ofk_rdflg),	/* resource/data flag */
  PACK(OFkPPtr,P_WORD,ofk_volid),	/* volume id */
  PACK(OFkPPtr,P_DWRD,ofk_dirid),	/* directory id */
  PACK(OFkPPtr,P_WORD,ofk_bitmap),	/* bitmap */
  PACK(OFkPPtr,P_WORD,ofk_mode),	/* access mode */
  PACK(OFkPPtr,P_BYTE,ofk_ptype),	/* path type */
  PAKS(OFkPPtr,P_PATH,ofk_path),	/* path name */
  PACKEND()
};

PackEntry ProtoOFkRP[] = {
  PACK(OFkRPPtr, P_WORD, ofkr_bitmap), /* file bitmap */
  PACK(OFkRPPtr, P_WORD, ofkr_refnum), /* open fork reference number */
  PACKEND()			/* file params follow */
};


PackEntry ProtoCFkP[] = {		/* FPCloseFork */
  PACK(CFkPPtr,P_BYTE,cfk_cmd),		/* command */
  PACK(CFkPPtr,P_ZERO,cfk_zero),	/* zero byte */
  PACK(CFkPPtr,P_WORD,cfk_refnum),	/* reference number */
  PACKEND()
};

PackEntry ProtoGFDPP[] = {		/* FPGetFileDirParms */
  PACK(GFDPPPtr,P_BYTE,gdp_cmd),	/* command */
  PACK(GFDPPPtr,P_ZERO,gdp_zero),	/* always zero */
  PACK(GFDPPPtr,P_WORD,gdp_volid),	/* volume id */
  PACK(GFDPPPtr,P_DWRD,gdp_dirid),	/* directory id */
  PACK(GFDPPPtr,P_WORD,gdp_fbitmap),	/* file request bitmap */
  PACK(GFDPPPtr,P_WORD,gdp_dbitmap),	/* directory request bitmap */
  PACK(GFDPPPtr,P_BYTE,gdp_ptype),	/* path type */
  PAKS(GFDPPPtr,P_PATH,gdp_path),	/* path */
  PACKEND()
  };

PackEntry ProtoCFP[] = {		/* FPCreateFile */
  PACK(CFPPtr,P_BYTE,crf_cmd),		/* CreateFile command */
  PACK(CFPPtr,P_BYTE,crf_flg),		/* flags */
  PACK(CFPPtr,P_WORD,crf_volid),		/* volume id */
  PACK(CFPPtr,P_DWRD,crf_dirid),		/* directory id */
  PACK(CFPPtr,P_BYTE,crf_ptype),		/* path name type */
  PAKS(CFPPtr,P_PATH,crf_path),		/* path name */
  PACKEND()
};

PackEntry ProtoSFPP[] = {		/* FPSetFileParms */
  PACK(SFPPPtr,P_BYTE,sfp_cmd),		/* command */
  PACK(SFPPPtr,P_ZERO,sfp_zero),	/* always zero */
  PACK(SFPPPtr,P_WORD,sfp_volid),	/* volume id */
  PACK(SFPPPtr,P_DWRD,sfp_dirid),	/* directory id */
  PACK(SFPPPtr,P_WORD,sfp_bitmap),	/* set bitmap */
  PACK(SFPPPtr,P_BMAP,sfp_bitmap),	/* for attributes */
  PACK(SFPPPtr,P_BYTE,sfp_ptype),	/* path type */
  PAKS(SFPPPtr,P_PATH,sfp_path),	/* path + file parameters to set */
  PACKEVEN(),				/* even out if necessary */
  PACKEND()
  };


PackEntry ProtoCpFP[] = {		/* FPCopyFile */
  PACK(CpFPPtr,P_BYTE,cpf_cmd),		/* command */
  PACK(CpFPPtr,P_ZERO,cpf_zero),	/* always zero */
  PACK(CpFPPtr,P_WORD,cpf_svolid),	/* source volume id */
  PACK(CpFPPtr,P_DWRD,cpf_sdirid),	/* source directory id */
  PACK(CpFPPtr,P_WORD,cpf_dvolid),	/* destination volume id */
  PACK(CpFPPtr,P_DWRD,cpf_ddirid),	/* destination directory id */
  PACK(CpFPPtr,P_BYTE,cpf_sptype),	/* source path type */
  PAKS(CpFPPtr,P_PATH,cpf_spath),	/* source path */
  PACK(CpFPPtr,P_BYTE,cpf_dptype),	/* destination path type */
  PAKS(CpFPPtr,P_PATH,cpf_dpath),	/* destination path */
  PACK(CpFPPtr,P_BYTE,cpf_newtype),	/* new path type */
  PAKS(CpFPPtr,P_PSTR,cpf_newname),	/* new name */
  PACKEND()
  };

PackEntry ProtoRFP[] = {		/* FPRenameFile */
  PACK(RPPtr,P_BYTE,ren_cmd),		/* command */
  PACK(RPPtr,P_ZERO,ren_zero),		/* always zero */
  PACK(RPPtr,P_WORD,ren_volid),		/* volume id */
  PACK(RPPtr,P_DWRD,ren_dirid),		/* directory id */
  PACK(RPPtr,P_BYTE,ren_ptype),		/* path type */
  PAKS(RPPtr,P_PATH,ren_path),		/* path name */
  PACK(RPPtr,P_BYTE,ren_ntype),		/* new type */
  PAKS(RPPtr,P_PATH,ren_npath),		/* new path */
  PACKEND()
  };

PackEntry ProtoMFP[] = {		/* FPMoveFile */
  PACK(MPPtr,P_BYTE,mov_cmd),		/* command */
  PACK(MPPtr,P_ZERO,mov_zero),		/* always zero */
  PACK(MPPtr,P_WORD,mov_volid),		/* volume id */
  PACK(MPPtr,P_DWRD,mov_sdirid),	/* source directory id */
  PACK(MPPtr,P_DWRD,mov_ddirid),	/* destination directory id */
  PACK(MPPtr,P_BYTE,mov_sptype),	/* source path type */
  PAKS(MPPtr,P_PATH,mov_spath),		/* source path */
  PACK(MPPtr,P_BYTE,mov_dptype),	/* destination path type */
  PAKS(MPPtr,P_PATH,mov_dpath),		/* destination path */
  PACK(MPPtr,P_BYTE,mov_newtype),	/* new type */
  PAKS(MPPtr,P_PATH,mov_newname),	/* new name */
  PACKEND()
  };

#ifdef notdef
PackEntry ProtoGDPP[] = {		/* GetDirParms */
  PACK(GDPPPtr,P_BYTE,gdp_cmd),		/* command */
  PACK(GDPPPtr,P_ZERO,gdp_zero),	/* always zero */
  PACK(GDPPPtr,P_WORD,gdp_volid),	/* volume ID */
  PACK(GDPPPtr,P_DWRD,gdp_dirid),	/* directory id */
  PACK(GDPPPtr,P_WORD,gdp_bitmap),	/* bitmap */
  PACK(GDPPPtr,P_BYTE,gdp_ptype),	/* path type */
  PAKS(GDPPPtr,P_PSTR,gdp_path),	/* path */
  PACKEND()
  };
#endif

PackEntry ProtoSDPP[] = {		/* FPSetDirParms */
  PACK(SDPPPtr,P_BYTE,sdp_cmd),		/* command */
  PACK(SDPPPtr,P_ZERO,sdp_zero),	/* always zero */
  PACK(SDPPPtr,P_WORD,sdp_volid),	/* volume ID */
  PACK(SDPPPtr,P_DWRD,sdp_dirid),	/* directory id */
  PACK(SDPPPtr,P_WORD,sdp_bitmap),	/* bitmap */
/*  PACK(SDPPPtr,P_BMAP,sdp_bitmap),	/* for attrib */
  PACK(SDPPPtr,P_BYTE,sdp_ptype),	/* path type */
  PAKS(SDPPPtr,P_PATH,sdp_path),	/* path */
  PACKEVEN(),				/* move to even boundary */
  PACKEND()
  };


PackEntry ProtoODP[] = {		/* FPOpenDir */
  PACK(ODPPtr,P_BYTE,odr_cmd),		/* command */
  PACK(ODPPtr,P_ZERO,odr_zero),		/* always zero */
  PACK(ODPPtr,P_WORD,odr_volid),	/* volume ID */
  PACK(ODPPtr,P_DWRD,odr_dirid),	/* directory ID */
  PACK(ODPPtr,P_BYTE,odr_ptype),	/* path type */
  PAKS(ODPPtr,P_PATH,odr_path),	/* path */
  PACKEND()
  };

PackEntry ProtoCDP[] = {		/* FPCloseDir */
  PACK(CDPPtr,P_BYTE,cdr_cmd),		/* command */
  PACK(CDPPtr,P_ZERO,cdr_zero),		/* always zero */
  PACK(CDPPtr,P_WORD,cdr_volid),	/* volume id */
  PACK(CDPPtr,P_DWRD,cdr_dirid),	/* directory id */
  PACKEND()
  };

PackEntry ProtoDFP[] = {		/* FPDeleteFile */
  PACK(DPPtr,P_BYTE,del_cmd),		/* command */
  PACK(DPPtr,P_ZERO,del_zero),		/* always zero */
  PACK(DPPtr,P_WORD,del_volid),		/* volume id */
  PACK(DPPtr,P_DWRD,del_dirid),		/* directory id */
  PACK(DPPtr,P_BYTE,del_ptype),		/* path type */
  PAKS(DPPtr,P_PATH,del_path),		/* path */
  PACKEND()
  };

PackEntry ProtoEP[] = {			/* FPEnumerate */
  PACK(EPPtr,P_BYTE,enu_cmd),		/* command */
  PACK(EPPtr,P_ZERO,enu_zero),		/* always zero */
  PACK(EPPtr,P_WORD,enu_volid),		/* volume id */
  PACK(EPPtr,P_DWRD,enu_dirid),		/* directory id */
  PACK(EPPtr,P_WORD,enu_fbitmap),	/* file bitmap */
  PACK(EPPtr,P_WORD,enu_dbitmap),	/* directory bitmap */
  PACK(EPPtr,P_WORD,enu_reqcnt),		/* request count */
  PACK(EPPtr,P_WORD,enu_stidx),		/* start index */
  PACK(EPPtr,P_WORD,enu_maxreply),	/* max reply size */
  PACK(EPPtr,P_BYTE,enu_ptype),		/* path type */
  PAKS(EPPtr,P_PATH,enu_path),		/* path */
  PACKEND()
  };

PackEntry ProtoEPR[] = {
  PACK(ERPPtr, P_WORD, enur_fbitmap),
  PACK(ERPPtr, P_WORD, enur_dbitmap),
  PACK(ERPPtr, P_WORD, enur_actcnt),
  PACKEND()
};

PackEntry ProtoCRDP[] = {		/* FPCreateDir */
  PACK(CRDPPtr,P_BYTE,crd_cmd),		/* command */
  PACK(CRDPPtr,P_ZERO,crd_zero),	/* always zero */
  PACK(CRDPPtr,P_WORD,crd_volid),	/* volume id */
  PACK(CRDPPtr,P_DWRD,crd_dirid),	/* directory id */
  PACK(CRDPPtr,P_BYTE,crd_ptype),	/* path type */
  PAKS(CRDPPtr,P_PATH,crd_path),	/* path */
  PACKEND()
  };

PackEntry ProtoODT[] = {		/* FPOpenDT */
  PACK(ODTPPtr,P_BYTE,odt_cmd),
  PACK(ODTPPtr,P_ZERO,odt_zero),  
  PACK(ODTPPtr,P_WORD,odt_volid),	/* volid */
  PACKEND()
  };

PackEntry ProtoCDT[] = {		/* FPCloseDT */
  PACK(CDTPPtr,P_BYTE,cdt_cmd),
  PACK(CDTPPtr,P_ZERO,cdt_zero),  
  PACK(CDTPPtr,P_WORD,cdt_dtrefnum),	/*  */
  PACKEND()
  };

PackEntry ProtoGI[] = {			/* GetIcon */
  PACK(GIPPtr,P_BYTE,gic_cmd),
  PACK(GIPPtr,P_ZERO,gic_zero),
  PACK(GIPPtr,P_WORD,gic_dtrefnum),
  PAKS(GIPPtr,P_BYTS,gic_fcreator),
  PAKS(GIPPtr,P_BYTS,gic_ftype),
  PACK(GIPPtr,P_BYTE,gic_itype),
  PACK(GIPPtr,P_ZERO,gic_zero2),
  PACK(GIPPtr,P_WORD,gic_length),
  PACKEND()
};

PackEntry ProtoGII[] = {		/* GetIconInfo */
  PACK(GIIPPtr,P_BYTE,gii_cmd),
  PACK(GIIPPtr,P_ZERO,gii_zero),
  PACK(GIIPPtr,P_WORD,gii_dtrefnum),
  PAKS(GIIPPtr,P_BYTS,gii_fcreator),
  PACK(GIIPPtr,P_WORD,gii_iidx),	/*  */
  PACKEND()
  };

PackEntry ProtoAAP[] = {		/* AddAPPL */
  PACK(AAPPtr,P_BYTE,aap_cmd),
  PACK(AAPPtr,P_ZERO,aap_zero),
  PACK(AAPPtr,P_WORD,aap_dtrefnum),
  PACK(AAPPtr,P_DWRD,aap_dirid),
  PAKS(AAPPtr,P_BYTS,aap_fcreator),
  PACK(AAPPtr,P_DWRD,aap_apptag),
  PACK(AAPPtr,P_BYTE,aap_ptype),
  PAKS(AAPPtr,P_PATH,aap_path),		/*  */
  PACKEND()
  };


PackEntry ProtoRMA[] = {		/* RemoveAppl */
  PACK(RAPPtr,P_BYTE,rma_cmd),
  PACK(RAPPtr,P_ZERO,rma_zero),
  PACK(RAPPtr,P_WORD,rma_refnum),
  PACK(RAPPtr,P_DWRD,rma_dirid),
  PAKS(RAPPtr,P_BYTS,rma_fcreator),
  PACK(RAPPtr,P_BYTE,rma_ptype),
  PAKS(RAPPtr,P_PATH,rma_path),		/*  */
  PACKEND()
  };

PackEntry ProtoGAP[] = {		/* GetAPPL */
  PACK(GAPPtr,P_BYTE,gap_cmd),
  PACK(GAPPtr,P_ZERO,gap_zero),
  PACK(GAPPtr,P_WORD,gap_dtrefnum),
  PAKS(GAPPtr,P_BYTS,gap_fcreator),
  PACK(GAPPtr,P_WORD,gap_applidx),
  PACK(GAPPtr,P_WORD,gap_bitmap),	/*  */
  PACKEND()
  };


PackEntry ProtoRP[] = {		/* FPRead */
  PACK(ReadPPtr, P_BYTE, rdf_cmd),
  PACK(ReadPPtr, P_ZERO, rdf_zero),
  PACK(ReadPPtr, P_WORD, rdf_refnum),
  PACK(ReadPPtr, P_DWRD, rdf_offset),
  PACK(ReadPPtr, P_DWRD, rdf_reqcnt),
  PACK(ReadPPtr, P_BYTE, rdf_flag),
  PACK(ReadPPtr, P_BYTE, rdf_nlchar),
  PACKEND()
};


PackEntry ProtoWP[] = {		/* FPWrite */
  PACK(WPPtr, P_BYTE, wrt_cmd),
  PACK(WPPtr, P_BYTE, wrt_flag),
  PACK(WPPtr, P_WORD, wrt_refnum),
  PACK(WPPtr, P_DWRD, wrt_offset),
  PACK(WPPtr, P_DWRD, wrt_reqcnt),
  PACKEND()
};


PackEntry ProtoLP[] = {		/* AFPLogin */
  PACK(LPPtr, P_BYTE, log_cmd),
  PAKS(LPPtr, P_PSTR, log_ver),
  PAKS(LPPtr, P_PSTR, log_uam),
  PKSB(LPPtr, P_PSTR, log_user, UAMP_USER),
  PAKB(LPPtr, P_EVEN, log_zero, UAMP_ZERO),
  PKSB(LPPtr, P_BYTS, log_passwd, UAMP_PASS),
  PACKEND()
};

PackEntry ProtoLOP[] = {
  PACK(LOPPtr, P_BYTE, lgo_cmd),
  PACKEND()
};

PackEntry ProtoAuthInfo[] = {
  PKSB(LPPtr, P_PSTR, log_user, UAMP_USER),
  PAKB(LPPtr, P_EVEN, log_zero, UAMP_ZERO),
  PKSB(LPPtr, P_BYTS, log_passwd, UAMP_PASS),
  PACKEND()
};

PackEntry ProtoLRP[] = {	/* FPLogin reply */
  PAKB(LRPPtr, P_WORD, logr_idnum, UAMP_INUM),
  PKSB(LRPPtr, P_BYTS, logr_randnum, UAMP_RAND),
  PACKEND()
};

PackEntry ProtoLCP[] = {		/* FPLoginCont */
  PACK(LCPPtr,P_BYTE,lgc_cmd),		/* command */
  PACK(LCPPtr,P_ZERO,lgc_zero),		/* is this here? */
  PAKB(LCPPtr,P_WORD,lgc_idno, UAMP_INUM), /* ID number */
  PKSB(LCPPtr,P_BYTS,lgc_encrypted, UAMP_ENCR), /* encrypted passwd */
  PKSB(LCPPtr,P_BYTS,lgc_wsencrypt, UAMP_TWAY), /* encrypted passwd */
  PACKEND()
};

PackEntry ProtoLCR[] = {		/* FPLoginCont Reply */
  PKSB(LCPPtr,P_BYTS,lgc_wsencrypt,UAMP_TWAY),
  PACKEND()
};

PackEntry ProtoSFDPP[] = {		/* FPSetFileDirParms */
  PACK(SFDPPPtr,P_BYTE,scp_cmd),	/* command */
  PACK(SFDPPPtr,P_ZERO,scp_zero),	/* always zero */
  PACK(SFDPPPtr,P_WORD,scp_volid),	/* volume id */
  PACK(SFDPPPtr,P_DWRD,scp_dirid),	/* directory id */
  PACK(SFDPPPtr,P_WORD,scp_bitmap),	/* set bitmap */
  PACK(SFDPPPtr,P_BMAP,scp_bitmap),	/* For attributes */
  PACK(SFDPPPtr,P_BYTE,scp_ptype),	/* path type */
  PAKS(SFDPPPtr,P_PATH,scp_path),	/* path + file parameters to set */
  PACKEVEN(),				/* even out if necessary */
  PACKEND()
  };


/* For FPEnumerate, etc. - client */
PackEntry ProtoFileAttr[] = {
  PAKB(FDParmPtr, P_WORD, fdp_attr,FP_ATTR),
  PAKB(FDParmPtr, P_DWRD, fdp_pdirid,FP_PDIR),
  PAKB(FDParmPtr, P_TIME, fdp_cdate,FP_CDATE),
  PAKB(FDParmPtr, P_TIME, fdp_mdate,FP_MDATE),
  PAKB(FDParmPtr, P_TIME, fdp_bdate,FP_BDATE),
  PKSB(FDParmPtr, P_BYTS, fdp_finfo,FP_FINFO),
  PKSB(FDParmPtr, P_OPTH, fdp_lname,FP_LNAME),
  PKSB(FDParmPtr, P_OPTH, fdp_sname,FP_SNAME),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.fp_parms.fp_fileno,FP_FILNO),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.fp_parms.fp_dflen,FP_DFLEN),
  PAKB(FDParmPtr,P_DWRD,fdp_parms.fp_parms.fp_rflen,FP_RFLEN),
  PAKB(FDParmPtr,P_WORD,fdp_prodos_ft,FP_PDOS),
  PAKB(FDParmPtr,P_DWRD,fdp_prodos_aux,FP_PDOS),
  PACKEND()
};


/* For FPEnumerate, etc. - client */
PackEntry ProtoDirAttr[] = {
  PAKB(FDParmPtr, P_WORD, fdp_attr,DP_ATTR),
  PAKB(FDParmPtr, P_DWRD, fdp_pdirid,DP_PDIR),
  PAKB(FDParmPtr, P_TIME, fdp_cdate,DP_CDATE),
  PAKB(FDParmPtr, P_TIME, fdp_mdate,DP_MDATE),
  PAKB(FDParmPtr, P_TIME, fdp_bdate,DP_BDATE),
  PKSB(FDParmPtr, P_BYTS, fdp_finfo,DP_FINFO),
  PKSB(FDParmPtr, P_OPTH, fdp_lname,DP_LNAME),
  PKSB(FDParmPtr, P_OPTH, fdp_sname,DP_SNAME),
  PAKB(FDParmPtr, P_DWRD, fdp_parms.dp_parms.dp_dirid,DP_DIRID),
  PAKB(FDParmPtr, P_WORD, fdp_parms.dp_parms.dp_nchild,DP_CHILD),
  PAKB(FDParmPtr, P_DWRD, fdp_parms.dp_parms.dp_ownerid,DP_CRTID),
  PAKB(FDParmPtr, P_DWRD, fdp_parms.dp_parms.dp_groupid,DP_GRPID),
  PAKB(FDParmPtr, P_DWRD, fdp_parms.dp_parms.dp_accright,DP_ACCES),
  PAKB(FDParmPtr, P_WORD,fdp_prodos_ft,DP_PDOS),
  PAKB(FDParmPtr, P_DWRD,fdp_prodos_aux,DP_PDOS),
  PACKEND()
};

/* For FPEnumerate, etc. - client */
PackEntry ProtoFileDirAttr[] = {
  PAKB(FDParmPtr, P_WORD, fdp_attr,DP_ATTR),
  PAKB(FDParmPtr, P_DWRD, fdp_pdirid,DP_PDIR),
  PAKB(FDParmPtr, P_TIME, fdp_cdate,DP_CDATE),
  PAKB(FDParmPtr, P_TIME, fdp_mdate,DP_MDATE),
  PAKB(FDParmPtr, P_TIME, fdp_bdate,DP_BDATE),
  PKSB(FDParmPtr, P_BYTS, fdp_finfo,DP_FINFO),
  PKSB(FDParmPtr, P_OPTH, fdp_lname,DP_LNAME),
  PKSB(FDParmPtr, P_OPTH, fdp_sname,DP_SNAME),
  PAKB(FDParmPtr, P_WORD,fdp_prodos_ft,DP_PDOS),
  PAKB(FDParmPtr, P_DWRD,fdp_prodos_aux,DP_PDOS),
  PACKEND()
};

PackEntry ProtoACP[] = {	/* FPAddComment */
  PACK(ACPPtr, P_BYTE, adc_cmd),
  PACK(ACPPtr, P_ZERO, adc_zero),
  PACK(ACPPtr, P_WORD, adc_dtrefnum),
  PACK(ACPPtr, P_DWRD, adc_dirid),
  PACK(ACPPtr, P_BYTE, adc_ptype),
  PAKS(ACPPtr, P_PATH, adc_path),
  PACKEVEN(),
/*  PACK(ACPPtr, P_BYTE, adc_clen), */
  PAKS(ACPPtr, P_PATH, adc_comment),
  PACKEND()
};

PackEntry ProtoBRL[] = {	/* FPByteRangeLock */
  PACK(BRLPPtr, P_BYTE, brl_cmd),
  PACK(BRLPPtr, P_BYTE, brl_flg),
  PACK(BRLPPtr, P_WORD, brl_refnum),
  PACK(BRLPPtr, P_DWRD, brl_offset),
  PACK(BRLPPtr, P_DWRD, brl_length),
  PACKEND()
};

PackEntry ProtoFFP[] = {
  PACK(FFkPPtr, P_BYTE, flf_cmd),
  PACK(FFkPPtr, P_ZERO, flf_zero),
  PACK(FFkPPtr, P_WORD, flf_refnum)
};

PackEntry ProtoGCP[] = {	/* FPGetComment */
  PACK(GCPPtr, P_BYTE, gcm_cmd),
  PACK(GCPPtr, P_ZERO, gcm_zero),
  PACK(GCPPtr, P_WORD, gcm_dtrefnum),
  PACK(GCPPtr, P_DWRD, gcm_dirid),
  PACK(GCPPtr, P_BYTE, gcm_ptype),
  PAKS(GCPPtr, P_PATH, gcm_path),
  PACKEND()
};

PackEntry ProtoGSPRP[] = {	/* GetSrvrParms Reply */
  PACK(GSPRPPtr, P_TIME, gspr_time),
  PACK(GSPRPPtr, P_BYTE, gspr_nvols),
  PACKEND()
};

PackEntry ProtoGSPRPvol[] = {
  PACK(VolParm *, P_BYTE, volp_flag),
  PAKS(VolParm *, P_PATH, volp_name),
  PACKEND()
};

PackEntry ProtoGVPRP[] = { 		/* GetVolParms Reply */
  PACK(GVPRPPtr,P_WORD,gvpr_bitmap),	/* bitmap specifies below items */
  PACK(GVPRPPtr,P_BMAP,gvpr_bitmap),	/* bitmap specifies below items */
  PAKB(GVPRPPtr,P_WORD,gvpr_attr,VP_ATTR), /* attributes word */
  PAKB(GVPRPPtr,P_WORD,gvpr_sig,VP_SIG), /* signature word */
  PAKB(GVPRPPtr,P_TIME,gvpr_cdate,VP_CDATE), /* creation date */
  PAKB(GVPRPPtr,P_TIME,gvpr_mdate,VP_MDATE), /* modification date */
  PAKB(GVPRPPtr,P_TIME,gvpr_bdate,VP_BDATE), /* last back date */
  PAKB(GVPRPPtr,P_WORD,gvpr_volid,VP_VOLID), /* volume id */
  PAKB(GVPRPPtr,P_DWRD,gvpr_free,VP_FREE), /* free bytes */
  PAKB(GVPRPPtr,P_DWRD,gvpr_size,VP_SIZE), /* size in bytes */
  PKSB(GVPRPPtr,P_OSTR,gvpr_name,VP_NAME), /* name of volume */
  PKSB(GVPRPPtr,P_BYTS,gvpr_efree,VP_EFREE), /* extended free bytes */
  PKSB(GVPRPPtr,P_BYTS,gvpr_esize,VP_ESIZE), /* extended total bytes */
  PACKEND()
};

PackEntry ProtoAIP[] = {	/* FPAddIcon */
  PACK(AIPPtr, P_BYTE, adi_cmd),
  PACK(AIPPtr, P_ZERO, adi_zero),
  PACK(AIPPtr, P_WORD, adi_dtref),
  PAKS(AIPPtr, P_BYTS, adi_fcreator),
  PAKS(AIPPtr, P_BYTS, adi_ftype),
  PACK(AIPPtr, P_BYTE, adi_icontype),
  PACK(AIPPtr, P_ZERO, adi_zero2),
  PACK(AIPPtr, P_DWRD, adi_icontag),
  PACK(AIPPtr, P_WORD, adi_iconsize),
  PACKEND()
};

PackEntry ProtoGAPR[] = {	/* GetAPPL reply */
  PACK(GARPPtr, P_WORD, gapr_bitmap),
  PACK(GARPPtr, P_DWRD, gapr_appltag),
  PACKEND()
};

PackEntry ProtoGIIR[] = {	/* GetIconInfo reply */
  PACK(GIIRPPtr, P_DWRD, giir_itag),
  PAKS(GIIRPPtr, P_BYTS, giir_ftype),
  PACK(GIIRPPtr, P_BYTE, giir_itype),
  PACK(GIIRPPtr, P_ZERO, giir_zero),
  PACK(GIIRPPtr, P_WORD, giir_size),
  PACKEND()
};

PackEntry ProtoRMC[] = {	/* FPRemoveComment */
  PACK(RCPPtr, P_BYTE, rmc_cmd),
  PACK(RCPPtr, P_ZERO, rmc_zero),
  PACK(RCPPtr, P_WORD, rmc_dtrefnum),
  PACK(RCPPtr, P_DWRD, rmc_dirid),
  PACK(RCPPtr, P_BYTE, rmc_ptype),
  PAKS(RCPPtr, P_PATH, rmc_path),
  PACKEND()
};

PackEntry ProtoSRP[] = {	/* GetSrvrInfo reply */
  PAKS(GSIRPPtr, P_OSTR, sr_machtype),
  PACK(GSIRPPtr, P_OPTR, sr_avo),
  PACK(GSIRPPtr, P_OPTR, sr_uamo),
  PACK(GSIRPPtr, P_OPTR, sr_vicono),
  PACK(GSIRPPtr, P_WORD, sr_flags),
  PAKS(GSIRPPtr, P_PATH, sr_servername),
  PACKEVEN(),
  PACK(GSIRPPtr, P_OPTR, sr_sigo),
  PACK(GSIRPPtr, P_OPTR, sr_naddro),
  PACKEND()
};

PackEntry ProtoCPP[] = {	/* ChangePassword */
  PACK(CPPtr, P_BYTE, cp_cmd),	/* command */
  PACK(CPPtr, P_ZERO, cp_zero),	/* always zero */
  PAKS(CPPtr, P_PSTR, cp_uam),	/* authentication method */
  PACK(CPPtr, P_EVEN, cp_pad),	/* pad to even */
  PAKS(CPPtr, P_PSTR, cp_user),	/* user name */
  PACK(CPPtr, P_EVEN, cp_pad),	/* pad to even */
  PAKS(CPPtr, P_BYTS, cp_oldpass), /* 8 bytes for old password */
  PAKS(CPPtr, P_BYTS, cp_newpass), /* 8 bytes for new password */
  PACKEND()
};

PackEntry ProtoGUIP[] = {	/* GetUserInfo */
  PACK(GUIPtr, P_BYTE, gui_cmd), /* command */
  PACK(GUIPtr, P_BYTE, gui_flag), /* flag word */
  PACK(GUIPtr, P_DWRD, gui_userid), /* user id */
  PACK(GUIPtr, P_WORD, gui_bitmap), /* bitmap of info to return */
  PACKEND()
};

PackEntry ProtoGUIRP[] = {	/* GetUserInfo Reply */
  PACK(GUIRPtr, P_BMAP, guir_bitmap), /* bitmap to return */
  PAKB(GUIRPtr, P_DWRD, guir_userid, UIP_USERID),
  PAKB(GUIRPtr, P_DWRD, guir_pgroup, UIP_PRIMARY_GID),
  PACKEND()
};

PackEntry ProtoExP[] = {	/* ExchangeFiles */
  PACK(EXPtr, P_BYTE, exc_cmd), /* command */
  PACK(EXPtr, P_ZERO, exc_zero), /* always zero */
  PACK(EXPtr, P_WORD, exc_volid), /* volume id */
  PACK(EXPtr, P_DWRD, exc_adirid), /* first directory id */
  PACK(EXPtr, P_DWRD, exc_bdirid), /* second directory id */
  PACK(EXPtr, P_BYTE, exc_aptype), /* first path type */
  PAKS(EXPtr, P_PATH, exc_apath), /* first path */
  PACK(EXPtr, P_BYTE, exc_bptype), /* second path type */
  PAKS(EXPtr, P_PATH, exc_bpath), /* second path */
  PACKEND()
};

PackEntry ProtoMsgP[] = {	/* GetSrvrMsg */
  PACK(SrvrMsgPtr, P_BYTE, msg_cmd), /* command */
  PACK(SrvrMsgPtr, P_ZERO, msg_zero), /* always zero */
  PACK(SrvrMsgPtr, P_WORD, msg_typ), /* message type */
  PACK(SrvrMsgPtr, P_WORD, msg_bitmap), /* bitmap */
  PACKEND()
};

PackEntry ProtoMsgRP[] = {	/* GetSrvrMsg Reply */
  PACK(SrvrMsgReplyPtr, P_WORD, msgr_typ), /* message type */
  PACK(SrvrMsgReplyPtr, P_WORD, msgr_bitmap), /* bitmap */
  PAKS(SrvrMsgReplyPtr, P_PSTR, msgr_data), /* message string */
  PACKEND()
};

PackEntry ProtoCreateID[] = {	/* CreateID */
  PACK(CreateIDPtr, P_BYTE, crid_cmd), 	/* command */
  PACK(CreateIDPtr, P_ZERO, crid_zero),	/* always zero */
  PACK(CreateIDPtr, P_WORD, crid_volid),/* volume id */
  PACK(CreateIDPtr, P_DWRD, crid_dirid),/* directory id */
  PACK(CreateIDPtr, P_BYTE, crid_ptype),/* path type */
  PAKS(CreateIDPtr, P_PATH, crid_path), /* path */
  PACKEND()
};

PackEntry ProtoDelID[] = {	/* DeleteID */
  PACK(DeleteIDPtr, P_BYTE, did_cmd), 	/* command */
  PACK(DeleteIDPtr, P_ZERO, did_zero), 	/* always zero */
  PACK(DeleteIDPtr, P_WORD, did_volid), /* volume id */
  PACK(DeleteIDPtr, P_DWRD, did_fileid), /* file id */
  PACKEND()
};

PackEntry ProtoRslvID[] = {	/* ResolveID */
  PACK(ResolveIDPtr, P_BYTE, rid_cmd), 		/* command */
  PACK(ResolveIDPtr, P_ZERO, rid_zero), 	/* always zero */
  PACK(ResolveIDPtr, P_WORD, rid_volid), 	/* volume id */
  PACK(ResolveIDPtr, P_DWRD, rid_fileid),	/* file id */
  PACK(ResolveIDPtr, P_WORD, rid_fbitmap),   	/* bitmap */ 
  PACKEND()
};
