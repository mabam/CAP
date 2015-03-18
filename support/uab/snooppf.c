static char rcsid[] = "$Author: djh $ $Date: 1991/08/31 15:26:11 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/snooppf.c,v 2.1 1991/08/31 15:26:11 djh Rel djh $";
static char revision[] = "$Revision: 2.1 $";

/*
 * snooppf.c - Simple "protocol" level interface to SGI SNOOP packetfilter
 *
 *  Provides ability to read/write packets at ethernet level
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
 *  August 1991	David Hornsby	djh@munnari.OZ.AU
 *
*/

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <ctype.h>

#include <netat/appletalk.h>
#include "proto_intf.h"
#include <net/raw.h>

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int socket;			/* file descriptor of socket */
  struct ifreq ifr;
  int protocol;			/* ethernet protocol */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];

/*
 * setup for particular device devno
 * all pi_open's will go this device
*/
export
pi_setup()
{
  int i;

  if (!inited) {
    for (i = 0 ; i < MAXOPENPROT; i++)
      ephlist[i].inuse = FALSE;
    (void)init_fdlistening();
    inited = TRUE;		/* don't forget now */
  }
  return(TRUE);
}

/*
 * Open up a protocol handle:
 *   user level data:
 *      file descriptor
 *      protocol
 * 
 *   returns -1 and ephandle == NULL if memory allocation problems
 *   returns -1 for other errors
 *   return edx > 0 for okay
*/
export int
pi_open(protocol, dev, devno)
int protocol;
char *dev;
int devno;
{
  int s, i;
  struct ephandle *eph;
  char devnamebuf[100];		/* room for device name */

  for (i = 0; i < MAXOPENPROT; i++) {
    if (!ephlist[i].inuse)
      break;
  }
  if (i == MAXOPENPROT)
    return(0);			/* nothing */
  eph = &ephlist[i];		/* find handle */

  sprintf(devnamebuf, "%s%d",dev,devno);
  strncpy(eph->ifr.ifr_name, devnamebuf, sizeof eph->ifr.ifr_name);

  if ((s = init_nit(32768, protocol, &eph->ifr)) < 0) {
    return(-1);
  }

  eph->inuse = TRUE;
  eph->socket = s;
  eph->protocol = protocol;
  return(i+1);			/* skip zero */
}

/* returns TRUE if machine will see own broadcasts */
export int
pi_delivers_self_broadcasts()
{
  return(FALSE);
}

export int
pi_close(edx)
int edx;
{
  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);
  fdunlisten(ephlist[edx-1].socket); /* toss listener */
  close(ephlist[edx-1].socket);
  ephlist[edx-1].inuse = 0;
  return(0);
}

/*
 * Initialize nit on a particular protocol type
 * 
 * Return: socket if no error, < 0 o.w.
 *
*/
private int
init_nit(chunksize, protocol, ifr)
u_long chunksize;
u_short protocol;
struct ifreq *ifr;
{
  int s;
  int on = 1;
  struct sockaddr_raw sr;

  if ((s = socket(PF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) < 0) {
    perror("socket");
    return(-1);
  }

  /* bind to interface */
  strncpy(sr.sr_ifname, ifr->ifr_name, sizeof(sr.sr_ifname));
  sr.sr_family = AF_RAW;
  sr.sr_port = 0;
  bind(s, &sr, sizeof(sr));
  
  if (setup_pf(s, protocol) < 0)
    return(-1);

#ifndef NOBUF
  setup_buf(s, chunksize);
#endif  NOBUF
  
  /* start listening */
  ioctl(s, SIOCSNOOPING, &on);
  return(s);
}

#ifndef NOBUF
/*
 * setup buffering
 *
*/
setup_buf(s, cc)
int s;
u_long cc;
{
  if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &cc, sizeof(cc)) != 0) {
    perror("setsockopt");
    return(-1);
  }
  return(0);
}
#endif

