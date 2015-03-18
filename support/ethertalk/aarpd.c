/*
 * RPC AARP daemon for CAP services via Native EtherTalk
 * (also maintains information about the current bridge)
 *
 * Created: Charles Hedrick, Rutgers University <hedrick@rutgers.edu>
 * Modified: David Hornsby, Melbourne University <djh@munnari.OZ.AU>
 *	16/02/91: add support for CAP 6.0 atalkdbm routines, back out
 *	of shared memory in favour of RPC based [SG]etBridgeAddress()
 *	Add the SVC descriptors to the low level scheduler.
 *	28/04/91: Add Phase 2 support, SetNetRange()
 */

#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#ifdef PHASE2
#include <sys/socket.h>
#if !defined(ultrix) && !defined(__osf__) && !defined(__386BSD__) && !defined(__FreeBSD__) && !defined(__bsdi__) && !defined(NeXT)
#include <sys/sockio.h>
#endif  /* ultrix && __osf__ && __386BSD__ && __FreeBSD__ */
#include <net/if.h>
#endif PHASE2
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netat/appletalk.h>

#if (defined(__386BSD__) || defined(SOLARIS) || defined(__FreeBSD__)) && !defined(__bsdi__)
#ifdef SOLARIS
#define PORTMAP
#endif SOLARIS
#include <rpc/rpc.h>
#else  /* __386BSD__ or SOLARIS or __FreeBSD__ */
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/svc.h>
#endif /* __386BSD__  or SOLARIS or __FreeBSD__ */
#include "../uab/ethertalk.h"		/* iso: level 1 */
#include "../uab/if_desc.h"
#include "../uab/log.h"
#include "aarpd.h"

extern struct lap_description ethertalk_lap_description;
extern u_char *etalk_resolve();		/* address resolver		*/
extern void aarpdprog();		/* the RPC program interface	*/
extern u_char interface[50];		/* which ethernet device	*/
extern u_char this_zone[34];		/* zone for this host		*/
extern struct in_addr bridge_addr;	/* IP address for local bridge	*/
extern byte bridge_node;		/* the local bridge		*/
extern word bridge_net;			/* the local bridge		*/
extern byte this_node;			/* this host node		*/
extern word this_net;			/* this host node		*/
extern byte nis_node;			/* atis running here		*/
extern word nis_net;			/* atis running here		*/
#ifdef PHASE2
extern word net_range_start;		/* phase 2 network range start	*/
extern word net_range_end;		/* phase 2 network range end	*/
extern int etalk_set_mynode();		/* update our node address	*/
#endif PHASE2

extern short lap_proto;			/* our LAP mechanism		*/

#ifndef ultrix
extern fd_set svc_fdset;
#endif  ultrix

byte pzone[INTZONESIZE+1];		/* zone (pascal string)		*/
char intrfc[INTFSIZE];			/* interface description	*/
u_char null_ether[6];			/* null ethernet address	*/
AddrBlock baddr;			/* current bridge address	*/
IDESC_TYPE id;				/* our node information		*/
int dlevel=0;				/* debug level			*/

u_char *aarp_resolve_svc();		/* the aarpd resolver		*/
u_char *rtmp_getbaddr_svc();		/* get the bridge address	*/
u_char *rtmp_setbaddr_svc();		/* set the bridge address	*/

#ifdef PHASE2
struct ifreq ifreq;
u_char e_broad[6] = {0x09, 0x00, 0x07, 0xff, 0xff, 0xff};
#endif PHASE2

