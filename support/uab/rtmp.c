static char rcsid[] = "$Author: djh $ $Date: 1992/07/15 14:04:45 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/rtmp.c,v 2.4 1992/07/15 14:04:45 djh Rel djh $";
static char revision[] = "$Revision: 2.4 $";

/*
 * rtmp.c: RTMP, ZIP, and NBP gateway protocol modules
 *
 * dropped NBP here because it needs access to routing table
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
 *  August, 1988  CCKim Created
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
#ifndef OWNHASH
#include "hash.h"
#endif

/* stupid function so we can link together items */
struct chain {
  struct chain *c_next;		/* next in list */
  caddr_t c_data;		/* data */
};
/* should be in appletalk.h */
#define ZIP_query 1		/* query type */
#define ZIP_reply 2		/* reply type */
#define ZIP_takedown 3		/* takedown (NYI) */
#define ZIP_bringup 4		/* bringup (NYI) */

struct zipddp {			/* ZIP packet */
  byte zip_cmd;
  byte zip_netcount;
};

/* zip name as found in zip reply and bringup packets */
struct zipname {
  word zip_net;
  byte zip_len;
};
#define zipNameLen 3

/* route states */
#define R_NONE 0		/* or false */
#define R_GOOD 1
#define R_BAD 2
#define R_SUSPECT 3

/* messages describing route states (shouldn't change) */
private char *rtmp_state_msg[4] = {
  "deleted",
  "good",
  "bad",
  "suspect"
};

#define MAXHOPS 15		/* maximum # of hops a route can have */

/* when to "age" route entries */
#define RTMP_VALIDITY_TIMEOUT 20
/* rtmp send timeout: initial is offset from zip timeout */
#define RTMP_INITIAL_SEND_TIMEOUT  30
#define RTMP_SEND_TIMEOUT 10
/* initial zip is for "local" zones */
#define ZIP_INITIAL_TIMEOUT 5
#define ZIP_QUERY_TIMEOUT 10

/* maximum number of routes */
#define NROUTES 500

private struct route_entry *routes; /* chain of routes */
private caddr_t route_htable_handle; /* route table handle */


/* bridge node handling stuff */
/* for now (should be hash table or some such)  */
private caddr_t bridgenode_htable_handle;
#define NUMBRNODES 500
struct bridge_node {		/* bridge node entry */
  NODE id;
  PORT_T port;
};

struct bridge_node_key {	/* structure to pass a key in */
  NODE *idp;
  PORT_T port;
};

private int zone_unknown = 0;	/* a zone is unknown */

/* same as pstr */
private caddr_t zone_hash_table;
#define NZONES 250		/* random, need not be too big */
private struct chain *zonelist;	/* chain of zones */

private int m_route = 0;
private int m_bnode = 0;
private int m_cnode = 0;
private int m_zone = 0;

export void rtmp_init();
export void rtmp_start();
private int route_compare();
private caddr_t route_alloc();
private u_int route_compress();
private void routes_init();
export struct route_entry *route_find();
private struct route_entry *route_create();
private void route_delete();
export struct route_entry *route_list_start();
export struct route_entry *route_next();
export int route_add_host_entry();
export char *node_format();
private int bstrcmp();
private int bstrcmpci();
private int bridgenode_compare();
private caddr_t bridgenode_alloc();
private u_int bridgenode_compress();
private void bridgenode_init();
private NODE *bridgenode_find();
private boolean rtmp_handler();
private void rtmp_dump_entry();
private void rtmp_dump_entry_to_file();
private int rtmp_send_timeout();
private void rtmp_send();
private int rtmp_validity_timeout();
private void rtmp_replace_entry();
private void rtmp_update_entry();
private boolean rtmprq_handler();
private boolean zip_handler();
private int zip_query_timeout();
private int zip_query_handler();
private int zip_reply_handler();
private int zip_atp_handler();
/* private int zone_hash(); */
private caddr_t zone_alloc();
private int pstrc();
private int zone_compare();
private u_int zone_compress();
private void zone_init();
export byte *zone_find();

private void rtmp_format_hash_stats();
export void rtmp_dump_stats();
export void rtmp_dump_table();

/*
 * initialize
 * 
 * clear up vars
*/
export void
rtmp_init()
{
  routes_init();
  bridgenode_init();
  zone_init();
}


/*
 * rtmp start - fires up the timers 
 *
*/
export void
rtmp_start()
{
  struct timeval tv;
  tv.tv_sec = RTMP_VALIDITY_TIMEOUT; /* 20 second validity timer */
  tv.tv_usec = 0;
  relTimeout(rtmp_validity_timeout, 0, &tv, TRUE);
  /* these last two could be combined */
  tv.tv_sec = RTMP_INITIAL_SEND_TIMEOUT;
  tv.tv_usec = 0;
  relTimeout(rtmp_send_timeout, 0, &tv, TRUE);
  tv.tv_sec = ZIP_INITIAL_TIMEOUT;
  tv.tv_usec = 0;
  relTimeout(zip_query_timeout, 0, &tv, TRUE);
}

/* routing table handler */
/*
 * compare key net to route entry
 *
*/
private int
route_compare(net,re)
word *net;
struct route_entry *re;
{
  return(((int)*net) - ((int)(re->re_ddp_net)));
}

