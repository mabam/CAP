/* "$Author: djh $ $Date: 1995/08/28 10:38:35 $" */
/* "$Header: /mac/src/cap60/applications/lwsrv/RCS/query.h,v 2.1 1995/08/28 10:38:35 djh Rel djh $" */
/* "$Revision: 2.1 $" */

/*
 * query.h -  handle the LaserWriter 8 printer queries
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#ifndef _QUERY_H_
#define	_QUERY_H_

struct printer_instance {
  char *dictdir;	/* dictionary directory	(-a) */
  time_t lastried;	/* dictdir last modified */
  KVTree **dictlist;	/* dictionary list */
  int dochecksum;	/* doing checksum	(-k) */
  int hflag;		/* print banner		(-h) */
  int rflag;		/* don't remove file	(-r) */
  char *tracefile;	/* tracefile		(-t) */
  int eflag;		/* maybe "eexec" in code(-e) */
  int toptions;		/* Transcript options	(-T) */
  int dsc2;		/* DSC2 compatibility	(-A) */
#ifdef LPRARGS
  List *lprargs;	/* lpr arguments	(-L) */
#endif LPRARGS
#ifdef PASS_THRU
  int passthru;		/* pass through		(-P) */
#endif PASS_THRU
#ifdef NeXT
  char* nextdpi;	/* NeXT resolution	(-R) */
#endif NeXT
#ifdef LW_TYPE
  char *prttype;	/* AppleTalk type	(-Y) */
#endif LW_TYPE
  int capture;		/* capture procset	(-N) */
/* NBP and UNIX name required */
  char *prtname;	/* NBP registered printername */
  char *unixpname;	/* UNIX printer name */
/* Query list */
  KVTree **querylist;
/* AppleTalk stuff */
  PAPStatusRec statbuf;
  int srefnum;		/* returned by SLInit */
  char nbpbuf[128];	/* registered name:type@zone */
  int rcomp;		/* flag: waiting for job? */
  int cno;		/* connection number of next job */
  int children;		/* number of active children */
};

void SendMatchedKVTree(/* PFILE *pf, KVTree **list, char *prefix,
  char *str */);
void SendMatchedResources(/* PFILE *pf, List *list, char *prefix,
  char *str */);
void SendQueryResponse(/* PFILE *pf, List *list */);
void SendResourceKVTree(/* PFILE *pf, KVTree **list, char *prefix */);
void SendResourceList(/* PFILE *pf, List *list, char *prefix */);
void SendQueryResponse(/* PFILE *pf, List *list */);
char *nextoken(/* char **cp */);

#endif /* _QUERY_H_ */
