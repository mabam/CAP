static char rcsid[] = "$Author: djh $ $Date: 1994/10/10 08:55:05 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/ethertalk/RCS/senetp.c,v 2.9 1994/10/10 08:55:05 djh Rel djh $";
static char revision[] = "$Revision: 2.9 $";

/*
 * senetp.c - Simple "protocol" level interface to enet
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
 *  June 1991  Rakesh Patel/David Hornsby Add Phase 2 support
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
#include <sys/enet.h>
extern int errno;
#else  pyr
#include <net/enet.h>
#endif pyr
#include <netinet/in.h>
#ifdef pyr
#include <net/if_ether.h>
#else  pyr
#include <netinet/if_ether.h>
#endif pyr
#include <netdb.h>
#include <ctype.h>

#include <netat/compat.h>
#include <netat/appletalk.h>
#include "../uab/proto_intf.h"

#ifdef PHASE2
#include <a.out.h>
#include <stropts.h>
#include <sys/stat.h>
#include <net/if_ieee802.h>
#endif PHASE2

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int fd;			/* file descriptor of socket */
  struct ifreq ifr;
  int protocol;			/* ethernet protocol */
  int socket;			/* ddp socket */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];

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
 * Return: socket if no error, < 0 o.w.
 *
*/
private int
init_nit(chunksize, protocol, socket, ifr)
u_long chunksize;
u_short protocol;
int socket;
struct ifreq *ifr;
{
  int s;
  u_long if_flags;
  char device[64];

  sprintf(device, "/dev/%s", interface);
  
#ifdef EIOCETHERT

  /* get clone */
  if ((s = open(device, O_RDWR)) < 0) {
    perror("open: /dev/enetXX");
    return(-1);
  }
  
#else  EIOCETHERT

  { 
    register int i, fid;
    register int failed = 0;
    char enetd[256];
 
    /*
     * try all ethernet minors from (devname)0 on up.
     * (e.g., /dev/enet0 .... )
     *
     * Algorithm: assumption is that names start at 0
     * and run up as decimal numbers without gaps.  We search
     * until we get an error that is not EBUSY (i.e., probably
     * either ENXIO or ENOENT), or until we sucessfully open one.
     */
 
    for (i = 0; !failed ; i++) {
      sprintf (enetd, "%s%d", device, i);
      if ((s = open (enetd, 2)) >= 0)
        break;
      /* if we get past the break, we got an error */
      if (errno != EBUSY) failed++;
    }
 
    if (failed) {
      perror("open: /dev/enetXX");
      return(-1);
    }
  }

#endif EIOCETHERT

  if (setup_pf(s, protocol, socket) < 0)
    return(-1);

#define NOBUF
#ifndef NOBUF
  setup_buf(s, chunksize);
#endif  NOBUF
  
  /* flush read queue */
#ifdef EIOCFLUSH
  ioctl(s, EIOCFLUSH, 0);
#endif EIOCFLUSH
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
  char device[64];

  sprintf(device, "/dev/%s", interface);

  ifr->ifr_addr.sa_family = AF_UNSPEC;
  bcopy(multi, ifr->ifr_addr.sa_data, EHRD);

  /* place the "real" interface name (ie0, le0 etc.) into ifr */
  if (pi_ifname(device, ifr) < 0)
    exit(1);

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

/*
 * ferret around in /dev/kmem for real interface details
 * (sometime it would be nice to have an ioctl to do this)
 *
 */

struct enet_info {
	struct ifnet *ifp;
};

int
pi_ifname(devName, ifr)
char *devName;
struct ifreq *ifr;
{
  int kmem;
  int dataLen;
  int enUnits;
  int minorDev;
  char *kernelfile;
  char namebuf[64];
  struct stat sbuf;
  struct nlist nl[3];
  struct ifnet ifnet;
  struct enet_info *enet_info;

  kernelfile = "/vmunix";
  if (stat(kernelfile, &sbuf) != 0) {
    perror("stat /vmunix");
    return(-1);
  }
  if ((kmem = open("/dev/kmem", 0)) < 0) {
    perror("open /dev/kmem");
    return(-1);
  }

  nl[0].n_un.n_name = "_enUnits";
  nl[1].n_un.n_name = "_enet_info";
  nl[2].n_un.n_name = "";

  nlist(kernelfile,nl);

  if (nl[0].n_type == 0 || nl[1].n_type == 0) {
    fprintf(stderr, "pi_ifname() can't find _enUnits or _enet_info\n");
    close(kmem);
    return(-1);
  }

