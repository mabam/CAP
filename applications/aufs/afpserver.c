/*
 * $Author: djh $ $Date: 1996/06/19 04:23:36 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpserver.c,v 2.17 1996/06/19 04:23:36 djh Rel djh $
 * $Revision: 2.17 $
 *
 */

/*
 * afpserver.c - Appletalk Filing Protocol Server Level Routines
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986,1987,1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  March 1987     Schilit	Created.
 *  December 1990  djh		tidy up for AFP 2.0
 *
 */

/*
 * Various server support routines:
 *
 *  SrvrInfo()
 *  SrvrRegister()
 *  SrvrSetTrace()
 *
 * Non OS dependant support routines:
 *
 *  FPLogin()
 *  FPLoginCont()
 *  FPLogout()
 *  FPGetSrvrParms()
 *  FPGetSrvrMsg()
 *  FPMapID() 
 *  FPMapName()
 *  FPCreateID()
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#ifndef _TYPES
 /* assume included by param.h */
# include <sys/types.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netat/appletalk.h>
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include "afpntoh.h"
#include "afps.h"
#ifdef USETIMES
# include <sys/times.h>
#endif

#ifdef DISTRIB_PASSWDS
#include <netat/afppass.h>
char *distribpassfile = NULL;
#endif /* DISTRIB_PASSWDS */

/* consistency */
#ifdef USETIMES
# define NORUSAGE
#endif

#ifdef DEBUG_AFP_CMD
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

typedef struct {
  int   dsp_cmd;
  PFE   dsp_rtn;
  char *dsp_name;
  int	dsp_flg;
#define DSPF_DMPIN  01
#define DSPF_DMPOUT 02
#define DSPF_DOESSPWRITE 0x4	/* needs more args.. */
#define DSPF_DMPBOTH (DSPF_DMPIN | DSPF_DMPOUT)
#ifdef STAT_CACHE
#define DSPF_FSTAT 0x8
#endif STAT_CACHE
  int   dsp_cnt;
#ifndef NORUSAGE
  struct timeval dsp_utime;
  struct timeval dsp_stime;
#endif
#ifdef USETIMES
  time_t dsp_utime;
  time_t dsp_stime;
#endif
} AFPDispEntry;

private char *afpmtyp = "Unix";

#define AFPVERSZ (6*16)			/* room for IndStr hold all versions */
/* define 1 more than needed just in case */
struct afpversionstruct {
  char *version_name;
  int version;
} afpversions[] = {
  {"AFPVersion 1.0", AFPVersion1DOT0},
  {"AFPVersion 1.1", AFPVersion1DOT1},
  {"AFPVersion 2.0", AFPVersion2DOT0},
  {"AFPVersion 2.1", AFPVersion2DOT1},
  {"AFP2.2", AFPVersion2DOT2},
  {NULL, AFPVersionUnknown}
};

#define NUAM "No User Authent"
#define CUAM "Cleartxt passwrd"
#define RUAM "Randnum exchange"
#define TUAM "2-Way Randnum exchange"

#define MAX_USE_UAM 5			/* maximum number at one time */

private int numuam = 0;
struct afpuamstruct {
  char *uamname;
  int uam;
};
private struct afpuamstruct afpuams[MAX_USE_UAM];

private byte uamflags[] = {
    0,					/* no flags for "No User Authent" */
    UAMP_USER|UAMP_PASS|UAMP_ZERO,	/* user and password for cleartext */
					/* zero means put a zero to even */
					/* out packet between user & passwd */
    UAMP_USER,				/* user only in randnum */
    UAMP_USER				/* user only in 2-way rand */
};

#define AFPUAMSZ (MAX_USE_UAM*17+1)	/* room for IndStr */


private AFPDispEntry *DispTab[AFPMaxCmd+1];

