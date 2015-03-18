/*
 * $Author: djh $ $Date: 1996/04/25 13:03:09 $
 * $Header: /mac/src/cap60/samples/RCS/ash.c,v 2.6 1996/04/25 13:03:09 djh Rel djh $
 * $Revision: 2.6 $
*/

/*
 * ash.c - afp "shell" - lets us try executing some afp client calls
 *
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Edit History:
 *
 *  March 1987 	CCKim		Created.
 *
 */

char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>		/* include if not there */
#endif
#ifdef AIX
#include <sys/select.h>
#endif AIX
#include <netinet/in.h>		/* so htons() works for non-vax */
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netat/appletalk.h>	/* include appletalk definitions */
#include <netat/afp.h>
#include <netat/afpcmd.h>
#include <netat/afpc.h>
#include <netat/compat.h>
#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
#endif
#ifdef USESTRINGDOTH
# include <string.h>
#else
# include <strings.h>
#endif

usage(s)
char *s;
{
  fprintf(stderr,"usage: %s [-d FLAGS] nbpentity\n",s); 
  exit(1);
}

doargs(argc,argv,name)
int argc;
char **argv;
char **name;
{
  char *s,*whoami = argv[0];
  int sawentity;

  sawentity = 0;		/* no entity seen yet */
  while (--argc) {
    s = *++argv;		/* move to next arg */
    if (s[0] == '-') {		/* option? */
      switch (s[1]) {
      case 'd':
      case 'D':
	dbugarg(&s[2]);		/* some debug flags */
	break;
      default:
	usage(whoami);
      }
      continue;
    }
    if (sawentity)
      usage(whoami);
    sawentity = 1;		/* got here because have non-flag arg */
    *name = s;
  }
}

main(argc,argv)
int argc;
char **argv;
{
  int err,i;
  AddrBlock addr;			/* Address of entity */
  char *name = "Charlie\250:afpserver@*";

  abInit(TRUE);				/* initialize appletalk driver */
  nbpInit();				/* initialize nbp */
  checksum_error(FALSE);		/* ignore these errors */
  doargs(argc,argv,&name);		/* handle arguments */
  lookup(name, &addr);
  handle_afp(&addr);
}

lookup(name, addr)
char *name;
AddrBlock *addr;
{
  EntityName en;			/* network entity name */
  NBPTEntry nbpt[1];			/* table of entity names */
  nbpProto nbpr;			/* nbp protocol record */
  int err;

  create_entity(name, &en);

  nbpr.nbpRetransmitInfo.retransInterval = sectotick(1);
  nbpr.nbpRetransmitInfo.retransCount = 10; /* one retransmit */
  nbpr.nbpBufPtr = nbpt;		/* place to store entries */
  nbpr.nbpBufSize = sizeof(nbpt);	/* size of above */
  nbpr.nbpDataField = 1;		/* max entries */
  nbpr.nbpEntityPtr = &en;		/* look this entity up */

#ifdef ISO_TRANSLATE
  cISO2Mac(en.objStr.s);
  cISO2Mac(en.typeStr.s);
  cISO2Mac(en.zoneStr.s);
#endif ISO_TRANSLATE

  printf("Looking for %s:%s@%s ...\n",en.objStr.s,en.typeStr.s,en.zoneStr.s);

  /* Find all objects in specified zone */
  err = NBPLookup(&nbpr,FALSE);		/* try synchronous */
  if (err != noErr || nbpr.abResult != noErr) {
    fprintf(stderr,"NBPLookup returned err %d\n",err);
    exit(1);
  }
  if (nbpr.nbpDataField == 0) {
    printf("Couldn't find it\n");
    exit(1);
  }
  NBPExtract(nbpt,nbpr.nbpDataField,1,&en,addr);
}

GSPRPPtr vpr;

int goodversion = FALSE;	/* valid version of AFP? */
int valid_uams[3] = {FALSE, FALSE, FALSE};

