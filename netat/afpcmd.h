/*
 * $Author: djh $ $Date: 1996/04/25 01:24:27 $
 * $Header: /mac/src/cap60/netat/RCS/afpcmd.h,v 2.6 1996/04/25 01:24:27 djh Rel djh $
 * $Revision: 2.6 $
 *
 */

/*
 * abafpcmd.h - header file for AppleTalk Filing Protocol Command Packets
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March, 1987 	Schilit    Created
 *
 */

typedef struct {		/* FPAddAPPL */
  byte aap_cmd;			/* Command */
  byte aap_zero;		/* Always zero */
  word aap_dtrefnum;		/* desk top refnum */
  sdword aap_dirid;		/* directory id */
  byte aap_fcreator[4];		/* file creator */
  dword aap_apptag;		/* application tag */
  byte aap_ptype;		/* path type */
  byte aap_path[MAXPATH];	/* path */
} AddAPPLPkt, *AAPPtr;

typedef struct {		/* AddComment */
  byte adc_cmd;			/* Command */
  byte adc_zero;		/* always zero */
  word adc_dtrefnum;		/* desk top refnum */
  sdword adc_dirid;		/* directory id */
  byte adc_ptype;		/* path type */
  byte adc_path[MAXPATH];	/* path */
  byte adc_clen;		/* comment length */
  byte adc_comment[MAXCLEN];	/* comment string (PASCAL) */
} AddCommentPkt, *ACPPtr;

typedef struct {		/* AddIcon */
  byte adi_cmd;			/* Command */
  byte adi_zero;
  word adi_dtref;		/* Desktop refnum */
  byte adi_fcreator[4];		/* file creator */
  byte adi_ftype[4];		/* file type */
  byte adi_icontype;		/* icon type */
  byte adi_zero2;
  dword adi_icontag;		/* user icon tag */
  word adi_iconsize;		/* icon size */
} AddIconPkt, *AIPPtr;

typedef struct {		/* ByteRangeLock */
  byte brl_cmd;			/* command */
  byte brl_flg;			/* flags */
#define BRL_START	0x100	/* high bit */
#define BRL_UNLOCK	0x001	/* low bit */
  word brl_refnum;		/* file refnum */
  dword brl_offset;		/* offset to start lock */
  dword brl_length;		/* number of bytes to lock */
} ByteRangeLockPkt, *BRLPPtr;

typedef struct {		/* ByteRangeLock Reply */
  dword brlr_rangestart;	/* range locked */
} ByteRangeLockReplyPkt, *BRLRPPtr;

typedef struct {		/* FPCloseDir */
  byte cdr_cmd;			/* command */
  byte cdr_zero;		/* always zero */
  word cdr_volid;		/* volume id */
  sdword cdr_dirid;		/* directory id */
} CloseDirPkt, *CDPPtr;

typedef struct {		/* FPCloseDT */
  byte cdt_cmd;			/* command */
  byte cdt_zero;		/* zero byte */
  word cdt_dtrefnum;		/* desktop database refnum */
} CloseDTPkt, *CDTPPtr;

typedef struct {		/* FPCloseFork */
  byte cfk_cmd;			/* command */
  byte cfk_zero;		/* zero byte */
  word cfk_refnum;		/* open fork reference number */
} CloseForkPkt, *CFkPPtr;

typedef struct {		/* FPCloseVol */
  byte cv_cmd;			/* command */
  byte cv_zero;			/* always zero */
  word cv_volid;		/* volume ID */
} CloseVolPkt, *CVPPtr;

typedef struct {		/* FPCopyFile (optional) */
  byte cpf_cmd;			/* command */
  byte cpf_zero;		/* always zero */
  word cpf_svolid;		/* source volume id */
  sdword cpf_sdirid;		/* source directory id */
  word cpf_dvolid;		/* destination volume id */
  sdword cpf_ddirid;		/* destination directory id */
  byte cpf_sptype;		/* source path type */
  byte cpf_spath[MAXPATH];	/* source path */
  byte cpf_dptype;		/* destination path type */
  byte cpf_dpath[MAXPATH];	/* destination path */
  byte cpf_newtype;		/* new path type */
  byte cpf_newname[MAXPATH];	/* new name */
} CopyFilePkt, *CpFPPtr;

