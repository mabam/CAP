/*
 * sdlpi.c - Simple "protocol" level interface to Streams based DLPI
 * (SunOS 5.x) (derived from snitp.c SunOS 4.x module)
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
 *  April '91  djh   Add Phase 2 support
 *  May-June 93  montjoy@thor.ece.uc.EDU,
 *               appro@fy.chalmers.se        SunOS 5.x support
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
#include <sys/fcntl.h>
#include <sys/sockio.h>
#include <sys/bufmod.h>
#include <sys/pfmod.h>
#include <sys/stropts.h>
#include <sys/ethernet.h>
#include <sys/dlpi.h>

#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#include "../uab/proto_intf.h"

#define IEEE802_2 16    /* IEEE 802.2 SAP field */
                        /* actually 0 < any_number < 1500 */

private int init_nit();
private int stream_readv ();

typedef struct ephandle {	/* ethernet protocol driver handle */
  int inuse;			/* true if inuse */
  int fd;			/* file descriptor of socket */
  struct ifreq ifr;
  int protocol;			/* ethernet protocol */
  int socket;			/* ddp socket */
} EPHANDLE;

private inited = FALSE;

private EPHANDLE ephlist[MAXOPENPROT];
private int setup_pf();

extern char interface[50];

#ifdef PHASE2
extern char this_zone[34];
private u_char *zone_mcast();
private int zip_toupper();
private u_short chkSum();
#endif

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
  return(FALSE);
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
 *
 */

export int
pi_open(protocol, socket, dev, devno)
int protocol;
int socket;
char *dev;
int devno;
{
  int s, i;
  struct ephandle *eph;

  for (i = 0; i < MAXOPENPROT; i++) {
    if (!ephlist[i].inuse)
      break;
  }
  if (i == MAXOPENPROT)
    return(0);			/* nothing */
  eph = &ephlist[i];		/* find handle */

  strncpy(eph->ifr.ifr_name, interface, sizeof eph->ifr.ifr_name);

  if ((s = init_nit(1024, protocol, socket, &eph->ifr)) < 0)
    	return(-1);

  eph->inuse = TRUE;
  eph->fd = s;
  eph->protocol = protocol;
  eph->socket = socket;
  return(i+1);			/* skip zero */
}

/*
 * returns TRUE if machine will see own broadcasts
 *
 */

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
  fdunlisten(ephlist[edx-1].fd); /* toss listener */
  close(ephlist[edx-1].fd);
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
init_nit(chunksize, protocol, socket, ifr)
u_long chunksize;
u_short protocol;
int socket;
struct ifreq *ifr;
{
  	int s,devno = 0;
  	char *p;
	char device[64];
#ifdef PHASE2
	u_char e_broad[6] = {0x09, 0x00, 0x07, 0xff, 0xff, 0xff};
	u_char *zmulti;
#endif PHASE2

        sprintf(device, "/dev/%s", interface);
        for (p = device; *p != '\0'; p++) {
                if (*p >= '0' && *p <= '9') {
                        devno = atoi(p);
                        *p = '\0';
                        break;
                }
        }

  	if ((s = open(device,  O_RDWR)) < 0) {
    		perror(device);
    		return(-1);
  	}
  

	if(!AttachDevice(s,devno))
		return(-1);
#ifdef PHASE2
	if(!BindProtocol(s,IEEE802_2,0,DL_CLDLS, 0, 0 )) 
		return(-1);
#else  PHASE2
	if(!BindProtocol(s,protocol,0,DL_CLDLS, 0, 0 )) 
		return(-1);
#endif PHASE2

	/*if(!PromMode(s,DL_PROMISC_MULTI))
		return(-1);*/

#ifdef PHASE2
	if((zmulti = zone_mcast(this_zone, strlen(this_zone))) == NULL) 
	{
		fprintf(stderr, "Unable to get Zone Multicast Address\n");
		return(-1);
	}
	if(!AddMultiAddress(e_broad,s))
		return(-1);
	if(!AddMultiAddress(zmulti,s))
		return(-1);
#endif PHASE2
	/* warning: Sun specific */
  	if (strioctl (s, DLIOCRAW, -1, 0, NULL) < 0) {
		perror("DLIORAW");
		return(-1);
  	}

       	/* set up messages */
        if (ioctl(s, I_SRDOPT, (char *)RMSGD) < 0) { /* want messages */
               perror("ioctl: discretem");
               return(-1);
        }