/*
 * establish protocol filter
 *
*/
private int
setup_pf(s, prot)
int s;
u_short prot;
{
  struct snoopfilter sf;
  struct ether_header *eh;

  bzero((char *) &sf, sizeof(sf));
  eh = RAW_HDR(sf.sf_mask, struct ether_header);
  eh->ether_type = 0xffff;
  eh = RAW_HDR(sf.sf_match, struct ether_header);
  eh->ether_type = htons(prot);
  ioctl(s, SIOCADDSNOOP, &sf);
  return(0);
}

export int
pi_get_ethernet_address(edx,ea)
int edx;
u_char *ea;
{
  struct sockaddr *sa;
  struct ephandle *eph;

  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);
  
  eph = &ephlist[edx-1];
  if (ioctl(eph->socket, SIOCGIFADDR, (caddr_t)&eph->ifr) < 0) {
    perror("ioctl: SIOCGIFADDR");
    return(-1);
  }
  sa = (struct sockaddr *)&eph->ifr.ifr_data;
  bcopy(sa->sa_data, ea, 6);
  return(0);
}

export
pi_listener(edx, listener, arg)
int edx;
int (*listener)();
caddr_t arg;
{
  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);

  fdlistener(ephlist[edx-1].socket, listener, arg, edx);
}

private char ebuf[2000];	/* big enough */

/*
 * cheat - iov[0] == struct etherheader
 *
*/
export int
pi_readv(edx, iov, iovlen)
int edx;
struct iovec *iov;
int iovlen;
{
  struct ephandle *eph ;
  int len, off, cc, i;
  char *p;

  if (edx < 1 || edx > MAXOPENPROT)
    return(-1);
  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);
  if ((cc = read(eph->socket, ebuf, sizeof(ebuf))) < 0) {
    perror("abread");
    return(cc);
  }
  off = sizeof(struct snoopheader)+RAW_HDRPAD(sizeof(struct ether_header));
  for (len = 0, p = ebuf+off, i = 0 ; i < iovlen ; i++) {
    if (iov[i].iov_base && iov[i].iov_len >= 0) {
      bcopy(p, iov[i].iov_base, iov[i].iov_len);
      p += iov[i].iov_len;	/* advance */
      len += iov[i].iov_len;	/* advance */
    }
  }
  return(len);
}

export int
pi_read(edx, buf, bufsiz)
int edx;
caddr_t buf;
int bufsiz;
{
  struct iovec iov[2];
  struct ethernet_addresses ea;
  int cc;

  iov[0].iov_base = (caddr_t)&ea;
  iov[0].iov_len = sizeof(ea);
  iov[1].iov_base = (caddr_t)buf;
  iov[1].iov_len = bufsiz;
  cc = pi_readv(edx, iov, 2);
  return(cc - sizeof(ea));
}

export int
pi_write(edx, buf, buflen, eaddr)
int edx;
caddr_t buf;
int buflen;
char *eaddr;
{
  int cc;
  struct ephandle *eph;
  struct ether_header eh;
  struct sockaddr sa;
  struct iovec iov[2];

  iov[0].iov_base = (caddr_t)&eh;
  iov[0].iov_len = 14;
  iov[1].iov_base = (caddr_t)buf;
  iov[1].iov_len = buflen;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);

  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  bcopy(eaddr, eh.ether_dhost, 6);
  eh.ether_type = htons(eph->protocol);

  if ((cc = writev(eph->socket, iov, 2)) < 0) {
    perror("writev");
    return(-1);
  }
  return(buflen);
}

export int
pi_writev(edx, iov, iovlen, eaddr)
int edx;
struct iovec *iov;
int iovlen;
unsigned char eaddr[6];
{
  int i;
  char *p;
  int len;
  
  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);
  if (!ephlist[edx-1].inuse)
    return(-1);

  for (len = 0, p = ebuf, i = 0 ; i < iovlen ; i++)
    if (iov[i].iov_base && iov[i].iov_len >= 0) {
      bcopy(iov[i].iov_base, p, iov[i].iov_len);
      p += iov[i].iov_len;	/* advance */
      len += iov[i].iov_len;	/* advance */
    }
  return(pi_write(edx, ebuf, len, eaddr));
}
