/*
 * simple RPC net range test
 *
 * Created: David Hornsby, Melbourne University <djh@munnari.OZ.AU>
 *
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netat/appletalk.h>
#include "aarpd.h"

#ifdef ultrix
#define ALT_RPC
#include <time.h>
#include <sys/socket.h>
#endif ultrix
#ifdef pyr
#define ALT_RPC
#endif pyr

extern u_char *rtmp_getbaddr_clnt();
extern u_char *rtmp_setbaddr_clnt();

extern u_short	this_net,	bridge_net,	nis_net,	async_net;
extern u_char	this_node,	bridge_node,	nis_node;
extern char	this_zone[34],	async_zone[34],	interface[50];

extern u_short net_range_start, net_range_end;

main(argc, argv)
int argc;
char **argv;
{
  CLIENT *cl;
  u_char *addr;
  u_short r_start, r_end;
  unsigned long range;
#ifdef ALT_RPC
  int sock;
  struct timeval tv;
  struct sockaddr_in sin;
#endif ALT_RPC

#ifdef ALT_RPC
  sin.sin_family = AF_INET;
  sin.sin_port = 0;
  bzero(sin.sin_zero, sizeof(sin.sin_zero));
  sin.sin_addr.s_addr = htonl(0x7f000001);
  sock = RPC_ANYSOCK;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  cl = clntudp_create(&sin, AARPDPROG, AARPDVERS, tv, &sock);
#else ALT_RPC
  cl = clnt_create("localhost", AARPDPROG, AARPDVERS, "udp");
#endif ALT_RPC
  if (cl == NULL) {
    clnt_pcreateerror("localhost");
    exit(1);
  }

  if (argc == 3) {
    r_start = htons(atoi(argv[1]));
    r_end = htons(atoi(argv[2]));
    range = r_start & 0xffff;
    range <<= 16;
    range |= r_end & 0xffff;
    addr = (u_char *) range_set_clnt(&range, cl);
    if (addr == NULL) {
      clnt_perror(cl, "localhost");
      exit(1);
    }
    printf("set net range: %d - %d\n", ntohs(r_start), ntohs(r_end));
  }

  openetalkdb(NULL);

  printf("zone %s this %d.%d gw %d.%d nis %d.%d intf %s\n", 
	 this_zone, ntohs(this_net), this_node, ntohs(bridge_net),
	 bridge_node, ntohs(nis_net), nis_node, interface);
  printf("range start %d, range end %d\n", ntohs(net_range_start),
	ntohs(net_range_end));
}
