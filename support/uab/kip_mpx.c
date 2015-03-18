static char rcsid[] = "$Author: djh $ $Date: 1995/05/29 10:45:50 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/kip_mpx.c,v 2.6 1995/05/29 10:45:50 djh Rel djh $";
static char revision[] = "$Revision: 2.6 $";

/*
 * kip_mpx.c - talks to cap processes via udp
 *
 *  demultiplexing communications point with various processes.
 *  Two versions in here: one version speaks directly to KIP module
 *  CAP clients (KIP).  The other speaks on a modified range (MKIP)
 *  
 *  KIP writes etalk.local and cannot be used when the rebroadcaster
 *    is in use.  NOTE: THIS DOES NOT (WILL NEVER) WORK CORRECTLY.
 *  MKIP writes etalk.local for use by CAP programs.
 *
 *  This version really cheats and uses a different range of ports,
 *  but the same encapsulation used for kip.
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
 * Edit History:
 *
 *  April 3, 1988  CCKim Created
 *  November, 1990 djh@munnari.OZ.AU formalize etalk.local, check portrange
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
#include <netdb.h>
#include "mpxddp.h"
#include "gw.h"
#include "log.h"

#define LOG_STD 2

/*
  ethertalk to kip - try 1:
  o forward packets by sending to ddp udp port
  o receive packets by having the remote send to "bridge" socket
  o much like kip methods except different socket range and fewer
    conditions in lower level code
*/

#ifndef TAB
# define TAB "/etc/etalk.local"
#endif
#ifndef MTAB
# define MTAB "/etc/etalk.local"
#endif

/* handle kip fakeout */
#define mkip_rebPort	903
#define kip_rebPort	902

struct kipdef {
 int s;				/* socket */
 word net;
 byte node;
 byte bridge_node;
 struct sockaddr_in sin;
};

extern word	this_net,	bridge_net,	nis_net,	async_net;
extern byte	this_node,	bridge_node,	nis_node;
extern char	this_zone[34],	async_zone[34];
extern struct	in_addr bridge_addr;

#define MKIP_HANDLE 0
#define KIP_HANDLE 1
#define NUM_KIP_HANDLE 2

#define VALID_HANDLE(h) ((h) >= 0 && (h) < NUM_KIP_HANDLE)
#define INVALID_HANDLE(h) ((h) >= NUM_KIP_HANDLE || (h) < 0)
private struct kipdef kipdefs[NUM_KIP_HANDLE];

#define KIP_PRIMARY 0		/* primary listening */
#define KIP_SECONDARY 1		/* secondary: local to process */

/* destination */
private struct in_addr desthost;

private DDP kddp;
private LAP klap;
private char kbuf[ddpMaxData+ddpSize]; /* add in space for lap */

private word portrange;		/* old or new ... */

private int kip_init();
private int mkip_init();
private int kip_grab();
private int kips_ahoy();
private int kip_sendddp();
private int kip_havenode();
private int kip_havenet();
private int kip_havezone();

export struct mpxddp_module mkip_mpx = {
  "modified KIP forwarding",
  "MKIP",
  mkip_init,
  NULL,				/* grab, don't do for now (ddpsrvc not done) */
  kip_sendddp,
  kip_havenode,
  kip_havenet,
  kip_havezone
};

export struct mpxddp_module kip_mpx = {
  "KIP forwarding",
  "KIP",
#ifdef notdef
  /* doesn't work right because of the way kip routing is done, sigh */
  kip_init,
  NULL,				/* grab, don't do for now (ddpsrvc not done) */
  kip_sendddp,
  kip_havenode,
  kip_havenet,
  kip_havezone
#else
  NULL,				/* init */
  NULL,				/* grab, don't do for now (ddpsrvc not done) */
  NULL,				/* senddp */
  NULL,				/* havenode */
  NULL,				/* have net */
  NULL				/* have zone */
#endif
};

static int name_toipaddr();

private
kip_listener(fd, k, level)
int fd;
struct kipdef *k;
int level;
{
  struct msghdr msg;
  struct iovec iov[3];
  int len;
  struct sockaddr_in from_sin;
  
  /* should check k */
  iov[0].iov_base = (caddr_t)&klap;
  iov[0].iov_len = lapSize;
  iov[1].iov_base = (caddr_t)&kddp;
  iov[1].iov_len = ddpSize;
  iov[2].iov_base = kbuf;
  iov[2].iov_len = ddpMaxData;
  msg.msg_name = (caddr_t) &from_sin;
  msg.msg_namelen = sizeof(from_sin);
  msg.msg_iov = iov;
  msg.msg_iovlen = 3;
#if (defined(__386BSD__) || defined(__FreeBSD__))
  msg.msg_control = 0;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
#else  /* __386BSD__ */
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
#endif /* __386BSD__ */
  if ((len = recvmsg(fd,&msg,0)) < 0) {
    logit(LOG_LOG|L_UERR, "recvmsg: kip listener");
    return(len);
  }
  /* could do rebroadcaster work here */
  if (len < (lapSize+ddpSize))
    return(-1);
  len -= (lapSize + ddpSize);
  if (klap.type != lapDDP)
    return(-1);
  switch (level) {
  case KIP_PRIMARY:
    if (klap.src != k->node)	/* drop!  must have wrong configuration */
      return(-1);
    if (kddp.srcNode != k->node || (kddp.srcNet != k->net && kddp.srcNet != 0))
      return(-1);
    break;
  case KIP_SECONDARY:
    /* always accept? */
    break;
  }
  ddp_output(NULL, &kddp, (byte *)kbuf, len);
  return(0);
}