private AFPDispEntry Entries[] = {
  {AFPByteRangeLock,FPByteRangeLock,"ByteRangeLock",0,0},
  {AFPChgPasswd, FPChgPasswd, "ChangePassword", 0, 0},		/* AFP2.0 */
  {AFPGetUserInfo, FPGetUserInfo, "GetUserInfo", 0, 0},		/* AFP2.0 */
  {AFPGetSrvrMsg, FPGetSrvrMsg, "GetSrvrMsg", 0, 0},		/* AFP2.1 */
#ifdef FIXED_DIRIDS
  {AFPCreateID, FPCreateID, "CreateID", 0, 0},			/* AFP2.1 */
  {AFPDeleteID, FPDeleteID, "DeleteID", 0, 0},			/* AFP2.1 */
  {AFPResolveID, FPResolveID, "ResolveID", 0, 0},		/* AFP2.1 */
#endif /* FIXED_DIRIDS */
  {AFPCloseVol,FPCloseVol,"CloseVol",0,0},
  {AFPCloseDir,FPCloseDir,"CloseDir",0,0},
  {AFPCreateDir,FPCreateDir,"CreateDir",0,0},
  {AFPEnumerate,FPEnumerate,"Enumerate",0,0},
  {AFPGetForkParms,FPGetForkParms,"GetForkParms",0,0},
  {AFPGetSrvrParms,FPGetSrvrParms,"GetSrvrParms",0,0},
  {AFPGetVolParms,FPGetVolParms,"GetVolParms",0,0},
  {AFPLogin,FPLogin,"Login",0,0},
  {AFPLoginCont,FPLoginCont,"LoginCont",0,0},
  {AFPLogout,FPLogout,"Logout",0,0},
  {AFPMapID,FPMapID,"MapID",0,0},
  {AFPMapName,FPMapName,"MapName",0,0},
  {AFPOpenVol,FPOpenVol,"OpenVol",0,0},
  {AFPOpenDir,FPOpenDir,"OpenDir",0,0},
  {AFPRead,FPRead,"Read",0,0},
  {AFPGetFileDirParms,FPGetFileDirParms,"GetFileDirParms",0,0},
  {AFPOpenDT,FPOpenDT,"OpenDT",0,0},
  {AFPCloseDT,FPCloseDT,"CloseDT",0,0},
  {AFPGetIcon,FPGetIcon,"GetIcon",0,0},
  {AFPGetIconInfo,FPGetIconInfo,"GetIconInfo",0,0},
  {AFPAddAPPL,FPAddAPPL,"AddAPPL",0,0},
  {AFPRmvAPPL,FPRmvAPPL,"RmvAPPL",0,0},
  {AFPGetAPPL,FPGetAPPL,"GetAPPL",0,0},
  {AFPAddComment,FPAddComment,"AddComment",0,0},
  {AFPRmvComment,FPRmvComment,"RmvComment",0,0},
  {AFPGetComment,FPGetComment,"GetComment",0,0},
  {AFPAddIcon,FPAddIcon,"AddIcon",DSPF_DOESSPWRITE,0},
#ifndef STAT_CACHE
  {AFPCloseFork,FPCloseFork,"CloseFork",0,0},
  {AFPCopyFile,FPCopyFile,"CopyFile",0,0},
  {AFPCreateFile,FPCreateFile,"CreateFile",0,0},
  {AFPDelete,FPDelete,"Delete",0,0},
  {AFPFlush,FPFlush,"Flush",0,0},
  {AFPFlushFork,FPFlushFork,"FlushFork",0,0},
  {AFPMove,FPMove,"Move",0,0},
  {AFPOpenFork,FPOpenFork,"OpenFork",0,0},
  {AFPRename,FPRename,"Rename",0,0},
  {AFPSetDirParms,FPSetDirParms,"SetDirParms",0,0},
  {AFPSetFileParms,FPSetFileParms,"SetFileParms",0,0},
  {AFPSetForkParms,FPSetForkParms,"SetForkParms",0,0},
  {AFPSetVolParms,FPSetVolParms,"SetVolParms",0,0},
  {AFPWrite,FPWrite,"Write",DSPF_DOESSPWRITE,0},
  {AFPSetFileDirParms,FPSetFileDirParms,"SetFileDirParms",0,0},
  {AFPExchangeFiles,FPExchangeFiles,"ExchangeFiles",0,0}
#else STAT_CACHE
  {AFPCloseFork,FPCloseFork,"CloseFork",DSPF_FSTAT,0},
  {AFPCopyFile,FPCopyFile,"CopyFile",DSPF_FSTAT,0},
  {AFPCreateFile,FPCreateFile,"CreateFile",DSPF_FSTAT,0},
  {AFPDelete,FPDelete,"Delete",DSPF_FSTAT,0},
  {AFPFlush,FPFlush,"Flush",DSPF_FSTAT,0},
  {AFPFlushFork,FPFlushFork,"FlushFork",DSPF_FSTAT,0},
  {AFPMove,FPMove,"Move",DSPF_FSTAT,0},
  {AFPOpenFork,FPOpenFork,"OpenFork",DSPF_FSTAT,0},
  {AFPRename,FPRename,"Rename",DSPF_FSTAT,0},
  {AFPSetDirParms,FPSetDirParms,"SetDirParms",DSPF_FSTAT,0},
  {AFPSetFileParms,FPSetFileParms,"SetFileParms",DSPF_FSTAT,0},
  {AFPSetForkParms,FPSetForkParms,"SetForkParms",DSPF_FSTAT,0},
  {AFPSetVolParms,FPSetVolParms,"SetVolParms",DSPF_FSTAT,0},
  {AFPWrite,FPWrite,"Write",DSPF_DOESSPWRITE|DSPF_FSTAT,0},
  {AFPSetFileDirParms,FPSetFileDirParms,"SetFileDirParms",DSPF_FSTAT,0},
  {AFPExchangeFiles,FPExchangeFiles,"ExchangeFiles",DSPF_FSTAT,0}
#endif STAT_CACHE
};

#define NumEntries (sizeof(Entries)/sizeof(AFPDispEntry))

private DumpBuf(), clockstart(), clockend();

private struct sig {
  byte filler[8];
  struct timeval s_time;
} sig;

#ifdef LOGIN_AUTH_PROG
extern char *srvrname;		/* NBP registered name of server */
extern char *login_auth_prog;	/* name of authorization program */
extern AddrBlock addr;		/* AppleTalk address of client */
char log_command[MAXPATHLEN];	/* buffer to build auth. command */
int log_status;			/* status return */
#endif LOGIN_AUTH_PROG

IniServer()			/* ini disp entries */
{
  int i;

  for (i=0; i < AFPMaxCmd; i++)		/* clear all pointers */
    DispTab[i] = (AFPDispEntry *) 0;	/* in dispatch table */
    
  for (i=0; i < NumEntries; i++) {	/* for each entry add into dispatch */
    DispTab[Entries[i].dsp_cmd] = &Entries[i];
#ifndef NORUSAGE
    Entries[i].dsp_utime.tv_sec = 0;
    Entries[i].dsp_utime.tv_usec = 0;
    Entries[i].dsp_stime.tv_sec = 0;
    Entries[i].dsp_stime.tv_usec = 0;
#endif NORUSAGE
#ifdef USETIMES
    Entries[i].dsp_utime = 0;
    Entries[i].dsp_stime = 0;    
#endif USETIMES
  }

  InitOSFI();			/* init finder file info */
  ECacheInit();			/* init afposenum cache */
  InitIconCache();		/* init afpdt cache */
#ifndef FIXED_DIRIDS
  InitDID();			/* init directory stuff  */
#endif FIXED_DIRIDS
#ifdef STAT_CACHE
  OSStatInit();			/* init stat cache */
#endif STAT_CACHE

  /* init server signature */
  bzero((char *)&sig, sizeof(struct sig));
  gettimeofday(&sig.s_time, NULL);
}

/*
 * login states
 *
 */
#define LOGGED_NY	0x00	/* not yet logged in */
#define LOGGED_IN	0x01	/* authorised OK */
#define LOGGED_PX	0x02	/* password expired */

private int loginstate = LOGGED_NY;

/*
 * command dispatcher
 *
 */

