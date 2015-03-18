@make(manual)
@device(ln03)
@section(FPAddAPPL)
@begin(example)
@tabdivide(8)
Call: FPAddAPPL(SessRefNum, aa, FPError)
Input:@\int SessRefNum;
Input:@\AddAPPLPkt *aa;
Output:@\dword *FPError;
Data Structures:
typedef struct {		/* FPAddAPPL */
  byte aap_cmd;			/* Command */
  byte aap_zero;		/* Always zero */
  word aap_volid;		/* volid */
  dword aap_dirid;		/* directory id */
  byte aap_fcreator[4];		/* file creator */
  dword aap_apptag;		/* application tag */
  byte aap_ptype;		/* path type */
  byte aap_path[MAXPATH];	/* path */
} AddAPPLPkt, *AAPPtr;
@end(Example)

@section(FPAddComment)
@begin(example)
@tabdivide(8)
Call:@\FPAddComment(SessRefNum, ac, FPError)
Input:@\int SessRefNum;
Input:@\AddCommentPkt *ac;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* AddComment */
  byte adc_cmd;			/* Command */
  byte adc_zero;		/* always zero */
  word adc_dtrefnum;		/* desk top refnum */
  dword adc_dirid;		/* directory id */
  byte adc_ptype;		/* path type */
  byte adc_path[MAXPATH];	/* path */
  byte adc_clen;		/* comment length */
  byte adc_comment[199];	/* comment string (PASCAL) */
} AddCommentPkt, *ACPPtr;
@end(example)

@section(FPAddIcon)
@begin(example)
@tabdivide(8)
Call:@\FPAddIcon(SessRefNum, adi, icon, iconlen, FPError)
Input:@\int SessRefNum;
Input:@\AddIconPkt *adi;
Input:@\byte *icon;
Input:@\int iconlen;
Output:@\dword *FPError;
Data Structures:

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
@end(example)

Add the icon bitmap pointed to by @i<icon> of length @i<iconlen> to
the desk top.


@section(FPByteRangeLock)
@begin(example)
@tabdivide(8)
Call:@\FPByteRangeLock(SessRefNum, brl, rangestart, FPError)
Input:@\int SessRefNum;
Input:@\ByteRangeLockPkt *brl;
Output:@\dword *rangestart;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* ByteRangeLock */
  byte brl_cmd;			/* command */
  byte brl_flg;			/* flags */
#define BRL_START	0x100	/* high bit */
#define BRL_UNLOCK	0x001	/* low bit */
  word brl_refnum;		/* file refnum */
  dword brl_offset;		/* offset to start lock */
  dword brl_length;		/* number of bytes to lock */
} ByteRangeLockPkt, *BRLPPtr;
@end(example)


@section(FPCloseDir)
@begin(example)
@tabdivide(8)
Call:@\FPCloseDir(SessRefNum, cd, FPError)
Input:@\int SessRefNum;
Input:@\CloseDirPkt *cd;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPCloseDir */
  byte cdr_cmd;			/* command */
  byte cdr_zero;		/* always zero */
  word cdr_volid;		/* volume id */
  dword cdr_dirid;		/* directory id */
} CloseDirPkt, *CDPPtr;
@end(example)


@section(FPCloseDT)
@begin(example)
@tabdivide(8)
Call:@\FPCloseDT(SessRefNum, cdt, FPError)
Input:@\int SessRefNum;
Input:@\CloseDTPkt *cdt;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPCloseDT */
  byte cdt_cmd;			/* command */
  byte cdt_zero;		/* zero byte */
  word cdt_dtrefnum;		/* desktop database refnum */
} CloseDTPkt, *CDTPPtr;
@end(example)

@section(FPCloseFork)
@begin(example)
@tabdivide(8)
Call:@\FPCloseFork(SRefNum, cfp, FPError)
Input:@\int SRefNum;
Input:@\CloseForkPkt *cfp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPCloseFork */
  byte cfk_cmd;			/* command */
  byte cfk_zero;		/* zero byte */
  word cfk_refnum;		/* open fork reference number */
} CloseForkPkt, *CFkPPtr;
@end(example)