handle_afp(addr)
AddrBlock *addr;
{
  int srn, comp, cnt, attn();
  GetSrvrInfoReplyPkt sr;

  if (FPGetSrvrInfo(addr, &sr) != noErr) {
    fprintf(stderr, "Server not responding\n");
    exit(1);
  }
  initclient(&sr);

  SPOpenSession(addr, attn, &srn, 2, -1, &comp);
  while (comp > 0)
    abSleep(4*9, TRUE);
  fprintf(stderr, "comp  = %d\n", comp);
  if (comp < 0) {
    printf("Can't open session to remote, error %d\n",comp);
    return;
  }
  tryit(srn);
  SPCloseSession(srn, 10, -1, &comp);
  while (comp > 0)
    abSleep(4*9, TRUE);
}

byte *
makepath(str)
byte *str;
{
  static byte path[256];
  int len, i;

  if (str != NULL && str[0] == ' ')
    str++;
  len = str == NULL ? 0 : strlen((char *)str);
  if (len > 255)
    len = 255;
  path[0] = len;
  for (i=0; i < len; i++)
    path[i+1] = (str[i] == ':') ? '\0' : str[i];
  return(path);
}

initclient(sr)
GetSrvrInfoReplyPkt *sr;
{
  char tbuf[32];
  int i, j, n;
  /* Order matters */
  static char *uam_which[3] = {
    "No User Authent",
    "Cleartxt passwrd",
    "Randnum exchange"
    };

  printf("%s server name ",sr->sr_machtype);
  dumppstr(sr->sr_servername);
  putchar('\n');
  n = IndStrCnt(sr->sr_avo);
  for (i = 0; i < n; i++) {
    GetIndStr(tbuf, sr->sr_avo, i);
    if (strcmp("AFPVersion 1.1", tbuf) == 0) {
      goodversion = TRUE;
      break;
    }
  }
  /* ignore for now */
  n = IndStrCnt(sr->sr_avo);
  printf("There %s %d AVO strings\n", n > 1 ? "are" : "is", n);
  PrtIndStr(sr->sr_avo);
  n = IndStrCnt(sr->sr_uamo);
  printf("There %s %d UAM strings\n", n > 1 ? "are" : "is", n);
  PrtIndStr(sr->sr_uamo);
  for (i=0; i < n; i++) {
    GetIndStr(tbuf, sr->sr_uamo, i);
    for (j = 0; j < 3; j++)
      if (strcmp(uam_which[j], tbuf) == 0)
	valid_uams[j] = TRUE;
  }
}

attn(srn, sessid, attncode)
int srn;
byte sessid;
word attncode;
{
  if ((attncode & 0xf000) == 0x8000) {
    if ((attncode & 0xfff) != 0xfff)
      fprintf(stderr,"***Server going down in %d minutes***\n",attncode&0xfff);
    else
      fprintf(stderr,"***Server shutdown canceled***\n");
  }
  printf("***ATTN*** Session %d, code %x\n",(int)sessid,(int)attncode);
}


/*
 * Dump a PAP status message
*/
dumppstr(pstr)
unsigned char *pstr;
{
  int len = (int)(pstr[0]);
  unsigned char *s = &pstr[1];

  while (len--) {
    if (isprint(*s))
      putchar(*s);
    else
      printf("\\%o",*s&0xff);
    s++;
  }
  return(((int)(pstr[0]))+1);
}

dumpstr(str)
byte *str;
{
  if (str == NULL)
    return;
  while (*str) {
    if (isprint(*str))
      putchar(*str);
    else
      printf("\\%o",*str&0xff);
    str++;
  }
}