/*
 * allocate data: create a route
 *
 *
*/
private caddr_t
route_alloc(net)
word *net;
{
  struct route_entry *re;

  if ((re = (struct route_entry *)malloc(sizeof(struct route_entry))) == NULL)
    return(NULL);
  m_route++;
  re->re_ddp_net = *net;	/* copy in network */
  re->re_state = R_NONE;	/* set state to none */
  re->re_next = routes;
  routes = re;			/* link to head */
  return((caddr_t)re);		/* and return */
}

/*
 * compress key to u_int
 *
*/
private u_int
route_compress(net)
word *net;
{
  return(*net);
}

private void
routes_init()
{
  routes = NULL;		/* no routes */
  route_htable_handle =
    h_new(HASH_POLICY_CHAIN, HASH_TYPE_MULTIPLICATIVE, NROUTES,
	  route_compare, route_alloc, route_compress, NULL, NULL, NULL);
  ddp_open(rtmpSkt, rtmp_handler);
}
/*
 * find route for a particular network
 *
*/
export
struct route_entry *
route_find(net)
word net;
{
  struct route_entry *re;
  re = (struct route_entry *)h_member(route_htable_handle, &net);
  if (re && re->re_state)	/* we don't really delete */
    return(re);
  return(NULL);
}

/*
 * create a routing entry and initialize it.
 *
*/
private struct route_entry *
route_create(net)
word net;
{
  register struct route_entry *re;
  int d, b;

  re = (struct route_entry *)
    h_operation(HASH_OP_INSERT, route_htable_handle, &net, -1,-1,&d,&b);

  if (re == NULL)		/* should not happen, but. */
    return(NULL);
  logit(LOG_LOTS, "new route for net %d.%d at hash [bkt %d, d %d]\n",
	 nkipnetnumber(net),nkipsubnetnumber(net),
	 b,d);
  zone_unknown++;		/* new route, so set */
  re->re_dist = 0;		/* assume zero distance */
  re->re_bridgeid_p = NULL;	/* means self */
/*  re->re_ddp_net = net; */ /* done already */
  re->re_zip_taken = FALSE;	/* make sure */
  re->re_zonep = NULL;		/* make sure */
  return(re);
}

/* delete route - for now just set state to none */
/* may want to time it out at some point */
private void
route_delete(re)
struct route_entry *re;
{
  re->re_state = R_NONE;
}


/* return route list start */
export struct route_entry *
route_list_start()
{
  return(routes);
}

/* get next in list: hidden in case we want to change way done */
export struct route_entry *
route_next(re)
struct route_entry *re;
{
  return(re->re_next);
}

/*
 * establish a new port
 *
 * net in network format (8 bits - word)
 * node as bytes (variable length, network order, zero padded in front)
 * nodesize - number of bits valid in node
 * ddp_node
 * zone to set if any
 *
*/
export
route_add_host_entry(port, net, zone)
PORT_T port;
word net;
byte *zone;
{
  struct route_entry *re;

  if (net == 0)
    return(-1);
  /* if network given, then construct internal route */
  /* do a find in case route already acquired */
  if ((re = route_find(net)) == NULL)
    if ((re = route_create(net)) == NULL)
      return(-1);
  /* reset or set */
  re->re_state = R_GOOD;
  re->re_port = port;
  re->re_dist = 0;
  re->re_bridgeid_p = NULL;
  logit(LOG_BASE, "port %d host route added for network %d.%d",
      port, nkipnetnumber(net), nkipsubnetnumber(net));
  rtmp_dump_entry("host port", re);
  if (zone == NULL)
    return(0);
  /* if zone given for net, then add it */
  re->re_zonep = zone_find(zone, TRUE);	/* insert zone name */
  if (re->re_zonep && zone_unknown > 0) {
    logit(LOG_BASE, "port %d zone name %s inserted", port, re->re_zonep+1);
    zone_unknown--;
  }
  return(0);
}



/*
 * format node structure for printing
 *
*/
export char *
node_format(node)
NODE *node;
{
  static char tmpbuf[200];
  static char *fmtstr = "%x%x%x%x%x%x%x%x";
  byte *id;
  int n;

  if (node == NULL)
    return("self");
  id = node->n_id;
  /* if less than 5 bytes, convert to network order int and print */
  switch (node->n_bytes) {
  case 4:
    n = ntohl((id[0]<<24)|(id[1]<<16)|(id[2]<<8)|id[3]);
    break;
  case 3:
    n = ntohl((id[0]<<16)|(id[1]<<8)|id[2]);
    break;
  case 2:
    n = ntohs(id[0]<<8|id[1]);
    break;
  case 1:
    n = id[0];
    break;
  default:
    sprintf(tmpbuf, fmtstr+2*(MAXNODEBYTE-node->n_bytes),
	    node->n_id[0], node->n_id[1],
	  node->n_id[2], node->n_id[3],
	  node->n_id[4], node->n_id[5],
	  node->n_id[6], node->n_id[7]);
    return(tmpbuf);
  }
  sprintf(tmpbuf, "%d", n);
  return(tmpbuf);
}