typedef struct {		/* FPCreateDir */
  byte crd_cmd;			/* command */
  byte crd_zero;		/* always zero */
  word crd_volid;		/* volume id */
  sdword crd_dirid;		/* directory id */
  byte crd_ptype;		/* path type */
  byte crd_path[MAXPATH];	/* path */
} CreateDirPkt, *CRDPPtr;

typedef struct {		/* FPCreateFile */
  byte crf_cmd;			/* command */
  byte crf_flg;			/* flags */
#define CRF_HARD 0x80		/* hard create */
  word crf_volid;		/* volume id */
  sdword crf_dirid;		/* directory id */
  byte crf_ptype;		/* path name type */
  byte crf_path[MAXPATH];	/* path name */
} CreateFilePkt, *CFPPtr;

typedef struct {		/* FPDelete */
  byte del_cmd;			/* command */
  byte del_zero;		/* always zero */
  word del_volid;		/* volume id */
  sdword del_dirid;		/* directory id */
  byte del_ptype;		/* path type */
  byte del_path[MAXPATH];	/* path */
} DeletePkt, *DPPtr;

typedef struct {		/* FPEnumerate */
  byte enu_cmd;			/* command */
  byte enu_zero;		/* always zero */
  word enu_volid;		/* volume id */
  sdword enu_dirid;		/* directory id */
  word enu_fbitmap;		/* file bitmap */
  word enu_dbitmap;		/* directory bitmap */
  word enu_reqcnt;		/* request count */
  word enu_stidx;		/* start index */
  word enu_maxreply;		/* max reply size */
  byte enu_ptype;		/* path type */
  byte enu_path[MAXPATH];	/* path */
} EnumeratePkt, *EPPtr;

typedef struct {
  byte enurfdp_len;
  byte enurfdp_flag;  
  byte enurfdp_parms[1];
} EnuReplyParms;

typedef struct {		/* FPEnumerate Reply */
  word enur_fbitmap;
  word enur_dbitmap;
  word enur_actcnt;
  EnuReplyParms enurfdp[1];
} EnumerateReplyPkt, *ERPPtr;

#define ENUR_ACTCNT_OFF 4	/* offset to enur_actcnt */

typedef struct {		/* FPFlush */
  byte fls_cmd;			/* command */
  byte fls_zero;		/* always zero */
  word fls_volid;		/* volume ID */
} FlushPkt, *FPPtr;

typedef struct {		/* FPFlushFork */
  byte flf_cmd;			/* command */
  byte flf_zero;		/* always zero */
  word flf_refnum;		/* open fork reference number */
} FlushForkPkt, *FFkPPtr;

typedef struct {		/* FPGetAPPL */
  byte gap_cmd;
  byte gap_zero;
  word gap_dtrefnum;		/* desk top reference number */
  byte gap_fcreator[4];		/* creator type of the appl to be returned */
  word gap_applidx;		/* index of the APPL entry to be retrieved */
  word gap_bitmap;		/* bitmap of parms to return */
} GetAPPLPkt, *GAPPtr;

typedef struct {		/* FPGetAPPL Reply */
  word gapr_bitmap;		/* returned bitmap */
  dword gapr_appltag;		/* appl tag */
  FileDirParm fdp;		/* file parms */
} GetAPPLReplyPkt, *GARPPtr;

typedef struct {		/* FPGetComment */
  byte gcm_cmd;			/* command */
  byte gcm_zero;
  word gcm_dtrefnum;		/* desktop reference number */
  sdword gcm_dirid;		/* directory id */
  byte gcm_ptype;		/* path type */
  byte gcm_path[MAXPATH];	/* path */
} GetCommentPkt, *GCPPtr;

typedef struct {		/* FPGetComment Reply */
  byte gcmr_clen;		/* comment length */
  byte gcmr_ctxt[MAXCLEN];	/* comment text */
} GetCommentReplyPkt, *GCRPPtr;

typedef struct {		/* FPGetFileDirParms */
  byte gdp_cmd;			/* command */
  byte gdp_zero;		/* always zero */
  word gdp_volid;		/* volume ID */
  sdword gdp_dirid;		/* directory id */
  word gdp_fbitmap;		/* file bitmap */
  word gdp_dbitmap;		/* directory bitmap */
  byte gdp_ptype;		/* path type */
  byte gdp_path[MAXPATH];	/* path */
} GetFileDirParmsPkt, *GFDPPPtr;