tryit(srn)
int srn;
{
  char *getline(), *getpass();
  char line[256];
  int i, n;
  dword curdirid, cr;
  word volid;
  GetVolParmsReplyPkt op;
  char *tokstart;

  setbuf(stdin,NULL);
  printf("User name?\n");
  if (getline(line)==NULL)
    return;
  if (!dologin(srn,line,line[0] == '\0' ? ((char *)0) : getpass("Password:"))) {
    fprintf(stderr, "Login failed!\n");
    return;
  }
  getsrvp(srn);

  if (vpr->gspr_nvols == 0) {
    printf("There are no volumes, you can't do anything!\n");
    printf("Bye.\n");
    return;
  }
    
  if (vpr->gspr_nvols == 1) {
    strcpy(line, (char *)vpr->gspr_volp[0].volp_name);
  } else {
    printf("What volume?\n");
    if (getline(line) == NULL)
      return;
  }
  if (eFPOpenVol(srn, (word)VP_VOLID, line, NULL, &op, &cr) < 0) {
    if (cr != 0)
      aerror("Openvol",cr);
    printf("Volume open failed\n");
    return;
  }
  if (cr != 0)
    aerror("Openvol",cr);
  volid = op.gvpr_volid;
  printf("VOLID = %x\n",volid);

  curdirid = 0x2;		/* root directory */
  do {
    putchar('>'); fflush(stdout);
    if (getline(line) == NULL)
      break;
    if ((tokstart = index(line, ' ')) != NULL) {
      while (*tokstart == ' ') tokstart++;
    }
    switch (line[0]) {
    case 'x':
      {
	FileDirParm epar;
	dostat(srn, volid, 1, "ds", &epar);
	printf("dir: %x\n", epar.fdp_parms.dp_parms.dp_dirid);
	printf("pdir: %x\n", epar.fdp_pdirid);
      }
      break;
    case 'd':
      if (strncmp(line, "delete", 6) == 0) {
	dodelete(srn, volid, curdirid, tokstart);
	printf("okay\n");
	break;
      }
      n = nchild(srn, volid, curdirid, tokstart);
      if (n >= 0) {
	printf("%d items\n",n);
	dodir(srn, volid, curdirid, n+1, tokstart, TRUE);
	printf("okay\n");
      } else printf("nothing there?");
      break;
    case 'l':
      n = nchild(srn, volid, curdirid, tokstart);
      if (n >= 0) {
	printf("%d items\n",n);
	dodir(srn, volid, curdirid, n+1, tokstart, FALSE);
	printf("okay\n");
      }
      break;
    case 'c':
      if (strncmp(line, "croot",5) == 0) {
	curdirid = 0x2;
	printf("back to root\n");
	break;
      }
      movetodir(srn, volid, curdirid, tokstart, &curdirid);
      printf("okay\n");
      break;
    case 'g':
      getfile(srn, volid, curdirid, tokstart);
      printf("okay\n");
      break;
    case 'p':
      putfile(srn, volid, curdirid, tokstart);
      printf("okay\n");
      break;
    case 'm':
      if (strncmp("mkdir",line, 5) == 0) {
	domakedirectory(srn, volid, curdirid, tokstart);
	printf("okay\n");
	break;
      }
      break;
    case '?':
      printf("Valid commands are:\n");
      printf("d[irectory] - long file listing\n");
      printf("l[s] - short file listing\n");
      printf("c[d] - connect to directory\n");
      printf("g[et] - get a file\n");
      printf("p[ut] - put a file\n");
      break;
    }
  }  while (1);
}

dodelete(srn, volid, dirid, path)
int srn;
word volid;
dword dirid;
byte *path;
{
  dword cr;
  int comp;

  comp = eFPDelete(srn, volid, dirid, makepath(path), &cr);
  if (comp < 0)
    return(FALSE);
  if (cr != noErr)
    aerror("FPDelete");
  return(TRUE);
}

domakedirectory(srn, volid, dirid, path)
int srn;
word volid;
dword dirid;
byte *path;
{
  dword cr, newdirid;
  int comp;

  comp = eFPCreateDir(srn, volid, dirid, makepath(path), &newdirid, &cr);
  if (comp < 0)
    return(FALSE);
  if (cr != noErr)
    aerror("FPCreateDir");
  return(TRUE);
}

