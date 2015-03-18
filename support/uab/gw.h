/*
 * $Author: djh $ $Date: 91/02/15 23:07:31 $
 * $Header: gw.h,v 2.1 91/02/15 23:07:31 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * gw.h - common definitions for our bridge
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
 *  August, 1988  CCKim Created
 *
*/

#include "node.h"
#include "ddpport.h"

/* probably shouldn't be defined here */
#ifndef DDP_BROADCAST_NODE
# define DDP_BROADCAST_NODE 0xff /* broadcast ddp node */
#endif

/*
 * a route entry describes a routing to a ddp network
 *
 * Most of the items are taken pretty much from inside appletalk.
 * 
 * The bridge "lap" id is stored as a pointer to a node address to
 * minimize the comparision overhead (though it produces a consistency
 * and lookup efficiency problem)--with a pointer we need only compare
 * two addreses.
 *
*/
struct route_entry {
  struct route_entry *re_next;	/* next route entry */
  PORT_T re_port;		/* port network is attached to */
  NODE *re_bridgeid_p;		/* bridge (lap level id) pointer */
  word re_ddp_net;		/* network we are routing */
  int re_dist;			/* distance (hops for now) */
  int re_state;			/* state of network */
  byte *re_zonep;		/* zone pascal string (NULL means none) */
  int re_zip_taken;		/* zip takedown in effect */
  int re_zip_helper;		/* just a helper var */
};

/* log levels */
#define LOG_LOG 0		/* always */
#define LOG_PRIMARY 1		/* primary information */
#define LOG_BASE 4		/* base info */
#define LOG_LOTS 6		/* lots of good info */
#define LOG_JUNK 8		/* starting to get into junk here */

/* function declarations */
/* rtmp.c */
void rtmp_init();
void rtmp_start();

struct route_entry *route_find();
struct route_entry *route_list_start();
struct route_entry *route_next();
int route_add_host_entry();

char *node_format();

byte *zone_find();

void rtmp_dump_stats();
void rtmp_dump_table();


/* ethertalk.c */
PORT_T ethertalk_init();

/* ddproute.c */
void ddp_route_init();
void ddp_route_start();
void sddp_route();
void ddp_route();
int ddp_open();		/* local ddp routines (input via listener) */
void ddp_output();
void ddp_dump_stats();

