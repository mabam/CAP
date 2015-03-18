/*
 * $Author: djh $ $Date: 91/02/15 23:07:20 $
 * $Header: ddpport.h,v 2.1 91/02/15 23:07:20 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * ddpport.c - ddp port manager
 *
 * Handles the ddp port which sits on the interface boundary between
 * DDP and LAP.  In addition, it carries the "local delivery" or
 * demuxing information necessary to pass data to individual modules
 *
 *
 * Copyright (c) 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 * Permission is granted to any individual or institution to use,
 * copy, or redistribute this software so long as it is not sold for
 * profit, provided that this notice and the original copyright
 * notices are retained.  Columbia University nor the author make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 *
 * Edit History:
 *
 *  Sept 4, 1988  CCKim Created
 *
*/

#ifndef _DDP_PORT_INCLUDED
#define _DDP_PORT_INCLUDED "yes"

#include "mpxddp.h"
#include "node.h"

/* this struct should really be hidden totally */
/*
 * PORT, struct port
 *
 * First, it describes the "lap" level parameters for a particular
 * interface.  Second, it describes the ddp network attached to that
 * interface.  ("lap" could be alap, elap, etc).
 *
 * Note: no routine should access port directly
 *
*/
typedef struct port {
  struct port *p_next;		/* pointer to next struct */
  int p_flags;			/* if any */
#define PORT_ISCONNECTED 0x1
/* could do at lower level, more efficient here? */
#define PORT_WANTSLONGDDP 0x2	/* wants to output long ddp */
#define PORT_NEEDSBROADCAST 0x4	/* needs broadcasts sent back */
#define PORT_FULLRTMP 0x8	/* full rtmp (advertise bridge) */
#define PORT_NETREADY 0x10	/* we know the net of the port */
#define PORT_ISREADY 0x20	/* port info complete (net,node,zone) */
  word p_ddp_net;		/* primary ddp network of port */
  byte p_ddp_node;		/* primary ddp node of port */
  byte *p_zonep;		/* remember our zone */
  NODE *p_id;			/* lap level id */
  caddr_t p_local_data;		/* local data for port manager */
  int (*p_send_if)();		/* send routine for that port */
  NODE *(*p_map_lap_if)();	/* ddp node to "lap" node mapper*/
  int (*p_map_ddp_if)();	/* map "lap" node to ddp node */
  /* other */
  struct mpxddp_module *p_mpx;	/* remember demuxer */
  int p_mpx_hdl;		/* mpx handle, in case for each port */
} PORT;

/* PORT_T may change to an int */
typedef struct port *PORT_T;

#define PORT_BAD(p) ((p) == NULL)
#define PORT_GOOD(p) ((p) != NULL)
#define PORT_NEXT(p) ((p)->p_next)
#define PORT_LIST_START() ((PORT_T)port_list_start())
#define PORT_FLAGS(p) ((p)->p_flags)
/* port is open */
# define PORT_CONNECTED(p) ((p)->p_flags & PORT_ISCONNECTED)
/* network defined */
# define PORT_READY(p) ((p)->p_flags & PORT_ISREADY)
# define PORT_WANTS_LONG_DDP(p) ((p)->p_flags & PORT_WANTSLONGDDP)
# define PORT_NEEDS_BROADCAST(p) ((p)->p_flags & PORT_NEEDSBROADCAST)
# define PORT_ISBRIDGING(p) ((p)->p_flags & PORT_FULLRTMP)
#define PORT_DDPNET(p) ((p)->p_ddp_net)
#define PORT_DDPNODE(p) ((p)->p_ddp_node)
#define PORT_GETLOCAL(p, type) ((type)(p)->p_local_data)
#define PORT_NODEID(p) ((p)->p_id)
/* send if possible */
#define PORT_SEND(p,dst,laptype,hdr,hdrlen,data,datalen) \
  (((p)->p_send_if) ? \
  ((*(p)->p_send_if)((p),(dst),(laptype),(hdr),(hdrlen),(data),(datalen))) : \
  -1)
#define PORT_MAKENODE(p,net,node) \
  (((p)->p_map_lap_if) ? \
  ((*(p)->p_map_lap_if)((p),(net),(node))) : \
  NULL)
#define PORT_DEMUX(p, ddp, data, datalen) \
  (((p)->p_mpx && (p)->p_mpx->mpx_send_ddp) ? \
  ((*(p)->p_mpx->mpx_send_ddp)((p)->p_mpx_hdl, (ddp), (data), (datalen))) : \
  -1)
/* set the port demuxer */
#define PORT_SETDEMUXER(p, desc) (port_setdemuxer((p), (desc)))
#define PORT_NET_READY(p, net, sid) (port_net_ready((p),(net),(sid)))
#define PORT_ZONE_KNOWN(p, zone) (port_zone_known((p),(zone)))
#define PORT_GRAB(p, skt) (((p)->p_mpx && (p)->p_mpx->mpx_grab) ? \
			   ((*(p)->p_mpx->mpx_grab)(skt, NULL)) : -1)
export PORT_T port_create();
export boolean port_set_demuxer();
export PORT_T port_list_start();
export boolean port_net_ready();
export boolean port_zone_known();

#endif /* INCLUDE THIS FILE */


