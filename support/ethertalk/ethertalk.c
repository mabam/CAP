static char rcsid[] = "$Author: djh $ $Date: 1994/10/10 08:55:05 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/ethertalk/RCS/ethertalk.c,v 2.6 1994/10/10 08:55:05 djh Rel djh $";
static char revision[] = "$Revision: 2.6 $";

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
 *  April 28, '91  djh   Added Phase 2 support
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

#include "../uab/proto_intf.h"		/* iso: level 0 */
#include "../uab/ethertalk.h"		/* iso: level 1 */
#include "../uab/aarp.h"		/* iso: level 1 */
#include "../uab/if_desc.h"		/* describes "if" */
#include "../uab/ddpport.h"		/* describes a ddp port to "lap" */
#include "../uab/log.h"

/* some logging ideas */
#define LOG_LOG 0
#define LOG_PRIMARY 1
#define LOG_LOTS 9

export int etalk_init();
private int etalk_getnode();	/* basis */
private etalk_initfinish();

private int etalk_send_ddp();
private int etalk_listener();
private NODE *etalk_ddpnode_to_node();
private int etalk_stats();
private int etalk_tables();

extern byte this_node;
#ifdef PHASE2
extern u_short this_net;
#endif PHASE2

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
export int
etalk_init(id, async)
IDESC_TYPE *id;
int async;
{
  E_HANDLE *eh;
  int hostid;


  if ((eh = (E_HANDLE *)malloc(sizeof(E_HANDLE))) == NULL)
    return(FALSE);

  pi_setup();

  /* init for a single node */
  eh->eh_ah = (caddr_t)aarp_init(id->id_intf, id->id_intfno, 1);
  if (eh->eh_ah == NULL) {
    free(eh);
    return(FALSE);
  }
  /* link in both directions */
  id->id_ifuse = (caddr_t)eh;	/* mark */
  eh->eh_id = id;		/* remember this */

  eh->eh_state = ELAP_WAITING;	/* mark waiting */

  /* acquire node address */
#ifdef PHASE2
  if ( this_node <= 0 || this_node >= 254)
#else  PHASE2
  if ( this_node <= 0 || this_node >= 255)
#endif PHASE2
    hostid = 0xff & gethostid();	/* use last byte of hostid as hint */
  else
    hostid = this_node;

  if (etalk_getnode(eh, hostid, etalk_initfinish) < 0) {
    free(eh);
    return(FALSE);
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

#ifdef PHASE2
  pa.dummy[0] = 0;  /* always zero */
  pa.dummy[1] = (ntohs(this_net) >> 8) & 0xff;
  pa.dummy[2] = (ntohs(this_net) & 0xff);
#else  PHASE2
  pa.dummy[0] = pa.dummy[1] = pa.dummy[2] = 0;
#endif PHASE2
  pa.node = hint;
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
#ifdef PHASE2
    if ((result = etalk_getnode(eh,(rand()%253)+1, etalk_initfinish)) < 0) {
#else  PHASE2
    if ((result = etalk_getnode(eh,(rand()%254)+1, etalk_initfinish)) < 0) {
#endif PHASE2
      logit(LOG_LOG, "could not acquire node on interface %s%d",
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
  eh->eh_enode.n_size = 8*nodesize; /* 8 or 32 bits */
  eh->eh_enode.n_bytes = nodesize;  /* 1 or 4 bytes */
#ifdef PHASE2
  eh->eh_enode.n_id[0] = pa.dummy[0];
  eh->eh_enode.n_id[1] = pa.dummy[1];
  eh->eh_enode.n_id[2] = pa.dummy[2];
  eh->eh_enode.n_id[3] = pa.node;
#else  PHASE2
  eh->eh_enode.n_id[0] = pa.node; /* this is it */
#endif PHASE2

  flags = PORT_WANTSLONGDDP;
  if (!pi_delivers_self_broadcasts())
    flags |= PORT_NEEDSBROADCAST;
  if (id->id_isabridge)
    flags |= PORT_FULLRTMP;

  eh->eh_state = ELAP_READY;
  logit(LOG_PRIMARY,"acquired node %d on interface %s%d",
	pa.node, id->id_intf, id->id_intfno);
}

export u_char *etalk_resolve(id, node)
IDESC_TYPE *id;
int node;
{
  E_HANDLE *eh;
  u_char *eaddr;
  struct ethertalkaddr tpa;

  eh = (E_HANDLE *)id->id_ifuse;

#ifdef PHASE2
  bcopy(&node, &tpa, ETPL);
#else  PHASE2
  tpa.dummy[0] = tpa.dummy[1] = tpa.dummy[2] = 0;
  tpa.node = ntohl(node);
#endif PHASE2

  if (aarp_resolve(eh->eh_ah, &tpa, tpa.node == 0xff, &eaddr) <= 0) {
#ifdef PHASE2
    logit (2, "etalk_resolve: node %d/%d.%d try again later",
		tpa.node, tpa.dummy[1], tpa.dummy[2]);
#else  PHASE2
    logit (2, "etalk_resolve: node %d try again later", tpa.node);
#endif PHASE2
    return(NULL);
  }
  return(eaddr);
}

#ifdef PHASE2
export int etalk_set_mynode(id, eaddr)
IDESC_TYPE *id;
struct ethertalkaddr *eaddr;
{
  E_HANDLE *eh;
  int hostid = eaddr->node;

  eh = (E_HANDLE *)id->id_ifuse;
  return(aarp_set_host_addr(eh->eh_ah, eaddr));
}
#endif PHASE2

export struct ethertalkaddr *etalk_mynode(id)
IDESC_TYPE *id;
{
  E_HANDLE *eh;
  static struct ethertalkaddr ea;

  eh = (E_HANDLE *)id->id_ifuse;
#ifdef PHASE2
  ea.dummy[0] = eh->eh_enode.n_id[0];
  ea.dummy[1] = eh->eh_enode.n_id[1];
  ea.dummy[2] = eh->eh_enode.n_id[2];
  ea.node = eh->eh_enode.n_id[3];
#else  PHASE2
  ea.dummy[0] = 0;
  ea.dummy[1] = 0;
  ea.dummy[2] = 0;
  ea.node = eh->eh_enode.n_id[0];
#endif PHASE2
  return (&ea);
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
#ifdef PHASE2
  static NODE node = { 4, 32}; /* initialize */
#else  PHASE2
  static NODE node = { 1, 8 }; /* initialize */
#endif PHASE2
  int myddpnet = PORT_DDPNET(port);

  if (ddpnet != 0 && myddpnet != ddpnet) /* only allow this net! */
    return(NULL);

#ifdef PHASE2
  node.n_id[0] = 0;
  node.n_id[1] = (ntohs(ddpnet) >> 8) & 0xff;
  node.n_id[2] = (ntohs(ddpnet) & 0xff);
  node.n_id[3] = ddpnode;
#else  PHASE2
  node.n_id[0] = ddpnode;	/* make node */
#endif PHASE2

  return(&node);
}

/* resolve a node to a ddp node (do we want this?) */
/* think we will need it + one that resolves it to a net in the future ? */
private byte
etalk_node_to_ddpnode(port, node)
PORT_T port;
NODE *node;
{
  if (node->n_size == 8)	/* 8 bits? */
    return(node->n_id[0]);
#ifdef PHASE2
  if (node->n_size == 32)	/* 32 bits? */
    return(node->n_id[3]);
#endif PHASE2
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
  lap.type = laptype;
#ifdef PHASE2
  tpa.dummy[0] = dstnode->n_id[0];
  tpa.dummy[1] = dstnode->n_id[1];
  tpa.dummy[2] = dstnode->n_id[2];
  tpa.node = dstnode->n_id[3];
  lap.dst = dstnode->n_id[3];	/* get dest node */
  /* source is always us! */
  lap.src = eh->eh_enode.n_id[3]; /* get source node */
#else  PHASE2
  tpa.dummy[0] = tpa.dummy[1] = tpa.dummy[2] = 0;
  tpa.node = dstnode->n_id[0];
  lap.dst = dstnode->n_id[0];	/* get dest node */
  /* source is always us! */
  lap.src = eh->eh_enode.n_id[0]; /* get source node */
#endif PHASE2

  if (aarp_resolve(eh->eh_ah, &tpa, lap.dst == 0xff, &eaddr) <= 0) {
    stats[ES_RESOLVE_ERR_OUTPUT]++;
    return(-1);
  }
#ifdef PHASE2
  /* delete LAP hdr */
  iov[0].iov_len = 0;
#else  PHASE2
  iov[0].iov_len = lapSize;
#endif PHASE2
  iov[0].iov_base = (caddr_t)&lap;
  iov[1].iov_len = hsize;
  iov[1].iov_base = (caddr_t)header;
  iov[2].iov_len = dlen;
  iov[2].iov_base = (caddr_t)data;
  if ((i = pi_writev(eh->eh_ph, iov, (dlen == 0) ? 2 : 3, eaddr)) < 0) {
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

#ifdef SOLARIS
#include <sys/systeminfo.h>

int
gethostid()
{
  char buf[32];

  sysinfo(SI_HW_SERIAL, buf, sizeof(buf)-2);

  return(atoi(buf));
}
#endif SOLARIS