typedef struct {		/* FPGetFileDirParms Reply */
  word gdpr_fbitmap;		/* file bitmap */
  word gdpr_dbitmap;		/* directory bitmap */
  word gdpr_flags;
  byte gdpr_zero;
				/* followed by packed parms */
} GetFileDirParmsReplyPkt, *GFDPRPPtr;

typedef struct {		/* FPGetForkParms */
  byte gfp_cmd;			/* command */
  byte gfp_zero;		/* zero word */
  word gfp_refnum;		/* open fork reference number */
  word gfp_bitmap;		/* bitmap */
} GetForkParmsPkt, *GFkPPPtr;

typedef struct {		/* FPGetForkParms Reply */
  byte gfpr_bitmap;		/* bitmap */
				/* followed by packed fork parms */
} GetForkParmsReplyPkt, *GFkPRPPtr;


typedef struct {		/* FPGetIcon */
  byte gic_cmd;
  byte gic_zero;
  word gic_dtrefnum;		/* desktop ref num */
  byte gic_fcreator[4];		/* file creator */
  byte gic_ftype[4];		/* file type */
  byte gic_itype;		/* icon type */
  byte gic_zero2;
  word gic_length;
} GetIconPkt, *GIPPtr;

typedef struct {		/* FPGetIconInfo */
  byte gii_cmd;
  byte gii_zero;
  word gii_dtrefnum;
  byte gii_fcreator[4];
  word gii_iidx;		/* icon index */
} GetIconInfoPkt, *GIIPPtr;

typedef struct {		/* FPGetIconInfo Reply */
  dword giir_itag;		/* icon tag */
  byte giir_ftype[4];		/* file type */
  byte giir_itype;		/* icon type */
  byte giir_zero;
  word giir_size;		/* size of icon */
} GetIconInfoReplyPkt, *GIIRPPtr;


typedef struct {		/* FPGetSrvrInfo */
  byte gsi_cmd;			/* GetSrvrInfo command */
} GetSrvrInfoPkt, *GSIPPtr;

typedef struct {
  char sr_machtype[17];		/* machine name */
  byte *sr_avo;			/* offset to afp versions */
  byte *sr_uamo;		/* user access methods offset (ISTR) */
  char *sr_vicono;		/* offset to volume icon */
  word sr_flags;		/* flags */
#define SupportsFPCopyFile 0x01	/* can do a local server copyfile */
#define SupportsChgPwd     0x02	/* AFP2.0: can do change password */
#define DontAllowSavePwd   0x04	/* AFP2.1: user can't save password */
#define SupportsServerMsgs 0x08	/* AFP2.1: can send server messages */
#define SupportsServerSig  0x10	/* AFP2.2: can supply unique signature */
#define SupportsTCPIP      0x20	/* AFP2.2: AFP commands via TCP/IP stream */
#define SupportsSrvrNotif  0x40	/* AFP2.2: server to client messages */
  byte sr_servername[33];	/* server name */
  byte *sr_sigo;		/* AFP2.2: offset to signature */
  byte *sr_naddro;		/* AFP2.2: offset to network address count */
} GetSrvrInfoReplyPkt, *GSIRPPtr;

typedef struct {		/* FPGetSrvrParms */
  byte gsp_cmd;			/* command */
} GetSrvrParmsPkt, *GSPPPtr;

typedef struct {		/* SrvrParm */
  byte volp_flag;		/* flags */
#define SRVRP_PASSWD 0x80	/* password is present */
#define SRVRP_CONFIG 0x01	/* user configuration for prodos */
  byte volp_name[MAXVLEN];	/* volume name */
} VolParm;


typedef struct {		/* FPGetSrvrParms Reply */
/*  word gspr_volid;		/* volume id */
  dword gspr_time;		/* server time */
  byte gspr_nvols;		/* number of volume parms returned */
  VolParm gspr_volp[1];		/* one VolParm for each volume */
} GetSrvrParmsReplyPkt, *GSPRPPtr;

typedef struct {		/* FPGetVolParms */
  byte gvp_cmd;			/* command */
  byte gvp_zero;		/* always zero */
  word gvp_volid;		/* volume id */
  word gvp_bitmap;		/* request bitmap */
} GetVolParmsPkt, *GVPPPtr;

