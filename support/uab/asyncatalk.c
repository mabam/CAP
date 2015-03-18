/*
 * asyncatalk.c - local UNIX async appletalk interface
 *
 * Async AppleTalk for UNIX:
 *
 * We allocate a complete (non-extended) network (async_net) for use by
 * asynchronous appletalk clients on this UNIX host. Packets destined for async
 * nodes on async_net are sent to the async node client process at the special
 * UDP port (nodeNum+PortOffset). If the packets are broadcast, they are sent
 * to the UDP port (aaBroad) for redirection.
 *
 * aaBroad is monitored by our async_listener(). Packets are only resent to
 * preregistered nodes. This cuts down on the amount of extra traffic sent for
 * broadcasts. This function is normally performed by the asyncad daemon if
 * async appletalk is being used in conjunction with a hardware gateway
 * (Webster MultiPort Gateway or FastPath).
 *
 * With UAB, we extend the protocol slightly by using aaBroad to receive
 * packets from the async client for gatewaying onto other nets or for local
 * processing (zone lists etc). This protocol extension is only valid when
 * the async node client process and the bridge IP address are the same host.
 * Normally, we send to the bridge on a range of UDP ports (cf sockets).
 * This is uneconomical for a UNIX KIP bridge as we have to open too many ports.
 *
 * Note that the "async bridge" exists only on 'bridge_net' as 'bridge_node'.
 * We treat async_net as a remote net, not directly connected to us (we
 * don't have a node number on the net).
 *
 * Currently:
 *		PortOffset	0xaa00
 *		aaBroad		750		Unofficial!
 *		
 * Edit History:
 *
 *  December 1, 1990 djh@munnari.OZ.AU created
 *
*/

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

#include "if_desc.h"		/* describes "if"			*/
#include "ddpport.h"		/* describes a ddp port to "lap"	*/
#include "log.h"

/* some logging ideas	*/
#define LOG_LOG 0
#define LOG_PRIMARY 1
#define LOG_LOTS 9

private int asyncatalk_init();
private int asyncatalk_getnode();
private int asyncatalk_listener();
private int asyncatalk_send_ddp();
private int asyncatalk_stats();
private int asyncatalk_tables();
private NODE *asyncatalk_ddpnode_to_node();

/* describe our interface to the world	*/
private char *asyncatalk_lap_keywords[] = {
  "asyncatalk",
  "async",
  NULL
 };

export struct lap_description asyncatalk_lap_description = {
  "Asynchronous AppleTalk Link Access Protocol",
  asyncatalk_lap_keywords,
  TRUE,					/* need more than just key	*/
  asyncatalk_init,			/* init routine			*/
  asyncatalk_stats,			/* stats routine		*/
  asyncatalk_tables			/* tables			*/
};

extern word async_net;			/* import from atalkdbm.c	*/
extern char async_zone[34];		/* import from atalkdbm.c	*/
extern word bridge_net;			/* import from atalkdbm.c	*/
extern byte bridge_node;		/* import from atalkdbm.c	*/

private char *astat_names[] = {
#define ES_PKT_INPUT  0			/* packets input		*/
  "packets input",
#define ES_PKT_ACCEPTED	1		/* accepted input packets	*/
  "packets accepted",
#define ES_PKT_BROADCAST 2		/* packets for rebroadcast	*/
  "packets for rebroadcast",
#define ES_PKT_NOTFORME 3		/* packets not for me		*/
  "packets not for me",
#define ES_PKT_BAD 4			/* bad packets			*/
  "bad packets",
#define ES_BYTES_INPUT 5		/* accepted bytes		*/
  "bytes input",
#define ES_ERR_INPUT 6 			/* number of input errors	*/
  "input errors",
#define ES_PKT_NOHANDLER 7		/* no handler			*/
  "packets without handlers",
#define ES_PKT_OUTPUT  8		/* packets output		*/
  "packets output",
#define ES_BYTES_OUTPUT 9		/* bytes output			*/
  "bytes output",
#define ES_ERR_OUTPUT 10		/* output errors		*/
  "output errors",
#define ES_RESOLVE_ERR_OUTPUT 11	/* could not resolve		*/
  "output resolve error"
#define ES_NUM_COUNTERS 12
};