OSErr
SrvrDispatch(pkt,len,rsp,rlen,cno,reqref)
byte *pkt,*rsp;
int len,*rlen;
int cno;
ReqRefNumType reqref;
{
  AFPDispEntry *d; 
  int err;
  byte *p;
  byte cmd;
#ifdef DEBUG_AFP_CMD
  time_t diff;
#endif /* DEBUG_AFP_CMD */
  

  *rlen = 0;
  cmd = *pkt;

  d = DispTab[cmd];

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    char *ctime();
    time_t now;
    time(&now);
    diff = clock();
    if (d == (AFPDispEntry *)0)
      fprintf(dbg, "## UNKNOWN (%d) %s\n", cmd, ctime(&now));
    else
      fprintf(dbg, "## %s (%d) %s", d->dsp_name, cmd, ctime(&now));
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (d == (AFPDispEntry *) 0) {
    printf("SrvrDispatch: Unknown cmd=%d (%x)\n",cmd,cmd);
    DumpBuf(pkt,len,"Unknown Cmd",cmd);
    return(aeCallNotSupported);    
  }

  if (DBSRV)
    printf("SrvrDispatch cmd=%s (%d 0x%x), len=%d\n",
	   d->dsp_name,cmd,cmd,len);


  d->dsp_cnt++;			/* increment counter */

  if (d->dsp_flg & DSPF_DMPIN)
   DumpBuf(pkt,len,d->dsp_name,cmd);

  if (statflg)
    clockstart();

  if (loginstate == 0
   && d->dsp_cmd != AFPLogin
   && d->dsp_cmd != AFPLoginCont) {
    logit(0,"SrvrDispatch: Security. Cmd before login %s\n",d->dsp_name);
    err = aeMiscErr;
  } else {
    if (loginstate & LOGGED_PX
     && d->dsp_cmd != AFPLogout
     && d->dsp_cmd != AFPChgPasswd) {
      logit(0,"SrvrDispatch: Security. Cmd before ChangePass %s\n",d->dsp_name);
      err = aePwdExpired;
    } else {
      if (d->dsp_flg & DSPF_DOESSPWRITE) 
        err = (*d->dsp_rtn)(pkt,len,rsp,rlen,cno,reqref);
      else
        err = (*d->dsp_rtn)(pkt,len,rsp,rlen);
    }
  }

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "## %s returns %d (%s) [%4.2f mS]\n\n\n", d->dsp_name,
      err, afperr(err), ((float)((float)clock()-(float)diff))/1000);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (statflg) 
    clockend(d);

#ifdef STAT_CACHE
  if (d->dsp_flg & DSPF_FSTAT)
     OSflush_stat();
  else
     OSstat_cache_update();
#endif STAT_CACHE

  if (d->dsp_flg & DSPF_DMPOUT)
    DumpBuf(rsp,*rlen,d->dsp_name,cmd);

  return(err);
}

#define PERLINE 20

/*
 * If you can understand this, then you are far better person than I...
 * This code is convoluted unecessarily, but it really isn't important
 * code, so...
 *
*/
private 
DumpBuf(p,l,s,c)
byte *p;
int l;
char *s;
byte c;
{
  int ln,ll,left;
  int lll;

  printf("DmpBuf for %s (%d 0x%x) length = %d\n",s,c,c,l);
  if (l > 1000)
    lll = 1000;
  else
    lll = l;

  for (ll = 0; ll < lll; ll += PERLINE) {
    
    printf(" %06d ",ll);
    left = (lll-ll < PERLINE ? lll-ll : PERLINE);
    for (ln=0; ln < left; ln++)
      printf("%02x ",(unsigned int) p[ln]);

    printf("\n        ");
    for (ln=0; ln < left; ln++)
      if ((p[ln] < 0200) && isprint(p[ln]))
	printf("%2c ",p[ln]);
      else
	printf(" . ");
    printf("\n");
    p += PERLINE;
  }
  if (lll < l)
    printf(" ... too large to print ... ");
}

/*
 * Map a user name to a user ID,
 * or a group name to a group ID.
 *
 */
 
/*ARGSUSED*/
OSErr
FPMapName(p,l,r,rl)
byte *p,*r;
int *rl,l;
{
  char name[MAXPSTR];
  OSErr err;
  sdword id;
  MNPPtr mnp = (MNPPtr) p;

  cpyp2cstr(name,mnp->mpn_name);	/* copy into c string */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_func();
    fprintf(dbg, "\tMapFn: %d\t", (int)mnp->mpn_fcn);
    dbg_print_func((int)mnp->mpn_fcn, 1);
    fprintf(dbg, "\tMapNm: \"%s\"\n", name);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  err = OSMapName(mnp->mpn_fcn,name,&id);
  if (err != noErr)
    return(err);
  PackDWord(id,r);		/* pack long format */
  *rl = 4;			/* length of reply */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tMapID: %08x\n", id);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * Map a user ID to a user name,
 * or a group ID to a group name.
 *
 */

OSErr
FPMapID(p,l,r,rl)
byte *p,*r;			/* packet and response */
int *rl,l;			/* response length */
{
  char name[MAXPSTR];
  OSErr err;
  MapIDPkt mip;

  ntohPackX(PsMapID,p,l,(byte *) &mip);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_func();
    fprintf(dbg, "\tMapFn: %d\t", (int)mip.mpi_fcn);
    dbg_print_func((int)mip.mpi_fcn, 0);
    fprintf(dbg, "\tMapID: %08x\n", mip.mpi_id);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  err = OSMapID(mip.mpi_fcn,name,mip.mpi_id);
  if (err != noErr)
    return(err);
  cpyc2pstr(r,name);		/* copy to pascal string for return */
  *rl = strlen(name)+1;		/* return length of reply packet */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tMapNm: \"%s\"\n", name);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}


/*
 * Begin login code
 *
 * Login methods assume we are "forked" from main server!!!
 *
*/

/*
 * map a uam string to internal uam number
 *
*/
private int
mapuamstr(uamstr)
char *uamstr;
{
  int i;

  if (uamstr == NULL)
    return(UAM_UNKNOWN);
  for (i = 0; i < numuam; i++) 
    /* do it case insenstive */
    if (strcmpci(uamstr, afpuams[i].uamname) == 0) 
      return(afpuams[i].uam);
  return(UAM_UNKNOWN);
}

private int
mapverstr(verstr)
char *verstr;
{
  struct afpversionstruct *av;

  for (av = afpversions; av->version_name != NULL ; av++) 
    if (strcmpci(verstr, av->version_name) == 0)
      return(av->version);
  return(AFPVersionUnknown);
}

export void
allowguestid(gn)
char *gn;
{
  if (!setguestid(gn))
    return;
  afpuams[numuam].uamname = NUAM;
  afpuams[numuam].uam = UAM_ANON;
  numuam++;
}

export void
allowcleartext()
{
  afpuams[numuam].uamname = CUAM;
  afpuams[numuam].uam = UAM_CLEAR;
  numuam++;
}

export void
allowrandnum(pw)
char *pw;
{
  if (!setpasswdfile(pw))
    return;
  afpuams[numuam].uamname = RUAM;
  afpuams[numuam].uam = UAM_RANDNUM;
  numuam++;
}

