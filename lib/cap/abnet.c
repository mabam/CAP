/*
 * $Author: djh $ $Date: 91/02/15 22:48:22 $
 * $Header: abnet.c,v 2.1 91/02/15 22:48:22 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * abnet.c - appletalk UNIX access.
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *  in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 14, 1986    Schilit	Created.
 *  June 18, 1986    CCKim      Chuck's handler runs protocol
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netat/appletalk.h>
#include <netat/compat.h>	/* to cover difference between bsd systems */
#include "cap_conf.h"

/*
 * Configuration defines
 *
 * NEEDNETBUF - system doesn't have scatter/gather recv/send
 * NOFFS - no ffs function defined, use our own
 *
*/
#ifdef NORECVMSG
# ifndef NEEDNETBUF
#  define NEEDNETBUF
# endif
#endif
#ifdef NOSENDMSG
# ifndef NEEDNETBUF
#  define NEEDNETBUF
# endif
#endif

/* good place to stick the copyright so it shows up in object files */
char Columbia_Copyright[] = "Copyright (c) 1986,1987,1988 by The Trustees of Columbia University in the City of New York";

private struct sockaddr_in abfsin; /* apple bus foreign socketaddr/internet */
private int abfd;		/* apple bus socket */
import word this_net;		/* our network number */
import byte this_node;		/* our node number */
import word nis_net;		/* network number of our nis */
import byte nis_node;		/* node number of our nis */
import int ddp_protocol();	/* DDP protocol handler */
export struct sockaddr_in from_sin; /* network struct of last packet rec. */
export struct in_addr ipaddr_src; /* IP src address of last packet received */

export DBUG dbug;		/* debug flags */

/* BUG: bind doesn't work when lsin is on the stack! */
private struct sockaddr_in lsin; /* local socketaddr/internet */

private int skt2fd[ddpMaxSkt+1]; /* translate socket to file descriptor */
private gfd_set fdbitmask;	/* file descriptors open... */
private gfd_set *fdbits;
private int fdmaxdesc = 0;

/* private int subshift, submask, ddpnet; - not used at present */

/*
 * OSErr GetNodeAddress(int *myNode,*myNet)
 *
 * GetNodeAddress returns the net and node numbers for the current
 * host.
 *
 * N.B. - the myNet address is in net (htons) format.
 *
*/
 
OSErr
GetNodeAddress(myNode,myNet)
int *myNode,*myNet;
{
  *myNode = this_node;
  *myNet = this_net;
  return(noErr);			   /* is ok */
}

