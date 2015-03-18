static char rcsid[] = "$Author: djh $ $Date: 1991/09/01 06:24:45 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/srawetherp.c,v 2.1 1991/09/01 06:24:45 djh Rel djh $";
static char revision[] = "$Revision: 2.1 $";

/*
 * srawether.c - Simple "protocol" level interface to Rawether
 * NEWS-OS 4.0
 *
 * TAYA Shin'ichiro <taya@sm.sony.co.jp>
 *
 */ 

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <ctype.h>

#include <newsif/if_rawether.h>

#include <netat/appletalk.h>
#include "proto_intf.h"

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int socket;			/* file descriptor of socket */
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
	if ((s = init_rawether( devnamebuf, protocol)) < 0) {
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
 * Initialize rawether on a particular protocol type
 * 
 * Runs in promiscous mode for now.
 *
 * Return: socket if no error, < 0 o.w.
*/
private int
init_rawether(name, protocol)
char *name;
u_short protocol;
{
	int s, type;
  
	/* get clone */
	if ((s = open(name, O_RDWR)) < 0) {
		perror(name);
		return(-1);
	}
	type = ETHER_ALL;
	if(ioctl(s, ETHERIOCSTYPE, &type) < 0){
		perror("ioctl");
		return (-1);
	}
	
	return(s);
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
  
  if(ioctl(eph->socket, ETHERIOCGADDR, ea) != 0){
	  perror("ioctl: SIOCGIFADDR");
	  exit(1);
  }
  
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

u_char pi_write_buf[ETHERMTU];

export int
pi_write(edx, buf, buflen, eaddr, protocol)
int edx;
caddr_t buf;
int buflen;
char *eaddr;
int protocol;
{
  struct ephandle *eph;
  struct ether_header eh;
  int l;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);

  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  bcopy(eaddr, &(eh.ether_dhost[0]), sizeof(eh.ether_dhost));
  eh.ether_type =protocol;
  bcopy(&eh, pi_write_buf, sizeof(struct ether_header));
  bcopy(buf, &pi_write_buf[sizeof(struct ether_header)], buflen);

  l = (buflen > ETHERMIN ? buflen : ETHERMIN) + sizeof(struct ether_header);
  
  if(write(eph->socket, pi_write_buf, l) < 0){
    return(-1);
  }
  return(buflen);
}

private char ebuf[2000];	/* big enough */

export int
pi_writev(edx, iov, iovlen, eaddr, protocol)
int edx;
struct iovec *iov;
int iovlen;
unsigned char eaddr[6];
int protocol;
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
  return(pi_write(edx, ebuf, len, eaddr, protocol));
}