#ifdef DISTRIB_PASSWDS
export void
allow2wayrand(pw)
char *pw;
{
  /*
   * 2-Way implies Randnum for
   * ChangePasswd function.
   *
   */
  distribpassfile = pw;
  afpuams[numuam].uamname = RUAM;
  afpuams[numuam].uam = UAM_RANDNUM;
  numuam++;
  afpuams[numuam].uamname = TUAM;
  afpuams[numuam].uam = UAM_2WAYRAND;
  numuam++;

  return;
}
#endif /* DISTRIB_PASSWDS */

/*
 * following vars are used for randnum exchange
 *
 */
private LoginReplyPkt rne = {	/* initialize to minimize */
  0x0,				/* possiblity of bad transaction */
  0xffff,			/* when starting */
  {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8}
};
private int useruam;		/* UAM_RANDNUM or UAM_2WAYRAND */
private char username[50];	/* some reasonable length */

int sessvers = 0;		/* keep protocol version  */

/*
 * establish an AFP session with the server
 *
 */

/*ARGSUSED*/
OSErr
FPLogin(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  LoginPkt lg;
  char upwd[MAXPASSWD+1];
  int uam, i;
  OSErr err;

#ifdef SHORT_NAMES
int temp;
#endif SHORT_NAMES

  if (loginstate)			/* already logged in */
    return(aeMiscErr);

  (void) ntohPackX(PsLogin,p,l,(byte *) &lg);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tAFPVer: \"%s\"\n", (char *)lg.log_ver);
    fprintf(dbg, "\tAFPUAM: \"%s\"\n", (char *)lg.log_uam);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * locate AFP version and UAM so we can unpack accordingly
   *
   */
  if ((sessvers = mapverstr(lg.log_ver)) == AFPVersionUnknown) {
    logit(0,"Bad version %s", lg.log_ver);
    return(aeBadVersNum);
  }
  if ((uam = mapuamstr(lg.log_uam)) == UAM_UNKNOWN) {
    logit(0,"BAD UAM %s in connection attempt", lg.log_uam);
    return(aeBadUAM);	/* no... forget it */
  }
  if (sessvers < AFPVersion2DOT1 && uam == UAM_2WAYRAND)
    return(aeBadUAM);

  /*
   * mactime <-> unix time is different for afp 1.1
   * this calldown is to afpcmd
   *
   */
  InitPackTime(sessvers == AFPVersion1DOT0);

#ifdef SHORT_NAMES
  temp = 3 + p[1] + p[p[1] +2];
  temp = temp + p[temp] + 1;
  if ((p[temp] != '\0') && (uam == UAM_CLEAR))  /* doesn't need the zero*/ 
    (void) ntohPackXbitmap(PsLogin,p,l,(byte *) &lg,UAMP_USER|UAMP_PASS);
  else
    (void) ntohPackXbitmap(PsLogin,p,l,(byte *) &lg, uamflags[uam]);
#else SHORT_NAMES
  (void) ntohPackXbitmap(PsLogin,p,l,(byte *) &lg, uamflags[uam]);
#endif SHORT_NAMES

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tUserNm: \"%s\"\n", (char *)lg.log_user);
    if (uam == UAM_CLEAR)
      fprintf(dbg, "\tUserPw: (%d bytes)\n", strlen(lg.log_passwd));
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

#ifdef LOGIN_AUTH_PROG
  if (login_auth_prog) {
    sprintf(log_command, "%s \"%d\" \"%d\" \"%s\" \"%s\"",
      login_auth_prog, ntohs(addr.net), addr.node, lg.log_user, srvrname);
    if ((log_status = ((system(log_command) >> 8) & 0xff)) != 0) {
      logit(0, "Login disallowed for %d %d (%s); authorization returned %d",
	ntohs(addr.net), addr.node, lg.log_user, log_status);
      return(aeParamErr);
    }
  }
#endif LOGIN_AUTH_PROG

  /*
   * 'Randnum exchange' or '2-Way Randnum exchange'
   *
   */
  if (uam == UAM_RANDNUM || uam == UAM_2WAYRAND) {
    if ((err = OSLoginRand(lg.log_user)) != noErr)
      return(err);
    for (i = 0; i < 8; i++)
      rne.logr_randnum[i] = (byte)OSRandom();
    useruam = uam;
    strcpy(username, lg.log_user); /* remember it */
    rne.logr_flag = UAMP_RAND | UAMP_INUM;
    rne.logr_idnum = OSRandom(); /* some random number */
    *rl = htonPackXbitmap(PsLoginReply, (byte *)&rne, r, rne.logr_flag);
#ifdef DEBUG_AFP_CMD
    if (dbg != NULL) {
      fprintf(dbg, "    Return Parameters:\n");
      fprintf(dbg, "\tAFPID: %04x\n", rne.logr_idnum);
      fprintf(dbg, "\tRandm: ");
      for (i = 0; i < 8; i++)
	fprintf(dbg, "%02x", rne.logr_randnum[i]);
      fprintf(dbg, "\n");
      fflush(dbg);
    }
#endif /* DEBUG_AFP_CMD */
    return(aeAuthContinue);
  }

  /*
   * 'Cleartxt Passwrd'
   *
   */
  if (uamflags[uam] & UAMP_PASS) {
    bcopy(lg.log_passwd,upwd,MAXPASSWD);	/* copy the password */
    upwd[MAXPASSWD] = '\0';		/* make sure null terminated */
  }

  if (DBSRV) {
    printf("Login ver=%s, uam=%s, user=%s, pwd=%s\n",
	    lg.log_ver,lg.log_uam,
	  ((uamflags[uam] & UAMP_USER) ? (char *)lg.log_user : "<anonymous>"),
	    ((uamflags[uam] & UAMP_PASS) ? upwd : "no passwd"));
  }

  if ((err = OSLogin((char *)lg.log_user, upwd, NULL, uam)) == noErr) {
    logit(0,"Using protocol '%s'", lg.log_ver);
    loginstate = LOGGED_IN;
  }

  return(err);
}  

/*
 * continue login and user authentication process started by FPLogin
 *
 */