/* like strncmp, but allows "0" bytes */
private int
bstrcmp(a,b,l)
register byte *a;
register byte *b;
register int l;
{
  register int c = 0;		/* if zero length, then same */

  while (l--) 			/* while data */
    if ((c = (*a++ - *b++)))	/* compare and get difference */
      break;			/* return c */
  return(c);			/* return value */
}
/* like bstrcmp, but case insensitive */
private int
bstrcmpci(a,b,l)
register byte *a;
register byte *b;
register int l;
{
  register int c = 0;		/* if zero length, then same */
  register byte aa, bb;

  while (l--) {			/* while data */
    aa = *a++;
    if ( isascii(aa) && isupper(aa) )
      aa = tolower(aa);
    bb = *b++;
    if ( isascii(bb) && isupper(bb) )
      bb = tolower(bb);
    if ((c = (aa - bb)))	/* compare and get difference */
      break;			/* return c */
  }
  return(c);			/* return value */
}

/* bridge node table handler */

/* compare (port,node) to bridgenode */
private int
bridgenode_compare(k, bn)
struct bridge_node_key *k;
struct bridge_node *bn;
{
  if (k->port != bn->port)
    return((int)(k->port - bn->port));
  if (k->idp->n_size != bn->id.n_size)
    return(k->idp->n_size - bn->id.n_size);
  return(bstrcmp((caddr_t)k->idp->n_id, (caddr_t)bn->id.n_id, bn->id.n_bytes));
}

/* allocate space for a bridge node */
private caddr_t
bridgenode_alloc(k)
struct bridge_node_key *k;
{
  struct bridge_node *bn;

  if ((bn = (struct bridge_node *)malloc(sizeof(struct bridge_node))) == NULL)
    return(NULL);
  m_bnode++;
#ifdef DEBUG
  logit(0, "BRIDGE NODE CREATE");
  logit(0, "PORT %d", k->port);
  logit(0, "ID len %d, byte 0 %d", k->idp->n_size, k->idp->n_id[0]);
#endif
  bn->id = *k->idp;		/* copy in */
  bn->port = k->port;
  return((caddr_t)bn);
}

/* compress key  to an u_int */
private u_int
bridgenode_compress(k)
struct bridge_node_key *k;
{
  u_int r = (u_int)k->port;
  int i = k->idp->n_bytes;	/* # of bytes */
  byte *p = k->idp->n_id;	/* data */
    
  /* add in p, but keep rotating r */
  r ^= k->idp->n_size;		/* xor size in */
  while (i--)
    r = ((r>>1)|(r<<31)) + *p++;
  return(r);
}


/* initialize */
/* should we limit the # of bridge nodes by using a open hash? */
private void
bridgenode_init()
{
  bridgenode_htable_handle =
    h_new(HASH_POLICY_CHAIN, HASH_TYPE_MULTIPLICATIVE, NUMBRNODES,
	  bridgenode_compare, bridgenode_alloc,
	  bridgenode_compress, NULL, NULL, NULL);
}

/* find a bridge node based on port,node key */
private NODE *
bridgenode_find(port, node)
PORT_T port;
NODE *node;
{
  struct bridge_node *bn;
  struct bridge_node_key bk;
  int d,b;

  bk.idp = node;
  bk.port = port;

  bn = (struct bridge_node *)
    h_operation(HASH_OP_INSERT, bridgenode_htable_handle, &bk, -1,-1,&d,&b);

  if (bn == NULL)
    return(NULL);
#ifdef notdef
  printf("look bridge node %s at hash [bkt %d, d %d]\n",
	 node_format(node), b,d);
#endif
  return(&bn->id);		/* return node id */
}

/* RTMP handling */

/*
 * handle incoming rtmp packets
 *
*/
/*ARGSUSED*/
private boolean
rtmp_handler(port, ddp, data, len)
PORT_T port;
DDP *ddp;			/* not used */
byte *data;
int len;
{
  struct route_entry *re;
  RTMPtuple tuple;
  word net;
  NODE id, *sid;

  if (ddp->type == ddpRTMP_REQ)	/* is it a rtmp request? */
    return(rtmprq_handler(port,ddp)); /* yes, handle it */
  if (ddp->type != ddpRTMP)	/* is it rtmp? */
    return(TRUE);		/* no, dump it */
  if (len < sizeof(net))	/* rtmpSize */
    return(TRUE);
  /* get net out */
  bcopy((caddr_t)data, (caddr_t)&net, sizeof(net));
  len -= sizeof(net);
  data += sizeof(net);
  if (len < 1)			/* id len */
    return(TRUE);
  id.n_size = *data;		/* get id length */
  id.n_bytes = (id.n_size + 7) / 8; /* make into bytes */
  if (len < (id.n_bytes+1))	/* id len + id */
    return;
  bcopy((caddr_t)data+1, (caddr_t)id.n_id, id.n_bytes);	/* copy id */
  len -= (id.n_bytes + 1);	/* reduce */
  data += (id.n_bytes + 1);	/* reduce */
  
  sid = bridgenode_find(port, &id); /* canonicalize */
  if (sid == NULL)		/* ourselves or no room */
    return(TRUE);
  logit(LOG_BASE, "NEW RTMP: port %d, source %s", port, node_format(sid));

  if (!PORT_NET_READY(port, net, sid))
    route_add_host_entry(port, net, NULL); /* zone isn't known yet! */

                              /* 15/06/92 <kenji-i@ascii.co.jp> */
  /* if non-extended network, first tuple is always 0.0[$82] */
  if (len < rtmpTupleSize)
    return(TRUE);
  bcopy((caddr_t)data, (caddr_t)&tuple, rtmpTupleSize);
  if (tuple.net == 0 && tuple.hops == 0x82) {
    logit(LOG_LOTS, "ignore_entry: non-extended network marker");
    data += rtmpTupleSize;
    len -= rtmpTupleSize;
  }

  /* use tuplesize because of byte alignment problems */
  while (len >= rtmpTupleSize) {
    bcopy((caddr_t)data, (caddr_t)&tuple, rtmpTupleSize);
    data += rtmpTupleSize, len -= rtmpTupleSize;
    re = route_find(tuple.net); /* get entry if any */
    if (re) 			/* update */
      rtmp_update_entry(re, port, sid, &tuple);
    else { 			/* create */
      re = route_create(tuple.net);
      if (!re)
	continue;
      logit(LOG_LOTS, "create_entry: net %d.%d",
	       nkipnetnumber(tuple.net),
	       nkipsubnetnumber(tuple.net));
      rtmp_replace_entry(re, port, sid, &tuple, FALSE);
    }
  }
  return(TRUE);
}


