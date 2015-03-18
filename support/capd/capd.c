/*
 * $Author: djh $ $Date: 1996/09/10 16:15:17 $
 * $Header: /mac/src/cap60/support/capd/RCS/capd.c,v 2.6 1996/09/10 16:15:17 djh Rel djh $
 * $Revision: 2.6 $
 *
 */

/*
 * capd - general purpose CAP daemon
 *
 * djh@munnari.OZ.AU
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#ifdef PHASE2
#include <sys/socket.h>
#if (!(defined(ultrix) || defined(linux)))
#include <sys/sockio.h>
#endif  ultrix || linux
#include <net/if.h>
#endif PHASE2
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netat/appletalk.h>

/*
 * etalkdbm globals
 *
 */
extern char interface[50];		/* which ethernet device	*/
extern char this_zone[34];		/* zone for this host		*/
extern struct in_addr bridge_addr;	/* IP address for local bridge	*/
extern byte bridge_node;		/* the local bridge		*/
extern word bridge_net;			/* the local bridge		*/
extern byte this_node;			/* this host node		*/
extern word this_net;			/* this host node		*/
extern byte nis_node;			/* atis running here		*/
extern word nis_net;			/* atis running here		*/
extern word net_range_start;		/* phase 2 network range start	*/
extern word net_range_end;		/* phase 2 network range end	*/

extern short lap_proto;			/* our LAP mechanism		*/

int node, net, net_lo, net_hi;
char *zonename = NULL;
char *ifname = NULL;

private char mcaddr[6] = { 0x09, 0x00, 0x07, 0xff, 0xff, 0xff };

int dlevel=0;				/* debug level			*/

main(argc, argv)
int argc;
char *argv[];
{
	int c;
	char *cp, *ep;
	extern int optind;
	extern char *optarg;
	struct ifreq ifreq;
	short capdIdent();
	void run();

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

	/*
	 * initialise externals
	 *
	 */
	this_node = 0;
	this_net = htons(0xff00);
	nis_net = nis_node = bridge_net = bridge_node = 0;
	bridge_addr.s_addr = inet_addr("127.0.0.1");
	net_range_start = htons(0);
	net_range_end = htons(0xfffe);

	lap_proto = capdIdent();

	/*
	 * get supplied interface & zone names
	 *
	 */
	if (argc == (optind+2)) {
	  ifname = argv[optind++];
	  zonename = argv[optind++];
	  strncpy(interface, ifname, sizeof(interface));
	  strncpy(this_zone, zonename, sizeof(this_zone));
	}

	/*
	 * wrong number of args ?
	 *
	 */
	if (optind != argc) {
	  fprintf(stderr,
	    "usage: capd [-D level] [-d opt] [-l log] [interface zone]\n");
	  exit(1);
	}

	if (ifname == NULL
	 || *ifname == '\0') {
	  fprintf(stderr, "No ethernet interface specified\n");
	  exit(1);
	}
	ep = NULL;
	for (cp = ifname; *cp != '\0'; cp++) {
	  if (*cp >= '0' && *cp <= '9')
	    ep = cp;
	}
	if (ep == NULL) { /* interface, but no number */
	  fprintf(stderr, "Specified interface invalid: %s\n", ifname);
	  exit(1);
	}
	if (zonename == NULL
	 || *zonename == '\0') {
	  fprintf(stderr, "No zone name specified\n");
	  exit(1);
	}

	/*
	 * config Kernel AppleTalk
	 * startup range, any node
	 *
	 */
	node = 0x00;
	net = 0xff00;
	net_lo = 0x0000;
	net_hi = 0xfffe;

	if (ifconfig(&node, &net, &net_lo, &net_hi) < 0) {
	  fprintf(stderr, "Can't initialise AppleTalk on %s\n", ifname);
	  exit(1);
	}

#ifdef PHASE2
	/*
	 * add multicast address to interface
	 *
	 */
	strncpy(ifreq.ifr_name, ifname, sizeof(ifreq.ifr_name));
	if (pi_addmulti(mcaddr, (caddr_t)&ifreq) < 0) {
	  fprintf(stderr, "Can't add multicast address!\n");
	  exit(1);
	}

	/*
	 * if phase 2, ask network for net
	 * ranges, get zone multicast address and
	 * verify user supplied zone name.
	 *
	 */
	if (getNetInfo() >= 0) {
	  net = net_lo;
	  strncpy(interface, ifname, sizeof(interface));
	  if (ifconfig(&node, &net, &net_lo, &net_hi) < 0) {
	    fprintf(stderr, "Can't reinitialise AppleTalk on %s\n", ifname);
	    exit(1);
	  }
	  net_range_start = htons(net_lo);
	  net_range_end = htons(net_hi);
	}
#endif /* PHASE2 */

	nis_net = this_net = net;
	nis_node = this_node = node;
	strncpy(interface, ifname, sizeof(interface));
	strncpy(this_zone, zonename, sizeof(this_zone));
	bridge_addr.s_addr = inet_addr("127.0.0.1");
	bridge_net = bridge_node = 0;

	etalkdbupdate(NULL);	/* rewrite gleaned information	*/

	if (!dbug.db_flgs && (dlevel == 0))
	  disassociate();

	run(); /* do all the CAPD work */

	(void)fprintf(stderr, "capd: run() returned!\n");

	exit(1);
}

