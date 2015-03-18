/*
 * $Author: djh $ $Date: 1994/10/24 06:42:08 $
 * $Header: /mac/src/cap60/netat/RCS/appletalk.h,v 2.8 1994/10/24 06:42:08 djh Rel djh $
 * $Revision: 2.8 $
*/

/*
 * appletalk.h - Appletalk definitions
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in 
 *   the City of New York.
 *
 * Portions Copyright (C) 1985, Stanford Univ. SUMEX project.
 * C language version (C) 1984, Stanford Univ. SUMEX project.
 * May be used but not sold without permission.
 *
 * Portions Copyright (C) 1984, Apple Computer Inc.
 *  Gene Tyacke, Alan Oppenheimer, G. Sidhu, Rich Andrews.
 *
 * Edit History:
 *
 *  June 13, 1986    Schilit	Created.
 *
 */

#include <netat/aberrors.h>
#include <netat/abqueue.h>
#include <netat/sysvcompat.h>	/* this definitely doesn't belong here!! */
/* but there's no way that I'm going through 50 files to fix it */

/* misc structures */

/* use these when protocol depends on actual number of bits somehow */
/* use ifdefs for different machines */
typedef unsigned char byte;
typedef char sbyte;
typedef unsigned short word;
typedef short sword;

#ifdef	__alpha
/* The alpha has 64bit longs */
typedef unsigned int dword;
typedef int sdword;
#else	__alpha
typedef unsigned long dword;
typedef long sdword;
#endif	__alpha

typedef int OSErr;

typedef struct {		/* BDS, buffer data structure */
	word	buffSize;
	char	*buffPtr;
	word	dataSize;
	dword	userData;
} BDS;
typedef BDS * BDSPtr;

typedef struct {		/* BDSType */
	BDS	a[8];
} BDSType;

typedef struct {		/* Str32 */
	byte	s[33];		/* make 33 in case we want a null byte */
} Str32;

#define	userBytes userData

typedef struct {		/* AddrBlock */
	word	net;
	byte	node;
	byte	skt;
} AddrBlock;


/*
 * For each protocol, its header structure, definitions, 
 * and control / status parameter block.
 */

/*
 * LAP protocol 
 *
*/
typedef struct {			/* LAP */
	byte	dst;
	byte	src;
	byte	type;
} LAP, LAPAdrBlock;

#define	dstNodeID	dst
#define	srcNodeID	src
#define	lapProtType	type

/* LAP definitions */
#define	lapShortDDP	1	/* short DDP type */
#define	lapDDP		2	/* DDP type */
#define lapMB		10	/* Masterbridge packet */

#define	lapSize		3	/* size of lap header */

/* LAP delivery mechanisms */

#define LAP_KIP		1	/* "standard" KIP	*/
#define LAP_MKIP	2	/* modified KIP for UAB	*/
#define LAP_ETALK	3	/* EtherTalk phase 1/2	*/
#define LAP_KERNEL	4	/* kernel EtherTalk	*/

/*
 * Masterbridge routing protocol
 *
*/
#define mbMBS  70		/* socket for masterbridge requests */
typedef struct {
  byte type;			/* type of packet */
  byte node;			/* destination node */
  word net;			/* destination net */
  dword bridge;			/* bridge address */
  dword src;			/* packet source (IP) */
				/* Maybe that should be a Appletalk address? */
				/* more general but less efficent */
} MBPACKET;

#define mbRRQ 50		/* route request packet */
#define mbRRP 51		/* route reply packet */
#define mbPING 52		/* ping packet */
#define mbPINGR 53		/* ping reply packet */


/*
 * DDP protocol 
 *
*/

typedef struct {			/* DDP */
	word 	length;
	word	checksum;
	word	dstNet;
	word	srcNet;
	byte	dstNode;
	byte	srcNode;
	byte	dstSkt;
	byte	srcSkt;
	byte	type;
} DDP;

typedef struct {		/* ShortDDP */
	word	length;
	byte	dstSkt;
	byte	srcSkt;
	byte	type;
} ShortDDP;