/*
 * dump rtmp table entry nicely
 *
*/
private void
rtmp_dump_entry(msg, re)
char *msg;
struct route_entry *re;
{
  if (!re->re_state)
    return;
  /* fixup */
  logit(LOG_LOTS, "%s: net %d.%d, bridge %s, dist %d, port %d, state %s",
      msg, nkipnetnumber(re->re_ddp_net), nkipsubnetnumber(re->re_ddp_net),
      node_format(re->re_bridgeid_p), re->re_dist, re->re_port,
      rtmp_state_msg[re->re_state]);
}

private void
rtmp_dump_entry_to_file(fd, re)
FILE *fd;
struct route_entry *re;
{
  if (!re->re_state)
    return;
  /* fixup */
fprintf(fd, " net %d.%d, bridge %s, dist %d, port %d, state %s, zone %d-%s\n",
      nkipnetnumber(re->re_ddp_net), nkipsubnetnumber(re->re_ddp_net),
      node_format(re->re_bridgeid_p), re->re_dist, re->re_port,
      rtmp_state_msg[re->re_state], 
      (re->re_zonep ? *re->re_zonep : 0),
      (re->re_zonep ? ((char *)re->re_zonep+1) : (char *)"<unknown>"));
}

/*
 * timeout: rtmp send - broadcast rtmp's
 * 
*/
private int
rtmp_send_timeout()
{
  PORT_T port;
  register struct route_entry *re;
  struct timeval tv;
  RTMP *rtmp;
  RTMPtuple tuple;
  char buf[ddpMaxData];
  char *p;
  int count, rest;
  
  rtmp = (RTMP *)buf;
  re = routes;
  /* load up rtmp entry */
  do {
    p = buf + sizeof(RTMP);	/* point into buf */
    rest = ddpMaxData - sizeof(RTMP);
    /* while room in packet */
    for (count = 0; re && rest >= rtmpTupleSize; re = re->re_next) {
      if (re->re_state != R_GOOD && re->re_state != R_SUSPECT)
	continue;		/* not good or suspect */
      if (re->re_zip_taken)	/* in zip takedown */
	continue;
      /* strict: don't send out updates for routes we don't know */
      /* the zone for -- this way bad data may go away */
      if (re->re_zonep == NULL)
	continue;
      if (!(re->re_dist < MAXHOPS))
	continue;
      tuple.net = re->re_ddp_net;
      tuple.hops = re->re_dist;
      bcopy((caddr_t)&tuple, p, rtmpTupleSize);
      count++;			/* found another */
      p += rtmpTupleSize;
      rest -= rtmpTupleSize;
    }
    for (port = PORT_LIST_START(); port != NULL; port = PORT_NEXT(port)) {
      NODE *pid;

      if (!PORT_ISBRIDGING(port))
	continue;
      if (!PORT_READY(port))
	continue;
      rtmp->net = PORT_DDPNET(port);
      pid = PORT_NODEID(port);
      rtmp->idLen = pid->n_size;
      bcopy((caddr_t)pid->n_id, (caddr_t)&rtmp->id, pid->n_bytes);
      rtmp_send(rtmp, 3+pid->n_bytes, count, rtmp->net, DDP_BROADCAST_NODE);
    }
  } while (re);			/* still routes */
  tv.tv_sec = RTMP_SEND_TIMEOUT; /* 10 second send timer */
  tv.tv_usec = 0;
  relTimeout(rtmp_send_timeout, 0, &tv, TRUE);
}

/*
 * send the rtmp packet on the specified port
 *
*/
private void
rtmp_send(rtmp, rtmp_size, count, dstnet, dst)
RTMP *rtmp;
int rtmp_size;
int count;
word dstnet;
byte dst;
{
  DDP rddp;		/* reply ddp header */
  int dlen = rtmp_size+rtmpTupleSize*count;

  rddp.srcSkt = rtmpSkt;
  rddp.dstNet = dstnet;
  rddp.dstNode = dst;
  rddp.dstSkt = rtmpSkt;
  rddp.type = ddpRTMP;
  ddp_output(NULL, &rddp, rtmp, dlen);
  logit(LOG_LOTS, "RTMP: sent %d length packet with %d tuples",dlen,count);
  logit(LOG_LOTS, "\tto net %d, node %d", dstnet, dst);
}