dologin(srn, user, passwd)
int srn;
char *user, *passwd;
{
  char pbuf[9];
  int i;
  dword cr = 0;
  int uam = 2;

  if (!valid_uams[uam])
    uam = 1;
  if (!valid_uams[uam])
    uam = 0;
  if (user[0] == '\0')
    uam=0;
  else {
    strcpy(pbuf, passwd);
    for (i=strlen(pbuf); i < 8; i++)
      pbuf[i] = '\0';
  }
  eFPLogin(srn, user, pbuf, uam, &cr);
  if (cr != 0) {
    if (cr == aeMiscErr) {
      fprintf(stderr,"Bad user name\n");
      return(FALSE);
    }
    aerror("Login",cr);
    return(FALSE);
  }
  return(TRUE);
}

dodir(srn, volid, dirid, toalloc, path, verbose)
int srn;
word volid;
dword dirid;
int toalloc;
byte *path;
int verbose;
{
  FileDirParm *epar, *ep;	/* room for 100 entries */
  FileParm *fp;
  DirParm *dp;
  int cnt,i,j,comp, totcnt;
  dword cr;
  int idx = 0;
  byte *newpath;

  if (toalloc == 0)
    return;
  if ((epar = (FileDirParm *)malloc(toalloc*sizeof(FileDirParm))) ==
      (FileDirParm *)NULL) {
    fprintf(stderr, "Out of memory\n");
    return;
  }
  totcnt = 0;
  cr = 0L;
  newpath =  makepath(path);
  while (cr != aeObjectNotFound && idx < toalloc) {
    comp = eFPEnumerate(srn, volid, dirid, newpath, idx+1,
		       (word)0x74e, (word)0x1f42,
		       epar+idx, toalloc-idx, &cnt, &cr);
    if (comp != noErr) {
      printf( "bad comp %d\n",comp);
      free(epar);
      return;
    }		      
    if (cr != 0 && cr != aeObjectNotFound) {
      aerror("FPEnumerate",cr);
      free(epar);
      return;
    }
    if (cr == 0) {
      idx += cnt;
      totcnt += cnt;
    }
  }
  printf("Count = %d\n",totcnt);
  for (ep = &epar[0], i = 0 ; i < totcnt; i++, ep++) {
    dumppstr(ep->fdp_lname);
    if (ep->fdp_flg) {
      dp = &ep->fdp_parms.dp_parms;
      printf(" [dir - %d, %d, %d entries]\n",
	     dp->dp_dirid, ep->fdp_pdirid,dp->dp_nchild);
      printf("  owner = %x, group = %x, accright = %x\n",
	  dp->dp_ownerid,		/* owner id */
	  dp->dp_groupid,		/* group id */
	  dp->dp_accright);		/* access rights */
    } else {
      long clock;
      char *created;
      char *ctime();

      fp = &ep->fdp_parms.fp_parms;
      printf(" [file %d, %d], ", fp->fp_fileno, ep->fdp_pdirid);
      printf("res %d + dat %d = %d",
	     fp->fp_rflen, fp->fp_dflen, fp->fp_rflen+fp->fp_dflen);
      if (verbose) {
	long clock;
	clock = ep->fdp_cdate;
	created = ctime(&clock);
	created[24] = '\0';
	printf(", cr: %s, ", created);
	clock = ep->fdp_mdate;
	printf("mo: %s", ctime(&clock));
      }
      else
	putchar('\n');
    }
  }
  free(epar);
}