/* DDP definitions */
#define ddpMaxSkt       0xFF	/* max socket number */
#define	ddpMaxWKS	0x7F
#define	ddpMaxData	586
#define	ddpLengthMask	0x3FF
#define	ddpHopShift	10
#define	ddpSize		13	/* size of DDP header */
#define	ddpSSize	5
#define	ddpWKS		128	/* boundary of DDP well known sockets */
#define	ddpEWKS		64	/* start of experimental socket range */
#define	ddpRTMP		1	/* RTMP type */
#define	ddpNBP		2	/* NBP type */
#define	ddpATP		3	/* ATP type */
#define ddpECHO		4	/* ECHO type */
#define ddpRTMP_REQ	5	/* RTMP request */
#define ddpZIP		6	/* ZIP packet */
#define ddpADSP		7	/* ADSP packet */
#define	ddpIP		22	/* IP type */
#define	ddpARP		23	/* ARP type */

#define	ddpWKSUnix	768	/* start of unofficial WKS range on UNIX */
#define	ddpOWKSUnix	200	/* start of official WKS range on UNIX */
#define	ddpNWKSUnix	16384	/* start of non-WKS .. */
#define	ddpIPSkt	72	/* socket used by Dartmouth encapsulation */


/*
 * Start of the DDP Encapsulated protocols
 *
*/


/*
 * RTMP protocol defintions
 *
*/

typedef struct {		/* RTMP */
	word	net;
	byte	idLen;
	byte	id;		/* start of ID field */
} RTMP;

typedef struct {
	word	net;
	byte	hops;
} RTMPtuple;

#define	rtmpSkt		1	/* number of RTMP socket */
#define	rtmpSize	4	/* minimum size */
#define	rtmpTupleSize	3

/*
 * NBP/NIS protocol definitions
 *
*/

#define	nbpEquals		'='
#define	nbpWild			0xc5
#define	nbpStar			'*'
  
#define ENTITYSIZE 32		/* use up to 32 for data */
/* note str32 is really 33 bytes now */
typedef struct {		/* EntityName */
  Str32 objStr;
  Str32 typeStr;
  Str32 zoneStr;
} EntityName;

typedef struct {		/* NBP Table entry */
  AddrBlock addr;
  byte enume;			/* add enumerator - cck */
  EntityName ent;
} NBPTEntry;

typedef struct {		/* RetransType */
	int	retransInterval;
	int	retransCount;
} RetransType;


/*
 * Echo protocol definitions
 *
*/
#define echoRequest 1		/* echo Request cmd */
#define echoReply 2		/* echo reply cmd */
#define echoSkt 4		/* echo socket */


/*
 * Zone Information Protocol protocol definitions
 *
 */
#define zipZIS 0x6		/* Zone information socket */

/* define atp zip commands */
#define zip_GetMyZone 7		/* get my zone command */
#define zip_GetZoneList 8	/* get zone list command */
#define zip_GetLocZones 9	/* get local zone list */

/* atp user bytes for zip commands */
/* no reply struct since return is packed array of pascal strings */

typedef struct zipUserBytes {
  byte zip_cmd;			/* zip command (LastFlag on return) */
  byte zip_zero;		/* always zero */
  word zip_index;		/* zip index (count on return) */
} zipUserBytes;

/*
 * Appletalk Transport Protocol protocol definitions
 *
*/

#define	atpSize		8	/* sizeof struct ATP */

typedef byte BitMapType;
typedef dword atpUserDataType;

typedef union {
  byte bytes[4];
  word words[2];
} inadword;

typedef struct {			/* ATP */
	byte control;
	BitMapType bitmap;
	word transID;
	atpUserDataType userData;
} ATP;

#define	atpMaxNum	8
#define	atpMaxData	578
#define	atpNData	512+16	/* normal amount of data */

/*
 * ATP Client protocols
 *
*/

/*
 * Printer Access Protocol protocol definitions
 *
*/
/* PAPType Values: */
#define PAPSegSize 512

typedef struct {
  dword SystemStuff;		/* PAP internal use (not used) */
  byte StatusStr[256];		/* status string + 1 byte for length */
} PAPStatusRec;

#define papNoError 0		/* no error - connection open */
#define papPrinterBusy 0xffff	/* printer busy */

/*
 * Appletalk Session Protocol (ASP) definitions
 *
*/