/*
 * timeout: rtmp validity
 *
 * run timer on rtmp validity
*/
private int
rtmp_validity_timeout()
{
  register struct route_entry *re;
  struct timeval tv;
  for (re = routes; re; re = re->re_next) {
    switch (re->re_state) {
    case R_GOOD:
      if (re->re_dist != 0)
	re->re_state = R_SUSPECT;
      break;
    case R_SUSPECT:
      rtmp_dump_entry("route went bad", re);
      re->re_state = R_BAD;
      break;
    case R_BAD:
      rtmp_dump_entry("route deleted", re);
      route_delete(re);
      break;
    }
  }
  tv.tv_sec = RTMP_VALIDITY_TIMEOUT; /* 20 second validity timer */
  tv.tv_usec = 0;

  relTimeout(rtmp_validity_timeout, 0, &tv, TRUE);
}

/*
 * rtmp_replace_entry: replace or add a route
 *
 * if istickler is set then this is a tickler packet that ensures
 * route stays good
 *
*/
private void
rtmp_replace_entry(re, port, sid, tuple, istickler)
struct route_entry *re;
PORT_T port;			/* source port */
NODE *sid;			/* source id */
RTMPtuple *tuple;
int istickler;			/* true if this replace should be */
				/* considered "a tickle" */
{
  int rewasthere = re->re_state;

  /* dump won't do anything if no state */
  if (rewasthere && !istickler)
    rtmp_dump_entry("replacing entry", re);
  re->re_dist = tuple->hops + 1;
  re->re_bridgeid_p = sid;
  re->re_port = port;
  re->re_state = R_GOOD;
  if (!istickler)
    rtmp_dump_entry(rewasthere ? "replaced entry" : "new" , re);
}

/*
 * rtmp_update_entry - figure out whether the route should be updated 
 *  or not
 *
*/
private void
rtmp_update_entry(re, port, sid,  tuple)
struct route_entry *re;
PORT_T port;			/* source port */
NODE *sid;			/* source id */
RTMPtuple *tuple;
{
  if (re->re_state == R_BAD && tuple->hops < MAXHOPS) { /* replace entry */
    logit(LOG_LOTS, "update_entry: net %d.%d, replacing because bad", 
	nkipnetnumber(tuple->net),
	nkipsubnetnumber(tuple->net));
    rtmp_replace_entry(re, port, sid, tuple, FALSE);
    return;
  }
  if (tuple->hops < MAXHOPS && re->re_dist > tuple->hops) {
    int istickler; 
    istickler = (re->re_dist == (tuple->hops+1));
    if (!istickler) {
      /* if not simple case of updating bad point */
      logit(LOG_LOTS, "update_entry: net %d.%d, replacing because better route",
	  nkipnetnumber(tuple->net),
	  nkipsubnetnumber(tuple->net));
    }
    rtmp_replace_entry(re, port, sid, tuple, istickler);
    return;
  }
  /* know we know that hops >= 15 or re->re_dist <= tuple->hops */
  /* if re's bridge matches the rmtp source bridge */
  /* and in on the same port, then the network is futher away... */
  if (re->re_bridgeid_p == sid && re->re_port == port) {
    re->re_dist++;
    if ((re->re_dist) <= MAXHOPS) {
      re->re_state = R_GOOD;
      rtmp_dump_entry("hop count increased", re);
    } else {
      rtmp_dump_entry("too many hops", re);
      route_delete(re);
    }
  }
}


/*
 * handle incoming rtmp request packet
 *
*/
private int
rtmprq_handler(port, ddp)
PORT_T port;
DDP *ddp;
{
  RTMP rtmp;
  NODE *pid;

  if (!PORT_ISBRIDGING(port))	/* not full bridge */
    return(TRUE);		/* so, don't advertise */
  if (!PORT_READY(port))	/* port isn't fully setup */
    return(TRUE);
  /* respond with data about the port */
  rtmp.net = PORT_DDPNET(port);
  pid = PORT_NODEID(port);
  rtmp.idLen = pid->n_size;
  bcopy((caddr_t)pid->n_id, (caddr_t)&rtmp.id, pid->n_bytes);
  /* no tuples */
  rtmp_send(&rtmp, 3+pid->n_bytes, 0, ddp->srcNet, ddp->srcNode);
  return(TRUE);
}

/*
 * handle incoming DDP zip packets 
*/
private boolean
zip_handler(port, ddp, data, datalen)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  struct zipddp zd;

  if (ddp->type == ddpATP)	/* atp? */
    return(zip_atp_handler(port, ddp, data, datalen)); /* yes */

  if (ddp->type != ddpZIP)	/* zip? */
    return(TRUE);		/* no, dump it */

  if (datalen < sizeof(zd))
    return(TRUE);
  bcopy((caddr_t)data, (caddr_t)&zd, sizeof(zd)); /* get zip header */
  datalen -= sizeof(zd), data += sizeof(zd);

  switch (zd.zip_cmd) {
  case ZIP_query:
    zip_query_handler(zd.zip_netcount, port, ddp, data, datalen);
    break;
  case ZIP_reply:
    zip_reply_handler(zd.zip_netcount, port, ddp, data, datalen);
    break;
  case ZIP_takedown:
    break;
  case ZIP_bringup:
    break;
  }
  return(TRUE);
}