@section(FPCloseVol)
@begin(example)
@tabdivide(8)
Call:@\FPCloseVol(SessRefNum, cv, FPError)
Input:@\int SessRefNum;
Input:@\CloseVolPkt *cv;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPCloseVol */
  byte cv_cmd;			/* command */
  byte cv_zero;			/* always zero */
  word cv_volid;		/* volume ID */
} CloseVolPkt, *CVPPtr;
@end(example)


@section(FPCopyFile)
@begin(example)
@tabdivide(8)
Call:@\FPCopyFile(SessRefNum, cf, FPError)
Input:@\int SessRefNum;
Input:@\CopyFilePkt *cf;
Output:@\dword *FPError;
Data Structures:

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
@end(example)


@section(FPCreateDir)
@begin(example)
@tabdivide(8)
Call:@\FPCreateDir(SessRefNum, cd, newdirid, FPError)
Input:@\int SessRefNum;
Input:@\CreateDirPkt *cd;
Output:@\dword *newdirid;
Input:@\dword *FPError;
Data Structures:

typedef struct {		/* FPCreateDir */
  byte crd_cmd;			/* command */
  byte crd_zero;		/* always zero */
  word crd_volid;		/* volume id */
  dword crd_dirid;		/* directory id */
  byte crd_ptype;		/* path type */
  byte crd_path[MAXPATH];	/* path */
} CreateDirPkt, *CRDPPtr;
@end(example)

The directory id of the new directory is returned through @i<newdirid>
if the call is successful.


@section(FPCreateFile)
@begin(example)
@tabdivide(8)
Call:@\FPCreateFile(SessRefNum, cf, FPError)
Input:@\int SessRefNum;
Input:@\CreateFilePkt *cf;
Output:@\dword *FPError;
Data Structures:
typedef struct {		/* FPCreateFile */
  byte crf_cmd;			/* command */
  byte crf_flg;			/* flags */
#define CRF_HARD 0x01		/*  hard create */
  word crf_volid;		/* volume id */
  sdword crf_dirid;		/* directory id */
  byte crf_ptype;		/* path name type */
  byte crf_path[MAXPATH];	/* path name */
} CreateFilePkt, *CFPPtr;
@end(example)


@section(FPDelete)
@begin(example)
@tabdivide(8)
Call:@\FPDelete(SessRefNum, dp, FPError)
Input:@\int SessRefNum;
Input:@\DeletePkt *dp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPDelete */
  byte del_cmd;			/* command */
  byte del_zero;		/* always zero */
  word del_volid;		/* volume id */
  sdword del_dirid;		/* directory id */
  byte del_ptype;		/* path type */
  byte del_path[MAXPATH];	/* path */
} DeletePkt, *DPPtr;
@end(example)


@section(FPEnumerate)
@begin(example)
@tabdivide(8)
Call:@\FPEnumerate(SessRefNum, ep, tbuf, tbufsiz, fdparms, 
@\fdparmslength, cnt, FPError)
Input:@\int SessRefNum;
Input:@\EnumeratePkt *ep;
Input:@\byte *tbuf;
Input:@\int tbufsiz;
Output:@\FileDirParm *fdparms;
Input:@\int fdparmslength;
Output:@\int *cnt;
Output:@\dword *FPError;
Data Structures:
typedef struct {		/* FPEnumerate */
  byte enu_cmd;			/* command */
  byte enu_zero;		/* always zero */
  word enu_volid;		/* volume id */
  dword enu_dirid;		/* directory id */
  word enu_fbitmap;		/* file bitmap */
  word enu_dbitmap;		/* directory bitmap */
  word enu_reqcnt;		/* request count */
  word enu_stidx;		/* start index */
  word enu_maxreply;		/* max reply size */
  byte enu_ptype;		/* path type */
  byte enu_path[MAXPATH];	/* path */
} EnumeratePkt, *EPPtr;
@end(example)

@i<tbuf> should be a buffer of size @i<tbuflen> that is used to hold
the reply from the remote side.  The enumerated items are filled in in
the array of length @i<fdparmslength> pointed to by @i<fdparms>.
Note: @i<tbuflen> should be at least as large as ep->enu_maxreply.


