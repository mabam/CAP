static char rcsid[] = "$Author: djh $ $Date: 91/02/15 23:07:21 $";
static char rcsident[] = "$Header: ddprouter.c,v 2.1 91/02/15 23:07:21 djh Rel $";
static char revision[] = "$Revision: 2.1 $";

/*
 * ddprouter.c - simple ddp router
 *
 * Follows specification set in "Inside Appletalk" by Gursharan Sidhu,
 * Richard F. Andrews, and Alan B. Oppenheimer, Apple Computer, Inc.
 * June 1986.
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
 *  August, 1988	CCKim Created
 *  November, 1990	djh@munnari.OZ.AU no short DDP, fix hop increment
 *
*/

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>

#include <netat/appletalk.h>
#include "gw.h"

export void ddp_route_init();
export void ddp_route_start();
export void sddp_route();
export void ddp_route();
export int ddp_open();		/* local ddp routines */
export int ddp_reopen();
private void ddp_input();
export void ddp_output();
private int ddp_matchournode();
private void dump_types();
export void ddp_dump_stats();

#define NUMDDPTYPES 8
char *ddp_type_names[NUMDDPTYPES] = {
  "unknown", "RTMP", "NBP", "ATP", "ECHO", "RTMP Request", "ZIP", "ADSP"
};

#define valid_type(d) (((d)->type>=ddpRTMP&&(d)->type<=ddpADSP)?(d)->type:0)
		       

struct ddp_stats {
  int short_ddp_routed;		/* short ddp sent to router */
  int long_ddp_routed;		/* long ddp sent to router */
  int ddp_forwarded;		/* long ddp forwarded */
  int ddp_types_forwarded[NUMDDPTYPES];	/* breakdown by type */
  int drop_noroute;		/* no routes */
  int drop_hops;		/* dropped because of hops */
  int long_ddp_output;		/* long ddp output */
  int long_ddp_types_output[NUMDDPTYPES]; /* breakdown by type */
  int short_ddp_output;		/* short ddp output */
  int short_ddp_types_output[NUMDDPTYPES];
  int ddp_input;		/* ddp packets input */
  int ddp_input_bad;		/* bad type packets input ... */
  int ddp_input_dropped;	/* broadcast from us, drop it */
  int ddp_types_input[NUMDDPTYPES];
};

private struct ddp_stats ddp_stats;

/* note: open for all ports */
private boolean (*ddp_handlers[ddpMaxSkt+1])(); /* define internal sockets */

/*
 * start ddp routing - need address of an internal router though to allow
 * mpx and aux. services (takes long ddp header, data and datalength )
 *
 * beabridge set to true tells us to tell rtmp to output rtmp packets
 * and to answer rtmp request packets
 *
*/
export void
ddp_route_start()
{
  rtmp_start();
  ddp_svcs_start();		/* start if needed (says we know net) */
}

/*
 * give us a chance to initialize.
 *
*/
export void
ddp_route_init()
{
  int i;

  for (i = 1 ; i < ddpMaxSkt; i++)
    ddp_handlers[i] = NULL;
  rtmp_init();
  ddp_svcs_init();		/* init other ddp services */
}

/*
 * route a short ddp packet coming in from an interface
 * 
 * basically, make the short header into a long ddp header and deliver
 * internally
 *
 * ddps is aligned okay, cannot assume for data
*/
export void
sddp_route(port, srcnode, ddps, data, datalen)
PORT_T port;
byte srcnode;
ShortDDP *ddps;
byte *data;
int datalen;
{
  DDP ddp;
  int i;

  /* get indicated data length */
  i = (ddpLengthMask & ntohs(ddps->length)) - ddpSSize;
  if (i < datalen)		/* reduce in case caller is enet, etc. */
    datalen = i;
  ddp.length = htons(datalen + (ddpSize-ddpSSize));
  ddp.checksum = 0;
  ddp.srcNet = ddp.dstNet = PORT_DDPNET(port);
  ddp.srcNode = srcnode;
  ddp.dstNode = PORT_DDPNODE(port);
  ddp.srcSkt = ddps->srcSkt;
  ddp.dstSkt = ddps->dstSkt;
  ddp.type = ddps->type;
  ddp_stats.short_ddp_routed++;
  ddp_input(port, &ddp, data, datalen);
}