typedef struct asyncatalk_handle {
  int ah_state;				/* this connection state	*/
#define AALAP_WAITING -1		/*   almost ready		*/
#define AALAP_BAD 0			/*   error			*/
#define AALAP_READY 1			/*   okay			*/
  int ah_skt;				/* asyncatalk socket		*/
  PORT_T ah_port;			/* interface port		*/
  NODE ah_enode;			/* host node id info		*/
  IDESC_TYPE *ah_id;			/* interface description	*/
  int ah_stats[ES_NUM_COUNTERS]; 	/* statistics			*/
} A_HANDLE;

struct ap {				/* DDP AppleTalk inside UDP	*/
	u_char ldst;			/* even				*/
	u_char lsrc;			/* odd				*/
	u_char ltype;			/* even				*/
	u_char dd[8];			/* hop&len,cksum,dstnet,srcnet	*/
	u_char dstnode;
	u_char srcnode;
	u_char dstskt;
	u_char srcskt;
	u_char dtype;
	u_char data[1024];
};

#define aaBroad		750		/* for UAB really aaReBroad	*/
#define PortOffset	0xaa00		/* start of range for nodes	*/
#define MAXNODE		0x7f		/* easier if only 2^n-1		*/

#define UP		1
#define DOWN		0
short node_is_up[MAXNODE];		/* list of nodes and states	*/

/*
 * async set-up, call with network number, interface name and number
 * (the async flag is currently ignored - nothing to wait for ...)
 *
 */

private int
asyncatalk_init(id, async)
IDESC_TYPE *id;
int async;
{
  long len;
  A_HANDLE *ah;
  int i, flags;				/* some more port description	*/
  struct sockaddr_in sin;
  void start_listener();
  private int async_listener();

  if (async_net != 0) {
    logit(LOG_PRIMARY, "only allowed one async network per host!");
    return(FALSE);
  }

  for(i = 0; i < MAXNODE ; i++)
    node_is_up[i] = DOWN;

  if ((ah = (A_HANDLE *)malloc(sizeof(A_HANDLE))) == NULL) {
    logit(LOG_PRIMARY, "can't get memory for port details");
    return(FALSE);
  }

  /* link in both directions */
  id->id_ifuse = (caddr_t) ah;		/* mark				*/
  ah->ah_id = id;			/* remember this		*/

  ah->ah_state = AALAP_WAITING;		/* mark not quite ready		*/
  ah->ah_enode.n_size = 8;		/* 8 bits			*/
  ah->ah_enode.n_bytes = 1;		/* 1 byte			*/
  ah->ah_enode.n_id[0] = 0;		/* this is it, and it's zero	*/
  /* the gateway doesn't have a node on the async net, it just forwards	*/

  async_net = id->id_network;
  if(id->id_zone != NULL)
    strncpy(async_zone, id->id_zone+1, *id->id_zone);

#if (defined(__386BSD__) || defined(__FreeBSD__))
  if((ah->ah_skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
#else  /* __386BSD__ */
  if((ah->ah_skt = socket(AF_INET, SOCK_DGRAM, 0, 0)) < 0) {
#endif /* __386BSD__ */
    logit(LOG_PRIMARY, "asyncatalk: Couldn't open output UDP socket");
    return(FALSE);
  }

  flags  =  PORT_WANTSLONGDDP;		/* only send out long DDP	*/
  flags &= ~PORT_FULLRTMP;		/* don't need RTMP advertising	*/
  flags &= ~PORT_NEEDSBROADCAST;	/* we don't want broadcasts	*/

  /* establish port */
  ah->ah_port = port_create(id->id_network, ah->ah_enode.n_id[0], id->id_zone,
		&ah->ah_enode,			/* just junk with async */
		flags,				/* details, details */
		(caddr_t) ah,			/* useful storage */
		asyncatalk_send_ddp,		/* send interface */
		asyncatalk_ddpnode_to_node,	/* map from ddp */
		NULL,				/* map to ddp */
		id->id_local);			/* demuxer */

  if (ah->ah_port) {
    start_listener(ah);
    ah->ah_state = AALAP_READY;
    logit(LOG_PRIMARY, "port %d valid on interface %s%d",
	ah->ah_port, id->id_intf, id->id_intfno);
  } else {
    ah->ah_state = AALAP_BAD;
    logit(LOG_PRIMARY,"interface %s%d: no space for port",
	id->id_intf, id->id_intfno);
    return(FALSE);
  }
  return(TRUE);
}

/*
 * start our gateway listener on UDP port 750 (aaBroad)
 * (this is really a KIP listener for bridge_net & bridge_node)
 *
 */

void
start_listener(ah)
A_HANDLE *ah;
{
  int s;
  long len;
  private int async_listener();
  struct sockaddr_in sin;

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    logit(LOG_LOG|L_UERR, "socket: async listener init");
    return;
  }
  len = 6 * 1024; /* should be enough ?? */
  if(setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&len, sizeof(long)) != 0)
    logit(LOG_PRIMARY, "setsockopt: Couldn't set socket recv buffer size");

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(aaBroad);
  if (bind(s, (caddr_t) &sin, sizeof(sin)) < 0) {
    logit(LOG_LOG|L_UERR, "bind: async listener init");
    close(s);
    return;
  }
  init_fdlistening();
  fdlistener(s, async_listener, (caddr_t) ah, 0);
  return;
}

