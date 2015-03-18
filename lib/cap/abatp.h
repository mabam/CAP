/*
 * $Author: djh $ $Date: 1996/03/11 09:47:07 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abatp.h,v 2.3 1996/03/11 09:47:07 djh Rel djh $
 * $Revision: 2.3 $
 *
 */

/*
 * abatp.c - Appletalk Transaction Protocol header file (internal only)
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  June 30, 1986    CCKim	Created
 *
 */

#define atpheaderlength (atpSize+lapSize+ddpSize)


#define atpCodeMask	0xc0	/* to get controls */
#define	atpReqCode	0x40
#define	atpRspCode	0x80
#define	atpRelCode	0xC0
#define	atpXO		0x20
#define	atpEOM		0x10
#define	atpSTS		0x08
#define	atpSendChk	0x01
#define	atpTIDValid	0x02
#define	atpFlagMask	0x3F
#define	atpControlMask	0xF8

typedef struct {
  byte lapddp[lapSize+ddpSize];
  ATP atp;
} ATPpkt;


typedef struct TCB {
  QElem link;
  ATP atp;			/* atp header */
  int skt;			/* local side socket */
  int (*callback)();
  caddr_t cbarg;			/* call back argument */
  ABusRecord *abr;
} TCB;

#define NUMTCB ddpMaxSkt	/* max connections */


/* 
 * Request Control Block
 *
*/

typedef struct {
  QElem link;			/* point to queue header */
  int atpsocket;		/* socket request went out on */
  ABusRecord *abr;		/* ABusRecords */
  int (*callback)();
  caddr_t cbarg;			/* call back argument */
} RqCB;

#define NUMRqCB 3		/* should suffice */

/*
 * Response Control Block
 *
 * Note: we don't need to copy the reponse data because the sndresponse
 * routines will not complete until the rel packet is received or
 * we get rscb timeout.  Thus, the user MUST NOT reuse the buffers until
 * the given routine completes!
 *
*/
typedef struct {
  QElem link;			/* point to queue header */
  struct timeval ctime;		/* time created */
  int atpsocket;		/* socket of rsp  */
  int atpTransID;		/* requesting transaction id */
  AddrBlock atpAddress;		/* address of remote */
  ABusRecord *abr;		/* pointer to abus record */
  int (*callback)();
  caddr_t cbarg;			/* call back argument */
} RspCB;

#define NUMRspCB 20

#define RESPONSE_CACHE_TIMEOUT 4*30 /* timeout is 30 seconds */

typedef struct {
  int inuse;			/* zero if not */
  int skt;
  AddrBlock addr;		/* filter */
  int usecount;			/* times in use */
} AtpSkt;

#define NUMATPSKT 5		/* up to 5 responding circuits */

private RspCB *find_rspcb();
private RspCB *find_rspcb_abr();
private RspCB *create_rspcb();
private RspCB *find_rspcb_skt();
private int delete_rspcb();

private TCB *create_tcb();
private delete_tcb();
private TCB *find_tcb();
private TCB *find_tcb_abr();

private RqCB *create_rqcb();
private RqCB *find_rqcb_abr();
private RqCB *find_rqcb();
private delete_rqcb();

private AtpSkt *create_atpskt();
private AtpSkt *find_atpskt();
private int delete_atpskt();