@section(FPFlush)
@begin(example)
@tabdivide(8)
Call:@\FPFlush(SessRefNum, fv, FPError)
Input:@\int SessRefNum;
Input:@\FlushPkt *fv;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPFlush */
  byte fls_cmd;			/* command */
  byte fls_zero;		/* always zero */
  word fls_volid;		/* volume ID */
} FlushPkt, *FPPtr;
@end(example)


@section(FPFlushFork)
@begin(example)
@tabdivide(8)
Call:@\FPFlushFork(SessRefNum, ff, FPError)
Input:@\int SessRefNum;
Input:@\FlushForkPkt *ff;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPFlushFork */
  byte flf_cmd;			/* command */
  byte flf_zero;		/* always zero */
  word flf_refnum;		/* open fork reference number */
} FlushForkPkt, *FFkPPtr;
@end(example)


@section(FPGetAPPL)
@begin(example)
@tabdivide(8)
Call:@\FPGetAPPL(SessRefNum, gap, gar, FPError)
Input:@\int SessRefNum;
Input:@\GetAPPLPkt *gap;
Output:@\GetAPPLReplyPkt *gar;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPGetAPPL */
  byte gap_cmd;
  byte gap_zero;
  word gap_dtrefnum;		/* desk top reference number */
  byte gap_fcreator[4];		/* creator type of the appl */
  word gap_applidx;		/* index of the APPL entry */
  word gap_bitmap;		/* bitmap of parms to return */
} GetAPPLPkt, *GAPPtr;

typedef struct {		/* FPGetAPPL Reply */
  word gapr_bitmap;		/* returned bitmap */
  dword gapr_appltag;		/* appl tag */
  FileDirParm fdp;		/* file parms */
} GetAPPLReplyPkt, *GARPPtr;
@end(example)


@section(FPGetComment)
@begin(example)
@tabdivide(8)
Call:@\FPGetComment(SessRefNum, gc, gcr, FPError)
Input:@\int SessRefNum;
Input:@\GetCommentPkt *gc;
Output:@\GetCommentReplyPkt *gcr;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPGetComment */
  byte gcm_cmd;			/* command */
  byte gcm_zero;
  word gcm_dtrefnum;		/* desktop reference number */
  dword gcm_dirid;		/* directory id */
  byte gcm_ptype;		/* path type */
  byte gcm_path[MAXPATH];	/* path */
} GetCommentPkt, *GCPPtr;

typedef struct {		/* FPGetComment Reply */
  byte gcmr_clen;		/* comment length */
  byte gcmr_ctxt[199];		/* comment text */
} GetCommentReplyPkt, *GCRPPtr;
@end(example)

Important notice: the comment text is a Pascal string, so the first
byte has the length of string and this length is independent of
gcmr_clen.


@section(FPGetFileDirParms)
@begin(example)
@tabdivide(8)
Call:@\FPGetFileDirParms(SessRefNum, gfdp, fdp, FPError)
Input:@\int SessRefNum;
Input:@\GetFileDirParmsPkt *gfdp;
Output:@\FileDirParm *fdp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPGetFileDirParms */
  byte gdp_cmd;			/* command */
  byte gdp_zero;		/* always zero */
  word gdp_volid;		/* volume ID */
  dword gdp_dirid;		/* directory id */
  word gdp_fbitmap;		/* file bitmap */
  word gdp_dbitmap;		/* directory bitmap */
  byte gdp_ptype;		/* path type */
  byte gdp_path[MAXPATH];	/* path */
} GetFileDirParmsPkt, *GFDPPPtr;
@end(example)


@section(FPGetForkParms)
@begin(example)
@tabdivide(8)
Call:@\FPGetForkParms(SessRefNum, gfp, fdp, FPError)
Inputs:@\int SessRefNum;
Input:@\GetForkParmsPkt *gfp;
Output:@\FileDirParm *fdp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPGetForkParms */
  byte gfp_cmd;			/* command */
  byte gfp_zero;		/* zero word */
  word gfp_refnum;		/* open fork reference number */
  word gfp_bitmap;		/* bitmap */
} GetForkParmsPkt, *GFkPPPtr;
@end(example)