/*
 * ZIP timeout
 *
 * query routes with unkown zones
 *
 *  algorithm: send zip query to bridge that sent us the rtmp.
 *  try to enclose as many networks as possible in the query
 *
*/
private int
zip_query_timeout()
{
  DDP ddp;
  struct route_entry *re;
  struct route_entry *first;
  PORT_T port;
  int first_idx;
  int j;
  NODE *curbridge;
  struct zipddp *zd;
  word zip_buf[ddpMaxData / sizeof(word)];
  int count;
  int mainnotknown = FALSE;

  if (zone_unknown) {		/* set whenever new route is created */
  /* initialize helper field, find first to query */
  for (first=NULL, re = routes, j = 0; re ; re = re->re_next)
    if (re->re_state && !re->re_zonep) {
      if (!first) {		/* remember first */
	first = re;
      }
      if (re->re_bridgeid_p == NULL) {
	if (!mainnotknown) {
	  first = re;		/* reset first one to do */
	  mainnotknown = TRUE;	/* don't know zone of a main interface */
	}
      }
      re->re_zip_helper = 0;
      j++;			/* mark work */
    }
  if (j == 0)
    zone_unknown = FALSE;

  /* query the various bridges */
  while (j && first) {
    curbridge = first->re_bridgeid_p; /* bridge id pointer */
    port = first->re_port;	/* get port for this bridge */
    count = 1;			/* skip first word */
    re = first;			/* this is where to start */
    first = NULL;		/* reset start point */
    for (; re ;  re = re->re_next) {
      /* skip if main interf. not known */
      if (mainnotknown && re->re_bridgeid_p) 
	continue;		/* and not local interface */
      if (!re->re_state || re->re_zonep || re->re_zip_helper) /* ignore */
	continue;
      if (re->re_bridgeid_p == curbridge &&
	  (count < (ddpMaxData / sizeof(word)))) {
	j--;
	re->re_zip_helper = 1;	/* mark  */
	zip_buf[count++] = re->re_ddp_net; /* to get */
	logit(LOG_LOTS, "will zip %s for %d.%d", node_format(curbridge), 
	    nkipnetnumber(re->re_ddp_net), nkipsubnetnumber(re->re_ddp_net));
      } else if (!first) {	/* remember next in sequence */
	  first = re;		/* set first */
	}
    }
    if (count == 1)		/* something weird happened */
      continue;
    zd = (struct zipddp *)zip_buf;
    zd->zip_cmd = ZIP_query;
    zd->zip_netcount = count - 1;
    ddp.dstNet = PORT_DDPNET(port);
    /* this will break on extended networks */
    ddp.dstNode = curbridge ? curbridge->n_id[0] : DDP_BROADCAST_NODE;
    ddp.dstSkt = zipZIS;
    ddp.srcSkt = zipZIS;
    ddp.type = ddpZIP;
    logit(LOG_LOTS, "zipping %s for %d networks", node_format(curbridge),
	count-1);
    ddp_output(curbridge, &ddp, zip_buf, count*sizeof(word));
    }
  }
  /* restart query */
  { struct timeval tv; 
    tv.tv_sec = ZIP_QUERY_TIMEOUT; 
    tv.tv_usec = 0;

    relTimeout(zip_query_timeout, 0, &tv, TRUE);
  }
}


/*
 * zip_query_handler: handle an incoming zip query packet
 *
*/
private int
zip_query_handler(count, port, ddp, data, datalen)
int count;
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  struct route_entry *re;
  DDP sddp;
  byte buf[ddpMaxData];
  int slen, i, zl;
  byte *p;
  word net;
  struct zipddp *zd;

  p = buf;
  p += sizeof(struct zipddp);
  slen = sizeof(struct zipddp);
  zd = (struct zipddp *)buf;
  zd->zip_cmd = ZIP_reply;	/* set command */
  zd->zip_netcount = 0;		/* zero nets in response as yet */
  /* best effort -- fit as many as possible, but don't bother with */
  /* multiple replies -- not clear remote would handle anyway */
  while (count--) {
    if (datalen < sizeof(net))	/* any data left? */
      break;			/* no, count is wrong, stop! */
    bcopy((caddr_t)data, (caddr_t)&net, sizeof(net));
    datalen -= sizeof(net), data += sizeof(net);
    if ((re = route_find(net)) == NULL)	/* no route skip */
      continue;
    if (!re->re_zonep)
      continue;
    i = ((*re->re_zonep) + 1 + sizeof(word));
    if ((slen + i) > ddpMaxData)
      break;
    /* copy in response data */
    bcopy((caddr_t)&net, (caddr_t)p, sizeof(net));
    p += sizeof(net);
    zl = *re->re_zonep + 1;		/* get zone length */
    bcopy((caddr_t)re->re_zonep, (caddr_t)p, zl); /* copy zone name */
    p += zl;
    zd->zip_netcount++;		/* increment count */
    logit(LOG_JUNK, "query on net %d.%d yields zone %s", 
	nkipnetnumber(net), nkipsubnetnumber(net), re->re_zonep+1);
    slen += i;
  }
  sddp.dstNet = ddp->srcNet;
  sddp.dstNode = ddp->srcNode;
  sddp.dstSkt = ddp->srcSkt;
  sddp.srcSkt = zipZIS;
  sddp.type = ddpZIP;
  ddp_output(NULL, &sddp, buf, slen);
}