/*ARGSUSED*/
OSErr
FPLoginCont(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  LoginContPkt lcp;
  word bitmap;
  OSErr err;
#ifdef DEBUG_AFP_CMD
  int i;
#endif /* DEBUG_AFP_CMD */

  /*
   * get id number
   *
   */
  ntohPackXbitmap(PsLoginCont, p, l, &lcp, UAMP_INUM);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tAFPID: %04x\n", lcp.lgc_idno);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (lcp.lgc_idno != rne.logr_idnum)
    return(aeParamErr);

  /*
   * ID numbers match, so unpack encrypted passwd(s)
   *
   */
  bitmap = UAMP_INUM | UAMP_ENCR | ((useruam==UAM_2WAYRAND) ? UAMP_TWAY : 0);
  ntohPackXbitmap(PsLoginCont, p, l, &lcp, bitmap);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tUserA: ");
    for (i = 0; i < 8; i++)
      fprintf(dbg, "%02x", lcp.lgc_encrypted[i]);
    fprintf(dbg, "\n");
    if (useruam == UAM_2WAYRAND) {
      if (l == 20) {
        fprintf(dbg, "\tRandm: ");
        for (i = 0; i < 8; i++)
          fprintf(dbg, "%02x", lcp.lgc_wsencrypt[i]);
        fprintf(dbg, "\n");
      } else
	fprintf(dbg, "\t<bad LoginCont>\n");
    }
  }
#endif /* DEBUG_AFP_CMD */

  if (DBSRV)
    printf("Login Randnum exchange for %s\n",username );

  err = OSLogin(username, lcp.lgc_encrypted, rne.logr_randnum, useruam);

  if (err == noErr)
    loginstate = LOGGED_IN;

  if (useruam != UAM_2WAYRAND)
    return(err);

#ifdef DISTRIB_PASSWDS
  { extern struct afppass *afp;
    void afpdp_encr();
    int afpdp_pwex();

    if (afp == NULL)
      err = aeUserNotAuth;

    if (err != noErr)
      return(err);

    /* encrypt in place */
    afpdp_encr(lcp.lgc_wsencrypt, afp->afp_password, NULL);

    *rl = htonPackXbitmap(PsLoginContR, (byte *)&lcp, r, UAMP_TWAY);

    /* password expired ? */
    if (afpdp_pwex(afp) != 0)
      loginstate |= LOGGED_PX;

#ifdef DEBUG_AFP_CMD
    if (dbg != NULL) {
      fprintf(dbg, "    Return Parameters:\n");
      fprintf(dbg, "\tSrvrA: ");
      for (i = 0; i < 8; i++)
        fprintf(dbg, "%02x", lcp.lgc_wsencrypt[i]);
      fprintf(dbg, "\n");
      fflush(dbg);
    }
#endif /* DEBUG_AFP_CMD */

  }
#endif /* DISTRIB_PASSWDS */

  return(err);
}

/*
 * AFP2.0: change user password
 *
 */

OSErr
FPChgPasswd(p, l, r, rl)
byte *p, *r;
int l, *rl;
{
  ChgPasswdPkt cpp;
  ChgPasswdReplyPkt cprp;
  OSErr err;
  int uam;

  (void)ntohPackX(PsChangePassword, p, l, (byte *)&cpp);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tChgUAM: \"%s\"\n", cpp.cp_uam);
    fprintf(dbg, "\tChgUsr: \"%s\"\n", cpp.cp_user);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if ((uam = mapuamstr(cpp.cp_uam)) == UAM_UNKNOWN) {
    logit(0,"BAD UAM %s in change password attempt", cpp.cp_uam);
    return(aeBadUAM);
  }
  if (uam == UAM_ANON)
    return(aeBadUAM);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    int i;
    switch (uam) {
      case UAM_CLEAR:
        fprintf(dbg, "\tOldPwd: (%d bytes)\n", strlen(cpp.cp_oldpass));
        fprintf(dbg, "\tNewPwd: (%d bytes)\n", strlen(cpp.cp_newpass));
	break;
      case UAM_RANDNUM:
        fprintf(dbg, "\tOldPwd: ");
        for (i = 0; i < 8; i++)
	  fprintf(dbg, "%02x", cpp.cp_oldpass[i]);
        fprintf(dbg, "\n");
        fprintf(dbg, "\tNewPwd: ");
        for (i = 0; i < 8; i++)
	  fprintf(dbg, "%02x", cpp.cp_newpass[i]);
        fprintf(dbg, "\n");
	break;
    }
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (DBSRV)
    printf("ChangePassword uam=%s, user=%s\n", cpp.cp_uam, cpp.cp_user);

  err = OSChangePassword(cpp.cp_user, cpp.cp_oldpass, cpp.cp_newpass, uam);

#ifdef DISTRIB_PASSWDS
  if (err == noErr)
    loginstate &= ~LOGGED_PX;
#endif /* DISTRIB_PASSWDS */

  if (err != noErr)
    logit(0, "ChangePassword failed for %s (%s)", cpp.cp_user, afperr(err));

  return(err);
}

/*
 * AFP2.1: get srvr message
 *
 */

OSErr
FPGetSrvrMsg(p, l, r, rl)
byte *p, *r;
int l, *rl;
{
  SrvrMsgPkt smp;
  SrvrMsgReplyPkt smrp;
  OSErr err;

  (void)ntohPackX(PsGetSrvrMsg, p, l, (byte *)&smp);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tMsgTyp: %04x\t", smp.msg_typ);
    fprintf(dbg, "(%s)\n", (smp.msg_typ == 0) ? "Login" : "Server");
    fprintf(dbg, "\tMsgBMp: %04x\n", smp.msg_bitmap);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (sessvers < AFPVersion2DOT1)
    return(aeCallNotSupported);
  if (smp.msg_bitmap != 0x01)
    return(aeBitMapErr);
  smrp.msgr_typ = smp.msg_typ;
  smrp.msgr_bitmap = smp.msg_bitmap;
  smrp.msgr_data[0] = '\0';
  err = OSGetSrvrMsg(smrp.msgr_typ, smrp.msgr_data);
  if (err != noErr)
    return(err);
  *rl = htonPackX(PsGetSrvrMsgReply, (byte *)&smrp, r);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_name();
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tMsgTyp: %04x\t", smrp.msgr_typ);
    fprintf(dbg, "(%s)\n", (smrp.msgr_typ == 0) ? "Login" : "Server");
    fprintf(dbg, "\tMsgBMp: %04x\n", smrp.msgr_bitmap);
    fprintf(dbg, "\tMsgStr: %s\n", smrp.msgr_data);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * AFP2.0: get user information 
 *
 */

FPGetUserInfo(p, l, r, rl)
byte *p, *r;
int l, *rl;
{
  GetUserInfoPkt guip;
  GetUserInfoReplyPkt guirp;
  OSErr err;

  (void)ntohPackX(PsGetUserInfo, p, l, (byte *)&guip);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "\tGUFlg: %02x\t", guip.gui_flag);
    fprintf(dbg, "%s\n", (guip.gui_flag & 0x01) ? "(ThisUser)" : "");
    fprintf(dbg, "\tUsrID: %08x\t", guip.gui_userid);
    fprintf(dbg, "%s\n", (guip.gui_flag & 0x01) ? "(Invalid)" : "");
    fprintf(dbg, "\tBtMap: %04x\n", guip.gui_bitmap);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  guirp.guir_bitmap = guip.gui_bitmap;
  err=OSGetUserInfo((guip.gui_flag & GUI_THIS_USER)!=0,guip.gui_userid,&guirp);
  if (err != noErr)
    return(err);
  *rl=htonPackXbitmap(PsGetUserInfoReply,(byte *)&guirp,r,guirp.guir_bitmap);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "    Return Parameters:\n");
    fprintf(dbg, "\tBtMap: %04x\n", guirp.guir_bitmap);
    if (guirp.guir_bitmap & 0x0001)
      fprintf(dbg, "\tUsrID: %08x\n", guirp.guir_userid);
    if (guirp.guir_bitmap & 0x0002)
      fprintf(dbg, "\tGrpID: %08x\n", guirp.guir_pgroup);
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