/*
 * route a long ddp packet.
 *
 *
 * ddp is aligned okay, cannot assume for data
*/
export void
ddp_route(port, ddp, data, datalen, isbroadcast)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
int isbroadcast;		/* if port okay, then came in as broadcast */
{
  struct route_entry *re;
  NODE *dstnode;
  int i;

  /* get indicated data length */
  i = (ddpLengthMask & ntohs(ddp->length)) - ddpSize;
  if (i < datalen)		/* reduce in case caller is enet, etc. */
    datalen = i ;

  ddp_stats.long_ddp_routed++;
  /* don't do certain things if the packet comes internally (flagged */
  /* by the fact that the port is null) */
  if (port) {
    /* if from self, then drop */
    if (ddp_matchournode(ddp->srcNet, ddp->srcNode, FALSE)) {
      return;
    }
    /* forward packets for self to self */
    /* don't forward broadcasts 'cept to self*/
    if (isbroadcast || 
	(ddp->dstNet==PORT_DDPNET(port) && ddp->dstNode==PORT_DDPNODE(port))) {
      ddp_input(port, ddp, data, datalen);
      return;
    }
  }

  /* get route to send the packet on */
  /* drop if no route or bad port */
  if ((re = route_find(ddp->dstNet)) == NULL) {
    ddp_stats.drop_noroute++;
    return;			/* drop the packet */
  }
  if (PORT_BAD(re->re_port) || !PORT_CONNECTED(re->re_port)) {
    ddp_stats.drop_noroute++;
    return;
  }
  if (re->re_dist == 0) {
    if (ddp->dstNode == PORT_DDPNODE(re->re_port)) {
      ddp_input(re->re_port, ddp, data, datalen);
      return;
    }
    if (ddp->dstNode == DDP_BROADCAST_NODE) /* deliver ourselves a copy */
      ddp_input(re->re_port, ddp, data, datalen);
    if (re->re_ddp_net == 0)	/* drop if no net established yet */
      return;
    dstnode = PORT_MAKENODE(re->re_port, ddp->dstNet, ddp->dstNode);
    ddp_stats.long_ddp_output++;
    ddp_stats.long_ddp_types_output[valid_type(ddp)]++;
  } else {
    /* move over into msg and skip first two bits, but only keep 4 bits */
    int hops = ntohs(ddp->length) >> (8+2) & 0xf;

    if (re->re_ddp_net == 0)	/* drop if no net established yet */
      return;
#define DDP_MAXHOPS 15
#define ddpHopShift 10		/* 10 bits to the left for hops */
    hops++;			/* forwarding, *HAVE* to increment hops */
    if (hops >= DDP_MAXHOPS) {
      ddp_stats.drop_hops++;
      return;			/* drop it */
    }
    ddp->length = htons((ntohs(ddp->length)&ddpLengthMask)|hops<<ddpHopShift);
    dstnode = re->re_bridgeid_p;
    ddp_stats.ddp_forwarded++;
    ddp_stats.ddp_types_forwarded[valid_type(ddp)]++;
  }
  /* wait until here in case for ourselves (okay) */
  if (re->re_ddp_net == 0) {	/* drop if no net established yet */
    return;
  }
  /* send it out */
  PORT_SEND(re->re_port, dstnode, lapDDP, ddp, ddpSize, data, datalen);
}

/*
 * Internal DDP input/output routines
 *
*/

/*
 * ddp open - open socket for processing - maybe allow socket
 * to be open on a single port only, but not necessary for now
 *
 * Generally, should only be for a very small number of priviledged
 * sockets
*/
export int
ddp_open(ddpskt, handler)
int ddpskt;
boolean (*handler)();
{
  register PORT_T port;

  if (ddpskt > 0 && ddpskt < ddpMaxSkt) {
    /* need to open mpx socket - remember to callback in mpx open too */
    /* how to do ? */
    ddp_handlers[ddpskt] = handler;
    for (port = PORT_LIST_START(); port ; port = PORT_NEXT(port))
      PORT_GRAB(port, ddpskt);
    return(TRUE);
  }
  return(FALSE);
}

