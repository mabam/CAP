static char rcsid[] = "$Author: djh $ $Date: 1996/04/25 00:49:30 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/ethertalk/RCS/spfiltp.c,v 2.6 1996/04/25 00:49:30 djh Rel djh $";
static char revision[] = "$Revision: 2.6 $";

/*
 * spfiltp.c - Simple "protocol" level interface to Ultrix packetfilter
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
 *  April 1991	Jeffrey Mogul @ DECWRL (created from senetp.c)
 *  June  1991  David Hornsby, add Phase 2 support
 *  June  1995  Conrad Huang, add FDDI support
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
#ifdef pyr
#include <net/pfilt.h>
extern int errno;
#else  pyr
#include <net/pfilt.h>
#endif pyr
#include <netinet/in.h>
#ifdef pyr
#include <net/if_ether.h>
#else  pyr
#include <netinet/if_ether.h>
#endif pyr
#ifdef USING_FDDI_NET
#include <netinet/if_fddi.h>
#include <net/bpf.h>
#endif USING_FDDI_NET
#include <netdb.h>
#include <ctype.h>

#include <netat/appletalk.h>
#include "../uab/proto_intf.h"

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int fd;			/* file descriptor of socket */
  struct ifreq ifr;
  int protocol;			/* ethernet protocol */
  int socket;			/* ddp socket */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];

private int header_size = 14;	/* assume Ethernet for now */

#ifdef USING_FDDI_NET
export int source_offset = 6;	/* assume Ethernet for now */
export int using_fddi = 0;	/* assume not FDDI for now */
#endif USING_FDDI_NET

extern char interface[50];

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
pi_open(protocol, socket, dev, devno)
int protocol;
int socket;
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

  if ((s = init_nit(1024, protocol, socket, &eph->ifr)) < 0) {
    return(-1);
  }

  eph->inuse = TRUE;
  eph->fd = s;
  eph->protocol = protocol;
  eph->socket = socket;
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
  fdunlisten(ephlist[edx-1].fd); /* toss listener */
  close(ephlist[edx-1].fd);
  ephlist[edx-1].inuse = 0;
  return(0);
}

/*
 * Initialize nit on a particular protocol type
 * 
 * Runs in promiscous mode for now.
 *
 * Return: socket if no error, < 0 o.w.
*/
private int
init_nit(chunksize, protocol, socket, ifr)
u_long chunksize;
u_short protocol;
int socket;
struct ifreq *ifr;
{
  u_long if_flags;
  int s;
  char device[64];

  if ((s = pfopen(interface, O_RDWR)) < 0) {
    sprintf(device, "open: %s", interface);
    perror(device);
    return(-1);
  }

  if (setup_pf(s, protocol, socket) < 0)
    return(-1);

#define NOBUF
#ifndef NOBUF
  setup_buf(s, chunksize);
#endif  NOBUF
  
  /* flush read queue */
  ioctl(s, EIOCFLUSH, 0);
  return(s);
}

#ifdef PHASE2
/*
 * add a multicast address to the interface
*/
int
pi_addmulti(multi, ifr)
char *multi;
struct ifreq *ifr;
{
  int sock;

  if ((sock = pfopen(interface, O_RDWR)) < 0) {
    perror("pfopen()");
    return(-1);
  }
  /*
   * get the real interface name (interface may be generic "pf0")
   *
   */
  if (ioctl(sock, EIOCIFNAME, ifr) < 0) {
    perror("EIOCIFNAME");
    return(-1);
  }
  close(sock);

  ifr->ifr_addr.sa_family = AF_UNSPEC;
  bcopy(multi, ifr->ifr_addr.sa_data, EHRD);
  /*
   * open a socket, temporarily, to use for SIOC* ioctls
   *
   */
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return(-1);
  }

  if (ioctl(sock, SIOCADDMULTI, (struct ifreq *)ifr) < 0) {
      perror("SIOCADDMULTI");
      close(sock);
      return(-1);
  }

  close(sock);
  return(0);
}
#endif PHASE2