#ifdef notdef
int IndStrPack(d,s)
byte *d;
char *s[];
{
  IniIndStr(d);				/* init indexed string */
  for (; *s != NULL; s++)		/* copy entries from array */
    AddIndStr(*s,d);			/*  adding each array element */
  return(IndStrLen(d));			/* return length */
}
#endif

#define MAXNADSIZ	8		/* tag #2, IP addr & port */
#define NUMNAD		5		/* no more than 5 IP addr/host */

int 
GetSrvrInfo(r,sname,icon,iconsize)
byte *r;
char *sname;
byte icon[];
int iconsize;
{
  byte *q;
  int i,len;
  extern int nopwdsave;
  GetSrvrInfoReplyPkt sr;
  OPTRType avo,uamo,vicono,sigo,nado;
  byte avobuf[AFPVERSZ],uamobuf[AFPUAMSZ];
  byte nadbuf[MAXNADSIZ*NUMNAD+1];
  extern u_short asip_port;
  extern u_int asip_addr;
  extern int asip_enable;
  struct hostent *he;
  char hostname[128];

  /*
   * set server capabilities
   *
   */
  sr.sr_flags = SupportsFPCopyFile;
#ifdef DISTRIB_PASSWDS
  sr.sr_flags |= SupportsChgPwd;
#endif DISTRIB_PASSWDS
  sr.sr_flags |= SupportsServerMsgs;
  sr.sr_flags |= SupportsServerSig;
  if (asip_enable)
    sr.sr_flags |= SupportsTCPIP;
  if (nopwdsave)
    sr.sr_flags |= DontAllowSavePwd;

  /*
   * set Server Name & Machine Type
   *
   */
  strcpy(sr.sr_machtype,afpmtyp);
  cpyc2pstr(sr.sr_servername,sname);

  /*
   * set Volume Icon & Mask
   *
   */
  vicono.optr_loc = icon;
  vicono.optr_len = iconsize;
  sr.sr_vicono = (char *) &vicono;

  /*
   * set AFP Versions
   *
   */
  IniIndStr(avobuf);
  for (i = 0; afpversions[i].version_name != NULL; i++)
    AddIndStr(afpversions[i].version_name, avobuf);
  avo.optr_len = IndStrLen(avobuf);
  avo.optr_loc = avobuf;
  sr.sr_avo = (byte *) &avo;

  /*
   * set UAMs
   *
   */
  IniIndStr(uamobuf);
  for (i=0 ; i < numuam; i++)
    AddIndStr(afpuams[i].uamname, uamobuf);
  uamo.optr_len = IndStrLen(uamobuf);
  uamo.optr_loc = uamobuf;
  sr.sr_uamo = (byte *) &uamo;

  /*
   * set server signature
   *
   */
  sigo.optr_len = 16;
  sigo.optr_loc = (byte *)&sig;
  sr.sr_sigo = (byte *)&sigo;

  /*
   * set network address(es)
   * use single bound address (-B <addr:port>)
   * or all known addresses for this machine
   *
   */
  q = nadbuf+1;
  nadbuf[0] = 0;
  if (asip_enable) {
    if (asip_addr) {
      nadbuf[0] = 1;
      q[2] = (asip_addr >> 24) & 0xff;
      q[3] = (asip_addr >> 16) & 0xff;
      q[4] = (asip_addr >>  8) & 0xff;
      q[5] = (asip_addr & 0xff);
      if (asip_port == ASIP_PORT) {
        q[0] = 0x06; /* len */
        q[1] = 0x01; /* tag */
        q += 6;
      } else {
        q[0] = 0x08; /* len */
        q[1] = 0x02; /* tag */
        q[6] = asip_port >> 8;
        q[7] = asip_port & 0xff;
        q += 8;
      }
    } else { /* list all known addresses */
      if (gethostname(hostname, sizeof(hostname)) == 0) {
        if ((he = gethostbyname(hostname)) != NULL) {
          for (i = 0; he->h_addr_list[i] && i < NUMNAD; i++) {
	    bcopy(he->h_addr_list[i], q+2, 4); /* copy IP */
	    if (asip_port == ASIP_PORT) {
	      q[0] = 0x06; /* len */
	      q[1] = 0x01; /* tag */
	      q += 6;
	    } else {
	      q[0] = 0x08; /* len */
	      q[1] = 0x02; /* tag */
	      q[6] = asip_port >> 8;
	      q[7] = asip_port & 0xff;
	      q += 8;
	    }
          }
          nadbuf[0] = i;
        }
      }
    }
  }
  nado.optr_len = q-nadbuf;
  nado.optr_loc = nadbuf;
  sr.sr_naddro = (byte *)&nado;

  /*
   * pack data for sending
   *
   */
  len = htonPackX(ProtoSRP,(byte *) &sr,r);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    void dbg_print_info();
    time_t now;
    time(&now);
    fprintf(dbg, "## FPGetSrvrInfo (15) %s", ctime(&now));
    fprintf(dbg, "    Return Parameters:\n");
    dbg_print_info(r, len);
    fprintf(dbg, "## FPGetSrvrInfo returns %d (%s)\n\n\n", 0, afperr(0));
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(len);
}