abInit(disp)
{
  int i;

  for (i=0; i < ddpMaxSkt+1; i++) {
    skt2fd[i] = -1;		/* mark all these as unused */
  }

  fdbits = &fdbitmask;
  FD_ZERO(fdbits);		/* make sure these are cleared */
  fdmaxdesc = 0;

  /* no need to bind since we don't recv on this socket, just send... */
  if ((abfd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("abinit");
    return(abfd);
  }
  abfsin.sin_family = AF_INET;
  abfsin.sin_addr.s_addr = INADDR_ANY;

  openatalkdb();	/* sets up this_* */

  if (disp) {
      printf("abInit: [ddp: %3d.%02d, %d]",
	     ntohs(this_net)>>8, htons(this_net)&0xff, this_node);
      if (this_net != nis_net || this_node != nis_node)
	printf(", [NBP (atis) Server: %3d.%02d, %d]",
	       ntohs(nis_net)>>8, htons(nis_net)&0xff, nis_node);
      printf(" starting\n");
  }
  LAPOpenProtocol(lapDDP,ddp_protocol); /* open protocol */

  return(0);
}  

/*
 * int abOpen(int *skt,rskt)
 *
 * abOpen opens the ddp socket in "skt" or if "skt" is zero allocates
 * and opens a new socket.  Upon return "skt" contains the socket number
 * and the returned value is the file descriptor for that socket.
 *
*/

int abOpen(skt,rskt)
int *skt;
int rskt;
{
  int i,fd,err;
  word ipskt;

  if ((fd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("abopen");
    return(fd);
  }
  lsin.sin_family = AF_INET;
  lsin.sin_addr.s_addr = INADDR_ANY;

  *skt = (rskt == 0 ? ddpWKS : rskt); /* zero rskt is free choice */
  ipskt = ddp2ipskt(*skt);	/* translate into ip socket number */
  for (i=0; i < 128; i++,ipskt++,(*skt)++) {
    lsin.sin_port = htons(ipskt);
    if ((err = bind(fd, (caddr_t)&lsin, sizeof(lsin))) == 0)
      break;
    if (rskt != 0)		/* bind failed and wanted exact? */
      return(err);		/* yes... */
  }
  if (err == 0 && i < 128) {
    FD_SET(fd, fdbits);
    fdmaxdesc = max(fd, fdmaxdesc);
    skt2fd[*skt] = fd;		/* remember file descriptor for socket */
    return(noErr);
  }
  perror("abopen bind");
  close(fd);
  return(err);
}

/*
 * returns true if socket is opened by abnet
 *
*/
abIsSktOpen(skt)
{
  if (skt < 0 || skt > ddpMaxSkt)
    return(FALSE);
  return(!(skt2fd[skt] == -1));
}

#ifdef notdef
abSktRdy(skt)
int skt;
{
  if (skt2fd[skt] < 0)
    return;
  FD_SET(skt2fd[skt],fdbits);
}

abSktNotRdy(skt)
int skt;
{
  if (skt2fd[skt] < 0)
    return;
  FD_CLR(skt2fd[skt],fdbits);
}
#endif
abClose(skt)
int skt;
{
  int i;

  if (skt < 0 || skt > ddpMaxSkt) {
    fprintf(stderr,"abClose: skt out of range\n");
    exit(0);
  }
  if (skt2fd[skt] < 0)
    return;
  if (close(skt2fd[skt]) != 0)
    perror("abClose");		/* some error... */
  FD_CLR(skt2fd[skt], fdbits);
  skt2fd[skt] = -1;		/* mark as unused */
  for (fdmaxdesc = 0, i = 0; i < ddpMaxSkt+1; i++)
    fdmaxdesc = max(fdmaxdesc, skt2fd[i]);
}

#ifndef NEEDNETBUF
abwrite(addr, skt, iov,iovlen)
struct in_addr addr;
unsigned short skt;
struct iovec *iov;
{
  struct msghdr msg;
  int err;

  abfsin.sin_addr = addr;
  abfsin.sin_port = skt;
  msg.msg_name = (caddr_t) &abfsin;
  msg.msg_namelen = sizeof(abfsin);
  msg.msg_iov = iov;
  msg.msg_iovlen = iovlen;
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
  if ((err = sendmsg(abfd,&msg,0)) < 0)
    perror("abwrite");
  return(err);  
}

abread(fd, iov, iovlen)
struct iovec *iov;
{
  struct msghdr msg;
  int err;

  msg.msg_name = (caddr_t) &from_sin;
  msg.msg_namelen = sizeof(from_sin);
  msg.msg_iov = iov;
  msg.msg_iovlen = iovlen;
  msg.msg_accrights = 0;
  msg.msg_accrightslen = 0;
  if ((err = recvmsg(fd,&msg,0)) < 0)
    perror("abread");
#ifdef notdef
#define rebPort 902
  ipaddr_src.s_addr=(sin.sin_port != htons(rebPort)) ? sin.sin_addr.s_addr:0;
#endif
  return(err);  
}
#else
/* Think about ways to make this better */
static char buf[630];		/* buffer */

abwrite(addr, skt, iov,iovlen)
struct in_addr addr;
unsigned short skt;
struct iovec *iov;
{
  int err;
  int i, pos;

  abfsin.sin_addr = addr;
  abfsin.sin_port = skt;
  for (i=0, pos=0; i < iovlen; i++) {
    bcopy(iov[i].iov_base, &buf[pos], iov[i].iov_len);
    pos+= iov[i].iov_len;
  }
  if ((err = sendto(abfd, buf, pos, 0, &abfsin, sizeof(abfsin))) < 0)
    perror("abwrite");
  return(err);  
}

abread(fd, iov, iovlen)
struct iovec *iov;
{
  int i,pos;
  int err;
  int sinlen;

  sinlen = sizeof(from_sin);
  if ((err=recvfrom(fd, buf, 630, 0, &from_sin, &sinlen)) < 0)
    perror("abread");
  for (pos=0,i=0; i < iovlen; i++) {
    bcopy(&buf[pos], iov[i].iov_base, iov[i].iov_len);
    pos += iov[i].iov_len;
  }
#ifdef notdef
#define rebPort 902
  ipaddr_src.s_addr=(sin.sin_port != htons(rebPort)) ? sin.sin_addr.s_addr:0;
#endif
  return(err);  
}
#endif

/* 
 * abSleep(int t) 
 *
 * does a sleep via select and handles incomming packets 
 *
*/

/*
 * optimization - holds the current gettimeofday() so lower level routines
 * can avoid system call overhead - updated when abSleep() is entered and
 * after every sleep
*/
export struct timeval tv_now;
export struct timezone tz_now;

abSleep(appt,nowait)
int appt,nowait;
{
  struct timeval sleept;

  gettimeofday(&tv_now, &tz_now); /* update current time */

  apptoabstime(&sleept,appt);	/* find absolute time for sleep */
  for (;;) {			/* loop until wait time elapsed */
    if (abSelect(&sleept) > 0 && nowait) /* handle packets */
      return;			/*  and return if not waiting */
    /* abSelect will have updated tv_now */
    if (cmptime(&sleept,&tv_now) < 0) /* past wait time? */
      return;
  }
}

#ifdef NOFFS

/* Find the First Set bit and return its number.  Numbering starts */
/* at one */
private int
ffs(pat)
register int pat;
{
  register int j;
  register int i;

  for (i = 0; i < 8*sizeof(int); i+=8) { /* each byte */
    if (pat & 0xff) {		/* if byte has bits */
      /* do a linear scan */
      for (j = 0; j < 8; j++) {
	if (pat & 0x1)
	  return(i+j+1);
	pat >>= 1;
      }
    }
    pat >>= 8;			/* go to the next byte */
  }
  return(-1);
}

#endif

/*
 * Find the first active file descriptor
 *
 * really doesn't belong here
 *
*/
FD_GETBIT(p) 
gfd_set *p;
{
  int i, w;
  for (w=0,i=0; i < howmany(FD_SETSIZE, NFDBITS); i++) {
    if ((w=ffs(p->fds_bits[i])) > 0)
      break;
  }
  if (w < 1)
    return(-1);
  w += (i*NFDBITS)-1;		/* find bit no */
  return((w > FD_SETSIZE) ? -1 : w);
}

/*
 * abSelect(struct timeval *awt)
 *
 * Call with absolute time.  Might return before that... 
 * Returns > 0 if event occured.
 *
*/
int
abSelect(awt)
struct timeval *awt;
{
  struct timeval rwt,mt;
  int fd,rdy;
  gfd_set rdybits;
  /*
   * optimization: if a timeout is less than the argument sleep time
   * then we don't have to return after the select() times out. If 
   * a timeout is not less then we don't have to scan the timeout lists.
   * Note: we must return if a timeout is less than the arg. sleep time
   * because this may have been an event we were waiting for!!!!!
   */
  int timeoutless;		/* true if timeout less than sleep time */

  rdybits = *fdbits;

  if (dbug.db_lap)
    fprintf(stderr,"abSelect enter... ");

  timeoutless = 0;
  abstoreltime(awt,&rwt);	/* convert requested time use requested */
  if (minTimeout(&mt))		/* find min timeout on functions if any */
    if (cmptime(&mt,awt) < 0) { /* smaller than requested? */
      abstoreltime(&mt,&rwt);	/* no, yes use it */
      ++timeoutless;		/* remember it */
    }
  if (rwt.tv_sec < 0) {
    if (dbug.db_lap)
      fprintf(stderr," negative ");
    rdy = 0;
  } else rdy = select(fdmaxdesc+1,&rdybits,0,0,&rwt); /* perform wait... */

  if (rdy > 0) {
    do {
      if ((fd = FD_GETBIT(&rdybits)) < 0)
	break;
      FD_CLR(fd, &rdybits);
      if (dbug.db_lap)
	fprintf(stderr,"lap_read call ");
      lap_read(fd);		/* process packet */
    } while (1);
  }
  gettimeofday(&tv_now, &tz_now);
  /*
   * Optimization: If a packet was waiting don't check the timeout lists -
   * odds are that the timeout hasn't expired yet, and we'll catch it the
   * next time through anyway
   */
  if (rdy <= 0 && timeoutless)
    /* An internal timeout occured before the user timeout, */
    /* and there were no packets available */
    doTimeout();
  if (dbug.db_lap)
    fprintf(stderr,"exit\n");
  return(rdy);			/* return count */
}