/*
 * handle incoming zip reply.  basically insert zone names
 * into the table if possible
 *
*/
private int
zip_reply_handler(count, port, ddp, data, datalen)
int count;
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  word net;
  struct route_entry *re;
  byte *p = data;
  byte *pp, *zone;
  int zonelen;

  while (count--) {
    if (datalen < (1+sizeof(net)))
      break;
    bcopy((caddr_t)p, (caddr_t)&net, sizeof(net)); /* get zone information */
    p += sizeof(net);		/* move to the name */
    datalen -= sizeof(net);
    zonelen = 1 + *p;
    /* now p points to a pstr */
    if (datalen < zonelen)	/* no data left? */
      break;
    zone = p;			/* p now points to zone */
    p += zonelen;
    datalen -= zonelen;
    if ((re = route_find(net)) == NULL) {
      continue;
    }
    pp = (byte *)zone_find(zone, TRUE); /* find or insert zone name */
    if (pp == NULL) {
      logit(LOG_BASE, "ZIP: no room for insert for zone\n");
      continue;
    }
    if (re->re_zonep) {		/* zone already known for net */
      if (pp && pp != re->re_zonep) {
	logit(LOG_BASE, "zone name conflict for %d.%d, received %s had %s\n",
	    nkipnetnumber(re->re_ddp_net), nkipsubnetnumber(re->re_ddp_net),
	    pp+1, re->re_zonep+1);
      }
      continue;
    }
    /* must have zone for this route (which didn't have one before) */
    re->re_zonep = pp;	/* mark zone */
    /* if zone is known for primary route, say so */
    if (re->re_bridgeid_p == NULL && re->re_ddp_net == PORT_DDPNET(port))
      PORT_ZONE_KNOWN(port, re->re_zonep);
    logit(LOG_BASE, "ZIPPED: from %d.%d.%d network %d.%d for zone %s",
	nkipnetnumber(ddp->srcNet), nkipsubnetnumber(ddp->srcNet),
	ddp->srcNode,
	nkipnetnumber(re->re_ddp_net), nkipsubnetnumber(re->re_ddp_net),
	re->re_zonep+1);
  }
}

/*
 * handle a zip atp command: Get Zone List or Get My Zone 
 *
 *
*/
/* these are only defined in abatp.h not appletalk.h (because nobody) */
/* should need such fine control under normal cirucmstances */
/* put the ifndefs around in case we decide to move them one day */
#ifndef atpCodeMask
# define atpCodeMask 0xc0
#endif
#ifndef atpReqCode
# define atpReqCode 0x40
#endif
#ifndef atpRspCode
# define atpRspCode 0x80
#endif
#ifndef atpEOM
# define atpEOM 0x10
#endif

private int
zip_atp_handler(port, ddp, data, datalen)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  int paranoia = FALSE;
  DDP sddp;
  ATP *atp;			/* pointer to atp header */
  zipUserBytes *zub;		/* pointer to zip user byte */
  char data_buf[ddpMaxData];	/* data buffer */
  char *p;			/* pointer to data */
  int ps = ddpMaxData;		/* room left in buffer */
  int sidx;			/* for getzonelist */
  int count, t, i;
  struct chain *zp;

  if (datalen < sizeof(ATP))
    return;
  bcopy((caddr_t)data, (caddr_t)data_buf, sizeof(ATP)); /* get bytes */
  atp = (ATP *)data_buf;	/* should be aligned */
  p = data_buf + sizeof(ATP);	/* point to data */
  ps -= sizeof(ATP);
  /* control must hold request and only request */
  if ((atp->control & atpCodeMask) != atpReqCode)
    return;
  /* bitmap should ask for at least one packet */
  if ((atp->bitmap & 0x1) == 0)
    return;
  zub = (zipUserBytes *)&atp->userData;
  if (paranoia && zub->zip_zero != 0)
    return;
  zub->zip_zero = 0;		/* ensure */
  switch (zub->zip_cmd) {
  case zip_GetMyZone:
    if (paranoia && ntohs(zub->zip_index) != 0)
      return;
    count = 1;
    zub->zip_cmd = 0;		/* zero because gmz */
    /* return zone of source network */
    /* if given network is 0 (mynet), use that of port */
    { struct route_entry *re;
      if (ddp->srcNet == 0) {
	if ((re = route_find(PORT_DDPNET(port))) == NULL)
	  return;
      } else {
	if ((re = route_find(ddp->srcNet)) == NULL)
	  return;
      }
      if (re->re_zonep == NULL)
	return;
      /* no way we could fill up buffer */
      { int zl;
	zl = 1 + *re->re_zonep;
	bcopy((caddr_t)re->re_zonep, (caddr_t)p, zl);
	ps -= zl;
      }
    }
    break;
  case zip_GetZoneList:
    sidx = ntohs(zub->zip_index);
    sidx--;			/* 1 is start */
    /* move through zonelist, decrementing sidx as we find a filled slot */
    /* a zone name may be sent more than once if we get an incoming zone */
    /* between GZL commands */
    /* move through sidx items */
    for (zp = zonelist; zp && sidx ; zp = zp->c_next)
      sidx--;
    if (sidx)			/* no more zones */
      break;
    /* i already set, zp already set */
    /* assume LastFlag */
    zub->zip_cmd = 0xff;	/* set every bit because not sure */
				/* which bit is lastflag */
    count = 0;
    while (zp) {
      byte *znp = (byte *)zp->c_data; /* get zone name */
      t = znp[0] + 1;		/* get length of zone name */
      if ((ps - t) < 0) {
	zub->zip_cmd = 0;	/* clear lastflag: one remains */
	break;
      }
      bcopy((caddr_t)znp, (caddr_t)p, t); /* copy data */
      count++;			/* bump count */
      ps -= t;			/* reduce available data */
      p += t;			/* move data pointer */
      zp = zp->c_next;		/* move to next zone */
    }
    zub->zip_index = count;	/* set count */
    break;
  default:			/* bad type, this is NOT paranoia */
    return;
  }
  zub->zip_index = htons(count); /* set count */
  atp->control = atpRspCode|atpEOM;
  atp->bitmap = 0;		/* sequence 0 */
  /* tid already set */
  sddp.dstNet = ddp->srcNet;
  sddp.dstNode = ddp->srcNode;
  sddp.dstSkt = ddp->srcSkt;
  sddp.srcSkt = ddp->dstSkt;
  sddp.type = ddpATP;
  ddp_output(NULL, &sddp, (byte *)data_buf, ddpMaxData - ps);
}