main(argc, argv)
int argc;
char *argv[];
{
	int c;
	fd_set rdfds;
	char *cp, *ep;
	SVCXPRT *transp;
	extern int optind;
	extern char *optarg;
	void init_enet(), svc_run(), set_svc_listen(), run();
#ifdef ISO_TRANSLATE
	void cISO2Mac();
#endif ISO_TRANSLATE

	while ((c = getopt(argc, argv, "D:d:l:")) != EOF) {
	  switch (c) {
	  case 'D':
	    dlevel = atoi(optarg);
	    if (dlevel > L_LVLMAX)
	      dlevel = L_LVLMAX;
	    break;
	  case 'd':
	    dbugarg(optarg);
	    dlevel = 1;
	    break;
	  case 'l':
	    logitfileis(optarg, "w");
	    break;
	  }
	}
	set_debug_level(dlevel);
	
	openetalkdb(NULL);		/* open/create etalk.local */

	nis_net = this_net = 0;		/* assume that we don't know */
	bridge_net = bridge_node = 0;
  	bridge_addr.s_addr = inet_addr("127.0.0.1");

#ifdef PHASE2
	this_net = htons(0xff00);	/* the startup range */
	net_range_start = htons(0);	/* the total range */
	net_range_end = htons(0xfffe);
#endif PHASE2

	baddr.node = bridge_node;	/* set bridge addr hint */
	baddr.net = bridge_net;
	baddr.skt = 0;

	if (argc == (optind+2)) {
	  /* arg list supplied interface & zone names */
	  strncpy((char *)interface, argv[optind++], sizeof(interface));
	  strncpy((char *)this_zone, argv[optind++], sizeof(this_zone));
#ifdef ISO_TRANSLATE
	  cISO2Mac(this_zone);
#endif ISO_TRANSLATE
	}

	if (optind != argc) {
	  fprintf(stderr,
	    "usage: aarpd [-D level] [-d opt] [-l log] [interface zone]\n");
	  exit(1);
	}

	if (*interface == '\0') {
	  fprintf(stderr, "No ethernet interface specified\n");
	  exit(1);
	}
	ep = NULL;
	strncpy(intrfc, interface, sizeof(intrfc));
	for (cp = intrfc; *cp != '\0'; cp++) {
	  if (*cp >= '0' && *cp <= '9')
	    ep = cp;
	}
	if (ep == NULL) { /* interface, but no number */
	  fprintf(stderr, "Specified interface invalid: %s\n", interface);
	  exit(1);
	}
	id.id_intfno = *ep - '0';
	id.id_intf = intrfc;
	*ep = '\0';

	if (*this_zone == '\0') {
	  fprintf(stderr, "No zone name specified\n");
	  exit(1);
	}
	/* convert zone to pascal string */
	strncpy(pzone+1, (char *)this_zone, sizeof(pzone)-1);
	pzone[0] = strlen(this_zone);
	id.id_zone = pzone;

	init_enet();		/* init & get a node number */
	etalkdbupdate(NULL);	/* rewrite gleaned information	*/

	bzero(null_ether, sizeof(null_ether));
#ifdef PHASE2
	strncpy(ifreq.ifr_name, interface, sizeof(ifreq.ifr_name));
	if (pi_addmulti(e_broad, (caddr_t)&ifreq) < 0) {
	  fprintf(stderr, "Can't add multicast address!\n");
	  exit(1);
	}
#endif PHASE2

	/* set up aarpd RPC services */

	(void)pmap_unset(AARPDPROG, AARPDVERS);
	(void)svc_unregister(AARPDPROG, AARPDVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		(void)fprintf(stderr, "aarpd: cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp,AARPDPROG,AARPDVERS,aarpdprog,IPPROTO_UDP)) {
		(void)fprintf(stderr,
			"unable to register (AARPDPROG, AARPDVERS, udp).\n");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		(void)fprintf(stderr, "aarpd: cannot create tcp service.\n");
		exit(1);
	}
	if (!svc_register(transp,AARPDPROG,AARPDVERS,aarpdprog,IPPROTO_TCP)) {
		(void)fprintf(stderr,
			"unable to register (AARPDPROG, AARPDVERS, tcp).\n");
		exit(1);
	}

	set_svc_listen();	/* add RPC descriptors to scheduler */

#ifdef PHASE2
	getNetInfo();		/* find out about our network */
#endif PHASE2

	if (!dbug.db_flgs && (dlevel == 0))
		disassociate();

	run(); /* do all the CAP and RPC work */

	(void)fprintf(stderr, "aarpd: run() returned!\n");
	exit(1);
}

/*
 * initialise network and get node number
 *
 */

void
init_enet()
{
  struct ethertalkaddr *ea, *etalk_mynode();
#ifdef ISO_TRANSLATE
  void pMac2ISO(), pISO2Mac();
#endif ISO_TRANSLATE

  id.id_ld = &ethertalk_lap_description;
  id.id_local = NULL;
  id.id_isabridge = 0;
  id.id_network = 0;
  id.id_next = NULL;
  id.id_ifuse = 0;

  if (dbug.db_flgs || (dlevel != 0)) {
#ifdef ISO_TRANSLATE
    pMac2ISO(id.id_zone);
    printf("Interface %s%d, zone %s, ",
      id.id_intf, id.id_intfno, id.id_zone+1);
    pISO2Mac(id.id_zone);
#else  ISO_TRANSLATE
    printf("Interface %s%d, zone %s, ",
      id.id_intf, id.id_intfno, id.id_zone+1);
#endif ISO_TRANSLATE
  }

  init_fdlistening();  			/* low level scheduler	*/
  if (!etalk_init(&id, FALSE)) {	/* set up EtherTalk	*/
    fprintf(stderr, "init_enet: network initialization failed\n");
    exit(1);
  }

  ea = etalk_mynode(&id);
  this_node = nis_node = ea->node;
#ifdef PHASE2
  this_net = nis_net = htons((ea->dummy[1] << 8) | ea->dummy[2]);
#endif PHASE2

  if (dbug.db_flgs || (dlevel != 0))
#ifdef PHASE2
    printf("acquired node number %d/%d.%d\n",
	this_node, (ntohs(this_net) >> 8) & 0xff, ntohs(this_net) & 0xff);
#else  PHASE2
    printf("acquired node number %d\n", this_node);
#endif PHASE2
}

disassociate()
{
  int i;
  /* disassociate */
  if (fork())
    _exit(0);			/* kill parent */
#ifdef POSIX
  (void)setsid();
#endif POSIX
  for (i=0; i < 3; i++) close(i); /* kill */
  (void)open("/",0);
#ifdef NODUP2
  (void)dup(0);			/* slot 1 */
  (void)dup(0);			/* slot 2 */
#else  NODUP2
  (void)dup2(0,1);
  (void)dup2(0,2);
#endif NODUP2
#ifndef POSIX
#ifdef TIOCNOTTY
  if ((i = open("/dev/tty",2)) >= 0) {
    (void)ioctl(i, TIOCNOTTY, (caddr_t)0);
    (void)close(i);
  }
#endif /* TIOCNTTY */
#else /* POSIX */
  (void) setsid();
#endif /* POSIX */
}

/*
 * these routines do the work for the RPC calls
 *
 */

u_char *
aarp_resolve_svc(node, rqstp)
  int *node;
  struct svc_req *rqstp;
{
  u_char *ea;

#ifdef PHASE2
  struct ethertalkaddr *pa = (struct ethertalkaddr *) node;
  logit(2, "request for %d/%d.%d", pa->node, pa->dummy[1], pa->dummy[2]);
#else  PHASE2
  logit(2, "request for %d", ntohl(*node));
#endif PHASE2
  ea = etalk_resolve(&id, *node);
  return((ea) ? ea : null_ether);
}

/*
 * return the current bridge address
 *
 */

u_char *
rtmp_getbaddr_svc(i, rqstp)
  int *i;
  struct svc_req *rqstp;
{
  logit(2, "request for bridge address: %d.%d", ntohs(baddr.net), baddr.node);
  return((u_char *) &baddr);
}

/*
 * set the bridge address
 * (normally called from atis, which is listening to RTMP packets)
 *
 */

u_char *
rtmp_setbaddr_svc(bad, rqstp)
  AddrBlock *bad;
  struct svc_req *rqstp;
{
  logit(2, "setting bridge address");
  bcopy(bad, &baddr, sizeof(baddr));
  bridge_node = baddr.node;
  bridge_net = baddr.net;
#ifndef PHASE2
  this_net = nis_net = baddr.net;
#endif  PHASE2
  bridge_addr.s_addr = inet_addr("127.0.0.1");
  etalkdbupdate(NULL);	/* write the info. back out */
  return((u_char *) &baddr);
}

#ifdef PHASE2
/*
 * Set the network range (Phase 2). Check that the current
 * node number is free for use in the new range by aarping
 * for the new address to see if anybody responds.
 *
 * (could be called from atis, which is listening to
 * RTMP packets or for an arriving GetNetInfo packet)
 *
 */

u_char *
range_set_svc(range, rqstp)
  unsigned long *range;
  struct svc_req *rqstp;
{
  int attempts, totalattempts;
  static struct ethertalkaddr eaddr;
  short new_net, new_net_start, new_net_end;

  new_net_start = ntohs((*range >> 16) & 0xffff); /* host byte order */
  new_net_end   = ntohs((*range & 0xffff));	  /* host byte order */
  new_net = new_net_start;			  /* host byte order */
  logit(5, "Changing network range: %04x-%04x -> %04x-%04x",
        net_range_start, net_range_end, new_net_start, new_net_end);
  eaddr.dummy[0] = 0;
  eaddr.dummy[1] = (new_net >> 8) & 0xff;
  eaddr.dummy[2] = (new_net & 0xff);
  eaddr.node = this_node;
  /* probe a few times for the new address */
  totalattempts = 0;
  for (attempts = 0 ; attempts < 20 ; attempts++) {
    logit(5, "Probing for %d/%d.%d", eaddr.node,
		eaddr.dummy[1], eaddr.dummy[2]);
    /* this is a hack, just sending aarp probes doesn't */
    /* work so we alternate them with aarp requests :-( */
    eaddr.dummy[0] = ((attempts & 0x01) == 0);
    if (etalk_resolve(&id, *(long *)&eaddr) != NULL) {
      logit(5, "Node number %d already in use!", eaddr.node);
      /* oops, pick another */
      if (++eaddr.node >= 0xfe) /* reserved */
	eaddr.node = 1;
      attempts = 0; /* same again */
      if (++totalattempts > 252) {
        /* oh dear, have to try another net */
	if (++new_net > new_net_end) {
	  logit(5, "OOPS: no spare nodes available on net!!");
	  return(NULL);
	}
        eaddr.dummy[1] = (new_net >> 8) & 0xff;
        eaddr.dummy[2] = (new_net & 0xff);
	totalattempts = 0;
      }
    }
    abSleep(1, FALSE); /* allow protocols to run */
  }
  eaddr.dummy[0] = 0;
  /* adopt it by updating our internal tables */
  if (etalk_set_mynode(&id, &eaddr) < 0) {
    logit(5, "Couldn't update net and node numbers");
    return(NULL);
  }
  logit(5, "Adopting %d/%d.%d", eaddr.node, eaddr.dummy[1], eaddr.dummy[2]);
  net_range_start = htons(new_net_start);
  net_range_end = htons(new_net_end);
  this_net = nis_net = htons(new_net);
  this_node = nis_node = eaddr.node;
  etalkdbupdate(NULL);
  return((u_char *) range);
}
#endif PHASE2

/*
 * add the RPC descriptors to the low level scheduler
 *
 */

void
set_svc_listen()
{
	int i;
	private int svc_listener();

#ifdef ultrix
	for(i = 0 ; i < 32 ; i++)
		if (svc_fds & (1<<i))
#else  ultrix
	for(i = 0 ; i < NFDBITS ; i++)
		if (FD_ISSET(i, &svc_fdset))
#endif ultrix
			fdlistener(i, svc_listener, NULL, 0);
}

/*
 * service a waiting SVC call
 *
 */

private int
svc_listener(fd, addr, n)
int fd;
caddr_t addr;
int n;
{
#ifdef ultrix
  int nfds;
  nfds = (1 << fd);
  svc_getreq(nfds);
#else  ultrix
  fd_set nfds;
  FD_ZERO(&nfds);
  FD_SET(fd, &nfds);
  svc_getreqset(&nfds);
#endif ultrix
}

/*
 * all the really hard work happens here
 *
 */

void
run()
{
  	for (;;)
	  abSleep(sectotick(30), TRUE);
}

#ifdef PHASE2

#define ZIPQuery	1
#define ZIPReply	2
#define ZIPEReply	8
#define ZIPGetInfoReq	5
#define ZIPGetInfoRepl	6

#define ZIPATTEMPTS	4

#define ZIPZoneBad	0x80
#define ZIPNoMultiCast	0x40
#define ZIPSingleZone	0x20

private int zisSkt;
private int heardFrom;
private char zmcaddr[6];
private char defzone[34];
private u_short range_start;
private u_short range_end;

/*
 * This is the ZIP ZIS listener
 * (at present we are only interested
 * in receiving one GetNetInfo packet
 * and that only at startup)
 *
 */

void
zis_listener(skt, type, zis, len, addr)
u_char skt;
u_char type;
u_char *zis;
int len;
AddrBlock *addr;
{
  int x;
  unsigned long range;
  u_char *GetMyZone();
  u_char *range_set_svc();

  if (heardFrom)
    return; /* drop it */

  if (type == ddpATP)
    return; /* drop it */

  if (type != ddpZIP) {
    logit(3, "ZIS listener - bad packet");
    return; /* drop it */
  }

  defzone[0] = '\0';

  switch (*zis) {
    case ZIPQuery:
    case ZIPReply:
    case ZIPEReply:
    case ZIPGetInfoReq:
      break; /* drop it */
    case ZIPGetInfoRepl:
      heardFrom = 1;
      range_start = htons((zis[2] << 8) | zis[3]);	/* net byte order */
      range_end   = htons((zis[4] << 8) | zis[5]);	/* net byte order */
      if ((x = zis[6]) < sizeof(defzone)) {
        bcopy(&zis[7], defzone, x);
	defzone[x] = '\0';
      }
      x += 7;	/* multicast address */
      if (zis[x] != sizeof(zmcaddr)) {
	fprintf(stderr, "Bogus Multicast Address length %d\n", zis[x]);
	exit(1);
      }
      bcopy(&zis[x+1], zmcaddr, sizeof(zmcaddr));
      logit(3, "GetNetInfo reply packet arrived:");
      logit(3, "%sFlags 0x%02x, rangeStart %04x, rangeEnd %04x",
		"    ", zis[1], ntohs(range_start), ntohs(range_end));
      logit(3, "%sZone %s (len %d), MCZAddr %x:%x:%x:%x:%x:%x",
		"    ", defzone, zis[6], (u_char) zmcaddr[0],
		    (u_char) zmcaddr[1], (u_char) zmcaddr[2],
		    (u_char) zmcaddr[3], (u_char) zmcaddr[4],
		    (u_char) zmcaddr[5]);
      if (zis[1] & ZIPZoneBad) {
        x += 7;
	if (zis[x] < sizeof(defzone)) {
	  bcopy(&zis[x+1], defzone, zis[x]);
	  defzone[zis[x]] = '\0';
	}
	fprintf(stderr, "Zone \"%s\" unknown, network default is \"%s\"\n",
		(char *)GetMyZone(), defzone);
	exit(1);
      }
      range = ((range_start << 16) | (range_end & 0xffff));
      if (range_set_svc(&range, 0L) == NULL) {
        fprintf(stderr, "Can't change network range!\n");
        exit(1);
      }
      strncpy(ifreq.ifr_name, interface, sizeof(ifreq.ifr_name));
      if (pi_addmulti(zmcaddr, (caddr_t)&ifreq) < 0) {
        fprintf(stderr, "Can't add zone multicast address!\n");
        exit(1);
      }
      break;
  }
}

/*
 * open the ZIS socket, start the listener, probe for netinfo
 *
 */

int
getNetInfo()
{
  void zis_listener();
  ABusRecord ddpr;
  u_char zipbuf[48];
  u_char *GetMyZone();
  int count;

  abInit(FALSE);
  heardFrom = 0;
  zisSkt = ddpZIP;
  if (DDPOpenSocket(&zisSkt, zis_listener) != noErr) {
    logit(0, "Failed to start ZIS listener!");
    exit(1);
  }
  zipbuf[0] = ZIPGetInfoReq; zipbuf[1] = 0x0;
  zipbuf[2] = 0x0; zipbuf[3] = 0x0;
  zipbuf[4] = 0x0; zipbuf[5] = 0x0;
  strncpy(&zipbuf[7], (char *)GetMyZone(), sizeof(zipbuf)-8);
  zipbuf[6] = strlen(&zipbuf[7]);
  ddpr.abResult = 0;
  ddpr.proto.ddp.ddpAddress.net  = 0x0000;	/* local net	*/
  ddpr.proto.ddp.ddpAddress.node = 0xff;	/* broadcast	*/
  ddpr.proto.ddp.ddpAddress.skt  = zisSkt;	/* to ZIS at GW	*/
  ddpr.proto.ddp.ddpSocket = zisSkt;		/* from our ZIS	*/
  ddpr.proto.ddp.ddpType = ddpZIP;
  ddpr.proto.ddp.ddpDataPtr = (u_char *) zipbuf;
  ddpr.proto.ddp.ddpReqCount = zipbuf[6] + 7;
  for (count = 0 ; count < ZIPATTEMPTS ; count++) {
    logit(3, "sending GetNetInfo request ...");
    DDPWrite(&ddpr, FALSE);			/* send it to GW    */
    abSleep(sectotick(4), FALSE);		/* wait for a reply */
    if (heardFrom) break;			/* don't ask again  */
  }
  /* abSleep(sectotick(10), FALSE);		/* time to re-init  */
  if (!heardFrom)
    heardFrom = 1;				/* ignore it later  */
  logit(3, "AARPD .... Running");
}
#endif PHASE2