/* smash through open socket list and call grab to grab the socket if */
/* necessary (generally for mpx) */
export int
ddp_reopen(grab, arg, carg)
int (*grab)();
int arg;
caddr_t carg;
{
  int i;

  for (i = 1; i < ddpMaxSkt; i++)
    if (ddp_handlers[i])
      (*grab)(i, arg, carg);
}

/* 
 * ddp input.  Processes an packet destined for internal use.
 *
*/
private void
ddp_input(port, ddp, data, datalen)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  int ds;			/* destination socket */

  ddp_stats.ddp_input++;

  /* should we checksum? - probably */
  if (ddp->dstSkt == 0 || ddp->dstSkt == ddpMaxSkt) {
    /* count ddp dropped bad type */
    ddp_stats.ddp_input_bad++;
    return;
  }

  /* reject any packet from us (already know from us) */
  /* that is a broadcast and whose srcSkt == dstSkt */
  if (ddp_matchournode(ddp->srcNet, ddp->srcNode, FALSE) &&
      ddp->dstNode == DDP_BROADCAST_NODE &&
      ddp->dstSkt == ddp->srcSkt) {
    /* RECORD */
    ddp_stats.ddp_input_dropped++;
    return;
  }
  ds = ddp->dstSkt;
  if (ddp_handlers[ds]) {
    /* they return TRUE if they handle, false if we should try to */
    /* forward it to an internal node */
    if ((*(ddp_handlers[ds]))(port, ddp, data, datalen)) {
      ddp_stats.ddp_types_input[valid_type(ddp)]++;
      return;
    }
  }

  /* send to process if naught else */
  if (port)  {
    ddp_stats.ddp_types_input[valid_type(ddp)]++;
    PORT_DEMUX(port, ddp, data, datalen);
  }
}

/*
 * ddp output sends out a packet
 *
 * Special processing:
 *  ddp output will output long ddp only if the appropriate flag is
 *    set and sufficient input is given.
 *  ddp output will also send back a broadcast packet to the input
 *    routines if the "lap" layer is not capable of receiving its own
 *    broadcasts
 *
 * Expect these ddp fields
 *   dstNet
 *   dstNode
 *   dstSkt
 *   srcSkt
 *   type
 * Fills in srcNet, srcNode, length, checksum with appropriate values
 *
 *
*/
export void
ddp_output(dstnode, ddp, data, size)
NODE *dstnode;
DDP *ddp;
byte *data;
int size;
{
  ShortDDP ddps;
  struct route_entry *re;
  PORT_T port;
  byte pddpnode;

  /* make sure we can route to destination */
  if ((re = route_find(ddp->dstNet)) == NULL)
    return;
  port = re->re_port;
  pddpnode = PORT_DDPNODE(re->re_port);
  
/*
 * short ddp?
 *
 * "Apple recommends, although currently does not require, that any DDP
 * packet sent through EtherTalk use the extended DDP header format. All
 * EtherTalk implementations must accept extended headers for any incoming
 * DDP packet. For compatability with third-party implementations of
 * AppleTalk that use EtherNet, EtherTalk implementations currently
 * also accept short DDP headers. The use of short DDP headers on EtherNet
 * is strongly discouraged by Apple and is expected to be phased out"
 *
 *			- Inside AppleTalk, 1989
 *
 * In light of the above, and the fact that an extra 8 bytes hardly makes
 * a difference with EtherNet packets (minimum 60 bytes anyway), lets back
 * out of the lapShortDDP PORT_SEND() below ...
 *
 * (NB: sddp_route() converts to long DDP format for internal delivery).
 *
 */

  /* yes, if dstNet is same as outgoing port */
  /* unless port wants long ddp and node was given */
  /* have "special" case for no node in case it is not possible to map */
  /* from "NODE *" type to a ddp node */
  if (ddp->dstNet == PORT_DDPNET(port) &&
      (dstnode || !PORT_WANTS_LONG_DDP(port))) {
    /* yes */
    /* set the length, as a side effect, zero the hop count */
    ddps.length = htons(ddpSSize + size);
    ddps.dstSkt = ddp->dstSkt;
    ddps.srcSkt = ddp->srcSkt;
    ddps.type = ddp->type;
    if (ddp->dstNode == DDP_BROADCAST_NODE && PORT_NEEDS_BROADCAST(port)) {
      sddp_route(port, pddpnode, &ddps, data, size);
    } else if (ddp->dstNode == pddpnode) {
      sddp_route(port, pddpnode, &ddps, data, size);
      return;
    }
#ifdef REALLY_WANT_TO_SEND_SHORT_DDP_PACKETS
    if (dstnode || (dstnode = PORT_MAKENODE(port, 0, ddp->dstNode))) {
      ddp_stats.short_ddp_output++;
      ddp_stats.short_ddp_types_output[valid_type(ddp)]++;
      PORT_SEND(port, dstnode, lapShortDDP, &ddps, ddpSSize, data, size);
    }
    return;
#else REALLY_WANT_TO_SEND_SHORT_DDP_PACKETS
    if (!(dstnode || (dstnode = PORT_MAKENODE(port, 0, ddp->dstNode))))
      return;
#endif REALLY_WANT_TO_SEND_SHORT_DDP_PACKETS
  }
  /* definitely should put checksum here !!!! */
  ddp->checksum = 0;		/* should we put the checksum in? probably */
  /* set the length, as a side effect, zero the hop count */
  ddp->length = htons(size+ddpSize); /* length */
  ddp->srcNet = PORT_DDPNET(port);
  ddp->srcNode = pddpnode;
  ddp_route(NULL, ddp, (byte *)data, size, FALSE);
}