/*
 * resolve a ddp node number to a node address on the specified port
 * (note: do we need more information in some cases?)
*/

private NODE *
asyncatalk_ddpnode_to_node(port, ddpnet, ddpnode)
PORT_T port;
word ddpnet;
byte ddpnode;
{
  static NODE node = { 1, 8 };		/* initialize */
  int myddpnet = PORT_DDPNET(port);

  if (ddpnet != 0 && myddpnet != ddpnet) /* only allow this net! */
    return(NULL);
  node.n_id[0] = ddpnode;		/* make node */
  return(&node);
}

/*
 * listener gets called by low-level select when
 * a packet is ready to be read from the network
 *
 */

private int
async_listener(fd, ah, intarg)
int fd;
A_HANDLE *ah;
int intarg;
{
  DDP hdr;
  struct ap ap;
  struct sockaddr_in fsin;
  int fsinlen, n, i, ddpnode;
  int *stats = ah->ah_stats;
  word dstnet;

  if(bridge_node == 0) {	/* not ready yet */
	logit(LOG_PRIMARY,"async_listener: received packet, gateway not ready");
	stats[ES_PKT_NOHANDLER]++;
  	return(1);
  }

  fsinlen = sizeof(fsin);
  if((n = recvfrom(fd, (caddr_t) &ap, sizeof(ap), 
  		0, (caddr_t) &fsin, &fsinlen)) < 0) {
  	logit(LOG_PRIMARY, "async_listener: recvfrom failed");
	stats[ES_ERR_INPUT]++;
  	return(1);
  }

  if(ap.ldst == 0xC0 && ap.lsrc == 0xDE) {
  	/* an async node is registering with us 	*/
  	i = ap.srcskt & MAXNODE;	/* who from	*/
  	node_is_up[i] = ap.ltype;	/* up or down	*/
  	logit(LOG_PRIMARY,"async node %d is %s", i, (ap.ltype) ? "UP" : "DOWN");
  	return(0);
  }

  if(ap.ldst != 0xFA || ap.lsrc != 0xCE || ap.ltype != 2) {
  	logit(LOG_PRIMARY, "async_listener: found a bad packet");
	stats[ES_PKT_BAD]++;
  	return(1);		/* not a valid lap header	*/
  }

  stats[ES_PKT_INPUT]++;
  stats[ES_BYTES_INPUT] += n;
  n -= (ddpSize+3);		/* remove header bytes	*/

  bcopy(ap.dd, &hdr, sizeof(hdr));
  dstnet = ntohs(hdr.dstNet);

  /* if not a broadcast on our net, route it internally */

  if(!(dstnet == async_net && hdr.dstNode == 0xff)) {
    ddp_route(ah->ah_port, &hdr, ap.data, n, (hdr.dstNode == 0xff));
    stats[ES_PKT_ACCEPTED]++;
    return(0);
  }

  stats[ES_PKT_BROADCAST]++;

  /* here we have a valid broadcast, redirect it to nodes */

  fsin.sin_family = AF_INET;
  fsin.sin_addr.s_addr = inet_addr("127.0.0.1");

  for(i = 1 ; i < MAXNODE ; i++) {	/* note: 0 is invalid node #	*/
  	if(node_is_up[i]) {
  		fsin.sin_port = htons((short)i + PortOffset);
  		if(sendto(fd, (caddr_t) &ap, n, 0, &fsin, sizeof(fsin)) != n) {
  			logit(LOG_PRIMARY, "rebroadcast: sendto failed");
    			stats[ES_ERR_OUTPUT]++; /* count and ignore */
  		}
  	}
  }
  return(0);
}

