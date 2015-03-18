static char rcsid[] = "$Author: djh $ $Date: 1996/09/24 13:55:14 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/ethertalk/RCS/bpfiltp.c,v 2.8 1996/09/24 13:55:14 djh Rel djh $";
static char revision[] = "$Revision: 2.8 $";

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
 *  June 1991  Rakesh Patel/David Hornsby Add Phase 2 support
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

#include <netat/compat.h>
#include <netat/appletalk.h>
#include "../uab/proto_intf.h"

#ifdef  bsdi
#include <kvm.h>
#define MULTI_BPF_PKT
#define USE_SIOCGIFCONF
#endif  bsdi

#ifdef __NetBSD__
#define MULTI_BPF_PKT
#define USE_SIOCGIFCONF
#endif __NetBSD__

#ifdef __FreeBSD__
#define MULTI_BPF_PKT
#define USE_SIOCGIFCONF
#endif __FreeBSD__

#ifdef NeXT
#include <errno.h>
#define MULTI_BPF_PKT
#undef USE_SIOCGIFCONF
#endif NeXT

#ifdef USE_SIOCGIFCONF
#include <net/if_dl.h>
#include <net/if_types.h>
#endif USE_SIOCGIFCONF

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
pi_open(protocol, socket, dev, devno)
int protocol;
int socket;
char *dev;
int devno;
{
  private int init_nit();
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
#ifdef MULTI_BPF_PKT
  extern int read_buf_len;
#endif /* MULTI_BPF_PKT */
  if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    return(-1);
  fdunlisten(ephlist[edx-1].fd); /* toss listener */
  close(ephlist[edx-1].fd);
  ephlist[edx-1].inuse = 0;
#ifdef MULTI_BPF_PKT
  read_buf_len = 0;
#endif /* MULTI_BPF_PKT */
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
  u_int imm;
  struct bpf_version vers;
  private int setup_pf();

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
      sprintf (enetd, "/dev/bpf%d", i);
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

  if (setup_pf(s, protocol, socket) < 0)
    return(-1);

  /* We have to read EXACTLY the buffer size. */

  if ((ioctl(s, BIOCGBLEN, &pf_bufsize)) < 0) {
    perror("ioctl: get buffer length ");
    return(-1);
  }

  read_buf = (char *)malloc(pf_bufsize);

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

#ifdef PHASE2
/*
 * add a multicast address to the interface
 *
 */

#ifdef NeXT

/*
 * For NEXTSTEP 3.1 and later
 *
 * Multicast is controled through an enhanced version of BPF for NeXT.
 * You can get it from "ftp.aa.ap.titech.ac.jp". (S.Adachi, 96/02/17)
 *
 * It was announced that NEXTSTEP (>=3.3) would officially support multicast.
 * However, it does not actually work.
 *
 */

#define SIOCADDMULTI    _IOW('i', 49, struct ifreq)     /* add m'cast addr */
#define SIOCDELMULTI    _IOW('i', 50, struct ifreq)     /* del m'cast addr */

int
pi_addmulti(multi, ifr)
char *multi;
struct ifreq *ifr;
{
  int s;
  int i;
  int failed = 0;
  char enetd[256];

  /* Prepare the interface request struct. */
  ifr->ifr_addr.sa_family = AF_UNSPEC;
  bcopy(multi, ifr->ifr_addr.sa_data, EHRD);

  /* Open a bpf device. */
  for (i = 0; !failed ; i++) {
    sprintf (enetd, "/dev/bpf%d", i);
    if ((s = open (enetd, O_RDWR)) >= 0)
      break;
    /* if we get past the break, we got an error */
    if (errno != EBUSY) failed++;
  }
  if (failed) {
    perror("open: /dev/bpfXX");
    return(-1);
  }

  /* Attach the Ethernet device /dev/en0 to the BPF. */
  if (ioctl(s, BIOCSETIF, ifr) < 0) {
    perror("ioctl: set interface");
    return(-1);
  }

  /* Set the multicast address. */
  if (ioctl(s, SIOCADDMULTI, (caddr_t)ifr) < 0) {
    perror("SIOCADDMULTI");
    close(s);
    return(-1);
  }
  close(s);
  return(0);
}

#else /* NeXT */

int
pi_addmulti(multi, ifr)
char *multi;
struct ifreq *ifr;
{
  int sock;

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
  if (ioctl(sock, SIOCADDMULTI, (caddr_t)ifr) < 0) {
    perror("SIOCADDMULTI");
    close(sock);
    return(-1);
  }
  close(sock);
  return(0);
}
#endif /* NeXT */
#endif PHASE2


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
  struct ether_header eh;
  struct bpf_program pf;
#define PROG_SIZE 20
  struct bpf_insn pf_insns[PROG_SIZE];
  int ppf=0;
  extern int errno;

#define s_offset(structp, element) (&(((structp)0)->element))
  offset = ((int)s_offset(struct ether_header *, ether_type));
#ifdef PHASE2
  offset += 8;	/* shorts: 2 bytes length + 6 bytes of 802.2 and SNAP */
#endif PHASE2

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
  if (sock >= 0) {
#ifdef PHASE2
    pf_insns[ppf].code = BPF_LD+BPF_B+BPF_ABS; /* Dest. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = offset+12;
    ppf++;
    pf_insns[ppf].code = BPF_JMP+BPF_JEQ+BPF_K;
    pf_insns[ppf].jt = 1;
    pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = sock & 0xff;
    ppf++;
    pf_insns[ppf].code = BPF_RET+BPF_K;       /* Fail. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = 0;
    ppf++;
#else  PHASE2
/* short form */
    pf_insns[ppf].code = BPF_LD+BPF_B+BPF_ABS; /* LAP type. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = offset+4;
    ppf++;
    pf_insns[ppf].code = BPF_JMP+BPF_JEQ+BPF_K;
    pf_insns[ppf].jt = 0;
    pf_insns[ppf].jf = 2;
    pf_insns[ppf].k = 1;
    ppf++;
    pf_insns[ppf].code = BPF_LD+BPF_B+BPF_ABS; /* Dest. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = offset+7;
    ppf++;
    pf_insns[ppf].code = BPF_JMP+BPF_JEQ+BPF_K;
    pf_insns[ppf].jt = 5;
    pf_insns[ppf].jf = 4;
    pf_insns[ppf].k = sock & 0xff;
    ppf++;
/* long form */
    pf_insns[ppf].code = BPF_LD+BPF_B+BPF_ABS; /* LAP type. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = offset+4;
    ppf++;
    pf_insns[ppf].code = BPF_JMP+BPF_JEQ+BPF_K;
    pf_insns[ppf].jt = 0;
    pf_insns[ppf].jf = 2;
    pf_insns[ppf].k = 2;
    ppf++;
    pf_insns[ppf].code = BPF_LD+BPF_B+BPF_ABS; /* Dest. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = offset+15;
    ppf++;
    pf_insns[ppf].code = BPF_JMP+BPF_JEQ+BPF_K;
    pf_insns[ppf].jt = 1;
    pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = sock & 0xff;
    ppf++;
    pf_insns[ppf].code = BPF_RET+BPF_K;       /* Fail. */
    pf_insns[ppf].jt = pf_insns[ppf].jf = 0;
    pf_insns[ppf].k = 0;
    ppf++;
#endif PHASE2
  }
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

#ifdef USE_SIOCGIFCONF
  {
    int len, sock;
    struct ifconf ifconf;
    struct sockaddr_dl *sadl;
    struct ifreq ifrbuf[32], *ifra, *ifrb;

    ifconf.ifc_len = sizeof(ifrbuf);
    ifconf.ifc_buf = (caddr_t)ifrbuf;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("SOCK_DGRAM");
      return(-1);
    }
    if (ioctl(sock, SIOCGIFCONF, &ifconf) < 0) {
      perror("SIOCGIFCONF");
      close(sock);
      return(-1);
    }
    close(sock);
    ifra = ifrbuf;
    ifrb = (struct ifreq *)((char *)ifra + ifconf.ifc_len);
    while (ifra < ifrb) {
      if (strcmp(eph->ifr.ifr_name, ifra->ifr_name) == 0) {
	if (ifra->ifr_addr.sa_family == AF_LINK) {
	  sadl = (struct sockaddr_dl *)&ifra->ifr_addr;
	  if (sadl->sdl_type == IFT_ETHER) {
	    bcopy((char *)LLADDR(sadl), ea, 6);
	    return(0);
	  }
#ifdef bsdi
	  if (sadl->sdl_alen == 0) /* no addr, try other method */
	    return(get_eth_addr(eph->fd, eph->ifr.ifr_name, ea));
#endif bsdi
	}
      }
      if ((len = ifra->ifr_addr.sa_len+sizeof(ifra->ifr_name)) < sizeof(*ifra))
	len = sizeof(*ifra);
      ifra = (struct ifreq *)((char *)ifra + len);
    }
  }
  return(-1);