typedef struct {		/* FPGetVolParms */
  word gvpr_bitmap;		/* return bitmap */
  word gvpr_attr;		/* attributes */
  word gvpr_sig;		/* volume signature */
  sdword gvpr_cdate;		/* volume creation date */
  sdword gvpr_mdate;		/* volume modification date */
  sdword gvpr_bdate;		/* volume backup date */
  word gvpr_volid;		/* volume id */
  sdword gvpr_size;		/* size of volume in bytes */
  sdword gvpr_free;		/* free bytes on volume */
  byte gvpr_name[MAXVLEN];	/* advertised name */
  byte gvpr_esize[8];		/* extended volume size */
  byte gvpr_efree[8];		/* extended bytes free */
} GetVolParmsReplyPkt, *GVPRPPtr;


#define UAM_UNKNOWN -1		/* internal code */
#define UAM_ANON 0
#define UAM_CLEAR 1
#define UAM_RANDNUM 2
#define UAM_2WAYRAND 3

#define UAMP_USER 1		/* need user name */
#define UAMP_PASS 2		/* need password */
#define UAMP_RAND 4		/* need randnum */
#define UAMP_ENCR 8		/* need encrypted passwd */
#define UAMP_ZERO 0x10		/* need zero between user and passwd */
#define UAMP_INUM 0x20		/* id number for fplogin exchange */
#define UAMP_TWAY 0x40		/* need second random dword */

typedef struct {		/* FPLogin */
  byte log_cmd;			/* Login command */
  byte log_ver[MAXPSTR];	/* version string */
  byte log_uam[MAXPSTR];	/* UAM method */
  byte log_auth[MAXPSTR];	/* authorization info (optional) */
  byte log_passwd[MAXPLEN];	/* 8 bytes for password (plaintext) */
  byte log_user[MAXPSTR];	/* user name */
  byte log_flag;		/* flags for authinfo */
  byte log_zero;		/* used for zero's */
} LoginPkt, *LPPtr;

typedef struct {		/* FPLogin Reply */
  byte logr_flag;		/* flags... */
  word logr_idnum;		/* id number (under some uams) */
  byte logr_randnum[8];		/* 64bit random number for RANDNUM uam */
} LoginReplyPkt, *LRPPtr;

typedef struct {		/* FPLoginCont */
  byte lgc_cmd;			/* command */
  byte lgc_zero;		/* is this here? */
  word lgc_idno;		/* ID number */
  byte lgc_encrypted[8];	/* encrypted version of random number */
				/* for randnum exchange uam (64 bits) */
  byte lgc_wsencrypt[8];	/* encrypted version of workstation id */
				/* for 2-Way rand exchange uam (64 bits) */
  byte lgc_flags;		/* really just the uam */
} LoginContPkt, *LCPPtr;

typedef struct {		/* FPLogout */
  byte lgo_cmd;
} LogoutPkt, *LOPPtr;

typedef struct {		/* FPMapID */
  byte mpi_cmd;			/* MapID command */
  byte mpi_fcn;			/* function */
  sdword mpi_id;		/* ID to map */
} MapIDPkt, *MIPPtr;

typedef struct {		/* FPMapID Reply */
  byte mpir_name[MAXPSTR];
} MapIDReplyPkt, *MIRPPtr;

typedef struct {		/* FPMapName */
  byte mpn_cmd;			/* command */
  byte mpn_fcn;			/* function */
  byte mpn_name[MAXPSTR];	/* name */
} MapNamePkt, *MNPPtr;

typedef struct {		/* FPMapName Reply */
  word mpnr_id;			/* returned id */
} MapNameReplyPkt, *MNRPPtr;

typedef struct {		/* FPMove */
  byte mov_cmd;			/* command */
  byte mov_zero;		/* always zero */
  word mov_volid;		/* volume id */
  sdword mov_sdirid;		/* source directory id */
  sdword mov_ddirid;		/* destination directory id */
  byte mov_sptype;		/* source path type */
  byte mov_spath[MAXPATH];	/* source path */
  byte mov_dptype;		/* destination path type */
  byte mov_dpath[MAXPATH];	/* destination path */
  byte mov_newtype;		/* new type */
  byte mov_newname[MAXPATH];	/* new name */
} MovePkt, *MPPtr;

