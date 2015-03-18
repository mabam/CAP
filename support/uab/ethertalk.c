static char rcsid[] = "$Author: djh $ $Date: 1991/09/01 06:13:59 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/ethertalk.c,v 2.3 1991/09/01 06:13:59 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * ethertalk.c - ethertalk interface
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
 *  April 3, 1988  CCKim Created
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
#ifdef sony_news
#include <netinet/if_ether.h>
#endif sony_news

#include <netat/appletalk.h>

#include "proto_intf.h"		/* iso: level 0 */
#include "ethertalk.h"		/* iso: level 1 */
#include "aarp.h"		/* iso: level 1 */

#include "if_desc.h"		/* describes "if" */
#include "ddpport.h"		/* describes a ddp port to "lap" */
#include "log.h"

#ifdef sony_news
#include "aarp_defs.h"
#endif sony_news

/* some logging ideas */
#define LOG_LOG 0
#define LOG_PRIMARY 1
#define LOG_LOTS 9

/*
   Ethertalk packet format:
   :: destination node ::
   :: source node ::
   <data>

*/

private int etalk_init();
private int etalk_getnode();	/* basis */
private etalk_initfinish();

private int etalk_send_ddp();
private int etalk_listener();
private NODE *etalk_ddpnode_to_node();
private int etalk_stats();
private int etalk_tables();

/* describe our interface to the world */
private char *ethertalk_lap_keywords[] = {
  "ethertalk",
  "elap",
  NULL
 };

export struct lap_description ethertalk_lap_description = {
  "EtherTalk Link Access Protocol",
  ethertalk_lap_keywords,
  TRUE,				/* need more than just key */
  etalk_init,			/* init routine */
  etalk_stats,			/* stats routine */
  etalk_tables			/* tables */
};


/* interface statistics */

private char *estat_names[] = {
#define ES_PKT_INPUT  0		/* packets input */
  "packets input",
#define ES_PKT_ACCEPTED	1	/* accepted input packets */
  "packets accepted",
#define ES_PKT_BAD 2		/* bad packets */
  "bad packets",
#define ES_PKT_NOTFORME 3	/* not for me packets */
  "packets not for me",
#define ES_BYTES_INPUT 4	/* accepted bytes */
  "bytes input",
#define ES_ERR_INPUT 5 		/* number of input errors */
  "input errors",
#define ES_PKT_NOHANDLER 6	/* no handler */
  "packets without handlers",
#define ES_PKT_OUTPUT  7	/* packets output */
  "packets output",
#define ES_BYTES_OUTPUT 8	/* bytes output */
  "bytes output",
#define ES_ERR_OUTPUT 9		/* output errors */
  "output errors",
#define ES_RESOLVE_ERR_OUTPUT 10 /* could not resolvve */
  "output resolve error"
#define ES_NUM_COUNTERS 11
};

typedef struct ethertalk_handle {
  int eh_state;			/* this connection state */
#define ELAP_WAITING -1
#define ELAP_BAD 0		/* error */
#define ELAP_READY 1		/* okay */
  PORT_T eh_port;		/* ethertalk port */
  int eh_ph;			/* ethertalk protocol handle */
  caddr_t eh_ah;		/* aarp module handle */
  NODE eh_enode;		/* host node id */
  IDESC_TYPE *eh_id;		/* interface description */
  int eh_stats[ES_NUM_COUNTERS]; /* statistics */
} E_HANDLE;