#ifndef NOBUF
/*
 * setup buffering (not wanted)
 *
*/
setup_buf(s, chunksize)
int s;
u_long chunksize;
{
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
#endif NOBUF

/*
 * establish protocol filter
 *
*/
private int
setup_pf(s, prot, sock)
int s;
u_short prot;
int sock;
{
  u_short offset;
  unsigned queuelen;
  u_int dlt;
  struct enfilter pf;
  register u_short *fwp = pf.enf_Filter;
  extern int errno;


  queuelen = 8;
  if (ioctl(s, EIOCSETW, &queuelen) < 0) {
    perror("ioctl: set recv queue length");
    return(-1);
  }

#ifdef USING_FDDI_NET
  if (ioctl(s, BIOCGDLT, &dlt) < 0) {
    perror("ioctl: get data link type");
    return(-1);
  }
  if (dlt == DLT_FDDI) {
    header_size = 16;
    source_offset = 10;
    using_fddi = 1;
  }
#endif USING_FDDI_NET

  pf.enf_Priority = 128;

#ifdef USING_FDDI_NET
  if (using_fddi)
    offset = (header_size - 2) / sizeof (ushort);
  else
#endif USING_FDDI_NET
#define s_offset(structp, element) (&(((structp)0)->element))
  offset = ((int)s_offset(struct ether_header *, ether_type))/sizeof(u_short);
#ifdef PHASE2
#ifdef ULT42PFBUG
  if (sock >= 0)
#endif ULT42PFBUG
  offset += 4;	/* shorts: 2 bytes length + 6 bytes of 802.2 and SNAP */
#endif PHASE2
  *fwp++ = ENF_PUSHWORD + offset;
  *fwp++ = ENF_PUSHLIT | (sock >= 0 ? ENF_CAND : ENF_EQ);
  *fwp++ = htons(prot);
  pf.enf_FilterLen = 3;
  if (sock >= 0) {
#ifdef PHASE2
    *fwp++ = ENF_PUSHWORD + offset + 6;
    *fwp++ = ENF_PUSHLIT | ENF_AND;
    *fwp++ = htons(0xff00);	/* now have dest socket */
    *fwp++ = ENF_PUSHLIT | ENF_COR;
    *fwp++ = htons((sock & 0xff) << 8);
/* if not wanted, fail it */
    *fwp++ = ENF_PUSHLIT;
    *fwp++ = 0;
    pf.enf_FilterLen += 7;
#else  PHASE2
/* short form */
    *fwp++ = ENF_PUSHWORD + offset + 2;
    *fwp++ = ENF_PUSHLIT | ENF_AND;
    *fwp++ = htons(0xff00);   /* now have lap type in LH */
    *fwp++ = ENF_PUSHWORD + offset + 3;
    *fwp++ = ENF_PUSHLIT | ENF_AND;
    *fwp++ = htons(0x00ff);   /* now have dest in RH */
    *fwp++ = ENF_NOPUSH | ENF_OR;  /* now have lap,,dest */
    *fwp++ = ENF_PUSHLIT | ENF_COR;
    *fwp++ = htons((1 << 8) | (sock & 0xff));
/* long form */
    *fwp++ = ENF_PUSHWORD + offset + 2;
    *fwp++ = ENF_PUSHLIT | ENF_AND;
    *fwp++ = htons(0xff00);   /* now have lap type in LH */
    *fwp++ = ENF_PUSHWORD + offset + 7;
    *fwp++ = ENF_PUSHLIT | ENF_AND;
    *fwp++ = htons(0x00ff);   /* now have dest in RH */
    *fwp++ = ENF_NOPUSH | ENF_OR;  /* now have lap,,dest */
    *fwp++ = ENF_PUSHLIT | ENF_COR;
    *fwp++ = htons((2 << 8) | (sock & 0xff));
/* if neither, fail it */
    *fwp++ = ENF_PUSHLIT ;
    *fwp++ = 0;
    pf.enf_FilterLen += 20;
#endif PHASE2
  }    

  if (ioctl(s, EIOCSETF, &pf) < 0) {
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
  struct endevp endev;
  struct ephandle *eph;

  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);
  
  eph = &ephlist[edx-1];
  if (ioctl(eph->fd, EIOCDEVP, &endev) < 0) {
    perror("Ioctl: SIOCGIFADDR");
    return(-1);
  }
  bcopy(endev.end_addr, ea, 6);
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

  fdlistener(ephlist[edx-1].fd, listener, arg, edx);
}

export
pi_listener_2(edx, listener, arg1, arg2)
int edx;
int (*listener)();
caddr_t arg1;
int arg2;
{
  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);

  fdlistener(ephlist[edx-1].fd, listener, arg1, arg2);
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
  if ((cc = readv(eph->fd, iov, iovlen)) < 0) {
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
  struct iovec iov[3];
  struct ethernet_addresses ea;
#ifdef USING_FDDI_NET
  struct fddi_header fh;
#endif USING_FDDI_NET
#ifdef PHASE2
  char header[8];
#endif PHASE2
  int cc;

#ifdef PHASE2
#ifdef USING_FDDI_NET
  if (using_fddi)
    iov[0].iov_base = (caddr_t)&fh;
  else
#endif USING_FDDI_NET
  iov[0].iov_base = (caddr_t)&ea;
  iov[0].iov_len = header_size;
  iov[1].iov_base = (caddr_t)header;	/* consume 802.2 hdr & SNAP */
#ifdef ULT42PFBUG
  iov[1].iov_len = 0;
#else  ULT42PFBUG
  iov[1].iov_len = sizeof(header);
#endif ULT42PFBUG
  iov[2].iov_base = (caddr_t)buf;
  iov[2].iov_len = bufsiz;
  cc = pi_readv(edx, iov, 3);
#ifdef USING_FDDI_NET
  if (using_fddi)
    return(cc - header_size - sizeof (header));
  else
#endif USING_FDDI_NET
#ifdef ULT42PFBUG
  return(cc - sizeof(ea));
#else  ULT42PFBUG
  return(cc - sizeof(ea) - sizeof(header));
#endif ULT42PFBUG
#else  PHASE2
  iov[0].iov_base = (caddr_t)&ea;
  iov[0].iov_len = sizeof(ea);
  iov[1].iov_base = (caddr_t)buf;
  iov[1].iov_len = bufsiz;
  cc = pi_readv(edx, iov, 2);
  return(cc - sizeof(ea));
#endif PHASE2
}

pi_reada(fd, buf, bufsiz, eaddr)
int fd;
caddr_t buf;
int bufsiz;
char *eaddr;
{
  struct iovec iov[3];
#ifdef PHASE2
  char header[5];	/* must be 5! */
#endif PHASE2
  int cc;

#ifdef PHASE2
  iov[0].iov_base = (caddr_t)eaddr;
  iov[0].iov_len = header_size;
  iov[1].iov_base = (caddr_t)header;	/* consume 802.2 hdr & SNAP but */
  iov[1].iov_len = sizeof(header);	/* make room for our fake LAP header */
  iov[2].iov_base = (caddr_t)buf;
  iov[2].iov_len = bufsiz;
#else  PHASE2
  iov[0].iov_base = (caddr_t)eaddr;
  iov[0].iov_len = header_size;
  iov[1].iov_base = (caddr_t)buf;
  iov[1].iov_len = bufsiz;
#endif PHASE2

#ifdef PHASE2
  if ((cc = readv(fd, iov, 3)) < 0) {
#else  PHASE2
  if ((cc = readv(fd, iov, 2)) < 0) {
#endif PHASE2
    perror("abread");
    return(cc);
  }
#ifdef PHASE2
  /* make a fake LAP header to fool the higher levels	*/
  buf[0] = buf[11];		/* destination node ID	*/
  buf[1] = buf[12];		/* source node ID	*/
  buf[2] = 0x02;		/* always long DDP	*/
  return(cc - header_size - 5);
#else  PHASE2
  return(cc - header_size);
#endif PHASE2
}

export int
pi_write(edx, buf, buflen, eaddr)
int edx;
caddr_t buf;
int buflen;
char *eaddr;
{
  struct ephandle *eph;
  struct ether_header eh;
#ifdef USING_FDDI_NET
  struct fddi_header fh;
#endif USING_FDDI_NET
  struct sockaddr sa;
  struct iovec iov[2];
#ifdef PHASE2
  char *q;
#endif PHASE2

#ifdef USING_FDDI_NET
  if (using_fddi)
    iov[0].iov_base = (caddr_t)&fh;
  else
#endif USING_FDDI_NET
  iov[0].iov_base = (caddr_t)&eh;
  iov[0].iov_len = header_size;
  iov[1].iov_base = (caddr_t)buf;
  iov[1].iov_len = buflen;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);

  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

#ifdef USING_FDDI_NET
  if (using_fddi) {
    bcopy(eaddr, fh.fddi_dhost, 6);
    fh.fddi_fc = FDDIFC_LLC_ASYNC;
    fh.fddi_ph[0] = 0;
    fh.fddi_ph[1] = 0;
    fh.fddi_ph[2] = 0;
  } else
#endif USING_FDDI_NET
  bcopy(eaddr, eh.ether_dhost, 6);
#ifdef PHASE2
#ifdef USING_FDDI_NET
  if (!using_fddi)
#endif USING_FDDI_NET
  eh.ether_type = htons(buflen);
  /*
   * Fill in the remainder of the 802.2 and SNAP header bytes.
   * Clients have to leave 8 bytes free at the start of buf as
   * NIT won't let us send any more than 14 bytes of header :-(
   */
  q = (char *) buf;
  *q++ = 0xaa;		/* destination SAP	*/
  *q++ = 0xaa;		/* source SAP		*/
  *q++ = 0x03;		/* control byte		*/
  *q++ = (eph->protocol == 0x809b) ? 0x08 : 0x00;
  *q++ = 0x00;		/* always zero		*/
  *q++ = (eph->protocol == 0x809b) ? 0x07 : 0x00;
  *q++ = (eph->protocol >> 8) & 0xff;
  *q++ = (eph->protocol & 0xff);
#else  PHASE2
  eh.ether_type = htons(eph->protocol);
#endif PHASE2

  if (writev(eph->fd, iov, 2) < 0) {
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

#ifdef PHASE2
  for (len = 8, p = ebuf+8, i = 0 ; i < iovlen ; i++)
#else  PHASE2
  for (len = 0, p = ebuf, i = 0 ; i < iovlen ; i++)
#endif PHASE2
    if (iov[i].iov_base && iov[i].iov_len >= 0) {
      bcopy(iov[i].iov_base, p, iov[i].iov_len);
      p += iov[i].iov_len;	/* advance */
      len += iov[i].iov_len;	/* advance */
    }
  return(pi_write(edx, ebuf, len, eaddr));
}