/*
 * initialize kip forwarding
 *
*/
private int
mkip_init()
{
  static int inited = FALSE;
  int i;

  if (inited)
    return(-1);
  if ((i = kips_ahoy(MKIP_HANDLE)) >= 0)
    inited = TRUE;
  return(i);
}

private int
kip_init()
{
  static int inited = FALSE;
  int i;

  if (inited)
    return(-1);
  if ((i = kips_ahoy(KIP_HANDLE)) >= 0)
    inited = TRUE;
  return(i);
}

private int
kips_ahoy(h)
{
  int cc;
  struct kipdef *k;
  word getPRange();

  if (h != MKIP_HANDLE && h != KIP_HANDLE)
    return(-1);
  k = &kipdefs[h];
  k->net = 0;
  k->node = 0;
  k->bridge_node = 0;
  portrange = getPRange();
  desthost.s_addr = inet_addr("127.0.0.1");
  if (desthost.s_addr == -1)
    return(-1);
  /* no need to bind since we don't recv on this socket, just send... */
  if ((k->s = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    logit(LOG_LOG|L_UERR, "socket: kip init");
    return(-1);
  }
  { long len;
    len = 6 * 1024; /* should be enough ?? */
    if(setsockopt(k->s,SOL_SOCKET,SO_RCVBUF,(char *)&len,sizeof(long))!=0){
      fprintf(stderr, "Couldn't set socket options\n");
    }
  }


  k->sin.sin_family = AF_INET;
  switch (h) {
  case MKIP_HANDLE:
    k->sin.sin_addr.s_addr = desthost.s_addr;
    k->sin.sin_port = htons(mkip_rebPort);
    break;
  case KIP_HANDLE:
    k->sin.sin_addr.s_addr = INADDR_ANY;
    k->sin.sin_port = htons(kip_rebPort);
    break;
  }
  if ((cc = bind(k->s, (caddr_t)&k->sin, sizeof(k->sin))) < 0) {
    close(k->s);
    logit(LOG_LOG|L_UERR, "bind: kip init");
    return(cc);
  }
  fdlistener(k->s, kip_listener, k, KIP_PRIMARY);
  return(h);
}

private int
kip_grab(hdl, skt)
int hdl;
int skt;
{
  int fd, e;
  struct sockaddr_in lsin;
  word mddp2ipskt();
  word ddp2ipskt();

  if (INVALID_HANDLE(hdl))
    return(-1);
  
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    logit(LOG_STD|L_UERR, "kip grab: socket for skt %d", skt);
    return(fd);
  }
  { long len;
    len = 6 * 1024; /* should be enough ?? */
    if(setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(char *)&len,sizeof(long))!=0){
      fprintf(stderr, "Couldn't set socket options\n");
    }
  }
  lsin.sin_family = AF_INET;
  /* bind only for localhost if mkip */
  switch (hdl) {
  case MKIP_HANDLE:
    lsin.sin_addr.s_addr = desthost.s_addr;
    lsin.sin_port = htons(mddp2ipskt(skt));
    break;
  case KIP_HANDLE:
    lsin.sin_addr.s_addr = INADDR_ANY;
    lsin.sin_port = htons(ddp2ipskt(skt));
    break;
  }
  if (lsin.sin_port == 0) {	/* same swapped or unswapped */
    close(fd);			/* bad ddp socket */
    return(-1);
  }
  if ((e = bind(fd, (caddr_t)&lsin, sizeof(lsin))) < 0) {
    logit(LOG_STD|L_UERR, "kip grab: bind for skt %d", skt);
    return(fd);
  }
  fdlistener(kipdefs[hdl].s, kip_listener, KIP_SECONDARY);
  return(fd);
}