PrtSrvrInfo(pkt,pktl)
byte *pkt;
int pktl;
{
  GetSrvrInfoReplyPkt sr;
  char name[MAXSNAM+1];

  ntohPackX(ProtoSRP,pkt,pktl,(byte *) &sr); /* unpack from net */
  cpyp2cstr(name,sr.sr_servername);

  printf("PrtSrvrInfo Packet length is %d\n",pktl);
  printf("  Machine Type = %s\n",sr.sr_machtype);
  printf("  Server Name  = %s\n",name);
  printf("  Flags = %d\n",sr.sr_flags);
  printf("  AFP Versions Supported:\n");
  PrtIndStr(sr.sr_avo);
  printf("  User Authentication Methods:\n");
  PrtIndStr(sr.sr_uamo);
  printf("  Icon %s present\n",(sr.sr_vicono == 0) ? "is not" : "is");
}

/*ARGSUSED*/
OSErr
FPLogout(p,l,r,rl)
char *p,*r;
int l,*rl;
{
  printf("FPLogout\n");
  return(noErr);
}

/*
 * OSErr FPGetSrvrParms(byte *p,byte *r,int *rl)
 *
 * This call is used to retrieve server-level parameters (for now only
 * the volume names)
 *
 */

/*ARGSUSED*/
OSErr
FPGetSrvrParms(p,l,r,rl)
byte *p,*r;
int l,*rl;
{
  extern PackEntry ProtoGSPRP[];
  GetSrvrParmsReplyPkt gpr;
  byte *up;
  int cnt;

  gpr.gspr_time = CurTime();
  gpr.gspr_nvols = VCount();
  cnt = htonPackX(ProtoGSPRP, (byte *)&gpr, r);
  cnt += VMakeVList(r+cnt);	/* add vols, return count */
  *rl = cnt;			/* pkt length */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    byte *q = r + 5;
    void dbg_print_name();
    fprintf(dbg, "\tSTime: %s", ctime((time_t *)&gpr.gspr_time));
    fprintf(dbg, "\tNVols: %02x\n", gpr.gspr_nvols);
    while (q < (r + (*rl))) {
      fprintf(dbg, "\tVFlag: %02x (", (int)(*q));
      fprintf(dbg, "%s", (*q & 0x80) ? "HasPasswd" : "");
      fprintf(dbg, "%s",((*q & 0x81) == 0x81) ? ", " : "");
      fprintf(dbg, "%s", (*q & 0x01) ? "HasConfig" : "");
      fprintf(dbg, ")\n");
      dbg_print_name("\tVName:", q+1);
      q += (*(q+1) + 2);
    }
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);		/* no error */
}

/*
 * void SrvrSetTrace(char *tstr)
 *
 * Set trace of command packets on input, output, or both.  Tstr 
 * is a string containing items of the form:
 *
 *	[I|O|B]CmdName [I|O|B]CmdName ...
 *
 * The first character of an item specifies if you want the packet
 * displayed on input, output, or both input and output. The rest
 * of the item is the name of the command, Enumerate or AddAPPL for
 * example.
 *
 */

int SetPktTrace(tstr)
char *tstr;
{
  int flg,i;
  char cmdbuf[40],*cp,*opt,c;
  
  while (*tstr != '\0') {
    while (*tstr == ' ' || *tstr == '\t')
      tstr++;

    if (*tstr == '\0')
      break;
    
    switch (c = *tstr++) {
    case 'I': case 'i':
      flg = DSPF_DMPIN; opt = "input"; break;
    case 'O': case 'o':
      flg = DSPF_DMPOUT; opt = "output"; break;
    case 'B': case 'b':
      flg = DSPF_DMPBOTH; opt = "input and output"; break;
    default:
      fprintf(stderr,"SetPktTrace: I, O, or B required.  %c unknown\n",c);
      return(FALSE);
    }

    cp = cmdbuf;
    while (*tstr != ' ' && *tstr != '\t' && *tstr != '\0')
      *cp++ = *tstr++;
    *cp++ = '\0';
    for (i=0; i < NumEntries; i++)
      if (strcmpci(cmdbuf,Entries[i].dsp_name) == 0) {
	fprintf(stderr,"SetPktTrace: Tracing %s on %s\n",
		Entries[i].dsp_name,opt);
	Entries[i].dsp_flg = flg;
	break;
      }
    if (i >= NumEntries) {
      fprintf(stderr,"SetPktTrace: Unknown command %s\n",cmdbuf);
      return(FALSE);
    }
  }
  return(TRUE);
}

#ifndef NORUSAGE
private 
acctime(b,a,s)
struct timeval *b,*a,*s;
{
  s->tv_sec += a->tv_sec - b->tv_sec;
  s->tv_usec += a->tv_usec - b->tv_usec;
  if (s->tv_usec < 0) {
    s->tv_sec--;
    s->tv_usec += uSEC;
  }
  if (s->tv_sec > uSEC) {
    s->tv_sec++;
    s->tv_usec -= uSEC;
  }
}

private
printt(s,tv)
char *s;
struct timeval *tv;
{
  printf("%3d.%06d %s ",tv->tv_sec,tv->tv_usec,s);
}

private struct rusage before;

private
clockstart()
{
  getrusage(RUSAGE_SELF,&before);
}

private
clockend(d)
AFPDispEntry *d; 
{
  struct rusage after;

  getrusage(RUSAGE_SELF,&after);

  acctime(&before.ru_utime,&after.ru_utime,&d->dsp_utime);
  acctime(&before.ru_stime,&after.ru_stime,&d->dsp_stime);
}
#endif

#ifdef USETIMES
private struct tms before;

private
clockstart()
{
  (void)times(&before);
}

private
clockend(d)
AFPDispEntry *d; 
{
  struct tms after;

  (void)times(&after);

  d->dsp_utime += after.tms_utime - before.tms_utime;
  d->dsp_stime += after.tms_stime - before.tms_stime;
  d->dsp_utime += after.tms_cutime - before.tms_cutime;
  d->dsp_stime += after.tms_cstime - before.tms_cstime;
}

#ifndef HZ			/* in case not defined... */
# define HZ 60
#endif
private
printt(s,tv)
char *s;
time_t *tv;
{
  float pval;
  pval = ((float)*tv)/((float)HZ);
  printf("%.06f %s ",pval,s);
}

#endif

SrvrPrintStats()
{
  int i;

  printf("\nServer Statistics:\n");
  for (i=0; i < NumEntries; i++)
    if (Entries[i].dsp_cnt != 0) {
      printf("  %20s Count=%5d ",Entries[i].dsp_name,
	     Entries[i].dsp_cnt);
      printt("Usr",&Entries[i].dsp_utime);
      printt("Sys",&Entries[i].dsp_stime);
      printf("\n");
    }
}