typedef struct {		/* FPOpenDir */
  byte odr_cmd;			/* command */
  byte odr_zero;		/* always zero */
  word odr_volid;		/* volume ID */
  sdword odr_dirid;		/* directory ID */
  byte odr_ptype;		/* path type */
  byte odr_path[MAXPATH];	/* path */
} OpenDirPkt, *ODPPtr;

typedef struct {		/* FPOpenDT */
  byte odt_cmd;			/* command */
  byte odt_zero;
  word odt_volid;		/* desktop volume id */
} OpenDTPkt, *ODTPPtr;

typedef struct {		/* FPOpenDT Reply */
  word odtr_dtrefnum;		/* desktop reference number */
} OpenDTReplyPkt, *ODTRPPtr;


typedef struct {		/* FPOpenFork */
  byte ofk_cmd;			/* command */
  byte ofk_rdflg;		/* resource/data flag */
#define OFK_RSRC 0x80		/*  resource fork */
  word ofk_volid;		/* volume id */
  sdword ofk_dirid;		/* directory id */
  word ofk_bitmap;		/* bitmap */
  word ofk_mode;		/* access mode */
#define OFK_MRD 0x01			/* read mode */
#define OFK_MWR 0x02			/* write mode */
#define OFK_MRDX 0x10			/* exclusive read mode */
#define OFK_MWRX 0x20			/* exclusive write mode */
  byte ofk_ptype;		/* path type */
  byte ofk_path[MAXPATH];	/* path name */
} OpenForkPkt, *OFkPPtr;

typedef struct {		/* FPOpenFork Reply */
  word ofkr_bitmap;		/* bitmap */
  word ofkr_refnum;		/* opened fork reference number */
				/* File parameters follow */
} OpenForkReplyPkt, *OFkRPPtr;

typedef struct {		/* FPOpenVol */
  byte ovl_cmd;			/* command */
  byte ovl_zero;		/* always zero */
  word ovl_bitmap;		/* request bitmap */
  byte ovl_name[MAXVNAME];	/* volume name packed... */
				/* possible null byte */
  byte ovl_pass[MAXPASSWD];	/* password (optional) */
} OpenVolPkt, *OVPPtr;

typedef struct {		/* FPOpenVol Reply */
  word ovlr_bitmap;		/* request bitmap */
				/* volume parameters follow */
} OpenVolReplyPkt, *OVRPPtr;

typedef struct {		/* FPRead */
  byte rdf_cmd;
  byte rdf_zero;
  word rdf_refnum;		/* fork reference number */
  sdword rdf_offset;		/* offset for read */
  sdword rdf_reqcnt;		/* request count */
  byte rdf_flag;
#define RDF_NEWLINE 0xFF
  byte rdf_nlchar;		/* newline char */
} ReadPkt, *ReadPPtr;

typedef struct {		/* FPRead Reply */
  byte rdfr_data[1];		/* data */
} ReadReplyPkt, *RRPPtr;

typedef struct {		/* FPRemoveAPPL */
  byte rma_cmd;
  byte rma_zero;
  word rma_refnum;
  sdword rma_dirid;
  byte rma_fcreator[4];
  byte rma_ptype;
  byte rma_path[MAXPATH];
} RemoveAPPLPkt, *RAPPtr;

typedef struct {		/* FPRemoveComment */
  byte rmc_cmd;
  byte rmc_zero;
  word rmc_dtrefnum;		/* dest top ref num */
  sdword rmc_dirid;
  byte rmc_ptype;
  byte rmc_path[MAXPATH];
} RemoveCommentPkt, *RCPPtr; 

typedef struct {		/* FPRename */
  byte ren_cmd;			/* command */
  byte ren_zero;		/* always zero */
  word ren_volid;		/* volume id */
  sdword ren_dirid;		/* directory id */
  byte ren_ptype;		/* path type */
  byte ren_path[MAXPATH];	/* path name */
  byte ren_ntype;		/* new type */
  byte ren_npath[MAXPATH];	/* new path */
} RenamePkt, *RPPtr;


typedef struct {		/* FPSetDirParms */
  byte sdp_cmd;			/* command */
  byte sdp_zero;		/* always zero */
  word sdp_volid;		/* volume ID */
  sdword sdp_dirid;		/* parent directory id */
  word sdp_bitmap;		/* bitmap */
  byte sdp_ptype;		/* path type */
  byte sdp_path[MAXPATH];	/* path */
				/* Possible null byte */
				/* Directory Parameters: */
  FileDirParm fdp;
} SetDirParmsPkt, *SDPPPtr;

