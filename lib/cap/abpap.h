/*
 * $Author: djh $ $Date: 1995/08/29 01:52:43 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abpap.h,v 2.4 1995/08/29 01:52:43 djh Rel djh $
 * $Revision: 2.4 $
 *
 */

/*
 * abpap.h - Printer Access protocol access definitions
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Edit History:
 *
 *  June 22, 1986    CCKim	Created
 *
 */

#define papOpenConn 1
#define papOpenConnReply 2
#define papSendData 3
#define papData 4
#define papTickle 5
#define papCloseConn 6
#define papCloseConnReply 7
#define papSendStatus 8
#define papStatusReply 9

#define min_PAPpkt_size (lapSize+ddpSize+atpSize)

typedef struct {		/* in ATP user bytes */
    byte connid;		/* connection ID */
    byte PAPtype;		/* type of PAP packet */
    union {
      struct {			
	byte eof;		/* eof in Data (TResp) */
	byte unused;		/* unused */
      } std;
      word seqno;		/* send data sequence number */
    } other;
} PAPUserBytes;
  
typedef union {				       /* in ATP data area */
    struct {				       /* OpenConn (TReq) */
      byte atprskt;			       /* ATP responding socket # */
      byte flowq;			       /* flow quantum */
      word wtime;			       /* wait time */
    } papO;
    struct {				       /* OpenConnReply (TResp) */
      byte atprskt;			       /* ATP responding socket # */
      byte flowq;			       /* flow quantum */
      word result;			       /* result */
      byte status[256];		       /* pascal string */ 
    } papOR;
    struct {				       /* Status (TResp) */
      byte unused[4];			       /* unused */
      byte status[256];		       /* status string */
    } papS;
} PAP;

typedef struct {
  byte lapddp[lapSize+ddpSize];
  ATP atp;
  PAP pap;
} PAPpkt;


typedef struct {
  int valid;			/* validity flag */
  u_short transID;
  u_char skt;
  int creditno;
} SDC;				/* outstand send data credit (to us) */


typedef struct {
  int valid;
  int active;
  BDS bds[8];
  ABusRecord abr;
  int *comp;
  int cno;
  int (*rspcallback)();
  u_long rspcbarg;
} PWR;				/* pap read request */


typedef struct {
  int valid;
  BDS bds[8];
  ABusRecord abr;
  int *eof;
  int *comp;
  int *datasize;
/*  int cno;  */
  int numbds;
  int (*callback)();
  u_long cbarg;
} PRR;				/* pap read request */

/* PAP queue element - for deferred events */
typedef struct {
  QElem link;			/* link to next for various things */
  int active;			/* 1 if active, 0 ow. */
  int state;			/* state of socket */

  int rrskt;			/* remote responding socket */
  int rflowq;			/* remote flow quantum */
  int papuseq;			/* remote pap data req. seq. used */
  int paprseq;			/* remote pap data request sequence recevied */

  int flowq;			/* our flow quantum */
  int papseq;			/* our pap data request sequence */
  int paprskt;			/* our responding socket */

  byte connid;			/* PAP connection id */

  QElemPtr rrhead;		/* head of list of outstanding read reqs */
  QElemPtr wrhead;		/* head of list of outstanding write reqs */
  PRR prr;			/* active pap read request */
  PWR pwr;			/* active pap write request */
  SDC sdc;			/* outstanding sdc */

  /* write only */
  ABusRecord request_abr;	/* used for requests on writes */
  int request_active;		/* indicates whether request is active */

  /* Tickle and opne */
  ABusRecord abr;		/* used for tickle and open */
  BDS bds[1];			/* same */

  /* used by open only */
  struct timeval wtime;		/* last tickle time (time packet read) */
  PAP po,por;			/* not used except for open */
  PAPStatusRec *statusbuff;	/* used by open and server */
  int *comp;

  /* used by getnextjob  only */
  AddrBlock addr;
  u_short transID;		/* atp transid of papopen */
} PAPSOCKET;

/* It does not make sense for NUMSPAP to be less than NUMPAP */
#define NUMPAP 12		/* 12 connections allowed at once */
#define NUMSPAP 10		/* Number of servers allowed */

#define PAP_UNUSED	-1
#define PAP_BLOCKED	0
#define PAP_UNBLOCKED	1
#define PAP_WAITING	2
#define PAP_ARBITRATE	3

/* pap socket states */
#define PAP_CLOSED	0	/* socket is closed */
#define PAP_OPENED	1	/* socket is open */
#define PAP_INACTIVE	3	/* socket in inactive */
#define PAP_OPENING	4	/* socket is opening */
#define PAP_GNJOPENING 5	/* socket being opened by server code */
#define PAP_GNJFOUND 6		/* job queued for socket */

#define nextpapseq(x) (x) = ((++(x) % 65536) == 0) ? 1 : (x) % 65536

/*
 * callback functions for open reply and status
 *
 */
extern byte *((*papopencallback)(/* int cno, AddrBlock *from, int result */));
extern byte *((*papstatuscallback)(/* int cno, AddrBlock *from */));

PAPSOCKET *cnotopapskt();
int ppskttocno();
int abskttocno();
void start_papc();
int ppgetskt();
