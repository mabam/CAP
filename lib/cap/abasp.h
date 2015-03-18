/*
 * $Author: djh $ $Date: 91/02/15 22:45:42 $
 * $Header: abasp.h,v 2.1 91/02/15 22:45:42 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * abasp.c - Appletalk Session Protocol
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July 28, 1986    CCKim	Created
 *  Aug   4, 1986    CCKim	Verified: level 0
*/


/* definition of ATP User Bytes */
typedef union {
  struct {			/* used by Tickle, CloseSess, GetStat, etc. */
    byte b1;
    byte b2;
    word data;
  } std;
  dword CmdResult;		/* 4 bytes, for vax, must use ntohl, htonl */
} ASPUserBytes;


/* Definition of ASP Server Listener Socket block */
typedef struct {
  int active;
  char *ssb;
  int ssbl;
  AddrBlock addr;
  ABusRecord abr;		/* for getrequest */
} ASPSSkt;
#define NUMSASP 1

/* Definition of ASP Station Socket block */
typedef struct {
  int active;			/* true if in use */
  int state;			/* state of connection */
  int type;			/* server/client */
  int SLSRefNum;		/* which SLS owns us */
  int SessRefNum;		/* our session refnum - really */
				/* redundant info right now */
  AddrBlock addr;		/* complete address of remote ss */

  ABusRecord tickle_abr;	/* for tickle */
  int tickling;			/* mark whether we are tickling */

  ABusRecord rabr;		/* request abr */
  word reqdata;			/* for writedata data */
  int ss;			/* service socket */
  byte SessID;			/* session id (per sls) */
  int (*attnroutine)();		/* attention callback */
  int next_sequence;		/* for write/command */
  QElemPtr wqueue;		/* queue of write's */
  int *comp;			/* pointer to completion var */

  int (*tickle_timeout_user)();	/* for tickle timeout */
  int ttu_arg;			/* an argument to call with */

#ifdef ASPPID
  int pid;			/* for spfork */
#endif
} ASPSkt;
#define NUMASP 5

/* Definitions for possible asp connection states */
#define SP_INACTIVE 0
#define SP_STARTING 1
#define SP_STARTED 2
#define SP_HALFCLOSED 3

/* ASP Connection types */
#define SP_SERVER 0
#define SP_CLIENT 1

/* Internal ASP queue elements - used to "remember" things */
typedef struct ASPQE {
  QElem link;
  int type;			/* what was the command type */
  int SessRefNum;		/* traceback */

  word seqno;

  struct ABusRecord abr;	/* request abr */
  BDS bds[atpMaxNum];
  word availableBufferSize;	/* data for wrtcontinue packet */

  struct ASPQE **ReqRefNum;
  int *SPReqType;
  int *ActRcvdReqLen;
  int *ActLenRcvd;
  int *comp;

  int *ActRcvdReplyLen;
  dword *CmdResult;
  int *ActRcvdStatusLen;

  char *WriteData;
  int WriteDataSize;
  int *ActLenWritten;
} ASPQE;			/* asp queue element */


/* QUeue element types */
#define tSPGetRequest 0
#define tSPCmdReply 1
#define tSPWrtContinue 2
#define tSPWrtReply 3
#define tSPAttention 4
#define tSP_Special_DROP 5
#define tSPGetStat      6
#define tSPOpenSess 7
#define tSPCommand 8
#define tSPWrite 9
#define tSPWrite2 10
#define tSPClose 11

/* Defines write v.s. std. queue element */
#define ASPAQE 0
#define ASPAWE 1

#define create_aspaqe()  create_aq(ASPAQE, (ASPSkt *)0)
#define create_aspawe(as) create_aq(ASPAWE, (ASPSkt *)(as))
#define delete_aspaqe(item) delete_aq((ASPQE *)(item),ASPAQE,(ASPSkt *)0)
#define delete_aspawe(item,as) delete_aq((ASPQE *)(item),ASPAWE,(ASPSkt *)(as))
#define get_aspaqe() get_aq(ASPAQE, (ASPSkt *)0)
#define get_aspawe(as) get_aq(ASPAWE, (ASPSkt *)(as))

/* should go into cap_conf.h? */
#define ASPTICKLETIMEOUT	 30*4	/* defined by Spec (30 seconds) */
#define ASPCONNECTIONTIMEOUT	120*4	/* defined by Spec ( 2 minutes) */
#define ASPGETSTATTIMEOUT	  2*4	/* arbitrary (2 seconds) */
#define ASPOPENSESSTIMEOUT	  2*4	/* arbitrary (2 seconds) */
#define ASPCLOSESESSIONTIMEOUT	  2*4	/* arbitrary (2 seconds) */
#define ASPCOMMANDTIMEOUT	  2*4	/* arbitrary (2 seconds) */
#define ASPWRITETIMEOUT		  2*4	/* arbitrary (2 seconds) */
#define ASPATTENTIONTIMEOUT	  2*4	/* arbitrary (2 seconds) */