@section(FPGetIcon)
@begin(example)
@tabdivide(8)
Call:@\FPGetIcon(SessRefNum, gi, icon, iconlen, FPError)
Input:@\int SessRefNum;
Input:@\GetIconPkt *gi;
Output:@\byte *icon;
Input:@\int iconlen;
Outputs:@\dword *FPError;
Data Structures:
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
@end(example)

The icon is returned in the array of length @i<iconlen> pointed to by
@i<icon>.


@section(FPGetIconInfo)
@begin(example)
@tabdivide(8)
Call:@\FPGetIconInfo(SessRefNum, gii, gicr, FPError)
Input:@\int SessRefNum;
Input:@\GetIconInfoPkt *gii;
Output:@\GetIconInfoReplyPkt *gicr;
Output:@\dword *FPError;
Data Structures:
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
@end(example)


@section(FPGetSrvrInfo)
@begin(example)
@tabdivide(8)
Call:@\FPGetSrvrInfo(addr, gsir)
Input:@\AddrBlock *addr;
Output:@\GetSrvrInfoReplyPkt *gsir;
Data Structures:

typedef struct {
  char sr_machtype[17];		/* machine name */
  byte *sr_avo;			/* offset to afp versions */
  byte *sr_uamo;		/* user access methods offset (ISTR) */
  char *sr_vicono;		/* offset to volume icon */
  word sr_flags;		/* flags */
  char sr_servername[33];	/* server name */
} GetSrvrInfoReplyPkt, *GSIRPPtr;
@end(example)

FPGetSrvrInfo uses SPGetStatus to get the specified information.


@section(FPGetSrvrParms)
@begin(example)
@tabdivide(8)
Call:@\FPGetSrvrParms(SessRefNum, sp, FPError)
Input:@\int SessRefNum;
Output:@\GetSrvrParmsReplyPkt *sp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* SrvrParm */
  byte volp_flag;		/* flags */
#define SRVRP_PASSWD 0x01	/* password is present */
  byte volp_name[27];		/* volume name */
} VolParm;

typedef struct {		/* FPGetSrvrParms Reply */
  dword gspr_time;		/* server time */
  byte gspr_nvols;		/* number of volume parms */
  VolParm gspr_volp[1];		/* one VolParm for each volume */
} GetSrvrParmsReplyPkt, *GSPRPPtr;
@end(example)


@section(FPGetVolParms)
@begin(example)
@tabdivide(8)
Call:@\FPGetVolParms(SessRefNum, gvp, gvpr, FPError)
Input:@\int SessRefNum;
Input:@\GetVolParmsPkt *gvp;
Output:@\GetVolParmsReplyPkt *gvpr;
Output:@\dword *FPError;
Data Structures:
typedef struct {		/* FPGetVolParms */
  byte gvp_cmd;			/* command */
  byte gvp_zero;		/* always zero */
  word gvp_volid;		/* volume id */
  word gvp_bitmap;		/* request bitmap */
} GetVolParmsPkt, *GVPPPtr;

typedef struct {		/* FPGetVolParms */
  byte gvpr_bitmap;		/* return bitmap */
  word gvpr_attr;		/* attributes */
  word gvpr_sig;		/* volume signature */
  sdword gvpr_cdate;		/* volume creation date */
  sdword gvpr_mdate;		/* volume modification date */
  sdword gvpr_bdate;		/* volume backup date */
  word gvpr_volid;		/* volume id */
  sdword gvpr_size;		/* size of volume in bytes */
  sdword gvpr_free;		/* free bytes on volume */
  byte gvpr_name[MAXVLEN];	/* advertised name */
} GetVolParmsReplyPkt, *GVPRPPtr;
@end(example)


@section(FPLogin)
@begin(example)
@tabdivide(8)
Call:@\FPLogin(SessRefNum, user, passwd, uam, FPError)
Input:@\int SessRefNum;
Input:@\byte *user;
Input:@\byte *passwd;
Input:@\int uam;
Output:@\dword *FPError;
Data Structures:

