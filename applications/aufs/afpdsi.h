/*
 * $Author: djh $ $Date: 91/03/14 13:45:20 $
 * $Header: afpdsi.h,v 2.2 91/03/14 13:45:20 djh Exp $
 * $Revision: 2.2 $
 *
 */

/*
 * afpdsi.h - Data Stream Interface Includes
 *
 * AFP via a Transport Protocol (eg: TCP/IP)
 *
 * AppleTalk package for UNIX
 *
 * The following routines implement a lightweight extra
 * layer between AFP (as embodied in the AUFS code), the
 * original ASP via ATP layer, and delivery via other
 * Transport Protocol layers, currently only TCP/IP.
 *
 * Refer: "AppleTalk Filing Protocol 2.2 &
 *         AFP over TCP/IP Specification"
 * 
 * SSS == Server Session Socket
 * SLS == Session Listening Socket
 * WSS == Workstation Session Socket
 *
 * Copyright (c) 1997 The University of Melbourne
 * David Hornsby <djh@munnari.OZ.AU>
 *
 */

/*
 * options
 *
 */
#define DSI_OPT_REQQ	0x00
#define DSI_OPT_ATTQ	0x01

#define DSI_OPT_REQLEN	4
#define DSI_OPT_ATTLEN	4

/*
 * quantum sizes
 *
 */
#define DSI_ATTN_SIZ	2
#define DSI_SRVR_CMD	1500
#define DSI_SRVR_MAX	64*1024

/*
 * the DSI header will be inserted in front of
 * every AFP request or reply packet
 *
 */

struct dsi_hdr {
  byte dsi_flags;		/* used to determine packet type */
#define DSI_REQ_FLAG	0x00
#define DSI_REP_FLAG	0x01
  byte dsi_command;		/* similar to ASP commands, except WrtCont */
#define DSICloseSession	1
#define DSICommand	2
#define DSIGetStatus	3
#define DSIOpenSession	4
#define DSITickle	5
#define DSIWrite	6
#define DSIAttention	8
  word dsi_requestID;		/* req ID on per-session basis, wraps */
  dword dsi_err_offset;		/* error for reply, offset for write, else 0 */
  dword dsi_data_len;		/* total data length following dsi_hdr */
  dword dsi_reserved;		/* reserved for future, should be zero */
};

/*
 * per-session demux info
 *
 */
struct dsi_sess {
  int sesstype;
#define DSI_SESSION_ATALK	0x01
#define DSI_SESSION_TCPIP	0x02
  int state;			/* type of DSI data expected */
#define DSI_STATE_HDR		0x01
#define DSI_STATE_AFP		0x02
#define DSI_STATE_DAT		0x03
#define DSI_STATE_REP		0x04
  char *ptr;			/* where we have to put incoming data */
  int lenleft;			/* amount of data expected to arrive */
  int timeout;			/* per-session tickle timer */
#define DSI_TIMEOUT		5*4
  word sess_id_out;		/* outgoing session ID (0-65535) */
  word sess_id_in;		/* incoming session ID (0-65535) */
  struct dsi_hdr hdr;		/* current incoming header (for reply) */
  ASPQE *aspqe;			/* callback data for GetRequest etc. */
};

/*
 * IP filter list
 *
 */
#define MAXIPFILTERS	100
#define MAXIPFILTSIZ	sizeof(struct ipFilter)

struct ipFilter {
  sword perm;
  dword mask;
  dword addr;
};