dogetdir(srn, volid, dirid, toalloc)
int srn;
word volid;
dword dirid;
int toalloc;
{
  FileDirParm *epar, *ep;	/* room for 100 entries */
  FileParm *fp;
  DirParm *dp;
  int cnt,i,j,comp, totcnt;
  dword cr;
  int idx = 0;
  byte path[256];

  if (toalloc == 0)
    return;
  if ((epar = (FileDirParm *)malloc(toalloc*sizeof(FileDirParm))) ==
      (FileDirParm *)NULL) {
    fprintf(stderr, "Out of memory\n");
    return;
  }
  totcnt = 0;
  cr = 0L;
  path[0] = 0;
  while (cr != aeObjectNotFound && idx < toalloc) {
    comp = eFPEnumerate(srn, volid, dirid, path, idx+1,
		       (word)0x74e, (word)0x1f42,
		       epar+idx, toalloc-idx, &cnt, &cr);
    if (comp != noErr) {
      printf( "bad comp %d\n",comp);
      free(epar);
      return;
    }		      
    if (cr != 0 && cr != aeObjectNotFound) {
      aerror("FPEnumerate",cr);
      free(epar);
      return;
    }
    if (cr == 0) {
      idx += cnt;
      totcnt += cnt;
    }
  }
  printf("Count = %d\n",totcnt);
  for (ep = &epar[0], i = 0 ; i < totcnt; i++, ep++) {
    printf("Copying: ");
    dumppstr(ep->fdp_lname);
    fflush(stdout);
    if (FDP_ISDIR(ep->fdp_flg)) {
      putchar('\n');
      dp = &ep->fdp_parms.dp_parms;
      dogetdir(srn, volid, dp->dp_dirid, dp->dp_nchild);
    } else {
      fp = &ep->fdp_parms.fp_parms;
      cpyp2cstr(path, ep->fdp_lname);
      getfile(srn, volid, dirid, path);
      printf("...Okay\n");
    }
  }
  free(epar);
}

getfile(srn, volid, dirid, path)
int srn;
word volid;
dword dirid;
byte *path;
{
  int len, i;
  int myfd;
  char buf[512];
  char *p;
  byte *usepath = makepath(path);
  FileDirParm epar;
  char *cvtmactounix();

  if (path == NULL)		/* NULL name means no file */
    return;
  if ((p = rindex((char *)path, ':')) == NULL) /* any ':'s in name? */
    p = (char *)path;		/* skip space */
  else
    p++;			/* skip over colon */
  
  p = cvtmactounix(p);
  if (!finderinfo(srn, volid, dirid, path, &epar) < 0) {
    return;
  }
  if (FDP_ISDIR(epar.fdp_flg)) {
    dogetdir(srn, volid, epar.fdp_parms.dp_parms.dp_dirid,
	     epar.fdp_parms.dp_parms.dp_nchild);
    return;
  }
  strcpy(buf, "tvol/.finderinfo/");
  strcat(buf, p);
  if ((myfd = open(buf,O_CREAT|O_TRUNC|O_RDWR, 0660)) < 0) {
    perror(buf);
    return;
  }
  write(myfd, epar.fdp_finfo, sizeof(epar.fdp_finfo));
  close(myfd);
  strcpy(buf, "tvol/");
  strcat(buf, p);
  if ((myfd = open(buf,O_CREAT|O_TRUNC|O_RDWR, 0660)) < 0) {
    perror(buf);
    return;
  }
  if (!dogetfile(srn, myfd, volid, dirid, usepath, FALSE)) {
    close(myfd);
    unlink(p);			/* get rid of incomplete file */
    return;
  }
  strcpy(buf,"tvol/.resource/");
  strcat(buf,p);
  if ((myfd = open(buf,O_CREAT|O_TRUNC|O_RDWR,0770)) < 0) {
    perror(buf);
    return;
  }
  dogetfile(srn, myfd, volid, dirid, usepath, TRUE);
}

