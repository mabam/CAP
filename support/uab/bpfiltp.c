static char rcsid[] = "$Author: djh $ $Date: 1995/05/29 10:45:45 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/bpfiltp.c,v 2.2 1995/05/29 10:45:45 djh Rel djh $";
static char revision[] = "$Revision: 2.2 $";

/*
 * bpfiltp.c - Simple "protocol" level interface to BPF
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
 *  Oct  1993  David Matthews.  Modified for Berkley Packet Filter
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
#include <net/bpf.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <ctype.h>

#include <netat/appletalk.h>
#include "proto_intf.h"
#include "../enet/enet.h"

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int socket;			/* file descriptor of socket */
  struct ifreq ifr;
  int protocol;			/* ethernet protocol */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];

static u_int pf_bufsize;
static char *read_buf;

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
 * Initialize nit on a particular protocol type
 * Return: socket if no error, < 0 o.w.
*/
private int
init_nit(chunksize, protocol, ifr)
u_long chunksize;
u_short protocol;
struct ifreq *ifr;
{
  u_long if_flags;
  int s;
  char device[64];
  u_int imm;
  struct bpf_version vers;

  sprintf(device, "/dev/%s", "bpf");
  
  { 
    register int i;
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
      if ((s = open (enetd, O_RDWR)) >= 0)
        break;
      /* if we get past the break, we got an error */
      if (errno != EBUSY) failed++;
    }
 
    if (failed) {
      perror("open: /dev/bpfXX");
      return(-1);
    }
  }

  /* Check the version number. */
  if ((ioctl(s, BIOCVERSION, &vers)) < 0) {
    perror("ioctl: get version");
    return(-1);
  }

  if (vers.bv_major != BPF_MAJOR_VERSION ||
      vers.bv_minor < BPF_MINOR_VERSION) {
	fprintf(stderr, "Incompatible packet filter version\n");
	return -1;
  }

  if (setup_pf(s, protocol) < 0)
    return(-1);
  
  /* We have to read EXACTLY the buffer size. */

  if ((ioctl(s, BIOCGBLEN, &pf_bufsize)) < 0) {
    perror("ioctl: get buffer length ");
    return(-1);
  }

  read_buf = malloc(pf_bufsize);

  if (ioctl(s, BIOCSETIF, ifr) < 0) {
    perror("ioctl: set interface");
    return(-1);
  }

  imm = 1;
  if (ioctl(s, BIOCIMMEDIATE, &imm) < 0) {
    perror("ioctl: set immediate mode");
    return(-1);
  }

  return(s);
}


/*
 * establish protocol filter
 *
*/
setup_pf(s, prot, sock)
int s;
u_short prot;
int sock;
{
  u_short offset;
  struct ether_header eh;
  struct bpf_program pf;
#define PROG_SIZE 4
  struct bpf_insn pf_insns[PROG_SIZE];
  int ppf=0;
  extern int errno;

#define s_offset(structp, element) (&(((structp)0)->element))
  offset = ((int)s_offset(struct ether_header *, ether_type));

  pf_insns[ppf].code = BPF_LD+BPF_H+BPF_ABS;
  pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
  pf_insns[ppf].k = offset;
  ppf++;
  pf_insns[ppf].code = BPF_JMP+BPF_JEQ+BPF_K;
  pf_insns[ppf].jt = 1;
  pf_insns[ppf].jf = 0;
  pf_insns[ppf].k = prot;
  ppf++;
  pf_insns[ppf].code = BPF_RET+BPF_K;       /* Fail. */
  pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
  pf_insns[ppf].k = 0;
  ppf++;
  pf_insns[ppf].code = BPF_RET+BPF_K;       /* Success. */
  pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
  pf_insns[ppf].k = (u_int)-1;
  ppf++;

  pf.bf_len = ppf;
  pf.bf_insns = pf_insns;
  if (ioctl(s, BIOCSETF, &pf) < 0) {
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
    perror("Ioctl: SIOCGIFADDR");
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

static int
bp_readv(fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  int cc;
  struct bpf_hdr *bp;
  char *p;
  int i, size;
  /* Must read exactly the buffer size. */
  if ((cc = read(fd, read_buf, pf_bufsize)) < 0) {
    return(cc);
  }
  /* The data begins with a header. */
  bp = (struct bpf_hdr*) read_buf;
  p = read_buf + bp->bh_hdrlen;
  size = bp->bh_caplen;
/*
  printf("Packet size %d dest %x:%x:%x:%x:%x:%x from source %x:%x:%x:%x:%x:%x\n",
    size,
    p[0]&0xff, p[1]&0xff, p[2]&0xff, p[3]&0xff, p[4]&0xff, p[5]&0xff,
    p[6]&0xff, p[7]&0xff, p[8]&0xff, p[9]&0xff, p[10]&0xff, p[11]&0xff);
*/
  for (i=0; i<iovlen && size > 0; i++) {
     if (size < iov[i].iov_len) bcopy(p, iov[i].iov_base, size);
     else bcopy(p, iov[i].iov_base, iov[i].iov_len);
     p += iov[i].iov_len;
     size -= iov[i].iov_len;
  }
  if (size > 0) return (bp->bh_caplen-size);
  else return bp->bh_caplen;
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
  if ((cc = bp_readv(eph->socket, iov, iovlen)) < 0) {
    perror("abread");
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

  bcopy(eaddr, &eh.ether_dhost, 6);
#ifdef __FreeBSD__
  /* This should really be fixed in the kernel packet filter code. */
  eh.ether_type = eph->protocol;
#else
  eh.ether_type = htons(eph->protocol);
#endif

  if (writev(eph->socket, iov, 2) < 0) {
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