typedef struct {		/* FPSetFileParms */
  byte sfp_cmd;			/* command */
  byte sfp_zero;		/* always zero */
  word sfp_volid;		/* volume id */
  sdword sfp_dirid;		/* directory id */
  word sfp_bitmap;		/* set bitmap */
  byte sfp_ptype;		/* path type */
  byte sfp_path[MAXPATH];	/* path + file parameters to set */
				/* possible null byte */
				/* File Parameters: */
  FileDirParm fdp;
} SetFileParmsPkt, *SFPPPtr;

typedef struct {		/* FPSetFileDirParms */
  byte scp_cmd;			/* set common parms command */
  byte scp_zero;
  word scp_volid;
  sdword scp_dirid;
  word scp_bitmap;
  byte scp_ptype;
  byte scp_path[MAXPATH];
				/* possible null byte */
				/* Common Parameters follow: */
  FileDirParm fdp;
} SetFileDirParmsPkt, *SFDPPPtr;

typedef struct {		/* FPSetForkParms */
  byte sfkp_cmd;		/* command */
  byte sfkp_zero;		/* zero word */
  word sfkp_refnum;		/* reference number */
  word sfkp_bitmap;		/* bitmap */
  sdword sfkp_rflen;		/* resource fork length */
  sdword sfkp_dflen;		/* data fork length */
} SetForkParmsPkt, *SFkPPPtr;

typedef struct {		/* FPSetVolParms */
  byte svp_cmd;			/* command */
  byte svp_zero;		/* always zero */
  word svp_volid;		/* volume id */
  word svp_bitmap;		/* set bitmap */
  dword svp_backdata;		/* backup data to set */
} SetVolParmsPkt, *SVPPPtr;

typedef struct {		/* FPWrite */
  byte wrt_cmd;
  byte wrt_flag;
#define WRT_START 0x01
  word wrt_refnum;
  dword wrt_offset;
  dword wrt_reqcnt;
} WritePkt, *WPPtr;

typedef struct {		/* FPWrite Reply */
  dword wrtr_lwrt;		/* last written */
} WriteReplyPkt, *WRPPtr;

/* New as of AFP 2.0 */
typedef struct {
  byte cp_cmd;			/* command */
  byte cp_zero;			/* always zero */
  byte cp_uam[MAXPSTR];		/* authentication method */
  byte cp_user[MAXPSTR];	/* user name */
  byte cp_oldpass[MAXPLEN];	/* 8 bytes for old password */
  byte cp_newpass[MAXPLEN];	/* 8 bytes for new password */
  byte cp_pad;			/* dummy for padding */
} ChgPasswdPkt, *CPPtr;

typedef struct {
  byte cpr_cmd;
  byte cpr_zero;
  byte cpr_uam[MAXPSTR];	/* authentication method */  
  byte cpr_user[MAXPSTR];	/* user name */
  byte cpr_newpass[MAXPLEN];	/* 8 bytes for new password */
  byte cpr_pad;			/* dummy */
} ChgPasswdReplyPkt, *CPRPtr;

typedef struct {
  byte gui_cmd;			/* command */
  byte gui_flag;		/* flag word */
#define GUI_THIS_USER 0x1	/* set implies current user, o.w. use userid */
  dword gui_userid;		/* user id */
  word gui_bitmap;		/* bitmap of info to return */
} GetUserInfoPkt, *GUIPtr;

typedef struct {
  word guir_bitmap;		/* bitmap to return */
  dword guir_userid;		/* user id */
  dword guir_pgroup;		/* primary group */
} GetUserInfoReplyPkt, *GUIRPtr;

typedef struct {		/* FPExchangeFiles */
  byte exc_cmd;			/* command */
  byte exc_zero;		/* always zero */
  word exc_volid;		/* volume id */
  sdword exc_adirid;		/* first directory id */
  sdword exc_bdirid;		/* second directory id */
  byte exc_aptype;		/* first path type */
  byte exc_apath[MAXPATH];	/* first path */
  byte exc_bptype;		/* second path type */
  byte exc_bpath[MAXPATH];	/* second path */
} ExchPkt, *EXPtr;

#define SRVRMSGLEN 200