dogetfile(srn, myfd, volid, dirid, path, type)
int myfd;
int srn;
word volid;
dword dirid;
byte *path;
{
  char buf[atpMaxData*atpMaxNum];
  int len;
  int eof = 0;
  dword offset = 0, cr;
  word fd;
  int i, comp;
  FileDirParm epar;
  int toread;
  word bitmap;

  bitmap = type ? FP_RFLEN : FP_DFLEN;

  comp = eFPOpenFork(srn, volid, dirid, 0x1, path, type,
		     bitmap, &epar, &fd, &cr);
  if (comp < 0) {
    return(FALSE);
  }
  if (cr != noErr) {
    aerror("OpenFork", cr);
    return(FALSE);
  }
  toread= type ? epar.fdp_parms.fp_parms.fp_rflen :
  epar.fdp_parms.fp_parms.fp_dflen;
  do {
    /* try to read more in case file size is off */
    len = eFPRead(srn, fd, buf, sizeof(buf), sizeof(buf), offset, &cr);
    if (len <= 0) {
      if (cr != aeEOFErr)
	aerror("FPRead",cr);
      break;
    }
    write(myfd, buf, len);
    offset += len;
    toread -= len;
  } while (cr != aeEOFErr);
  close(myfd);
  eFPCloseFork(srn, fd, &cr);
  if (cr != 0)
    aerror("CloseFork",cr);
  return(TRUE);
}

int
ugetsize(fd)
int fd;
{
  struct stat sbuf;

  if (fstat(fd, &sbuf) < 0) {
    perror("fstat");
    return(0);
  }
  return(sbuf.st_size);
}

putfile(srn, volid, dirid, path)
int srn;
word volid;
dword dirid;
byte *path;
{
  int len, i;
  int myfd;
  char buf[512];
  char *p;
  byte *usepath;
  FileDirParm epar;

  if (path == NULL)		/* NULL name means no file */
    return;
  if ((p = rindex((char *)path, '/')) == NULL)
    p = (char *)path;
  else
    p++;			/* skip over the slash */
  usepath = makepath(p);
  strcpy(buf, "tvol/");
  strcat(buf, (char *)path);
  if ((myfd = open(buf,O_RDONLY)) < 0) {
    perror(buf);
    return;
  }
  if (!docreatefile(srn, volid, dirid, usepath)) {
    close(myfd);
    return;
  }
  strcpy(buf, "tvol/.finderinfo/"); /* finder info file? */
  strcat(buf, (char *)path);
  {
    int tmpfd ;
    if ((tmpfd = open(buf,O_RDONLY)) >= 0) {
      read(tmpfd, epar.fdp_finfo, sizeof(epar.fdp_finfo));
      close(tmpfd);
    } else {
      bzero(epar.fdp_finfo, sizeof(epar.fdp_finfo));
      strcpy((char *)epar.fdp_finfo, "TEXTEDIT");
    }
  }
  if (setfinderinfo(srn, volid, dirid, p, &epar) != noErr)
    return;
  if (!doputfile(srn, myfd, volid, dirid, usepath, FALSE)) {
    close(myfd);
    return;
  }
  strcpy(buf,"tvol/.resource/");
  strcat(buf,(char *)path);
  if ((myfd = open(buf,O_RDONLY)) < 0) {
    perror(buf);
    return;
  }
  doputfile(srn, myfd, volid, dirid, usepath, TRUE);
}

docreatefile(srn, volid, dirid, path)
int srn;
word volid;
dword dirid;
byte *path;
{
  dword cr;
  eFPCreateFile(srn, volid, dirid, TRUE, path, &cr);
  if (cr != noErr) {
    if (cr == aeObjectExists)
      return(TRUE);
    aerror("createfile", cr);
    return(FALSE);
  }
  return(TRUE);
}