disassociate()
{
	int i;
	/* disassociate */
	if (fork())
	  _exit(0);			/* kill parent */
	for (i=0; i < 3; i++) close(i); /* kill */
	(void)open("/",0);
#ifdef NODUP2
	(void)dup(0);			/* slot 1 */
	(void)dup(0);			/* slot 2 */
#else  NODUP2
	(void)dup2(0,1);
	(void)dup2(0,2);
#endif NODUP2
#ifdef TIOCNOTTY
	if ((i = open("/dev/tty",2)) > 0) {
	  (void)ioctl(i, TIOCNOTTY, (caddr_t)0);
	  (void)close(i);
	}
#endif TIOCNTTY
#ifdef POSIX
	(void) setsid();
#endif POSIX
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
	struct ifreq ifreq;

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
	    net_lo = (zis[2] << 8) | zis[3];		/* net byte order */
	    net_hi = (zis[4] << 8) | zis[5];		/* net byte order */
	    if ((x = zis[6]) < sizeof(defzone)) {
	      bcopy(&zis[7], defzone, x);
	      defzone[x] = '\0';
	    }

	    /*
	     * check multicast address length
	     *
	     */
	    x += 7;
	    if (zis[x] != sizeof(zmcaddr)) {
	      fprintf(stderr, "Bogus Multicast Address length %d\n", zis[x]);
	      exit(1);
	    }

	    /*
	     * get zone multicast address
	     *
	     */
	    bcopy((char *)&zis[x+1], zmcaddr, sizeof(zmcaddr));

	    logit(3, "GetNetInfo reply packet arrived:");
	    logit(3, "%sFlags 0x%02x, rangeStart %04x, rangeEnd %04x",
		"    ", zis[1], net_lo, net_hi);
	    logit(3, "%sZone %s (len %d), MCZAddr %x:%x:%x:%x:%x:%x",
		"    ", defzone, zis[6], (u_char) zmcaddr[0],
		    (u_char) zmcaddr[1], (u_char) zmcaddr[2],
		    (u_char) zmcaddr[3], (u_char) zmcaddr[4],
		    (u_char) zmcaddr[5]);

	    /*
	     * supplied zone name valid ?
	     *
	     */
	    if (zis[1] & ZIPZoneBad) {
	      x += 7;
	      if (zis[x] < sizeof(defzone)) {
	        bcopy((char *)&zis[x+1], defzone, zis[x]);
	        defzone[zis[x]] = '\0';
	      }
	      fprintf(stderr, "Zone \"%s\" unknown, network default is \"%s\"\n",
	        zonename, defzone);
	      exit(1);
	    }

	    /*
	     * set zone multicast address on interface
	     *
	     */
	    strncpy(ifreq.ifr_name, ifname, sizeof(ifreq.ifr_name));
	    if (pi_addmulti(zmcaddr, (caddr_t)&ifreq) < 0) {
	      fprintf(stderr, "Can't add zone multicast address on %s!\n", ifname);
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
	int count, error;

	abInit(FALSE);
	heardFrom = 0;
	zisSkt = ddpZIP;

	if (DDPOpenSocket(&zisSkt, zis_listener) != noErr) {
	  logit(0, "Failed to start ZIS listener!");
	  exit(1);
	}

	zipbuf[0] = ZIPGetInfoReq;
	zipbuf[1] = 0x0;
	zipbuf[2] = 0x0;
	zipbuf[3] = 0x0;
	zipbuf[4] = 0x0;
	zipbuf[5] = 0x0;
	zipbuf[6] = strlen(zonename);
	strncpy(&zipbuf[7], zonename, sizeof(zipbuf)-8);

	ddpr.abResult = 0;
	ddpr.proto.ddp.ddpAddress.net  = 0x0000;	/* local net	*/
	ddpr.proto.ddp.ddpAddress.node = 0xff;		/* broadcast	*/
	ddpr.proto.ddp.ddpAddress.skt  = zisSkt;	/* to ZIS at GW	*/
	ddpr.proto.ddp.ddpSocket = zisSkt;		/* from our ZIS	*/
	ddpr.proto.ddp.ddpType = ddpZIP;
	ddpr.proto.ddp.ddpDataPtr = (u_char *)zipbuf;
	ddpr.proto.ddp.ddpReqCount = zipbuf[6] + 7;

	for (count = 0 ; count < ZIPATTEMPTS ; count++) {
	  logit(3, "sending GetNetInfo request ...");
	  DDPWrite(&ddpr, FALSE);			/* send it to GW */
	  abSleep(sectotick(4), FALSE);			/* wait for a reply */
	  if (heardFrom)
	    break;					/* don't ask again  */
	}

	error = (heardFrom) ? 0 : -1;

	if (!heardFrom)
	  heardFrom = 1;				/* ignore it later  */

	return(error);
}

/*
 * add a multicast address to the interface
 *
 */

int
pi_addmulti(multi, ifr)
char *multi;
struct ifreq *ifr;
{
	int sock;

	ifr->ifr_addr.sa_family = AF_UNSPEC;
	bcopy(multi, ifr->ifr_addr.sa_data, 6);

	/*
	 * open a socket, temporarily, to use for SIOC* ioctls
	 *
	 */
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	  perror("socket()");
	  return(-1);
	}

	if (ioctl(sock, SIOCADDMULTI, (caddr_t)ifr) < 0) {
	  perror("SIOCADDMULTI");
	  close(sock);
	  return(-1);
	}

	close(sock);

	return(0);
}
#endif PHASE2
