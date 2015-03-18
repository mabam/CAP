static char rcsid[] = "$Author: djh $ $Date: 91/02/15 23:07:26 $";
static char rcsident[] = "$Header: dlip.c,v 2.1 91/02/15 23:07:26 djh Rel $";
static char revision[] = "$Revision: 2.1 $";

/*
 * dlip.c - Simple "protocol" level interface to DLI
 *
 *  Provides ability to read/write packets at ethernet level
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

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdnet/dli_var.h>

#include <netat/appletalk.h>
#include "proto_intf.h"

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int socket;			/* file descriptor of socket */
  int protocol;			/* ethernet protocol */
  struct sockaddr_dl sdli;	/* dli interface: to send with */
  struct sockaddr_dl rdli;	/* dli interface: to receive with */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];

/*
 * setup for particular device devno
 * all pi_open's will go this device
 *
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
}

/*
 * Open up a protocol handle:
 *   user level data:
 *      file descriptor
 *      protocol
 * 
 *   returns -1 and ephandle == NULL if memory allocation problems
 *   returns -1 for other errors
 *   return 0 for okay
*/
export int
pi_open(protocol, dev, devno)
int protocol;
char *dev;
int devno;
{
  struct ephandle *eph;
  struct sockaddr_dl *dl;
  int s;
  int i;

  for (i = 0; i < MAXOPENPROT; i++) {
    if (!ephlist[i].inuse)
      break;
  }
  if (i == MAXOPENPROT)
    return(0);			/* nothing */
  eph = &ephlist[i];		/* find handle */

  dl = &eph->sdli;		/* point */
  dl->dli_family = AF_DLI;
  strcpy(dl->dli_device.dli_devname, dev);
  dl->dli_device.dli_devnumber = devno;
  dl->dli_substructype = DLI_ETHERNET;
  /*  update these */
  dl->choose_addr.dli_eaddr.dli_ioctlflg = DLI_EXCLUSIVE;
  dl->choose_addr.dli_eaddr.dli_protype = protocol;

  if ((s = socket(AF_DLI, SOCK_DGRAM, DLPROTO_DLI)) < 0)
    return(-1);
  if (bind(s, dl, sizeof(struct sockaddr_dl)) < 0) {
    close(s);
    return(-1);
  }
  bcopy(dl, &eph->rdli, sizeof(struct sockaddr_dl));

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

export int
pi_get_ethernet_address(edx,ea)
int edx;
u_char *ea;
{
  struct ifdevea buf;
  struct ephandle *eph;

  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);

  eph = &ephlist[edx-1];		/* find handle */
  sprintf(buf.ifr_name, "%s%d",eph->sdli.dli_device.dli_devname,
	  eph->sdli.dli_device.dli_devnumber);
  if (ioctl(eph->socket,SIOCRPHYSADDR, &buf) < 0) {
    perror("iotcl");
    return(-1);
  }
  bcopy(buf.current_pa, ea, DLI_EADDRSIZE);
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
  return(0);
}


export int
pi_readv(edx, iov, iovlen)
int edx;
struct iovec *iov;
int iovlen;
{
  struct msghdr msg;
  int cc;
  struct ephandle *eph ;
  struct ethernet_addresses *ea;

  if (edx < 1 || edx > MAXOPENPROT)
    return(-1);
  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  msg.msg_iov = iov+1;
  msg.msg_iovlen = iovlen-1;
  msg.msg_name = (caddr_t)&eph->rdli;
  msg.msg_namelen = sizeof(eph->rdli);
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
  if ((cc = recvmsg(eph->socket, &msg, 0)) < 0) {
    perror("recvmsg");
    return(cc);
  }
  ea = (struct ethernet_addresses *)iov[0].iov_base;
  ea->etype = eph->protocol;
  /* check length -- naw */
  bcopy(eph->rdli.choose_addr.dli_eaddr.dli_target, ea->saddr, EHRD);
  bcopy(eph->rdli.choose_addr.dli_eaddr.dli_dest, ea->daddr, EHRD);
  return(cc+iov[0].iov_len);
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
  struct iovec iov[1];

  iov[0].iov_base = buf;
  iov[0].iov_len = buflen;
  return(pi_writev(edx, iov, 1, eaddr));
}

export int
pi_writev(edx, iov, iovlen, eaddr)
int edx;
struct iovec *iov;
int iovlen;
char *eaddr;
{
  struct ephandle *eph;
  struct msghdr msg;
  int cc;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);
  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  bcopy(eaddr, eph->sdli.choose_addr.dli_eaddr.dli_target, DLI_EADDRSIZE);
  msg.msg_name = (caddr_t)&eph->sdli;
  msg.msg_namelen = sizeof(eph->sdli);
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
  msg.msg_iov = iov;
  msg.msg_iovlen = iovlen;
  cc = sendmsg(eph->socket, &msg, 0);
  return(cc);
}
