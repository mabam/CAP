static char rcsid[] = "$Author: djh $ $Date: 1992/07/30 09:41:26 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/sllap.c,v 2.1 1992/07/30 09:41:26 djh Rel djh $";
static char revision[] = "$Revision: 2.1 $";

/*
 * sllap.c - Simple "protocol" level interface to HP Link Level Access
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
 *  October 1991	djh@munnari.OZ.AU  created
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
#include <sys/utsname.h>
#include <stdlib.h>

#include <netat/appletalk.h>
#include "proto_intf.h"
#include <netio.h>

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int socket;			/* file descriptor of socket */
  struct ifreq ifr;
  int protocol;			/* ethernet protocol */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];

/*
 * simulate gethostid()
 *
 */

export int
gethostid()
{
  struct utsname xx;

  uname(&xx);
  return(atoi(xx.idnumber));
}

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

  sprintf(devnamebuf, "/dev/%s%d", dev, devno);
  strncpy(eph->ifr.ifr_name, devnamebuf, sizeof eph->ifr.ifr_name);

  if ((s = init_nit(16, protocol, &eph->ifr)) < 0)
    return(-1);

  eph->inuse = TRUE;
  eph->socket = s;
  eph->protocol = protocol;
  return(i+1);			/* skip zero */
}

/* returns TRUE if machine will see own broadcasts */
export int
pi_delivers_self_broadcasts()
{
  return(TRUE);
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
 * Initialize LLA on a particular protocol type
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
  u_long if_flags;
  char device[64];

  if ((s = open(ifr->ifr_name, O_RDWR)) < 0) {
    sprintf(device, "open: %s", ifr->ifr_name);
    perror(device);
    return(-1);
  }
  
  if (setup_pf(s, protocol) < 0)
    return(-1);

  setup_buf(s, chunksize);
  
  return(s);
}

/*
 * setup input packet queue length
 *
 */

setup_buf(s, chunksize)
int s;
u_long chunksize;
{
	struct fis fis;

	fis.reqtype = LOG_READ_CACHE;
	fis.vtype = INTEGERTYPE;
	fis.value.i = chunksize;

	ioctl(s, NETCTRL, &fis);
	/* ignore error */
}

/*
 * establish protocol filter
 *
 */

private int
setup_pf(s, prot)
int s;
u_short prot;
{
  struct fis fis;

  fis.reqtype = LOG_TYPE_FIELD;
  fis.vtype = INTEGERTYPE;
  fis.value.i = prot; /* htons() ?? */

  if (ioctl(s, NETCTRL, &fis) < 0)
    return (-1);

  return(0);
}

export int
pi_get_ethernet_address(edx,ea)
int edx;
u_char *ea;
{
  struct ephandle *eph;
  struct fis fis;

  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);
  eph = &ephlist[edx-1];

  fis.reqtype = LOCAL_ADDRESS;
  if (ioctl(eph->socket, NETSTAT, &fis) < 0) {
    perror("Ioctl: LOCAL_ADDRESS");
    return(-1);
  }
  bcopy(fis.value.s, ea, 6);
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
  struct fis fis;
  int len, cc, i;
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

  fis.reqtype = FRAME_HEADER;
  if (ioctl(eph->socket, NETSTAT, &fis) < 0)
    return(-1);

  bcopy(fis.value.s, iov[0].iov_base, (len = iov[0].iov_len));

  for (p = ebuf, i = 1 ; i < iovlen ; i++) {
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
  int cc;
  struct ephandle *eph ;

  if (edx < 1 || edx > MAXOPENPROT)
    return(-1);
  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  cc = read(eph->socket, buf, bufsiz);

  return(cc);
}

export int
pi_write(edx, buf, buflen, eaddr)
int edx;
caddr_t buf;
int buflen;
char *eaddr;
{
  struct ephandle *eph;
  struct fis fis;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);

  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  /* urk */
  fis.reqtype = LOG_DEST_ADDR;
  fis.vtype = 6;
  bcopy(eaddr, fis.value.s, 6);
  if (ioctl(eph->socket, NETCTRL, &fis) < 0)
    return(-1);

  if (write(eph->socket, buf, buflen) < 0)
    return(-1);

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
