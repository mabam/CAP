/*
 * $Author: djh $ $Date: 1996/04/25 03:15:24 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpntoh.h,v 2.5 1996/04/25 03:15:24 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * afpntoh.h - Server Net to Host Unpacking.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University 
 * in the City of New York.
 *
 * Edit History:
 *
 *  Sun Apr  5  Schilit	   Created, based on afpcmd.c
 *
 */

extern PackEntry ProtoBRL[];		/* 1 ByteRangeLock */
#define PsByteRangeLock ProtoBRL
extern PackEntry ProtoCVP[];		/* 2 CloseVol */
#define PsCloseVol  ProtoCVP
extern PackEntry ProtoCDP[];		/* 3 CloseDir */
#define PsCloseDir  ProtoCDP
extern PackEntry ProtoCFkP[];		/* 4 CloseFork */
#define PsCloseFork  ProtoCFkP
extern PackEntry ProtoCpFP[];		/* 5 CopyFile */
#define PsCopyFile  ProtoCpFP
extern PackEntry ProtoCRDP[];		/* 6 CreateDir */
#define PsCreateDir  ProtoCRDP
extern PackEntry ProtoCFP[];		/* 7 CreateFile */
#define PsCreateFile  ProtoCFP
extern PackEntry ProtoDFP[];		/* 8 Delete */
#define PsDelete  ProtoDFP
extern PackEntry ProtoEP[];		/* 9 Enumerate */
#define PsEnumerate  ProtoEP
extern PackEntry ProtoFVP[];		/* 10 Flush */
#define PsFlush  ProtoFVP
					/* 11 Flush a fork */
extern PackEntry ProtoGFkPP[];		/* 14 GetForkParms */
#define PsGetForkParms  ProtoGFkPP
					/* 15 GetSrvrInfo */
					/* 16 GetSrvrParms */
extern PackEntry ProtoGVPP[];		/* 17 GetVolParms */
#define PsGetVolParms  ProtoGVPP
extern PackEntry ProtoLP[];		/* 18 Login */
#define PsLogin  ProtoLP
extern PackEntry ProtoLRP[];	/* login reply */
#define PsLoginReply ProtoLRP
extern PackEntry ProtoLCP[];		/* 19 LoginCont */
#define PsLoginCont  ProtoLCP
extern PackEntry ProtoLCR[];	/* loginCont reply */
#define PsLoginContR ProtoLCR
					/* 20 Logout */
extern PackEntry ProtoMIP[];		/* 21 MapID */
#define PsMapID  ProtoMIP
extern PackEntry ProtoMNP[];		/* 22 MapName */
#define PsMapName  ProtoMNP
extern PackEntry ProtoMFP[];		/* 23 Move */
#define PsMove  ProtoMFP
extern PackEntry ProtoOVP[];		/* 24 OpenVol */
#define PsOpenVol  ProtoOVP
extern PackEntry ProtoODP[];		/* 25 OpenDir */
#define PsOpenDir  ProtoODP
extern PackEntry ProtoOFkP[];		/* 26 OpenFork */
#define PsOpenFork  ProtoOFkP
extern PackEntry ProtoRP[];		/* 27 Read */
#define PsRead  ProtoRP
extern PackEntry ProtoRFP[];		/* 28 Rename */
#define PsRename  ProtoRFP
extern PackEntry ProtoSDPP[];		/* 29 SetDirParms */
#define PsSetDirParms  ProtoSDPP
extern PackEntry ProtoSFPP[];		/* 30 SetFileParms */
#define PsSetFileParms  ProtoSFPP
extern PackEntry ProtoSFkPP[];		/* 31 SetForkParms */
#define PsSetForkParms  ProtoSFkPP
extern PackEntry ProtoSVPP[];		/* 32 SetVolParms */
#define PsSetVolParms  ProtoSVPP
extern PackEntry ProtoWP[];		/* 33 Write */
#define PsWrite  ProtoWP
extern PackEntry ProtoGFDPP[];		/* 34 GetFileDirParms */
#define PsGetFileDirParms  ProtoGFDPP
extern PackEntry ProtoSFDPP[];		/* 35 SetFileDirParms */
#define PsSetFileDirParms  ProtoSFDPP
extern PackEntry ProtoMsgP[];		/* 38 GetSrvrMsg */
#define PsGetSrvrMsg ProtoMsgP
extern PackEntry ProtoCreateID[]; 	/* 39 CreateID */
#define PsCreateID ProtoCreateID
extern PackEntry ProtoDelID[]; 		/* 40 DeleteID */
#define PsDelID ProtoDelID
extern PackEntry ProtoRslvID[]; 	/* 41 ResolveID */
#define PsRslvID ProtoRslvID
extern PackEntry ProtoExP[];		/* 42 ExchangeFiles */
#define PsExchange ProtoExP
extern PackEntry ProtoODT[];		/* 48 OpenDT */
#define PsOpenDT  ProtoODT
extern PackEntry ProtoCDT[];		/* 49 CloseDT */
#define PsCloseDT  ProtoCDT
extern PackEntry ProtoGI[];		/* 51 GetIcon */
#define PsGetIcon  ProtoGI
extern PackEntry ProtoGII[];		/* 52 GetIconInfo */
#define PsGetIconInfo  ProtoGII
extern PackEntry ProtoAAP[];		/* 53 AddAPPL */
#define PsAddAPPL  ProtoAAP
extern PackEntry ProtoRMA[];		/* 54 RmvAPPL */
#define PsRmvAPPL  ProtoRMA
extern PackEntry ProtoGAP[];		/* 55 GetAPPL */
#define PsGetAPPL  ProtoGAP
extern PackEntry ProtoACP[];		/* 56 AddComment */
#define PsAddComment  ProtoACP
extern PackEntry ProtoRMC[];		/* 57 RmvComment */
#define PsRmvComment  ProtoRMC
extern PackEntry ProtoGCP[];		/* 58 GetComment */
#define PsGetComment  ProtoGCP
extern PackEntry ProtoAIP[];		/* 192 AddIcon */
#define PsAddIcon  ProtoAIP

#define PsChangePassword ProtoCPP
extern PackEntry ProtoCPP[];
#define PsGetUserInfo ProtoGUIP
extern PackEntry ProtoGUIP[];
#define PsGetUserInfoReply ProtoGUIRP
extern PackEntry ProtoGUIRP[];
#define PsGetIconInfoReply ProtoGIIR
extern PackEntry ProtoGIIR[];
#define PsGetSrvrMsgReply ProtoMsgRP
extern PackEntry ProtoMsgRP[];

extern PackEntry ProtoAuthInfo[];
extern PackEntry DirParmPackR[];
extern PackEntry FilePackR[];
extern PackEntry ProtoFileAttr[];
extern PackEntry DirPackR[];
extern PackEntry EnumPackR[];
extern PackEntry ProtoDirAttr[];
extern PackEntry ProtoFileDirAttr[];
extern PackEntry ProtoSRP[];
