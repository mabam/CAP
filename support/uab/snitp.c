static char rcsid[] = "$Author: djh $ $Date: 1991/07/10 12:44:03 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/snitp.c,v 2.3 1991/07/10 12:44:03 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * snitp.c - Simple "protocol" level interface to Streams based NIT
 * (SunOS 4.0)
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
 *  July 1988  CCKim Created
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
#include <net/if.h>
#include <net/nit.h>
#include <net/nit_if.h>
#include <net/nit_pf.h>
#include <net/nit_buf.h>
#include <net/packetfilt.h>
#include <stropts.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <ctype.h>

#ifndef DEV_NIT
#define DEV_NIT	"/dev/nit"
#endif  DEV_NIT

#include <netat/appletalk.h>
#include "proto_intf.h"

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
  struct ephandle *eph;
  char devnamebuf[100];		/* room for device name */
  int s;
  int i;

  for (i = 0; i < MAXOPENPROT; i++) {
    if (!ephlist[i].inuse)
      break;
  }
  if (i == MAXOPENPROT)
    return(0);			/* nothing */
  eph = &ephlist[i];		/* find handle */

  sprintf(devnamebuf, "%s%d",dev,devno);
  strncpy(eph->ifr.ifr_name, devnamebuf, sizeof eph->ifr.ifr_name);
  eph->ifr.ifr_name[sizeof eph->ifr.ifr_name - 1] = ' ';

  if ((s = init_nit(1024, protocol, &eph->ifr)) < 0) {
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
*/
private int
init_nit(chunksize, protocol, ifr)
u_long chunksize;
u_short protocol;
struct ifreq *ifr;
{
  u_long if_flags;
  struct strioctl si;
  int s;
  
  /* get clone */
  if ((s = open(DEV_NIT, O_RDWR)) < 0) {
    perror(DEV_NIT);
    return(-1);
  }
  
  /* set up messages */
  if (ioctl(s, I_SRDOPT, (char *)RMSGD) < 0) { /* want messages */
    perror("ioctl: discretem");
    return(-1);
  }
  
  si.ic_timout = INFTIM;
  
  if (setup_pf(s, protocol) < 0)
    return(-1);
#define NOBUF
#ifndef NOBUF
  setup_buf(s, chunksize);
#endif  
  /* bind */
  si.ic_cmd = NIOCBIND;		/* bind */
  si.ic_timout = 10;
  si.ic_len = sizeof(*ifr);
  si.ic_dp = (caddr_t)ifr;
  if (ioctl(s, I_STR, (caddr_t)&si) < 0) {
    perror(ifr->ifr_name);
    return(-1);
  }
  
  /* flush read queue */
  ioctl(s, I_FLUSH, (char *)FLUSHR);
  return(s);
}

#ifndef NOBUF
/*
 * setup buffering (not wanted)
 *
*/
setup_buf(s, chunksize)
int s;
u_long chunksize;
{
  struct strioctl si;
  struct timeval timeout;

  /* Push and configure the buffering module. */
  if (ioctl(s, I_PUSH, "nbuf") < 0) {
    perror("ioctl: nbuf");
  }
  timeout.tv_sec = 0;
  timeout.tv_usec = 200;
  si.ic_cmd = NIOCSTIME;
  si.ic_timout = 10;
  si.ic_len = sizeof timeout;
  si.ic_dp = (char *)&timeout;
  if (ioctl(s, I_STR, (char *)&si) < 0) {
    perror("ioctl: timeout");
    return(-1);
  }
  
  si.ic_cmd = NIOCSCHUNK;
  
  si.ic_len = sizeof chunksize;
  si.ic_dp = (char *)&chunksize;
  if (ioctl(s, I_STR, (char *)&si)) {
    perror("ioctl: chunk size");
    return(-1);
  }
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
  u_short offset;
  struct ether_header eh;
  struct packetfilt pf;
  register u_short *fwp = pf.Pf_Filter;
  struct strioctl si;
#define s_offset(structp, element) (&(((structp)0)->element))
  offset = ((int)s_offset(struct ether_header *, ether_type))/sizeof(u_short);
  *fwp++ = ENF_PUSHWORD + offset;
  *fwp++ = ENF_PUSHLIT | ENF_EQ;
  *fwp++ = htons(prot);
  pf.Pf_FilterLen = 3;

  si.ic_cmd = NIOCSETF;
  si.ic_timout = 10;
  si.ic_len = sizeof(pf);
  si.ic_dp = (char *)&pf;
  if (ioctl(s, I_PUSH, "pf") < 0) {
    perror("ioctl: push protocol filter");
    return(-1);
  }
  if (ioctl(s, I_STR, (char *)&si) < 0) {
    perror("ioctl: protocol filter");
    return(-1);
  }
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
  if (ioctl(eph->socket, SIOCGIFADDR, &eph->ifr) < 0) {
    perror("ioctl: SIOCGIFADDR");
    return(-1);
  }
  sa = (struct sockaddr *)eph->ifr.ifr_data;
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
  int cc;

  if (edx < 1 || edx > MAXOPENPROT)
    return(-1);
  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);
  if ((cc = readv(eph->socket, iov, iovlen)) < 0) {
    perror("abread");
    return(cc);
  }
  return(cc);
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
  struct ephandle *eph;
  struct ether_header *eh;
  struct strbuf pbuf, dbuf;
  struct sockaddr sa;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);

  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  sa.sa_family = AF_UNSPEC;	/* by def. */
  eh = (struct ether_header *)sa.sa_data;		/* make pointer */
  bcopy(eaddr, &eh->ether_dhost, sizeof(eh->ether_dhost));
  eh->ether_type = htons(eph->protocol);
  pbuf.len = sizeof(sa);
  pbuf.buf = (char *)&sa;
  dbuf.len = buflen;
  dbuf.buf = (caddr_t)buf;

  if (putmsg(eph->socket, &pbuf, &dbuf, 0) < 0) {
    return(-1);
  }
  return(buflen);
}

private char ebuf[2000];	/* big enough */

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