/*
 * send a ddp packet on async appletalk (take long DDP only)
 * 
 * port = port to send on
 * dstnode == destination async node
 * laptype == laptype of packet (header)
 * header = packet header (for laptype)
 * hsize = packet header length
 * data = data
 * dlen = datalength
 *
 * Should we be doing the broadcast multiplexing in here to save time ?
 */

private
asyncatalk_send_ddp(port, dstnode, laptype, header, hsize, data, dlen)
PORT_T port;
NODE *dstnode;
int laptype;
byte *header;
int hsize;
u_char *data;
int dlen;
{
  DDP *ddp;
  word length;
  struct ap ap;
  struct sockaddr_in sin;
  A_HANDLE *ah = PORT_GETLOCAL(port, A_HANDLE *);
  int *stats = ah->ah_stats;

  if (bridge_node == 0) {			/* not ready, drop it */
    stats[ES_RESOLVE_ERR_OUTPUT]++;
    return(-1);
  }

  if (hsize != ddpSize) {			/* drop it */
    stats[ES_ERR_OUTPUT]++;
    return(-1);
  }

  if (ah->ah_state != AALAP_READY) {		/* drop it */
    stats[ES_ERR_OUTPUT]++;
    return(-1);
  }

  if (dstnode == NULL) {			/* drop it */
    stats[ES_ERR_OUTPUT]++;
    return(-1);
  }

  if (dstnode->n_size != ah->ah_enode.n_size) {	/* drop it */
    stats[ES_ERR_OUTPUT]++;
    return(-1);
  }

  ddp = (DDP *) header;

  ap.ldst = 0xFA;			/* magic */
  ap.lsrc = 0xCE;			/* magic */
  ap.ltype = lapDDP;			/* long DDP only on KIP */
  if(ddp->srcNet == htons(async_net)) {	/* not really connected	*/
    ddp->srcNet = htons(bridge_net);
    ddp->srcNode = bridge_node;
  }
  bcopy(header, ap.dd, sizeof(ap.dd));	/* copy, as words are on odd addr */
  ap.dstnode = ddp->dstNode;
  ap.srcnode = ddp->srcNode;
  ap.dstskt = ddp->dstSkt;
  ap.srcskt = ddp->srcSkt;
  ap.dtype = ddp->type;
  bcopy(data, ap.data, dlen);
  sin.sin_family = AF_INET;
  sin.sin_port = htons((ap.dstnode==0xff) ? aaBroad : (ap.dstnode+PortOffset));
  sin.sin_addr.s_addr = inet_addr("127.0.0.1");

  length = dlen + ddpSize + 3;

  if(sendto(ah->ah_skt, (caddr_t)&ap, length, 0, &sin, sizeof(sin)) != length) {
    logit(LOG_PRIMARY, "Couldn't write to the asyncatalk client");
    stats[ES_ERR_OUTPUT]++;
    return(-1);				/* drop it */
  }
  stats[ES_PKT_OUTPUT]++;
  stats[ES_BYTES_OUTPUT] += length;
  return(length);
}

private int
asyncatalk_stats(fd, id)
FILE *fd;
IDESC_TYPE *id;
{
  int i;
  A_HANDLE *ah = (A_HANDLE *)id->id_ifuse; /* get handle */
  
  fprintf(fd, "Interface %s%d statisitics\n", id->id_intf,
	  id->id_intfno);
  fprintf(fd, " Interface counters\n");
  for (i = 0; i < ES_NUM_COUNTERS; i++) {
    fprintf(fd, "  %8d\t%s\n", ah->ah_stats[i], astat_names[i]);
  }
  putc('\n', fd);		/* carriage return */
}

private int
asyncatalk_tables(fd, id)
FILE *fd;
IDESC_TYPE *id;
{
  A_HANDLE *ah = (A_HANDLE *)id->id_ifuse; /* get handle */

  fprintf(fd, "Interface dump for %s%d\n",id->id_intf, id->id_intfno);
  putc('\n', fd);		/* carriage return */
}