#else  USE_SIOCGIFCONF
  if (ioctl(eph->fd, SIOCGIFADDR, &eph->ifr) < 0) {
    perror("Ioctl: SIOCGIFADDR");
    return(-1);
  }
  sa = (struct sockaddr *)&eph->ifr.ifr_data;
  bcopy(sa->sa_data, ea, 6);
  return(0);
#endif USE_SIOCGIFCONF
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
 * Note: BPF can return multiple packets in one read
 *
 */

#ifdef MULTI_BPF_PKT
#define NUMRDS	32

struct RDS {
  u_short dataLen;
  u_char *dataPtr;
};

struct RDS RDS[NUMRDS];
export int read_buf_len = 0;
#endif /* MULTI_BPF_PKT */

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

#ifdef MULTI_BPF_PKT
  static int rds_index;
  char *q;

  if (read_buf_len == 0) {
    /* Must read exactly the buffer size. */
    if ((cc = read(fd, read_buf, pf_bufsize)) < 0)
      return(cc);
    /* fill in RDS */
    p = read_buf;
    q = read_buf+cc;
    read_buf_len = cc;
    for (i = 0; i < (NUMRDS-1) && p < q; i++) {
      bp = (struct bpf_hdr *)p;
      RDS[i].dataLen = bp->bh_caplen;
      RDS[i].dataPtr = (u_char *)(p + bp->bh_hdrlen);
      p += BPF_WORDALIGN(bp->bh_hdrlen+bp->bh_caplen);
    }
    RDS[i].dataLen = 0;
    rds_index = 0;
  }
  if ((size = (int)RDS[rds_index].dataLen) == 0) {
    read_buf_len = 0;
    return(0);
  }
  p = (char *)RDS[rds_index].dataPtr;
  for (i = 0; i < iovlen && size > 0; i++) {
    if (size < iov[i].iov_len)
      cc = size;
    else
      cc = iov[i].iov_len;
    bcopy(p, iov[i].iov_base, cc);
    p += cc;
    size -= cc;
  }
  cc = RDS[rds_index].dataLen;
  if (size > 0)
    cc -= size;
  rds_index++;
  read_buf_len = RDS[rds_index].dataLen;
  return(cc);