doputfile(srn, myfd, volid, dirid, path, type)
int srn;
int myfd;
word volid;
dword dirid;
byte *path;
int type;
{
  char wbuf[atpMaxData*atpMaxNum];
  int towrite, comp, written;
  dword offset;
  word fd;
  dword cr;
  int retry = 0;
  FileDirParm epar;
  int wlen;

  comp = eFPOpenFork(srn, volid, dirid, 0x2, path, type, (word)0, &epar,
		     &fd, &cr);
  if (comp < 0) {
    return(FALSE);
  }
  if (cr != noErr) {
    aerror("OpenFork", cr);
    return(FALSE);
  }
  
  offset = 0;			/* make sure we start at zero */
  towrite = ugetsize(myfd);
  wlen = written = 0;		/* start these at zero... */
  do {
    wlen -= written;		/* get number of bytes written */
    wlen = read(myfd, wbuf, (atpMaxData)*(atpMaxNum) - wlen);
    if (wlen <= 0) {
      break;
    }
    printf("Write %d, offset %d, left %d\n", wlen, offset, towrite);
    comp = eFPWrite(srn, fd, wbuf, wlen, towrite, &written, &offset, &cr);
    if (comp < 0) {
      fprintf(stderr,"FPWRite err %d, %d\n",cr, comp);
      eFPCloseFork(srn, fd, &cr);
      return(FALSE);
    }
    if (cr < 0) {
      fprintf(stderr,"FPWrite err %d\n",-cr);
      break;
    }
    printf("Wrote %d, last offset %d\n", written, offset);
    if (written > wlen) {
      fprintf(stderr, "wrote more than requested?\n");
      break;
    }
    towrite -= written;
  } while (1);
  eFPCloseFork(srn, fd, &cr);
  close(myfd);
  return(TRUE);
}

nchild(srn,volid,dirid,path)
int srn;
word volid;
dword dirid;
byte *path;
{
  FileDirParm epar;

  if (dostat(srn, volid, dirid, path, &epar) != noErr)
    return;
  if (FDP_ISDIR(epar.fdp_flg))
    return(epar.fdp_parms.dp_parms.dp_nchild);
  else
    return(-1);
}

setfinderinfo(srn, volid, dirid, path, epar)
int srn;
word volid;
dword dirid;
byte *path;
FileDirParm *epar;
{
  int comp;
  dword cr;
  byte *pp = makepath(path);

  comp = eFPSetFileParms(srn, volid, dirid, (word)FP_FINFO, pp, epar, &cr);
  if (comp != noErr)
    return(comp);
  if (cr != noErr) {
    aerror("setfinderinfo: GetFileDirParms",cr);
    return(-1);
  }
  return(noErr);
}

finderinfo(srn,volid,dirid,path, epar)
int srn;
word volid;
dword dirid;
byte *path;
FileDirParm *epar;
{
  return(dostat(srn, volid, dirid, path, epar));
}

movetodir(srn,volid,dirid,path,newdirid)
int srn;
word volid;
dword dirid;
char *path;
dword *newdirid;
{
  FileDirParm epar;

  if (dostat(srn, volid, dirid, path, &epar) != noErr)
    return(FALSE);

  if (FDP_ISDIR(epar.fdp_flg))
    *newdirid = epar.fdp_parms.dp_parms.dp_dirid;
  else
    return(FALSE);
  return(TRUE);
}

dostat(srn, volid, dirid, path, epar)
int srn;
word volid;
dword dirid;
byte *path;
FileDirParm *epar;
{
  int comp;
  dword cr;
  byte *pp = makepath(path);

  comp = eFPGetFileDirParms(srn, volid, dirid, (word)FP_FILNO|FP_FINFO,
			    (word)DP_DIRID|DP_CHILD|DP_FINFO, pp, epar, &cr);
  if (comp != noErr)
    return(comp);
  if (cr != noErr) {
    aerror("GetFileDirParms",cr);
    return(-1);
  }
}