/*
 * send along a ddp packet to the appropriate process
 *
*/
private int
kip_sendddp(hdl, ddp, data, cc)
int hdl;
DDP *ddp;
caddr_t data;
int cc;
{
  struct msghdr msg;
  word destskt;
  int err;
  struct iovec iov[4];
  LAP lap;
  int t;
  struct kipdef *k;
  word mddp2ipskt();
  word ddp2ipskt();

  if (hdl != MKIP_HANDLE && hdl != KIP_HANDLE)
    return(-1);
  k = &kipdefs[hdl];

  /* I THINK this is the right place for this -- this way, the demuxer */
  /* could possible handle multiple networks */
  if (ddp->dstNet != k->net && ddp->dstNode != k->node) /* drop */
    return(0);

  if (hdl == KIP_HANDLE)
    destskt = htons(ddp2ipskt(ddp->dstSkt)+128);
  else
    destskt = htons(mddp2ipskt(ddp->dstSkt));
  /* establish a dummy lap header */
  lap.type = lapDDP;
  lap.dst = k->node;
  lap.src = ddp->srcNode;
  iov[IOV_LAP_LVL].iov_base = (caddr_t) &lap; /* LAP header */
  iov[IOV_LAP_LVL].iov_len = lapSize; 	  /* size  */
  iov[IOV_DDP_LVL].iov_base = (caddr_t) ddp;
  iov[IOV_DDP_LVL].iov_len = ddpSize;
  /* figure out what the data length should be */
  t = (ntohs(ddp->length)&ddpLengthMask) - ddpSize;
  /* if data size passed down is too large, then truncate.  (sunos */
  /* trailing bytes or packet less than 60 bytes */
  /* so, pass back min of cc, length */
  iov[2].iov_len = (cc > t) ? t : cc;
  iov[2].iov_base = data;
  /* send through */
  k->sin.sin_addr = desthost;
  k->sin.sin_port = destskt;
  msg.msg_name = (caddr_t) &k->sin;
  msg.msg_namelen = sizeof(k->sin);
  msg.msg_iov = iov;
  msg.msg_iovlen = 3;
#if (defined(__386BSD__) || defined(__FreeBSD__))
  msg.msg_control = 0;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
#else  /* __386BSD__ */
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
#endif /* __386BSD__ */
  if ((err = sendmsg(k->s,&msg,0)) < 0)
    logit(LOG_LOG|L_UERR, "sendmsg: kip write");
  return(err);  
}

/*
 * have node now  -- just remember
 *
*/
private int
kip_havenode(hdl, node)
int hdl;
byte node;
{
  if (hdl != MKIP_HANDLE && hdl != KIP_HANDLE)
    return(FALSE);
  kipdefs[hdl].node = node;
  return(TRUE);
}

/*
 * have network now
 *
*/
private int
kip_havenet(hdl, net, node)
word net;
byte node;
{
  if (hdl != MKIP_HANDLE && hdl != KIP_HANDLE)
    return(FALSE);
  kipdefs[hdl].net = net;
  kipdefs[hdl].bridge_node = node;
  return(TRUE);
}

/*
 * have zone: ready to go
 *
*/
private int
kip_havezone(hdl, zone)
int hdl;
byte *zone;
{
  FILE *fp;
  int i;
  struct kipdef *k;
  char *file;
  word save_async_net;
  char save_async_zone[34];

  if (hdl != MKIP_HANDLE && hdl != KIP_HANDLE)
    return(FALSE);
  k = &kipdefs[hdl];
  file = (hdl == MKIP_HANDLE) ? MTAB : TAB;

  save_async_net = async_net;
  strncpy(save_async_zone, async_zone, sizeof(save_async_zone));

  /* set some defaults */
  openetalkdb(file);
  this_net = k->net;
  this_node = k->node;
  /* zone is pascal string, this_zone is 'C' */
  cpyp2cstr(this_zone, zone);
  /* 
   * if same net, leave node & addr unchanged
   *  - we could be using other bridge
   */
  if (bridge_net != k->net) {
    bridge_net = k->net;
    bridge_node = k->bridge_node;
    name_toipaddr("127.0.0.1", &bridge_addr);
  }
  /*
   * if same net, leave node unchanged, atis running elsewhere
   */
  if (nis_net != k->net) {
    nis_net = k->net;
    nis_node = k->node;
  }
  /*
   * if async_net wasn't zero, must be using UAB internal, keep it.
   */
  if (save_async_net != 0) {
    async_net = save_async_net;
    strncpy(async_zone, save_async_zone, sizeof(async_zone));
  }

  etalkdbupdate(file);	/* write out the new stuff */

  return(TRUE);
}

static int
name_toipaddr(name, ipaddr)
char *name;
struct in_addr *ipaddr;
{
  struct hostent *host;

  if (isdigit(name[0])) {
    if ((ipaddr->s_addr = inet_addr(name)) == -1)
      return(-1);
    return(0);
  }
  if ((host = gethostbyname(name)) == 0)
    return(-1);
  bcopy(host->h_addr, (caddr_t)&ipaddr->s_addr, sizeof(ipaddr->s_addr));
}

/* don't hardwire the port ranges, not nice */

private word
mddp2ipskt(skt)
word skt;
{
	return((skt&0x80) ? (skt+ddpNWKSUnix) : (skt+portrange));
}

/* fix this when (if) normal KIP works */

private word
ddp2ipskt(skt)
word skt;
{
	return((skt&0x80) ? (skt+ddpNWKSUnix) : (skt+portrange));
}

private word
getPRange()
{
	struct servent *getservbyname();

	if(getservbyname("at-rtmp", "udp") == NULL)
		return(ddpWKSUnix);	/* 768 */
	else
		return(ddpOWKSUnix);	/* 200 */
}