/*
 * call with provisional network number, interface name and number
 *
 * provisional number should be 0 if not seeding
 *
*/
private int
etalk_init(id, async)
IDESC_TYPE *id;
int async;
{
  int etph;
  E_HANDLE *eh;
  int hostid;


  if ((eh = (E_HANDLE *)malloc(sizeof(E_HANDLE))) == NULL)
    return(NULL);

  pi_setup();

  if ((etph = pi_open(ETHERTYPE_APPLETALK, id->id_intf, id->id_intfno)) < 0) {
    logit(LOG_LOG|L_UERR,"pi_open");
    free(eh);
    return(NULL);
  }
  eh->eh_ph = etph;
 
  /* init for a single node */
#ifdef sony_news
  eh->eh_ah = (caddr_t)aarp_init("", etph, 1);
#else  sony_news
  eh->eh_ah = (caddr_t)aarp_init(id->id_intf, id->id_intfno, 1);
#endif sony_news
  if (eh->eh_ah == NULL) {
    logit(LOG_LOG|L_UERR, "aarp_init");
    pi_close(etph);
    free(eh);
    return(NULL);
  }
  /* link in both directions */
  id->id_ifuse = (caddr_t)eh;	/* mark */
  eh->eh_id = id;		/* remember this */

  eh->eh_state = ELAP_WAITING;	/* mark waiting */

  /* acquire node address */
  hostid = 0xff & gethostid();	/* use last byte of hostid as hint */
  if (etalk_getnode(eh, hostid, etalk_initfinish) < 0) {
    pi_close(etph);
    free(eh);
  }

  if (async)			/* async means to stop early */
    return(TRUE);
  /* wait for node acquisition? */
  while (eh->eh_state == ELAP_WAITING)
    abSleep(10, TRUE);
  return(eh->eh_state == ELAP_READY); /* true if okay, 0 o.w. */
}

/*
 * try to acquire an ethertalk host node address using hint as the basis
 *  callback to who (cbarg is E_HANDLE, result where -1 if address in use
 *  host node address index o.w.)
 *
*/
private int
etalk_getnode(eh, hint, who)
E_HANDLE *eh;
int hint;
int (*who)();
{
  struct ethertalkaddr pa;
  int n;

  pa.dummy[0] = pa.dummy[1] = pa.dummy[2] = 0;
  /* EtherTalk II fixup */
  pa.node = hint;		/* use fourth byte of eaddr as guess */
  while ((n=aarp_acquire_etalk_node(eh->eh_ah, &pa, who, eh)) != 0) {
    if (n < 0) {
      /* error */
      /* clean up */
      return(-1);
    }
    pa.node++;			/* try next */
  }
  return(0);
}

/*
 * finish off the init
 *
*/
private
etalk_initfinish(eh, result)
E_HANDLE *eh;
int result;
{
  PORT_T eh_port;		/* ethertalk port */
  struct ethertalkaddr pa;
  int flags;
  int nodesize;
  IDESC_TYPE *id = eh->eh_id;	/* get interface description */

  if (result < 0) {
    if ((result = etalk_getnode(eh,(rand()%254)+1, etalk_initfinish)) < 0) {
      logit(LOG_LOG, "could not acquire node on interface %s%d\n",
	  id->id_intf, id->id_intfno);
      eh->eh_state = ELAP_BAD;
    }
    return;
  }

  if ((nodesize = aarp_get_host_addr(eh->eh_ah, &pa, result)) < 0) { 
    logit(LOG_PRIMARY, "aarp get host node address failed for %d", result);
    logit(LOG_PRIMARY, "interface %s%d can't be intialized",
	id->id_intf, id->id_intfno);
    eh->eh_state = ELAP_BAD;	/* mark bad */
    return;
  }
  eh->eh_enode.n_size = 8*nodesize; /* 8 bits */
  eh->eh_enode.n_bytes = nodesize; /* 1 byte */
  /* EtherTalk II fixup */
  eh->eh_enode.n_id[0] = pa.node; /* this is it */

  flags = PORT_WANTSLONGDDP;
  if (!pi_delivers_self_broadcasts())
    flags |= PORT_NEEDSBROADCAST;
  if (id->id_isabridge)
    flags |= PORT_FULLRTMP;

  /* establish port */
  /* EtherTalk II fixup */
  eh_port = port_create(id->id_network, pa.node, id->id_zone,
			&eh->eh_enode, flags, (caddr_t)eh,
			etalk_send_ddp, /* send interface */
			etalk_ddpnode_to_node, /* map from ddp */
			NULL,	/* map node to ddp node, net */
			id->id_local);	/* demuxer */
  if (eh_port) {
    /* go to ethertalk level */
    pi_listener(eh->eh_ph, etalk_listener, (caddr_t)eh_port);
    eh->eh_state = ELAP_READY;
    logit(LOG_PRIMARY, "port %d acquired node %d on interface %s%d",
	eh_port, pa.node, id->id_intf, id->id_intfno);
  } else {
    eh->eh_state = ELAP_BAD;
   logit(LOG_PRIMARY,"acquired node %d on interface %s%d, but no space for port",
	pa.node, id->id_intf, id->id_intfno);
  }
  /* phew */
}