  	if (setup_pf(s, protocol, socket) < 0)
    		return(-1);
#define NOBUF
#ifndef NOBUF
  	if( setup_buf(s, chunksize) < 0)
		return(-1);
#endif  NOBUF

  	/* flush read queue */
  	ioctl(s, I_FLUSH, (char *)FLUSHR);

  	return(s);
}

#ifdef PHASE2
/*
 * add a multicast address to the interface
 *
 */

int
pi_addmulti(multi, ifr)
u_char *multi;
struct ifreq *ifr;
{
	/*
	 * multicast addresses are per-stream now
	 * so just a NO OP
	 *
	 */

	return(0);
}

#endif PHASE2


#ifndef NOBUF
/*
 * setup buffering
 *
 */

setup_buf(s, chunksize)
int s;
u_long chunksize;
{
  struct timeval timeout;

  /* Push and configure the buffering module. */
  if (ioctl(s, I_PUSH, "bufmod") < 0) {
    perror("ioctl: nbuf");
    return(-1);
  }
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if(strioctl (s, SBIOCSTIME, INFTIM, sizeof(timeout), (char *)&timeout) < 0){
    perror("ioctl: timeout");
    return(-1);
  }
  
  if(strioctl (s,SBIOCSCHUNK,INFTIM,sizeof(chunksize),(char *)&chunksize) < 0){
    perror("ioctl: chunk size");
    return(-1);
  }
  return(0);
}
#endif

/*
 * establish protocol filter
 *
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
  struct packetfilt pf;
  register u_short *fwp = pf.Pf_Filter;

#define s_offset(structp, element) (&(((structp)0)->element))
  offset = ((int)s_offset(struct ether_header *, ether_type))/sizeof(u_short);
#ifdef PHASE2
  offset += 4;	/* shorts: 2 bytes length + 6 bytes of 802.2 and SNAP */
#endif PHASE2
  *fwp++ = ENF_PUSHWORD + offset;
  *fwp++ = ENF_PUSHLIT | (sock >= 0 ? ENF_CAND : ENF_EQ);
  *fwp++ = htons(prot);
  pf.Pf_FilterLen = 3;
  if (sock >= 0) {
#ifdef PHASE2
    *fwp++ = ENF_PUSHWORD + offset + 6;
    *fwp++ = ENF_PUSHLIT | ENF_AND;
    *fwp++ = htons(0xff00);   /* now have dest socket */
    *fwp++ = ENF_PUSHLIT | ENF_COR;
    *fwp++ = htons((sock & 0xff) << 8);
/* if not wanted, fail it */
    *fwp++ = ENF_PUSHLIT ;
    *fwp++ = 0;
    pf.Pf_FilterLen += 7;
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
    pf.Pf_FilterLen += 20;
#endif PHASE2
  }    

  if (ioctl(s, I_PUSH, "pfmod") < 0) {
    perror("ioctl: push protocol filter");
    return(-1);
  }
  if(strioctl (s, PFIOCSETF, 10, sizeof(pf), (char *)&pf) < 0) {
    perror("ioctl: protocol filter");
    return(-1);
  }
  return(0);
}

private u_char my_eaddr [EHRD];
private int my_eaddr_valid = 0;