#else /* MULTI_BPF_PKT */
  /* Must read exactly the buffer size. */
  if ((cc = read(fd, read_buf, pf_bufsize)) < 0) {
    return(cc);
  }
  /* The data begins with a header. */
  bp = (struct bpf_hdr*) read_buf;
  p = read_buf + bp->bh_hdrlen;
  size = bp->bh_caplen;
  for (i=0; i<iovlen && size > 0; i++) {
     if (size < iov[i].iov_len) bcopy(p, iov[i].iov_base, size);
     else bcopy(p, iov[i].iov_base, iov[i].iov_len);
     p += iov[i].iov_len;
     size -= iov[i].iov_len;
  }
  if (size > 0) return (bp->bh_caplen-size);
  else return bp->bh_caplen;
#endif /* MULTI_BPF_PKT */
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
  if ((cc = bp_readv(eph->fd, iov, iovlen)) < 0) {
    perror("abread");
  }
  return cc;
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
  iov[1].iov_base = (caddr_t)header; /* consume 802.2 hdr & SNAP */
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
  if ((cc = bp_readv(fd, iov, 3)) < 0) {
#else  PHASE2
  if ((cc = bp_readv(fd, iov, 2)) < 0) {
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
  return(cc - 14);
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
#ifdef __FreeBSD__
  /* This should really be fixed in the kernel. */
  eh.ether_type = buflen;
#else
  eh.ether_type = htons(buflen);
#endif
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
#ifdef __FreeBSD__
  /* This should really be fixed in the kernel. */
  eh.ether_type = eph->protocol;
#else
  eh.ether_type = htons(eph->protocol);
#endif
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

#ifdef	bsdi
/*
 * lifted from UAR pf.c
 *
 */

struct nlist nlst[] = {
  { "_ifnet" },
  "",
};

/*
 * BSDI
 *
 * Get interface address from the kernel since the SIOCGIFADDR
 * ioctl isn't implemented in a number of ethernet drivers.
 *
 */

int
get_eth_addr(s, interface, addr)
int s;
char *interface;
u_char *addr;
{
  kvm_t *kvmd;
  int err = -1;
  off_t ifnetptr;
  off_t aifnetptr;
  off_t ifaddrptr;
  struct sockaddr sa;
  struct ifnet ifnet;
  struct ifaddr ifaddr;
  struct arpcom arpcom;
  struct sockaddr_dl *sdl;
  char *cp, if_name[16];

  if ((kvmd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) == NULL) {
    fprintf(stderr, "kvm_open: can't open kernel!\n");
    return(-1);
  }
  if (kvm_nlist(kvmd, nlst) < 0 || nlst[0].n_type == 0) {
    fprintf(stderr, "kvm_list: can't find namelist!\n");
    kvm_close(kvmd);
    return(-1);
  }
  if (nlst[0].n_value == 0) {
    fprintf(stderr, "kvm_list: _ifnet symbol not defined!\n");
    kvm_close(kvmd);
    return(-1);
  }
  if (kvm_read(kvmd, nlst[0].n_value, (char *)&ifnetptr,
   sizeof(ifnetptr)) != sizeof(ifnetptr)) {
    fprintf(stderr, "kvm_read: bogus read!\n");
    kvm_close(kvmd);
    return(-1);
  }
  while (ifnetptr) {
    aifnetptr = ifnetptr;
    if (kvm_read(kvmd, ifnetptr, (char *)&ifnet,
     sizeof(ifnet)) == sizeof(ifnet)) {
      if (kvm_read(kvmd, (off_t)ifnet.if_name, if_name,
       sizeof(if_name)) == sizeof(if_name)) {
        ifnetptr = (off_t)ifnet.if_next;
	if_name[15] = '\0';
        cp = (char *)index(if_name, '\0');
	sprintf(cp, "%d", ifnet.if_unit);
	if (strcmp(if_name, interface) != 0)
	  continue;
	ifaddrptr = (off_t)ifnet.if_addrlist;
	while (ifaddrptr) {
	  if (kvm_read(kvmd, ifaddrptr, (char *)&ifaddr,
	   sizeof(ifaddr)) == sizeof(ifaddr)) {
	    ifaddrptr = (off_t)ifaddr.ifa_next;
	    if (kvm_read(kvmd, (off_t)ifaddr.ifa_addr, (char *)&sa,
	     sizeof(sa)) == sizeof(sa)) {
	      if (sa.sa_family == AF_LINK) {
		sdl = (struct sockaddr_dl *)&sa;
		cp = (char *)LLADDR(sdl);
		if (sdl->sdl_alen == 0) {
		/* no address here, try ethernet driver */
		  if (kvm_read(kvmd, aifnetptr, (char *)&arpcom,
		   sizeof(arpcom)) == sizeof(arpcom))
		    cp = (char *)arpcom.ac_enaddr;
		}
		bcopy(cp, (char *)addr, 6);
	        fprintf(stderr, "BSDI: %-5s eth address %x:%x:%x:%x:%x:%x\n",
		  interface, addr[0], addr[1], addr[2],
		  addr[3], addr[4], addr[5]);
		err = 0;
		break;
	      }
	      continue;
	    }
	  }
	  break;
	}
      }
    }
    break;
  }
  kvm_close(kvmd);
  return(err);
}
#endif	bsdi