  (void) lseek(kmem, (nl[0].n_value), 0);
  if (read(kmem, &enUnits, sizeof(enUnits)) != sizeof(enUnits)) {
    perror("read /dev/kmem #1");
    close(kmem);
    return(-1);
  }
  dataLen = enUnits*sizeof(struct enet_info);
  if ((enet_info = (struct enet_info *) malloc(dataLen)) == 0) {
    fprintf(stderr, "pi_ifname() can't malloc enet_info\n");
    close(kmem);
    return(-1);
  }
  (void) lseek(kmem, (nl[1].n_value), 0);
  if (read(kmem, enet_info, dataLen) != dataLen) {
    perror("read /dev/kmem #2");
    close(kmem);
    return(-1);
  }

  if (stat(devName, &sbuf) != 0) {
    perror(devName);
    close(kmem);
    return(-1);
  }
  if (!S_ISCHR(sbuf.st_mode)) {
    fprintf(stderr, "%s: not character special device!\n", devName);
    close(kmem);
    return(-1);
  }
  minorDev = sbuf.st_rdev & 0xff;	/* minor device number */

  if (minorDev >= enUnits) {
    fprintf(stderr,"%s: invalid minor device number %d\n", devName, minorDev);
    close(kmem);
    return(-1);
  }
  (void) lseek(kmem, enet_info[minorDev].ifp, 0);
  if (read(kmem, &ifnet, sizeof(ifnet)) != sizeof(ifnet)) {
    perror("read /dev/kmem #3");
    close(kmem);
    return(-1);
  }
  (void) lseek(kmem, ifnet.if_name, 0);
  if (read(kmem, namebuf, sizeof(namebuf)) != sizeof(namebuf)) {
    perror("read /dev/kmem #4");
    close(kmem);
    return(-1);
  }
  sprintf(ifr->ifr_name, "%s%d", namebuf, ifnet.if_unit);
  logit(7, "ENET %s maps to interface %s", devName, ifr->ifr_name);

  free(enet_info);
  close(kmem);
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
  int ethert;
  unsigned queuelen;
  struct ether_header eh;
  struct enfilter pf;
  register u_short *fwp = pf.enf_Filter;
  extern int errno;

#ifdef PHASE2
  ethert = htons(EF_8023_TYPE); /* special 802.3 type */
#else  PHASE2
  ethert = htons(prot);
#endif PHASE2
#ifdef EIOCETHERT
  if (ioctl(s, EIOCETHERT, &ethert) < 0 && errno != EEXIST ) {
    perror("ioctl: protocol filter");
    return(-1);
  }
#endif EIOCETHERT

  queuelen = 8;
  if (ioctl(s, EIOCSETW, &queuelen) < 0) {
    perror("ioctl: set recv queue length");
    return(-1);
  }

  pf.enf_Priority = 128;

#define s_offset(structp, element) (&(((structp)0)->element))
  offset = ((int)s_offset(struct ether_header *, ether_type))/sizeof(u_short);
#ifdef PHASE2
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
    *fwp++ = ENF_PUSHLIT ;
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
#ifdef PHASE2
  char header[8];
#endif PHASE2
  int cc;

#ifdef PHASE2
  iov[0].iov_base = (caddr_t)&ea;
  iov[0].iov_len = sizeof(ea);
  iov[1].iov_base = (caddr_t)header;	/* consume 802.2 hdr & SNAP */
  iov[1].iov_len = sizeof(header);
  iov[2].iov_base = (caddr_t)buf;
  iov[2].iov_len = bufsiz;
  cc = pi_readv(edx, iov, 3);
  return(cc - sizeof(ea) - sizeof(header));
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
  iov[0].iov_len = 14;
  iov[1].iov_base = (caddr_t)header;	/* consume 802.2 hdr & SNAP but */
  iov[1].iov_len = sizeof(header);	/* make room for our fake LAP header */
  iov[2].iov_base = (caddr_t)buf;
  iov[2].iov_len = bufsiz;
#else  PHASE2
  iov[0].iov_base = (caddr_t)eaddr;
  iov[0].iov_len = 14;
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
  return(cc - 14 - sizeof(header));
#else  PHASE2
  return(cc - 14);
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
  struct sockaddr sa;
  struct iovec iov[2];
#ifdef PHASE2
  char *q;
#endif PHASE2

  iov[0].iov_base = (caddr_t)&eh;
  iov[0].iov_len = sizeof(struct ether_header);
  iov[1].iov_base = (caddr_t)buf;
  iov[1].iov_len = buflen;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);

  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  bcopy(eaddr, &eh.ether_dhost, 6);
#ifdef PHASE2
  eh.ether_type = htons(buflen);
  /*
   * Fill in the remainder of the 802.2 and SNAP header bytes.
   * Clients have to leave 8 bytes free at the start of buf as
   * we can't send more than 14 bytes of header :-(
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

#ifdef PHASE2	/* leave room for rest of ELAP hdr */
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