/* FOR ASP VERSION 1.0 */
#define ASP_PROTOCOL_VERSION 0x0100

/* ASP Packet types - needed since spgetrequest returns */
#define aspCloseSession 1
#define aspCommand 2
#define aspGetStat 3
#define aspOpenSess 4
#define aspTickle 5
#define aspWrite 6
#define aspWriteData 7
#define aspAttention 8

/* ReqRefNum returned by SPGetRequest and used by SPWriteContinue, */
/* SPWrtReply and SPCmdReply is a pointer to a special command block */
/* Use ReqRefNumType to define a reqRefNum argument... */
typedef char * ReqRefNumType;

/*
 * ABusRecord definitions
 *
 */


/* abopcode defintions */
#define tNBPConfirm 20
#define tNBPLookUp 21
#define tNBPRegister 100
#define tNBPDelete 101
#define tNBPTickle 102
#define tNBPSLookUp 103 

typedef struct atpProto {
  byte atpSocket;		/* listening/responding socket number */
  AddrBlock atpAddress;		/* destination or source socket address */
  int atpReqCount;		/* request buffer size in bytes */
  char *atpDataPtr;		/* Pointer to request data */
  BDSPtr atpRspBDSPtr;		/* pointer to response buffer list */
  BitMapType atpBitMap;		/* transaction bitmap */
  int atpTransID;		/* transaction id */
  int atpActCount;		/* count of bytes received */
  atpUserDataType atpUserData;	/* user bytes */
#ifdef AIX
  unsigned fatpXO : 1,		/* exactly once boolean */
#else  AIX
  byte fatpXO : 1,		/* exactly once boolean */
#endif AIX
         fatpEOM : 1;		/* end of message boolean */
  byte atpTimeOut;		/* retry timeout interval in seconds */
  byte atpRetries;		/* number of retries - 255 max */
  byte atpNumBufs;		/* number of elements in response BDS */
				/* or number of response pkts sent */
  byte atpNumRsp;		/* number of of response pkts received */
				/* or sequence number (on send) */
  byte atpBDSSize;		/* number of elements in response bds */
  atpUserDataType atpRspUData;	/* user bytes sent or received in */
				/* transaction response */
  char *atpRspBuf;		/* Pointer to response buffer */
  int atpRspSize;		/* size of response message buffer */
} atpProto;


typedef struct lapProto {	/* LAP record for LAP calls */
  LAPAdrBlock lapAddress;	/* address for lap */
  word lapReqCount;		/* bytes to read/write */
  word lapActCount;		/* actual quantity from above operation */
  byte *lapDataPtr;		/* pointer to data */
} lapProto;

typedef struct ddpProto {	/* DDP record for DDP calls */
  word ddpType;		/* type field */
  word ddpSocket;		/* source socket */
  AddrBlock ddpAddress;		/* destination address */
  word ddpReqCount;		/* bytes to read/write */
  word ddpActCount;		/* actual quantity from above operation */
  byte *ddpDataPtr;		/* pointer to data buffer */
  word ddpNodeID;		/* beats me */
} ddpProto;

typedef struct nbpProto {
  QElem abQElem;		/* internal: queue links  */
  int abOpcode;			/* type of call */
  int abResult;			/* result code */
  dword abUserReference;	/* for programmer use */
  EntityName *nbpEntityPtr;	/* pointer to entity name */
  NBPTEntry *nbpBufPtr;		/* user storage for NBP entities */
  word nbpBufSize;		/* count of entries that fit in buffer */
  word nbpDataField;
  AddrBlock nbpAddress;
  RetransType nbpRetransmitInfo;
  int nbpMaxEnt;		/* internal: max entries */
  byte nbpID;			/* internal: transaction ID */
  int retranscount;		/* internal: modifiable retranscount */
} nbpProto;



typedef struct ABusRecord {
#ifdef notdefined		/* not needed */
  ABCallType abOpcode;		/* type of call */
#endif
  struct ABusRecord *abNext;	/* INTERNAL USE ONLY: used to link read queue*/
  int abResult;			/* result code */
  dword abUserReference;		/* for programmer use */
  union {			/* for various protocols */
    lapProto lap;
    ddpProto ddp;
    atpProto atp;
  } proto;
} ABusRecord;