/*
 * Given a ddp network and node number, ddp matchournode will return true
 * if it corresponds to the ddp network/node number of any port.
 *
*/
private int
ddp_matchournode(net, node, br)
register word net;
register byte node;
int br;
{
  register PORT_T port;

  for (port = PORT_LIST_START() ; port ; port = PORT_NEXT(port))
    if ((net == 0 || PORT_DDPNET(port) == net) &&
	((br && node == DDP_BROADCAST_NODE) || PORT_DDPNODE(port) == node))
      return(TRUE);
  return(FALSE);
}


private void
dump_types(fd, table)
FILE *fd;
int table[];
{
  int i;

  fprintf(fd, "  by type\n");
  for (i = 1; i < NUMDDPTYPES; i++)
    fprintf(fd, "\t%d %s\n", table[i], ddp_type_names[i]);
  /* output "unknown last */
  fprintf(fd, "\t%d %s\n", table[0], ddp_type_names[0]);
}

export void
ddp_dump_stats(fd)
FILE *fd;
{
  fprintf(fd, "\nddp route\n");
  fprintf(fd, " short ddp received: %d\n", ddp_stats.short_ddp_routed);

  fprintf(fd, " long ddp received: %d\n", ddp_stats.long_ddp_routed);
  fprintf(fd, " forwarded to another bridge: %d\n", ddp_stats.ddp_forwarded);
  dump_types(fd, ddp_stats.ddp_types_forwarded);
  fprintf(fd, " output: %d\n", ddp_stats.long_ddp_output);
  dump_types(fd, ddp_stats.long_ddp_types_output);

  fprintf(fd, " ddp packets input for node: %d\n", ddp_stats.ddp_input);
  dump_types(fd, ddp_stats.ddp_types_input);
  fprintf(fd, " dropped: bad type %d\n", ddp_stats.ddp_input_bad);
  fprintf(fd, "\t   self broadcast %d\n", ddp_stats.ddp_input_dropped);

  fprintf(fd, " short ddp output: %d\n", ddp_stats.short_ddp_output);
  dump_types(fd, ddp_stats.short_ddp_types_output);
  fprintf(fd, " dropped: no route %d\n", ddp_stats.drop_noroute);
  fprintf(fd, "\t   too many hops %d\n", ddp_stats.drop_hops);
}