#ifdef sony_news
u_char recv_buf[ETHERMTU];
#endif sony_news

/*
 * listen to incoming ethertalk packets and handle them 
 *
*/
/*ARGSUSED*/
private
etalk_listener(fd, port, etph)
int fd;				/* dummy */
PORT_T port;
int etph;
{
  static LAP lap;
  /* room for packet and then some */
  static byte rbuf[ddpMaxData+ddpSize+lapSize+100];
  int cc;
  struct iovec iov[3];
  struct ethertalkaddr spa;
  struct ethernet_addresses ea;
  struct ethertalk_handle *eh = PORT_GETLOCAL(port, struct ethertalk_handle *);
  int *stats = eh->eh_stats;
  int ddpnode;
#ifdef sony_news
  int idx = 0;
#endif sony_news

  iov[0].iov_base = (caddr_t)&ea;
  iov[0].iov_len = sizeof(ea);
#ifdef sony_news
  iov[1].iov_base = (caddr_t)recv_buf;
  iov[1].iov_len = ETHERMTU;
  if ((cc = pi_readv(etph, iov, 2)) < 0) {
#else  sony_news
  iov[1].iov_base = (caddr_t)&lap;
  iov[1].iov_len = lapSize;
  iov[2].iov_base = (caddr_t)rbuf;
  iov[2].iov_len = sizeof(rbuf);
  if ((cc = pi_readv(etph, iov, 3)) < 0) {
#endif sony_news
    logit(LOG_LOG|L_UERR, "pi_readv: ethertalk_listener");
    stats[ES_ERR_INPUT]++;		/* input error */
    return(cc);
  }
#ifdef sony_news
  if (ea.etype == ETHERTYPE_AARP)
    return(aarp_listener(fd, ((E_HANDLE *)(port->p_local_data))->eh_ah,
      etph, recv_buf, cc));
  if (ea.etype != ETHERTYPE_APPLETALK)
    return(-1);
  idx = 0;
  bcopy(&(recv_buf[idx]), &lap, lapSize);
  idx += lapSize;
  bcopy(&(recv_buf[idx]), rbuf, cc-idx);
#endif sony_news
  /* eat the packet and drop it */
  if (eh->eh_state != ELAP_READY) /* drop */
    return(cc);
  /* handle packet */
  cc -= (lapSize+sizeof(ea));

  if (lap.src == 0xff) {		/* bad, bad, bad */
    stats[ES_PKT_BAD]++;
    return(-1);
  }
  /* lap dest isn't right */
  /* fixup point */
  ddpnode = PORT_DDPNODE(port);
  if (lap.dst != 0xff && lap.dst != ddpnode) {
    stats[ES_PKT_NOTFORME]++;
    return(-1);
  }

  stats[ES_PKT_INPUT]++;
  stats[ES_BYTES_INPUT] += cc;
  /* pick out source for aarp table management if not self */
  /* EtherTalk II fixup */
  if (lap.src != ddpnode) {
    spa.dummy[0] = spa.dummy[1] = spa.dummy[2] = 0;
    spa.node = lap.src;
    if (!aarp_insert(eh->eh_ah, ea.saddr, &spa, FALSE)) /* drop it */
      return(-1);		/* enet address change */
  }
  
  switch (lap.type) {
  case lapDDP:
    ddp_route(port, rbuf, rbuf+ddpSize, cc, lap.dst == 0xff);
    break;
  case lapShortDDP:		/* don't allow short ddp for now */
    /*munge short ddp to ddp */
    sddp_route(port, lap.src, rbuf, rbuf+ddpSSize, cc);
    break;
  default:
    stats[ES_PKT_NOHANDLER]++;
    return(-1);
  }
  return(0);
}

/*
 * resolve a ddp node number to a node address on the specified port
 * (note: do we need more information in some cases?)
*/
private NODE *
etalk_ddpnode_to_node(port, ddpnet, ddpnode)
PORT_T port;
word ddpnet;
byte ddpnode;
{
  /* EtherTalk II fixup */
  /* think this is okay */
  static NODE node = { 1, 8 }; /* initialize */
  int myddpnet = PORT_DDPNET(port);

  if (ddpnet != 0 && myddpnet != ddpnet) /* only allow this net! */
    return(NULL);
  node.n_id[0] = ddpnode;	/* make node */
  return(&node);
}

/* resolve a node to a ddp node (do we want this?) */
/* think we will need it + one that resolves it to a net in the future ? */
private byte
etalk_node_to_ddpnode(port, node)
PORT_T port;
NODE *node;
{
  /* EtherTalk II fixup */
  if (node->n_size == 8)	/* 8 bits? */
    return(node->n_id[0]);
  return(0);
}


/*
 * send a ddp packet on ethertalk
 * (should we convert short to long ddp?)
 * 
 * port = port to send on
 * dstnode == destination ethertalk node
 * laptype == laptype of packet (header)
 * header = packet header (for laptype)
 * hsize = packet header length
 * data = data
 * dlen = datalength
*/
private
etalk_send_ddp(port, dstnode, laptype, header, hsize, data, dlen)
PORT_T port;
NODE *dstnode;
int laptype;
byte *header;
int hsize;
u_char *data;
int dlen;
{
  struct iovec iov[3];
  u_char *eaddr;
  LAP lap;
  struct ethertalk_handle *eh = PORT_GETLOCAL(port, struct ethertalk_handle *);
  struct ethertalkaddr tpa;
  int *stats = eh->eh_stats;
  int i;

  if (eh->eh_state != ELAP_READY) { /* drop */
    stats[ES_ERR_OUTPUT]++;
    return(-1);
  }
  if (dstnode == NULL) {	/* can't! */
    stats[ES_ERR_OUTPUT]++;		/* can't */
    return(-1);
  }

  /* should be higher? */
  if (dstnode->n_size != eh->eh_enode.n_size) { /* for now? */
    stats[ES_ERR_OUTPUT]++;		/* can't */
    return(-1);
  }
  /* source is always us! */
  lap.src = eh->eh_enode.n_id[0]; /* get source node */
  lap.dst = dstnode->n_id[0];	/* get dest node */
  lap.type = laptype;
  /* EtherTalk II fixup */
  tpa.dummy[0] = tpa.dummy[1] = tpa.dummy[2] = 0;
  tpa.node = dstnode->n_id[0];

  if (aarp_resolve(eh->eh_ah, &tpa, lap.dst == 0xff, &eaddr) <= 0) {
    stats[ES_RESOLVE_ERR_OUTPUT]++;
    return(-1);
  }
  iov[0].iov_len = lapSize;
  iov[0].iov_base = (caddr_t)&lap;
  iov[1].iov_len = hsize;
  iov[1].iov_base = (caddr_t)header;
  iov[2].iov_len = dlen;
  iov[2].iov_base = (caddr_t)data;
#ifdef sony_news
  if ((i = pi_writev(eh->eh_ph, iov, (dlen == 0) ? 2 : 3, eaddr,
    ETHERTYPE_APPLETALK)) < 0) {
#else  sony_news
  if ((i = pi_writev(eh->eh_ph, iov, (dlen == 0) ? 2 : 3, eaddr)) < 0) {
#endif sony_news
    stats[ES_ERR_OUTPUT]++;
    return(i);
  }
  stats[ES_PKT_OUTPUT]++;
  stats[ES_BYTES_OUTPUT] += i;
  return(i);
}


private int
etalk_stats(fd, id)
FILE *fd;
IDESC_TYPE *id;
{
  E_HANDLE *eh = (E_HANDLE *)id->id_ifuse; /* get handle */
  int i;
  
  fprintf(fd, "Interface %s%d statisitics\n", id->id_intf,
	  id->id_intfno);
  fprintf(fd, " Interface counters\n");
  for (i = 0; i < ES_NUM_COUNTERS; i++) {
    fprintf(fd, "  %8d\t%s\n", eh->eh_stats[i], estat_names[i]);
  }
  putc('\n', fd);		/* carriage return */
  /* call up aarp too */
  aarp_dump_stats(fd, eh->eh_ah);
  putc('\n', fd);		/* finish */
}

private int
etalk_tables(fd, id)
FILE *fd;
IDESC_TYPE *id;
{
  E_HANDLE *eh = (E_HANDLE *)id->id_ifuse; /* get handle */

  fprintf(fd, "Interface dump for %s%d\n",id->id_intf, id->id_intfno);
  aarp_dump_tables(fd, eh->eh_ah);
  putc('\n', fd);
}