export int
pi_get_ethernet_address(edx,ea)
int edx;
u_char *ea;
{
	char	buffer[120];
  	struct ephandle *eph;
	
  	if (edx < 1 || edx > MAXOPENPROT || !ephlist[edx-1].inuse)
    		return(-1);

	if (!my_eaddr_valid) { /* take it once */
		eph = &ephlist[edx-1];
		if((GetEthernetAddress(buffer,eph->fd)) < 0)
			return(-1);

		my_eaddr_valid = 1;
		memcpy(my_eaddr, buffer, EHRD);
	}
	if (ea) memcpy(ea, buffer, EHRD);	
	return(1);
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
  if ((cc=stream_readv(eph->fd, iov, iovlen)) < 0)
     perror ("pi_readv");
  return (cc);
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
  if ((cc = stream_readv(fd, iov, 3)) < 0) {
#else  PHASE2
  if ((cc = stream_readv(fd, iov, 2)) < 0) {
#endif PHASE2
    perror("pi_reada");
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

private u_char buf[2048];

export int
pi_writev(edx, iov, iovlen, eaddr)
int edx;
struct iovec *iov;
int iovlen;
unsigned char eaddr[6];
{
  int ret,len;
  struct ephandle *eph;
  struct strbuf dbuf;
  u_char *bufp;
  struct ether_header *eh;

  if (edx < 1 || edx > MAXOPENPROT || eaddr == NULL)
    return(-1);
  eph = &ephlist[edx-1];
  if (!eph->inuse)
    return(-1);

  if (!my_eaddr_valid) pi_get_ethernet_address (edx,NULL);

  eh   = (struct ether_header *)buf;
  bufp = buf + sizeof (struct ether_header);
  memcpy (&eh->ether_dhost, eaddr,    sizeof(eh->ether_dhost));
  memcpy (&eh->ether_shost, my_eaddr, sizeof(eh->ether_shost));
#ifndef PHASE2
  eh->ether_type = htons(eph->protocol);
#else   PHASE2
  /*
   * Fill in the remainder of the 802.2 and SNAP header bytes.
   */
  *bufp++ = 0xaa;		/* destination SAP	*/
  *bufp++ = 0xaa;		/* source SAP		*/
  *bufp++ = 0x03;		/* control byte		*/
  *bufp++ = (eph->protocol == 0x809b) ? 0x08 : 0x00;
  *bufp++ = 0x00;		/* always zero		*/
  *bufp++ = (eph->protocol == 0x809b) ? 0x07 : 0x00;
  *bufp++ = (eph->protocol >> 8) & 0xff;
  *bufp++ = (eph->protocol & 0xff);
#endif PHASE2
  /* assemble a packet */
  for (ret=0,len=bufp-buf;iovlen;iovlen--,iov++)
      if (iov->iov_base && iov->iov_len >= 0) {
         memcpy(bufp, iov->iov_base, iov->iov_len);
         bufp += iov->iov_len;
         len  += iov->iov_len;
         ret  += iov->iov_len;
      }
#ifdef PHASE2
  eh->ether_type = htons(ret+8); /* see below */
#endif
  dbuf.len = len;
  dbuf.buf = (caddr_t)buf;

  if (putmsg(eph->fd, NULL, &dbuf, 0) < 0) {
    return(-1);
  }
  return(ret);
}

export int
pi_write(edx, buf, buflen, eaddr)
int edx;
caddr_t buf;
int buflen;
char *eaddr;
{
  struct iovec iov;

  iov.iov_base = buf;
  iov.iov_len  = buflen;
#ifdef PHASE2
  iov.iov_base += 8; /* what a bad design */
  iov.iov_len  -= 8; /* but not my fault anyway */
#endif PHASE2
  return (pi_writev (edx,&iov,1,eaddr));
}

/* handy functions */

private int
stream_readv (fd, iov, iovlen)
int fd;
struct iovec *iov;
int iovlen;
{
  int cc,flag=0,left,bytes;
  char *bufp,buf [2048];
  struct strbuf dat_ctl;

  dat_ctl.maxlen = sizeof (buf);
  dat_ctl.buf    = buf;
  if ((cc=getmsg (fd,NULL,&dat_ctl,&flag)) >= 0)
     for (cc=0,bufp=dat_ctl.buf,left=dat_ctl.len;
              iovlen && left>0;iovlen--,iov++)
     {   bytes = (iov->iov_len<left) ? iov->iov_len : left;
         memcpy (iov->iov_base,bufp,bytes);
         cc   += bytes; /* bytes read */
         bufp += bytes; 
         left -= bytes; /* bytes left */
     }
  return(cc);
}

strioctl(fd, cmd, timout, len, dp)
int     fd;
int     cmd;
int     timout;
int     len;
char    *dp;
{
        struct  strioctl  sioc;
        int     rc;

        sioc.ic_cmd = cmd;
        sioc.ic_timout = timout;
        sioc.ic_len = len;
        sioc.ic_dp = dp;
        rc = ioctl (fd, I_STR, &sioc);

        if (rc < 0)
                return (rc);
        else
                return (sioc.ic_len);
}

/*
 * DLPI Support Routines
 *
 */

Acknoledge (dlp_p,ack,msg)
union DL_primitives *dlp_p;
int ack;
char *msg;
{
        if (dlp_p->dl_primitive != ack) {
		fprintf(stderr,"dlpi: %s is nacked.\n",msg);
		if (dlp_p->dl_primitive == DL_ERROR_ACK)
			fprintf(stderr,	"dlpi: dlpi_errno %d\n"
					"dlpi: unix_errno %d\n",
			dlp_p->error_ack.dl_errno,
			dlp_p->error_ack.dl_unix_errno);
	   	else
			fprintf(stderr,"dlpi: spiritual primitive %d.\n",
			dlp_p->dl_primitive);
		return(0);
	}
	return(1);
}

AttachDevice(fd,devno)
int fd,devno;
{
  	int retval;
  	int flags = RS_HIPRI;
  	struct strbuf ctlbuf;
  	union DL_primitives rcvbuf;
  	dl_attach_req_t Request;


  	/* bind to underlying interface */
  	Request.dl_primitive 	= DL_ATTACH_REQ;
  	Request.dl_ppa 		= devno;
  	ctlbuf.len 		= sizeof(Request);
  	ctlbuf.buf 		= (caddr_t)&Request;

  	if (putmsg(fd, &ctlbuf ,NULL,0)  < 0) {
    		perror("Attach Device:");
    		return(0);
  	}

  	ctlbuf.maxlen = sizeof(union DL_primitives);
  	ctlbuf.len = 0;
  	ctlbuf.buf = (char *)&rcvbuf;
  	if ((retval = getmsg(fd, &ctlbuf ,NULL, &flags))  < 0) {
    		perror("Attach Device ack!");
    		return(0);
  	}

	return (Acknoledge(&rcvbuf,DL_OK_ACK,"DL_ATTACH_REQ"));
}

BindProtocol(fd,sap,max_conind,service_mode, conn_mgmt, xidtest_flg )
int	fd,sap,max_conind,service_mode, conn_mgmt, xidtest_flg ;
{
  	int retval;
  	int flags = RS_HIPRI;
  	struct strbuf ctlbuf;
  	union DL_primitives rcvbuf;
  	dl_bind_req_t   BindRequest;


  	BindRequest.dl_primitive 		= DL_BIND_REQ;
  	BindRequest.dl_sap       		= sap;
  	BindRequest.dl_max_conind       	= max_conind;
  	BindRequest.dl_service_mode       	= service_mode;
  	BindRequest.dl_conn_mgmt       		= conn_mgmt;
  	BindRequest.dl_xidtest_flg       	= xidtest_flg;
	
  	ctlbuf.len = sizeof(BindRequest);
  	ctlbuf.buf = (caddr_t)&BindRequest;
	
  	if (putmsg(fd, &ctlbuf ,NULL,0)  < 0) {
    		perror("Bind Protocol:");
    		return(0);
  	}

  	ctlbuf.maxlen = sizeof(union DL_primitives);
  	ctlbuf.len = 0;
  	ctlbuf.buf = (char *)&rcvbuf;
  	if ((retval = getmsg(fd, &ctlbuf ,NULL, &flags))  < 0) {
    		perror("Bind Protocol ACK!");
    		return(0);
  	}

	return (Acknoledge(&rcvbuf,DL_BIND_ACK,"DL_BIND_REQ"));
}

PromMode(fd,level)
int	fd,level;
{
  	int retval;
  	int flags = RS_HIPRI;
  	struct strbuf ctlbuf;
  	union DL_primitives rcvbuf;
	dl_promiscon_req_t PromRequest;


  	PromRequest.dl_primitive 		= DL_PROMISCON_REQ;
  	PromRequest.dl_level 			= level;

        ctlbuf.len = sizeof(PromRequest);
        ctlbuf.buf = (caddr_t)&PromRequest;

        if (putmsg(fd, &ctlbuf ,NULL,0)  < 0) {
                perror("Prom Mode:");
                return(0);
        }

        ctlbuf.maxlen = sizeof(union DL_primitives);
        ctlbuf.len = 0;
        ctlbuf.buf = (char *)&rcvbuf;
        if ((retval = getmsg(fd, &ctlbuf ,NULL, &flags))  < 0) {
                perror("Prom Mode ack!");
                return(0);
        }

	return (Acknoledge(&rcvbuf,DL_OK_ACK,"DL_PROMISCON_REQ"));
}

GetEthernetAddress(EtherBuf,fd)
u_char *EtherBuf;
int  fd;
{
  	int retval;
  	int flags = RS_HIPRI;
  	char buf[80];
  	union DL_primitives rcvbuf;
  	dl_phys_addr_req_t PRequest;
  	struct strbuf ctlbuf;


  	PRequest.dl_primitive  = DL_PHYS_ADDR_REQ;
  	PRequest.dl_addr_type  = DL_CURR_PHYS_ADDR;
  	ctlbuf.len = sizeof(PRequest);
  	ctlbuf.buf = (caddr_t)&PRequest;

  	if (putmsg(fd, &ctlbuf ,NULL,0)  < 0) 
	{
    		perror("Ethernet Address:");
    		return(-1);
  	}

  	ctlbuf.maxlen = sizeof(union DL_primitives);
  	ctlbuf.len = 0;
  	ctlbuf.buf = (char *)&rcvbuf;
  	if ((retval = getmsg(fd, &ctlbuf ,NULL, &flags))  < 0) 
	{
    		perror("Ethernet Address ack!");
    		return(-1);
  	}

	if (Acknoledge(&rcvbuf,DL_PHYS_ADDR_ACK,"DL_PHYS_ADDR_REQ")) {
		memcpy(	EtherBuf,
			&ctlbuf.buf[rcvbuf.physaddr_ack.dl_addr_offset],
			EHRD);
		return(1);
	}
	return(0);
}

int
AddMultiAddress(multi,fd)
u_char *multi;
int fd;
{
  	int retval;
  	int flags = RS_HIPRI;
  	u_char buf[512];
  	union DL_primitives rcvbuf;
  	struct strbuf databuf;
  	struct strbuf ctlbuf;
  	dl_enabmulti_req_t *MultiRequest = (dl_enabmulti_req_t *)buf;

	
  	MultiRequest->dl_primitive 	= DL_ENABMULTI_REQ;
  	MultiRequest->dl_addr_length	= EHRD;
  	MultiRequest->dl_addr_offset	= DL_ENABMULTI_REQ_SIZE;

	memcpy(&buf[DL_ENABMULTI_REQ_SIZE],multi,EHRD);
	
  	ctlbuf.maxlen 			= 0;
  	ctlbuf.len 			= DL_ENABMULTI_REQ_SIZE + EHRD;
  	ctlbuf.buf 			= (caddr_t)buf;
	
  	if ((retval =  putmsg(fd, &ctlbuf ,NULL, flags))  < 0) {
    		perror("bogus2");
    		return(0);
  	}


  	ctlbuf.maxlen = sizeof(rcvbuf);
  	ctlbuf.len = 0;
  	ctlbuf.buf = (char *)&rcvbuf;

  	databuf.maxlen = 512;
  	databuf.len    = 0;
  	databuf.buf    = (char *)buf;

  	if ((retval = getmsg(fd, &ctlbuf, &databuf, &flags))  < 0) {
    		perror("bogus!");
    		return(0);
  	}

	return (Acknoledge(&rcvbuf,DL_OK_ACK,"DL_ENABMULTI_REQ"));
}

#ifdef PHASE2
/*
 * return pointer to zone multicast address
 *
 */

private u_char *
zone_mcast(znam, zlen)
u_char *znam;
short zlen;
{
	int i;
	u_char zone[34];
	u_short chkSum();
	static u_char zmcaddr[7] = {0x09,0x00,0x07,0x00,0x00,0x00};

	if (zlen > sizeof(zone))
	  return(NULL);

	for (i = 0; i < (int)zlen; i++)
	  zone[i] = (u_char)zip_toupper(znam[i]);

	zmcaddr[5] =  (u_char)(chkSum(zone, zlen) % 253);

	return(zmcaddr);
}

/*
 * DDP checksum calculator
 *
 */

private 
u_short chkSum(pkt, len)
u_char *pkt;
int len;
{
	int sum = 0;

	while (len-- > 0) {
          sum = ((sum&0xffff)+*pkt++) << 1;
          if (sum&0x10000)
            sum++;
	}
	sum &= 0xffff;

	return((sum == 0) ? 0xffff : sum);
}

/*
 * convert lowercase to uppercase
 *
 */

private int
zip_toupper(c)
int c;
{
	if (!(c & 0x80))
	  return(toupper(c));

	switch (c) {
	  case 0x88:
	    return(0xcb);
	    break;
	  case 0x8a:
	    return(0x80);
	    break;
	  case 0x8b:
	    return(0xcc);
	    break;
	  case 0x8c:
	    return(0x81);
	    break;
	  case 0x8d:
	    return(0x82);
	    break;
	  case 0x8e:
	    return(0x83);
	    break;
	  case 0x96:
	    return(0x84);
	    break;
	  case 0x9a:
	    return(0x85);
	    break;
	  case 0x9b:
	    return(0xcd);
	    break;
	  case 0x9f:
	    return(0x86);
	    break;
	  case 0xbe:
	    return(0xae);
	    break;
	  case 0xbf:
	    return(0xaf);
	    break;
	  case 0xcf:
	    return(0xce);
	    break;
	}
	return(c);
}

#endif PHASE2