#ifdef DEBUG_AFP_CMD
/*
 * print the uid/gid Name/ID function
 *
 */

void
dbg_print_func(func, typ)
int func;
int typ;
{
  if (dbg != NULL) {
    if (typ == 0 && (!(func == 1 || func == 2))) {
      fprintf(dbg, "<unknown MapID fn %d>\n", func);
      return;
    }
    if (typ == 1 && (!(func == 3 || func == 4))) {
      fprintf(dbg, "<unknown MapName fn %d>\n", func);
      return;
    }
    switch (func) {
      case 1:
        fprintf(dbg, "(ID to Usrname)\n");
	break;
      case 2:
        fprintf(dbg, "(ID to Grpname)\n");
	break;
      case 3:
        fprintf(dbg, "(Usrname to ID)\n");
	break;
      case 4:
        fprintf(dbg, "(Grpname to ID)\n");
	break;
      default:
	fprintf(dbg, "(<unknown>\n)");
	break;
    }
  }

  return;
}

/*
 * dump server information
 *
 */

#define get2(s) (u_short)(((s)[0]<<8)|((s)[1]))
#define get4(s) (u_int)(((s)[0]<<24)|((s)[1]<<16)|((s)[2]<<8)|((s)[3]))

void
dbg_print_info(r, rl)
byte *r;
int rl;
{
  int i;
  byte *q = r;
  u_short srvrflags;
  short machoff, afpoff, uamoff;
  short vicnoff, sigoff, nadoff;
  void dbg_print_name();
  void dbg_print_sflg();
  void dbg_print_icon();

  machoff = get2(q); q += 2;
  fprintf(dbg, "\tMchOff: %d\n", machoff);
  afpoff = get2(q);  q += 2;
  fprintf(dbg, "\tAFPOff: %d\n", afpoff);
  uamoff = get2(q);  q += 2;
  fprintf(dbg, "\tUAMOff: %d\n", uamoff);
  vicnoff = get2(q); q += 2;
  fprintf(dbg, "\tICNOff: %d\n", vicnoff);
  srvrflags = get2(q); q += 2;
  fprintf(dbg, "\tVolFlg: %04x\t", srvrflags);
  dbg_print_sflg(srvrflags);
  dbg_print_name("\tSrvrNm:", q);
  q += ((*q)+1);
  if ((u_long)q & 0x01)
    q++; /* even */
  sigoff = get2(q); q += 2;
  fprintf(dbg, "\tSIGOff: %d\n", sigoff);
  nadoff = get2(q); q += 2;
  fprintf(dbg, "\tNADOff: %d\n", nadoff);
  if (machoff != 0)
    dbg_print_name("\tMchTyp:", r+machoff);
  if (afpoff != 0) {
    q = r + afpoff;
    fprintf(dbg, "\tVerCnt: %d\n", (int)(*q));
    for (i = *q++; i > 0; i--) {
      dbg_print_name("\tAFPVer:", q);
      q += ((*q) + 1);
    }
  } else
    fprintf(dbg, "\t<no AFP>\n");
  if (uamoff != 0) {
    q = r + uamoff;
    fprintf(dbg, "\tUAMCnt: %d\n", (int)(*q));
    for (i = *q++; i > 0; i--) {
      dbg_print_name("\tUAMStr:", q);
	q += ((*q) + 1);
    }
  } else
    fprintf(dbg, "\t<no UAM>\n");
  if (vicnoff != 0) {
    fprintf(dbg, "\tVolICN:\n");
    dbg_print_icon((r+vicnoff), 256);
    fprintf(dbg, "\n");
  } else
    fprintf(dbg, "\t<no ICON>\n");
  if (sigoff != 0) {
    q = r + sigoff;
    fprintf(dbg, "\tSrvSIG: ");
    for (i = 0; i < 16; i++)
      fprintf(dbg, "%02x ", *(q+i));
    fprintf(dbg, "\n");
  } else
    fprintf(dbg, "\t<no SIG>\n");
  if (nadoff != 0) {
    q = r + nadoff;
    fprintf(dbg, "\tNADCnt: %d\n", (int)(*q));
    for (i = *q++; i > 0; i--) {
      fprintf(dbg, "\tAFPNAD: len %d tag %d ", q[0], q[1]);
      switch (q[1]) {
	case 0x01:
	  fprintf(dbg, "IP %d.%d.%d.%d\n",
	    q[2], q[3], q[4], q[5]);
	  break;
	case 0x02:
	  fprintf(dbg, "IP %d.%d.%d.%d Port %d\n",
	    q[2], q[3], q[4], q[5], (q[6] << 8) | q[7]);
	  break;
	case 0x03:
	  fprintf(dbg, "DDP net %d.%d node %d skt %d\n",
	    q[2], q[3], q[4], q[5]);
	  break;
	default:
	  fprintf(dbg, "<unknown>\n");
	  break;
      }
      q += q[0];
    }
  } else
    fprintf(dbg, "\t<no NAD>\n");

  return;
}

/*
 * print a pascal string
 *
 */

void
dbg_print_name(str, nam)
char *str;
byte *nam;
{
  int i;

  if (dbg != NULL) {
    fprintf(dbg, "%s \"", str);
    for (i = 0; i < (int)*nam; i++)
      fprintf(dbg, "%c", *(nam+i+1));
    fprintf(dbg, "\"\n");
  }

  return;
}

/*
 * print server flags
 *
 */

void
dbg_print_sflg(bmap)
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
	    fprintf(dbg, "CopyFile");
	    j++;
	    break;
	  case 1:
	    fprintf(dbg, "ChngPass");
	    j++;
            break;
	  case 2:
	    fprintf(dbg, "DontSavePass");
	    j++;
	    break;
	  case 3:
	    fprintf(dbg, "SrvrMesg");
	    j++;
	    break;
	  case 4:
	    fprintf(dbg, "SrvrSig");
	    j++;
	    break;
	  case 5:
	    fprintf(dbg, "TCP/IP");
	    j++;
	    break;
	  case 6:
	    fprintf(dbg, "SrvrNotf");
	    j++;
	    break;
	  case 15:
	    fprintf(dbg, "MGetReqs");
	    j++;
	    break;
	  default:
	    fprintf(dbg, "<unknown %d>", i);
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
#endif /* DEBUG_AFP_CMD */