@end(example)

FPLogin is not yet finished.


@section(FPLogout)
@begin(example)
@tabdivide(8)
Call:@\FPLogout(SessRefNum, FPError)
Input:@\int SessRefNum;
Output:@\dword *FPError;

@end(example)


@section(FPMapID)
@begin(example)
@tabdivide(8)
Call:@\FPMapID(SessRefNum, mi,mapr, FPError)
Input:@\int SessRefNum;
Input:@\MapIDPkt *mi;
Output:@\MapIDReplyPkt *mapr;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPMapID */
  byte mpi_cmd;			/* MapID command */
  byte mpi_fcn;			/* function */
  sdword mpi_id;		/* ID to map */
} MapIDPkt, *MIPPtr;

typedef struct {		/* FPMapID Reply */
  byte mpir_name[MAXPSTR];
} MapIDReplyPkt, *MIRPPtr;
@end(example)


@section(FPMapName)
@begin(example)
@tabdivide(8)
Call:@\FPMapName(SessRefNum, mnp, id, FPError)
Input:@\int SessRefNum;
Input:@\MapNamePkt *mnp;
Output:@\dword *id;
Outputs:@\dword *FPError;
Data Structures:

typedef struct {		/* FPMapName */
  byte mpn_cmd;			/* command */
  byte mpn_fcn;			/* function */
  byte mpn_name[MAXPSTR];	/* name */
} MapNamePkt, *MNPPtr;
@end(example)

The user or group id is returned through @i<id>.

@section(FPMoveFile)
@begin(example)
@tabdivide(8)
Call:@\FPMoveFile(SessRefNum, mf, FPError)
Inputs:@\int SessRefNum;
Input:@\MovePkt *mf;
Outputs:@\dword *FPError;
Data Structures:

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
@end(example)


@section(FPOpenDir)
@begin(example)
@tabdivide(8)
Call:@\FPOpenDir(SessRefNum, od, retdirid, FPError)
Inputs:@\int SessRefNum;
Input:@\OpenDirPkt *od;
Output:@\dword *retdirid;
Outputs:@\dword *FPError;
Data Structures:

typedef struct {		/* FPOpenDir */
  byte odr_cmd;			/* command */
  byte odr_zero;		/* always zero */
  word odr_volid;		/* volume ID */
  dword odr_dirid;		/* directory ID */
  byte odr_ptype;		/* path type */
  byte odr_path[MAXPATH];	/* path */
} OpenDirPkt, *ODPPtr;
@end(example)

The directory id is returned through @i<retdirid>.


@section(FPOpenDT)
@begin(example)
@tabdivide(8)
Call:@\FPOpenDT(SessRefNum, odt, dtrefnum, FPError)
Input:@\int SessRefNum;
Input:@\OpenDTPkt *odt;
Output:@\word *dtrefnum;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPOpenDT */
  byte odt_cmd;			/* command */
  byte odt_zero;
  word odt_volid;		/* desktop volume id */
} OpenDTPkt, *ODTPPtr;
@end(example)

The desk top reference number is returned through @i<dtrefnum>.


@section(FPOpenFork)
@begin(example)
@tabdivide(8)
Call:@\FPOpenFork(SessRefNum, of, epar, refnum, FPError)
Input:@\int SessRefNum;
Input:@\OpenForkPkt *of;
Output:@\FileDirParm *epar;
Output:@\word *refnum;
Output:@\dword *FPError;
Data Structures:
typedef struct {		/* FPOpenFork */
  byte ofk_cmd;			/* command */
  byte ofk_rdflg;		/* resource/data flag */
#define OFK_RSRC 0x01		/*  resource fork */
  word ofk_volid;		/* volume id */
  sdword ofk_dirid;		/* directory id */
  word ofk_bitmap;		/* bitmap */
  word ofk_mode;		/* access mode */
  byte ofk_ptype;		/* path type */
  byte ofk_path[MAXPATH];	/* path name */
} OpenForkPkt, *OFkPPtr;
@end(example)