getsrvp(srn)
int srn;
{
  dword cr;
  int comp,i;

  comp = eFPGetSrvrParms(srn, &vpr, &cr);
  if (comp < 0) {
    fprintf(stderr,"GetSrvrParms fails with completetion code %d\n",comp);
    return(0);
  }
  if (cr != 0) {
    fprintf(stderr, "getSrvrParms fails with code %d\n",-cr);
    aerror("",cr);
    return(0);
  }
  printf("Found %d volumes\n",vpr->gspr_nvols);
  for (i = 0; i < (int)vpr->gspr_nvols; i++) {
    printf("Volume ");
    dumpstr(vpr->gspr_volp[i].volp_name);
    printf(" has %s password\n", vpr->gspr_volp[i].volp_flag ? "a" : "no");
  }
  {
    long clock;
    printf("Returned server: %ld\n",vpr->gspr_time);
    clock = vpr->gspr_time;
    printf("Server time %s",ctime(&clock));
  }
}

/*
 * get a line from stdin - really assumed to be a terminal 
 *
 * we go through contortions to ensure that we don't block on reads
 * this is because we MUST run protocol
 *
*/
fd_set aset ;
struct timeval t = {0, 250};
getinit()
{
  t.tv_sec = 0;
  t.tv_usec = 250;		/* 1 second? */
  FD_ZERO(&aset);
  setbuf(stdin, NULL);		/* make sure! */
}

int
getch()
{
  int c;
  int rdy;

  do {
    FD_SET(fileno(stdin), &aset);
    rdy = select(fileno(stdin)+1, &aset, 0, 0, &t);
    if (rdy > 0) {
      break;
    }
    abSleep(1, TRUE);		/* .25 second too */
  } while (1);
  c = getchar();
  return(c);
}

char *
getline(bp)
char *bp;
{
  static int eof = FALSE;
  int i, c;

  if (eof)
    return(NULL);
  getinit();
  for (; (c = getch()) != EOF && c != '\n'; bp++)
    *bp = c;
  if (c == EOF) {
    eof = TRUE;
    *bp = '\0';
    return(bp);
  }
  *bp = '\0';
  return(bp);
}

#ifdef notdef
char *
getpassword(prompt)
char *prompt;
{
  struct sgttyb ttystat;
  int saved_flags, i, c;
  static char pass[9];
  int (*sig)();
  char *p;
  
  sig = signal(SIGINT, SIG_IGN);
  gtty(fileno(stdin), &ttystat);
  saved_flags = ttystat.sg_flags;
  ttystat.sg_flags &= ~ECHO;
  stty(fileno(stdin), &ttystat);
  fprintf(stderr,"%s",prompt);
  for (i=0, p=pass; (c=getch()) != '\n' && c != EOF && i < 8; i++, p++)
    *p = c;
  *p = '\0';
  fprintf(stderr, "\n");
  ttystat.sg_flags = saved_flags;
  stty(fileno(stdin), &ttystat);
  signal(SIGINT, sig);
  return(pass);
}
#endif

char *
cvtmactounix(path)
byte *path;
{
  static char buf[1025];
  static char *hexdigits = "0123456789abcdef";
  char *p, *bp, c;
  int i, len, bcnt;

  if ((p = rindex((char *)path, ':')) == NULL)
    p = (char *)path;		/* get right name */
  else
    p++;			/* skip over colon */
  len = strlen(p);		/* length of remaining */
  for (i=0, bp=buf, bcnt = 0; i < len; i++) {
    c = *p++;
    if (!iscntrl(c) && c != ' ' && isprint(c)) {
      *bp++ = c;
      if (++bcnt >= 1024)
	break;
      continue;
    }
    bcnt += 3;
    if (bcnt >= 1024)
      break;
    *bp++ = '\\';		/* \\ */
    *bp++ = hexdigits[(c&0xf0) >> 4];
    *bp++ = hexdigits[(c&0xf)];
  }
  *bp++ = '\0';
  return(buf);
}

/*
 * this is a dummy routine for abasp.c
 *
 */

dsiTCPIPCloseSLS()
{
  return(noErr);
}