typedef struct {		/* FPGetSrvrMsg */
  byte msg_cmd;			/* command */
  byte msg_zero;		/* always zero */
  word msg_typ;			/* message type */
  word msg_bitmap;		/* bitmap */
} SrvrMsgPkt, *SrvrMsgPtr;

typedef struct {
  word msgr_typ;		/* message type */
  word msgr_bitmap;		/* bitmap */
  byte msgr_data[SRVRMSGLEN];	/* message string */
} SrvrMsgReplyPkt, *SrvrMsgReplyPtr;

typedef struct {		/* FPCreateID */
  byte crid_cmd;		/* command */
  byte crid_zero;		/* always zero */
  word crid_volid;		/* volume ID */
  sdword crid_dirid;		/* directory ID */
  byte crid_ptype;		/* path type */
  byte crid_path[MAXPATH];	/* path */
} CreateIDPkt, *CreateIDPtr;

typedef struct {		/* FPDeleteID */
  byte did_cmd;			/* command */
  byte did_zero;		/* always zero */
  word did_volid;		/* volume ID */
  sdword did_fileid;		/* file ID */
} DeleteIDPkt, *DeleteIDPtr;

typedef struct {		/* FPResolveID */
  byte rid_cmd;			/* command */
  byte rid_zero;		/* always zero */
  word rid_volid;		/* volume ID */
  sdword rid_fileid;		/* file id */
  word rid_fbitmap;		/* result bitmap */
} ResolveIDPkt, *ResolveIDPtr;

typedef struct {		/* FPResolveID Reply */
  word ridr_fbitmap;		/* result bitmap */
				/* followed by packed parms */
} ResolveIDReplyPkt, *ResolveIDRPtr;


typedef struct {
  int pe_typ;				/* type of data */
  int pe_off;				/* offset in host structure */
  int pe_siz;				/* size or max size */
  int pe_bit;				/* bit if specified by bitmap */
} PackEntry;

#define P_END -1		/* end of structure */
#define P_WORD 0		/* 2 byte word */
#define P_BYTE 1		/* 1 byte */
#define P_DWRD 2		/* 4 byte double word */
#define P_BYTS 3		/* arbitrary length bytes set by struct len */
#define P_PSTR 4		/* pascal string/c string */
#define P_BMAP 5		/* bitmap (not sent to net) */
#define P_OSTR 6		/* offset c/pascal string */
#define P_PATH 7		/* preserved pascal string */
#define P_OPTR 8		/* offset to object pointer */
#define P_OPTH 9		/* offset to preserved pascal string */
#define P_EVEN 10		/* move to even boundary (+0,+1) */
#define P_ZERO 11		/* mark entry as zero */
#define P_TIME 12		/* entry points to time (dword) */

/* Pack(pointer, type of data, scalar result field) */
#define PACK(pt,t,s)	{t,(int) &((pt) 0)->s,sizeof(((pt) 0)->s),0}
/* Paks(pointer, type of data, array result field) */
#ifdef ADDRINPACK
#define PAKS(pt,t,s)	{t,(int) &((pt) 0)->s,sizeof(((pt) 0)->s),0}
#else ADDRINPACK
#define PAKS(pt,t,s)   	{t,(int) ((pt) 0)->s,sizeof(((pt) 0)->s),0}
#endif ADDRINPACK
/* Both require that a "type of data" field type P_BMAP be set first */
/* pakb(pointer, type of data, scalar result field, bit of bitmap */
#define PAKB(pt,t,s,b)	{t,(int) &((pt) 0)->s,sizeof(((pt) 0)->s),b}
/* pksb(pointer, type of data, array result field, bit of bitmap */
#ifdef ADDRINPACK
#define PKSB(pt,t,s,b)	{t,(int) &((pt) 0)->s,sizeof(((pt) 0)->s),b}
#else ADDRINPACK
#define PKSB(pt,t,s,b)	{t,(int) ((pt) 0)->s,sizeof(((pt) 0)->s),b}
#endif ADDRINPACK

/* Pack even - to mark that we should be on even boundary */
#define PACKEVEN() {P_EVEN, 0, 0, 0}

/* PACKEND() for ending the packet */
#define PACKEND() {P_END,0,0,0}

typedef struct {		/* type for packing offset objects */
  int optr_len;
  byte *optr_loc;
} OPTRType;
