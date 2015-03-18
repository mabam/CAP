/*
 * $Date: 91/02/15 21:21:01 $
 * $Header: async.h,v 2.1 91/02/15 21:21:01 djh Rel $
 * $Log:	async.h,v $
 * Revision 2.1  91/02/15  21:21:01  djh
 * CAP 6.0 Initial Revision
 * 
 * Revision 1.3  91/01/09  01:53:54  djh
 * Add XON/XOFF definitions for use with AsyncTerm DA.
 * 
 * Revision 1.2  90/07/10  18:17:57  djh
 * Add support for AsyncTerm DA for simultaneous logins
 * Alter speedTab entries for 19200 and 38400
 * 
 * Revision 1.1  90/03/17  22:06:31  djh
 * Initial revision
 * 
 *
 * async.h 
 *
 *	Asynchronous AppleTalk for CAP UNIX boxes.
 *	David Hornsby, djh@munnari.oz, August 1988
 *	Department of Computer Science.
 *	The University of Melbourne, Parkville, 3052
 *	Victoria, Australia.
 *
 * Copyright 1988, The University of Melbourne.
 *
 * You may use, modify and redistribute BUT NOT SELL this package provided
 * that all changes/bug fixes are returned to the author for inclusion.
 *
 *	
 *	Ref: Async Appletalk, Dr. Dobbs Journal, October 1987, p18.
 */

#include "macros.h"

#define PortOffset	0xaa00	/* obviously async appletalk :-)	*/
#define AALAP		600	/* maximum bytes of data		*/
#define aaBroad		750	/* a UDP port for DDP broadcasts	*/
#define MAXNODE		0x7f	/* for the moment (2^n-1 please)	*/

#define GOAWAY		"\1\1\1\1"

#define NBPTYPE		"AsyncATalk"
#define ZONE		"*"

#define IMType		0x86	/* an 'I aM' frame			*/
#define URType		0x87	/* a 'U aRe' frame			*/
#define SDDPType	0x01	/* short DDP frame			*/
#define DDPType		0x02	/* ordinary DDP frame			*/

#define SERIALType	0x43	/* a new LAP type to talk serial bytes	*/

#define SB_OPEN		1
#define SB_DATA		2
#define SB_CLOSE	3
#define SB_XON		0x11
#define SB_XOFF		0x13

#define DDP_RTMP_D	1
#define DDP_NBP		2
#define DDP_ATP		3
#define DDP_RTMP_R	5
#define DDP_ZIP		6

#define RTMP_SKT	1
#define NBP_SKT		2
#define ECHO_SKT	4
#define ZIP_SKT		6

#define ATP_GMZ		7
#define ATP_GZL		8
#define ATP_CMD		0x90
#define ATP_SEQ		0

#define NBitID		8

#define FrameChar	0xa5	/* framing char at start and end	*/
#define DLEChar		0x10	/* escape character			*/
#define XONChar1	0x11	/* XON character, parity bit not set	*/
#define XONChar2	0x91	/* XON character, parity bit set	*/
#define XOFFChar1	0x13	/* XOFF charaacter, parity bit not set	*/
#define XOFFChar2	0x93	/* XOFF charaacter, parity bit set	*/
#define XOR		0x40	/* what we XOR escaped characters with	*/

#define CR		0x0d	/* carriage return 			*/
#define CRP		0x8d	/* carriage return with parity bit on	*/
#define NL		0x0a	/* line feed	 			*/
#define NLP		0x8a	/* line feed with parity bit on		*/

#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif

#define UP		1
#define DOWN		0

#define RTMPTIME	6	/* seconds	*/

#define u_char	unsigned char

struct aalap_hdr {	/* aalap data + length + flag	*/
	u_char aalap_frame;
	u_char aalap_type;
	u_char aalap_data[AALAP + 3];	/* 600 + 2 + 1 (AALAP + CRC + frame) */
	u_char padding;
	short aalap_datalen;
	short aalap_framecomplete;
};

struct aalap_trlr {	/* frame trailer		*/
	unsigned short aalap_crc;
	u_char aalap_frame;
};

struct aalap_imur {	/* data section of IM/UR frame	*/
	unsigned short aalap_net;
	u_char aalap_node;
};

struct ap {		/* DDP AppleTalk inside UDP	*/
	u_char ldst;
	u_char lsrc;
	u_char ltype;
	u_char dd[10];
	u_char dstskt;
	u_char srcskt;
	u_char type;
	u_char data[1024];
};

struct ddp {		/* DDP AppleTalk inside AALAP	*/
	unsigned short hop_and_length;
	unsigned short ddp_checksum;
	unsigned short ddp_dstNet;
	unsigned short ddp_srcNet;
	u_char ddp_dstNode;
	u_char ddp_srcNode;
	u_char ddp_dstSkt;
	u_char ddp_srcSkt;
	u_char ddp_type;
	u_char ddp_data[586];
};

struct sddp {		/* short DDP AppleTalk inside AALAP	*/
	unsigned short hop_and_length;
	u_char ddp_dstSkt;
	u_char ddp_srcSkt;
	u_char ddp_type;
	u_char ddp_data[586];
};

struct rtmp {		/* RTMP in DDP data			*/
	unsigned short rtmp_senderNet;
	u_char rtmp_idLen;
	u_char rtmp_senderId
};

struct atp {		/* ATP header in DDP data area		*/
	u_char atp_command;
	u_char atp_bitseq;
	unsigned short atp_transid;
	u_char atp_user1;
	u_char atp_user2;
	u_char atp_user3;
	u_char atp_user4;
	u_char atp_data[578];
};