The open fork reference number is returned through @i<refnum>.


@section(FPOpenVol)
@begin(example)
@tabdivide(8)
Call:@\FPOpenVol(SessRefNum, ov, op, FPError)
Inputs:@\int SessRefNum;
Input:@\OpenVolPkt *ov;
Output:@\GetVolParmsReplyPkt *op;
Outputs:@\dword *FPError;
Data Structures:

typedef struct {		/* FPGetVolParms */
  byte gvp_cmd;			/* command */
  byte gvp_zero;		/* always zero */
  word gvp_volid;		/* volume id */
  word gvp_bitmap;		/* request bitmap */
} GetVolParmsPkt, *GVPPPtr;

typedef struct {		/* FPGetVolParms */
  byte gvpr_bitmap;		/* return bitmap */
  word gvpr_attr;		/* attributes */
  word gvpr_sig;		/* volume signature */
  sdword gvpr_cdate;		/* volume creation date */
  sdword gvpr_mdate;		/* volume modification date */
  sdword gvpr_bdate;		/* volume backup date */
  word gvpr_volid;		/* volume id */
  sdword gvpr_size;		/* size of volume in bytes */
  sdword gvpr_free;		/* free bytes on volume */
  byte gvpr_name[MAXVLEN];	/* advertised name */
} GetVolParmsReplyPkt, *GVPRPPtr;
@end(example)


@section(FPRead)
@begin(example)
@tabdivide(8)
Call:@\FPRead(SessRefNum, buf, buflen, rp, rlen, FPError)
Input:@\int SessRefNum;
Output:@\byte *buf;
Input:@\int buflen;
Input:@\ReadPkt *rp;
Output:@\int *rlen;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPRead */
  byte rdf_cmd;
  byte rdf_zero;
  word rdf_refnum;		/* fork reference number */
  dword rdf_offset;		/* offset for read */
  dword rdf_reqcnt;		/* request count */
  byte rdf_flag;
#define RDF_NEWLINE 0x01
  byte rdf_nlchar;		/* newline char */
} ReadPkt, *ReadPPtr;
@end(example)

The FPRead results are placed in the array pointed to by @i<buf>.  The
size of the buffer is buflen.  Number of bytes read is returned in
rlen.

@section(FPRemoveAPPL)
@begin(example)
@tabdivide(8)
Call:@\FPRemoveAPPL(SessRefNum, ra, FPError)
Input:@\int SessRefNum;
Input:@\RemoveAPPLPkt *ra;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPRemoveAPPL */
  byte rma_cmd;
  byte rma_zero;
  word rma_refnum;
  dword rma_dirid;
  byte rma_fcreator[4];
  byte rma_ptype;
  byte rma_path[MAXPATH];
} RemoveAPPLPkt, *RAPPtr;
@end(example)


@section(FPRemoveComment)
@begin(example)
@tabdivide(8)
Call:@\FPRemoveComment(SessRefNum, rc, FPError)
Input:@\int SessRefNum;
Input:@\RemoveCommentPkt *rc;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPRemoveComment */
  byte rmc_cmd;
  byte rmc_zero;
  word rmc_dtrefnum;		/* dest top ref num */
  dword rmc_dirid;
  byte rmc_ptype;
  byte rmc_path[MAXPATH];
} RemoveCommentPkt, *RCPPtr; 
@end(example)


@section(FPRename)
@begin(example)
@tabdivide(8)
Call:@\FPRename(SessRefNum, rn, FPError)
Input:@\int SessRefNum;
Input:@\RenamePkt *rn;
Output:@\dword *FPError;
Data Structures:

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
@end(example)


@section(FPSetDirParms)
@begin(example)
@tabdivide(8)
Call:@\FPSetDirParms(SessRefNum, sdp, fdp, FPError)
Input:@\int SessRefNum;
Input:@\SetDirParmsPkt *sdp;
Input:@\FileDirParm *fdp;
Outputs:@\dword *FPError;
Data Structures:

typedef struct {		/* FPSetDirParms */
  byte sdp_cmd;			/* command */
  byte sdp_zero;		/* always zero */
  word sdp_volid;		/* volume ID */
  dword sdp_dirid;		/* parent directory id */
  word sdp_bitmap;		/* bitmap */
  byte sdp_ptype;		/* path type */
  byte sdp_path[MAXPATH];	/* path */
} SetDirParmsPkt, *SDPPPtr;
@end(example)

The directory parameters in @i<fdp> are set according to the bitmap.


@section(FPSetFileParms)
@begin(example)
@tabdivide(8)
Call:@\FPSetFileParms(SessRefNum, sfp, fdp, FPError)
Input:@\int SessRefNum;
Input:@\SetFileParmsPkt *sfp;
Input:@\FileDirParm *fdp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPSetFileParms */
  byte sfp_cmd;			/* command */
  byte sfp_zero;		/* always zero */
  word sfp_volid;		/* volume id */
  dword sfp_dirid;		/* directory id */
  word sfp_bitmap;		/* set bitmap */
  byte sfp_ptype;		/* path type */
  byte sfp_path[MAXPATH];	/* path + file parameters to set */
} SetFileParmsPkt, *SFPPPtr;
@end(example)

The file parameters in @i<fdp> are set according to the bitmap.

@section(FPSetFileDirParms)
@begin(example)
@tabdivide(8)
Call:@\FPSetFileDirParms(SessRefNum, sfdp, fdp, FPError)
Inputs:@\int SessRefNum;
Input:@\SetFileDirParmsPkt *sfdp;
Input:@\FileDirParm *fdp;
Outputs:@\dword *FPError;
Data Structures:
typedef struct {		/* FPSetFileDirParms */
  byte scp_cmd;			/* set common parms command */
  byte scp_zero;
  word scp_volid;
  dword scp_dirid;
  word scp_bitmap;
  byte scp_ptype;
  byte scp_path[MAXPATH];
} SetFileDirParmsPkt, *SFDPPPtr;
@end(example)

The file or directory parameters in @i<fdp> are set according to the
bitmap.

@section(FPSetForkParms)
@begin(example)
@tabdivide(8)
Call:@\FPSetForkParms(SessRefNum, sfp, FPError)
Input:@\int SessRefNum;
Input:@\SetForkParmsPkt *sfp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPSetForkParms */
  byte sfkp_cmd;		/* command */
  byte sfkp_zero;		/* zero word */
  word sfkp_refnum;		/* reference number */
  word sfkp_bitmap;		/* bitmap */
  sdword sfkp_rflen;		/* resource fork length */
  sdword sfkp_dflen;		/* data fork length */
} SetForkParmsPkt, *SFkPPPtr;
@end(example)

@section(FPSetVolParms)
@begin(example)
@tabdivide(8)
Call:@\FPSetVolParms(SessRefNum, svp, FPError)
Input:@\SetVolParmsPkt *svp;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPSetVolParms */
  byte svp_cmd;			/* command */
  byte svp_zero;		/* always zero */
  word svp_volid;		/* volume id */
  word svp_bitmap;		/* set bitmap */
  dword svp_backdata;		/* backup data to set */
} SetVolParmsPkt, *SVPPPtr;
@end(example)


@section(FPWrite)
@begin(example)
@tabdivide(8)
Call:@\FPWrite(SessRefNum, wbuf, wlen, wp, actcnt, 
@\loff_written, FPError)
Input:@\int SessRefNum;
Input:@\char *wbuf;
Input:@\int wlen;
Input:@\WritePkt *wp;
Output:@\dword *actcnt;
Output:@\dword *loff_written;
Output:@\dword *FPError;
Data Structures:

typedef struct {		/* FPWrite */
  byte wrt_cmd;
  byte wrt_flag;
#define WRT_START 0x01
  word wrt_refnum;
  dword wrt_offset;
  dword wrt_reqcnt;
} WritePkt, *WPPtr;
@end(example)

The buffer pointed to by @i<wbuf> is written to the remote.  The
buffer length is specified by wlen.  The count of bytes actually
written is returned through @i<actcnt>.  The last offset written is
returned in loff_written.