typedef struct ABusRecord *abRecPtr;


/* IO Vector protocol levels for scatter gather processing */

#define IOV_LAP_LVL 0		/* lap index */
#define IOV_DDP_LVL 1		/* ddp index */
#define IOV_ATP_LVL 2		/* atp index */
#define IOV_ATPU_LVL 3		/* user data from atp index */

/* size of iov's */
#define IOV_LAP_SIZE 1		/* lap size */
#define IOV_DDP_SIZE 2		/* ddp size */
#define IOV_ATP_SIZE 3		/* atp size */
#define IOV_ATPU_SIZE 4		/* user data from atp size */

/* maximum IOV level expected by DDP read routine */
#define IOV_READ_MAX 10		/* no more than this allowed */


/* Debug flags structure */

typedef union {
  struct {
    unsigned dbg_lap : 1;	/* debug LAP */
    unsigned dbg_ddp : 1;	/* debug DDP */
    unsigned dbg_atp : 1;	/* debug ATP */
    unsigned dbg_nbp : 1;	/* debug NBP */
    unsigned dbg_pap : 1;	/* debug PAP */
    unsigned dbg_ini : 1;	/* debug init code */
    unsigned dbg_asp : 1;	/* debug asp */
    unsigned dbg_skd : 1;	/* debug scheduler */
  } U_dbg;
  unsigned db_flgs;
} DBUG;

#define db_lap U_dbg.dbg_lap
#define db_ddp U_dbg.dbg_ddp
#define db_atp U_dbg.dbg_atp
#define db_nbp U_dbg.dbg_nbp
#define db_pap U_dbg.dbg_pap
#define db_ini U_dbg.dbg_ini
#define db_asp U_dbg.dbg_asp
#define db_skd U_dbg.dbg_skd

extern DBUG dbug;

#define ticktosec(sec) ((sec)/4) /* converts from internal tick units */
				 /* to seconds */
#define sectotick(sec) ((sec)*4) /* converts from seconds to internal ticks */


/* define some things for the c compiler - CCK: I think some if not all */
/* of these should be in the compiler, thus I use all lowercased as they */
/* would be (as I normally use c compiler stuff). */
#define private static
#define export
#define import extern
typedef int boolean;

/* levels of registers - we declare importance of different register */
/* vars this way */
#define REGISTER1 register
#define REGISTER2 register
#define REGISTER3 register

typedef int (*ProcPtr)();	/* procedure pointer */
#define NILPROC (ProcPtr) 0	/* null procedure */
#define NILPTR (u_char *) 0

/* Some general definitions - probably don't belong here */

#define TRUE 1
#define FALSE 0

#define min(x,y) (((int)(x) > (int)(y)) ? (y) : (x))
#define max(x,y) (((int)(x) > (int)(y)) ? (x) : (y))

#define tokipnet(x,y) ((y)|((x)<<8))

#ifdef pyr
#define ntohs(x) (x)
#endif

/* high and low part of a "kip" network number - assume number in network */
/* format */
#define nkipnetnumber(x) ntohs((x))>>8
#define nkipsubnetnumber(x) ntohs((x))&0xff
/* same but assumes in host form */
#define kipnetnumber(x) (x)>>8
#define kipsubnetnumber(x) (x)&0xff

/* version information */
struct cap_version {
  int cv_version;		/* e.g. 3 */
  int cv_subversion;		/* e.g. 0 - if original was subverted :-) */
  int cv_patchlevel;		/* e.g. 100, probably more */
  char *cv_copyright;		/* pointer to copyright notice */
  char *cv_name;		/* "CAP" */
  char *cv_type;		/* "UDP", "EtherTalk", etc. */
  char *cv_rmonth;		/* release month */
  char *cv_ryear;		/* release year */
};

/* routine declarations (only if not OSErr or int) */
unsigned char *GetMyZone();
struct cap_version *what_cap_version();

#define CAP_6_DBM	1	/* new atalk.local management */

/* logging flags */

#define L_UERR 0x20		/* want unix error message */
#define L_EXIT 0x10		/* exit after logging */
#define L_LVL 0xf		/* debug levels */
#define L_LVLMAX 15		/* maximum level */

