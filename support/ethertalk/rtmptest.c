/*
 * simple RPC RTMP test
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

main(argc, argv)
int argc;
char **argv;
{
  CLIENT *cl;
  u_char *addr;
  AddrBlock baddr;
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
    baddr.net = htons(atoi(argv[1]));
    baddr.node = atoi(argv[2]);
    addr = rtmp_setbaddr_clnt(&baddr, cl);
    if (addr == NULL) {
      clnt_perror(cl, "localhost");
      exit(1);
    }
    printf("set bridge addr %x.%x %x %x\n", addr[0], addr[1], addr[2], addr[3]);
    addr = rtmp_getbaddr_clnt(&baddr, cl);
    if (addr == NULL) {
      clnt_perror(cl, "localhost");
      exit(1);
    }
    printf("get bridge addr %x.%x %x %x\n", addr[0], addr[1], addr[2], addr[3]);
  }

  openetalkdb(NULL);

  printf("zone %s this %d.%d gw %d.%d nis %d.%d intf %s\n", 
	 this_zone, ntohs(this_net), this_node, ntohs(bridge_net),
	 bridge_node, ntohs(nis_net), nis_node, interface);
}