/* keep zone name in a linked list */

/* take a zone pstring and duplicate it -- make sure null terminated */
private caddr_t
zone_alloc(p)
byte *p;
{
  int len = (int)*p;		/* get length */
  struct chain *cnode;
  byte *r;

  if ((cnode = (struct chain *)malloc(sizeof(struct chain))) == NULL)
    return(NULL);
  m_cnode++;
  if (p == NULL)		/* translate NULL string */
    p = '\0';
  r = (byte *)malloc(len+2);	/* one for null, one for lenth */
  if (r == NULL) {
    free(cnode);
    return(NULL);
  }
  m_zone++;
  bcopy(p, r, len+1);		/* copy in data */
  r[len+1] = '\0';		/* make sure tied off */
  cnode->c_next = zonelist;	/* link next to head */
  zonelist = cnode;		/* link link to this */
  cnode->c_data = (caddr_t)r;	/* copy in data */
  return((caddr_t)cnode);
}

/* needs to be case insensitive */
private int
pstrc(p,s)
byte *p, *s;
{
  int r = (*p - *s);
  if (r)
    return(r);
  return(bstrcmpci(p+1, s+1, *p));
}

/* needs to be case insensitive */
private int
zone_compare(s,cnode)
byte *s;
struct chain *cnode;
{
  return(pstrc(s, cnode->c_data));
}

/* needs to be case insensitive */
private u_int
zone_compress(p)
byte *p;
{
  u_int r = 0;
  int i = (int) *p++;
  byte pp;
  /* add in p, but keep rotating r */
  while (i--) {
    pp = *p++;
    if ( isascii(pp) && isupper(pp) )
      pp = tolower(pp);
    r = ((r>>1)|(r<<31)) + pp;
  }
  return(r);
}

private void
zone_init()
{
  zone_hash_table = h_new(HASH_POLICY_CHAIN, HASH_TYPE_MULTIPLICATIVE,
			  NZONES, zone_compare, zone_alloc, zone_compress,
			  NULL, NULL, NULL);
  zonelist = NULL;
  ddp_open(zipZIS, zip_handler);
}

export byte *
zone_find(name, insert)
byte *name;
int insert;
{
  int b, d;
  struct chain *cnode;
  cnode = (struct chain *)h_operation(insert ? HASH_OP_INSERT : HASH_OP_MEMBER,
		      zone_hash_table, name, -1,-1,&d,&b);
  if (cnode == NULL)
    return(NULL);
  logit(LOG_LOTS, "%s for %s [%d,%d]\n",
      insert ? "insert" : "lookup", cnode->c_data+1, b,d);
  return((byte *)cnode->c_data); /* return data */
}


private void
rtmp_format_hash_stats(fd, s)
FILE *fd;
struct hash_statistics *s;
{
  fprintf(fd, "\t%d lookups since last rehash, average distance %.02f\n",
	  s->hs_lnum, s->hs_lnum ? ((float)s->hs_lsum / s->hs_lnum) : 0.0);
  fprintf(fd, "\t%d lookups total, average distance %.02f\n",
	  s->hs_clnum, s->hs_clnum ? ((float)s->hs_clsum / s->hs_clnum) : 0.0);
}

export void
rtmp_dump_stats(fd)
FILE *fd;
{
  putc('\n', fd);
  fprintf(fd, "Hash table statistics for zone lookups\n");
  rtmp_format_hash_stats(fd, h_statistics(zone_hash_table));
  fprintf(fd, "\nHash table statistics for routing table lookups\n");
  rtmp_format_hash_stats(fd, h_statistics(route_htable_handle));
  putc('\n', fd);		/* output cr */
  fprintf(fd,"%d routes, %d bridge nodes allocated\n", m_route, m_bnode);
  fprintf(fd,"%d zones, %d zone chain nodes allocated\n", m_zone, m_cnode);
  putc('\n', fd);		/* output cr */
}

export void
rtmp_dump_table(fd)
FILE *fd;
{
  register struct route_entry *re;

  fprintf(fd, "Routing table dump\n");
  for (re = routes; re ; re = re->re_next) 
    if (re->re_state)
      rtmp_dump_entry_to_file(fd, re);
  putc('\n', fd);
}
