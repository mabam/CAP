/*
 * $Date: 1993/08/05 15:53:56 $
 * $Header: /mac/src/cap60/contrib/AsyncATalk/RCS/async.c,v 2.2 1993/08/05 15:53:56 djh Rel djh $
 * $Log: async.c,v $
 * Revision 2.2  1993/08/05  15:53:56  djh
 * change wait handling
 *
 * Revision 2.1  91/02/15  21:20:58  djh
 * CAP 6.0 Initial Revision
 * 
 * Revision 1.6  91/01/09  02:30:45  djh
 * Back out of asynchronous NBP register to avoid core dumps ... :-(
 * 
 * Revision 1.5  91/01/09  01:52:08  djh
 * Add XON/XOFF packets for AsyncTerm DA, do NBP asynchronously
 * to speedup the startup of the Chooser.
 * 
 * Revision 1.4  91/01/05  02:25:47  djh
 * General cleanup and additions for use with UAB and CAP 6.0
 * 
 * Revision 1.3  90/07/10  18:16:19  djh
 * Add support for AsyncTerm DA for simultaneous logins
 * Alter speedTab entries for 19200 and 38400
 * 
 * Revision 1.2  90/03/20  16:10:29  djh
 * Fix stupid bug in speedTab contents.
 * 
 * Revision 1.1  90/03/17  22:05:27  djh
 * Initial revision
 * 
 *
 * async.c 
 *
 *	Asynchronous AppleTalk for CAP UNIX boxes.
 *	David Hornsby, djh@munnari.oz, August 1988
 *	Department of Computer Science.
 *	The University of Melbourne, Parkville, 3052
 *	Victoria, Australia.
 *
 * Copyright 1988, The University of Melbourne.
 *
 * You may use, modify and redistribute BUT NOT SELL this package provided
 * that all changes/bug fixes are returned to the author for inclusion.
 *
 *	
 *	Ref: Async Appletalk, Dr. Dobbs Journal, October 1987, p18.
 *	Original Async AppleTalk by:
 *
 *		Rich Brown
 *		Manager of Special Products
 *		Dartmouth College
 *		Kiewit Computer Center
 *		Hanover, New Hampshire 03755
 *		603/646-3648
 *
 *	CAP by:
 *		Charlie Kim
 *		Academic Computing and Communications Group
 *		Centre for Computing Activities
 *		Columbia University
 *
 *
 *	If BROAD is defined, SINGLE USE is possible without running the
 *	associated asyncad daemon. (NB: async must then be setuid root).
 *
 *	if SECURE is defined, the AsyncTerm DA connects directly to a
 *	shell (/bin/csh), otherwise it just uses /bin/login.
 *
 *	WARNING!
 *		If your C compiler is brain-damaged and pads structures
 *		strangely, then this code may not work.
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <signal.h>
#include <sgtty.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netat/appletalk.h>
#include "async.h"

struct aalap_hdr rx_aalap;	/* AALAP receive buffer (WRT the Mac)	*/
struct aalap_hdr tx_aalap;	/* AALAP xmit buffer (WRT the Mac)	*/
struct aalap_hdr rtx_aalap;	/* RTMP AALAP xmit buffer (WRT the Mac)	*/
struct sockaddr_in sin;		/* our UDP socket			*/
struct sockaddr_in sin_broad;	/* our UDP socket for DDP broadcasts	*/
struct sgttyb sgttyb;		/* terminal settings from ioctl()	*/
struct timeval timeout;		/* select() timeout			*/
struct stat buf;		/* for biff and mesg use		*/

/* various	*/

int s;				/* a socket for UDP packets		*/
#ifdef BROAD
int s_broad;			/* a socket for DDP broadcasts (via UDP)*/
#endif BROAD
int dstSkt;			/* dynamic socket used at other end	*/
int skt;			/* a DDP socket for NBP			*/
int fd;				/* file desc. for control terminal	*/
int nfds;			/* for select call			*/
int p;				/* our pseudo TTY file descriptor	*/
int t;				/* our "real" TTY file descriptor	*/
int closing;			/* 2 CR's or NL's outside a frame	*/

/* Booleans	*/

int alarmed;			/* need to send an RTMP packet		*/
int inFrame;			/* currently within a received frame	*/
int maskIt;			/* next char was escaped in input	*/
int linkUp;			/* we have established a link		*/
int serOpen;			/* shell has been started		*/
int serWait;			/* character buffer other end full	*/
int debug;			/* hmmmmm ...				*/

/* Network info	*/

u_char ourNode;			/* 1 <= ourNode <= MAXNODE		*/
unsigned short ourNet;		/* one net per CAP host	(atalk.local)	*/
char ourZone[34];		/* our local zone (atalk.local)		*/
u_char bridgeNode;		/* local Multigate KIP node number	*/
unsigned short bridgeNet;	/* local Multigate KIP AppleTalk net #	*/
unsigned long bridgeIP;		/* local Multigate KIP IP address	*/

/* more various	*/

u_char lastChar;		/* the last char read on input		*/
char nbpObj[34];		/* the object we wish to register	*/
char *getlogin();
EntityName en;

/* externs from modified CAP routine openatalkdb()	*/
/* these are all in correct network byte order		*/

extern char async_zone[];
extern u_short async_net;
extern u_char  bridge_node;
extern u_short bridge_net;
extern struct in_addr bridge_addr;

FILE *fopen(), *dbg;		/* log file				*/
long timeForRTMP, now;

unsigned short crctbl[16] = {	/* Async AppleTalk CRC lookup table	*/

	0x0000,	0xcc01,	0xd801,	0x1400,
	0xf001,	0x3c00,	0x2800,	0xe401,
	0xa001,	0x6c00,	0x7800,	0xb401,
	0x5000,	0x9c01,	0x8801,	0x4400
};

short speedTab[16] = {		/* Bytes/sec for various sgtty speeds	*/

	30,	30,	30,	30,
	30,	30,	30,	30,
	60,	120,	180,	240,
	480,	960,	1920,	3840
};

main(argc, argv)
int argc;
char *argv[];
{
	char *t;
	long len;
	fd_set readfds;
	int i, cleanup();

	timeForRTMP = now = serWait = 0;
	debug = inFrame = maskIt = linkUp = serOpen = alarmed = FALSE;

	while(--argc > 0 && (*++argv)[0] == '-')
		for(t = argv[0]+1 ; *t != '\0' ; t++)
			switch (*t) {
			case 'd':
				debug = TRUE;
				break;
			}

	if(debug) dbg = fopen("asyncLog", "w");

	readatalkdb("/etc/atalk.local");

	/* select() timeout	*/
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	signal(SIGHUP, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGTERM, cleanup);

	/* setup the CAP stuff for NBP					*/
	abInit(0);
	nbpInit();

	/* set terminal line to 8 bits, no parity, 1 stop, raw mode	*/
	if(ioctl((fd=1),TIOCGETP,&sgttyb) != -1) {
		sgttyb.sg_flags |= RAW; sgttyb.sg_flags &= ~ECHO;
		ioctl(fd, TIOCSETP, &sgttyb);
		sgttyb.sg_flags &= ~RAW; sgttyb.sg_flags |= ECHO;
	}

	/* turn off 'biff' mail notification and 'mesg' delivery	*/
	if(fstat(fd, &buf) == 0) {
		buf.st_mode &= ~0122;		/* go-w, u-x	*/
		fchmod(fd, buf.st_mode);
		buf.st_mode |= 0122;		/* go+w, u+x	*/
	}

	if((s = socket(AF_INET, SOCK_DGRAM, 0, 0)) < 0) {
		fprintf(stderr, "Couldn't open UDP socket\n");
		cleanup();
	}

	len = 6 * 1024; /* should be enough ?? */
	if(setsockopt(s,SOL_SOCKET,SO_RCVBUF,(char *)&len,sizeof(long))!=0){
		fprintf(stderr, "Couldn't set socket options\n");
		cleanup();
	}

#ifdef BROAD

	if((s_broad = socket(AF_INET, SOCK_DGRAM, 0, 0)) < 0) {
		fprintf(stderr, "Couldn't open broadcast UDP socket\n");
		cleanup();
	}
	sin_broad.sin_port = htons(aaBroad);
	if(bind(s_broad, (caddr_t) &sin_broad, sizeof(sin_broad), 0) < 0) {
		fprintf(stderr, "Couldn't bind broadcast UDP port\n");
		cleanup();
	}
#endif BROAD

	skt = 0;
	if(DDPOpenSocket(&skt, 0L) != noErr) {
		fprintf(stderr, "Couldn't open a DDP socket\n");
		cleanup();
	}

	nfds = 16;

	if(debug) write(1, "Debug ON.\r\n", 11);

	/* announce success	*/
	write(1, "Connected ...\r\n", 15);
	sleep(1);

	/* send something innocuous to make the login window go away	*/
	write(1, GOAWAY, 4);	
	sleep(1);

	time(&now);
	timeForRTMP = now + RTMPTIME;

	for( ; ; ) {
		if(debug) fflush(dbg);
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(s, &readfds);
#ifdef BROAD
		FD_SET(s_broad, &readfds);
#endif BROAD
		if(serOpen)
			FD_SET(p, &readfds);

		if(select(nfds, &readfds, (fd_set *) 0, (fd_set *) 0, 
		    (struct timeval *) &timeout) > 0) {
			if(FD_ISSET(0, &readfds))
				doAALAPRead(&rx_aalap);
			if(FD_ISSET(s, &readfds))
				doDDPRead(&tx_aalap);
#ifdef BROAD
			if(FD_ISSET(s_broad, &readfds))
				doDDPRead_broad(&tx_aalap);
#endif BROAD
			if(serOpen && !serWait && FD_ISSET(p, &readfds))
				doShellRead(&tx_aalap);
		}
		if(linkUp) {
			time(&now);
			if(now > timeForRTMP) {
				sendRTMP();
				timeForRTMP = now + RTMPTIME;
			}
		}
	}
}

/*
 * send a UR frame back to the Mac
 * (rx_aalap has the corresponding IM frame)
 *
 */

sendUR(rx_aalap)
struct aalap_hdr *rx_aalap;
{
	struct aalap_imur *imur;
	unsigned short checkNet();
	u_char checkNode();

	if(rx_aalap->aalap_datalen != 3)
		return;		/* drop it */

	/* ensure the frame has the right stuff	*/

	imur = (struct aalap_imur *) rx_aalap->aalap_data;
	rx_aalap->aalap_frame = (u_char) FrameChar;
	rx_aalap->aalap_datalen = 3;
	imur->aalap_net = htons(checkNet(imur->aalap_net));
	imur->aalap_node = checkNode(imur->aalap_node);	

	if(debug) {
		fprintf(dbg, "Received IM/Sent UR\n");
		fprintf(dbg, "Suggested Node = %02x, Net = %04x\n",
			imur->aalap_node, ntohs(imur->aalap_net));
	}

	doAALAPWrite(rx_aalap, URType);	/* does checksum & trailing frame */
}

/*
 * receive a UR frame and check it ...
 *
 */

recvUR(rx_aalap)
struct aalap_hdr *rx_aalap;
{
	struct aalap_imur *imur;

	if(rx_aalap->aalap_datalen != 3)
		return;		/* drop it */

	imur = (struct aalap_imur *) rx_aalap->aalap_data;

	if(debug) {
		fprintf(dbg, "Received UR\n");
		fprintf(dbg, 
		"ourNode=%02x, theirNode=%02x, ourNet=%04x, theirNet=%04x\n",
		ourNode, imur->aalap_node, ourNet, ntohs(imur->aalap_net));
	}
	if(ourNet == ntohs(imur->aalap_net) && ourNode == imur->aalap_node)
		linkUp = TRUE;
	else
		if(ourNode == 0) {
			if(checkNode(imur->aalap_node) == imur->aalap_node)
				sendIM(rx_aalap);
		} else
			if(imur->aalap_node == ourNode)
				sendIM(rx_aalap);
}

/*
 * send an IM frame to the Mac
 *
 */

sendIM(tx_aalap)
struct aalap_hdr *tx_aalap;
{
	struct aalap_imur *imur;
	unsigned short net;

	if(debug) fprintf(dbg, "Sending IM\n");

	/* ensure the frame has the right stuff	*/

	tx_aalap->aalap_frame = (u_char) FrameChar;
	tx_aalap->aalap_datalen = 3;
	imur = (struct aalap_imur *) tx_aalap->aalap_data;
	imur->aalap_net = htons(ourNet);
	imur->aalap_node = ourNode;
	doAALAPWrite(tx_aalap, IMType);	/* does checksum & trailing frame */
}

/*
 * we have to process a short DDP packet which could be:
 *	an RTMP request: (dst socket=1, DDP type=5, Command=1)
 *		respond locally
 *	an NBP lookup: (dst socket=2, DDP type = 2)
 *		repackage as long DDP and forward to gateway
 *	a ZIP query: (dst socket=6, DDP type=6)
 *		not implemented (yet)
 *	a ZIP/ATP Zone query: (dst socket=6, DDP type=3)
 *		GZL: repackage as long DDP and forward to gateway, watch 
 *		for the	return packet and repackage it as a short DDP
 *		GMZ: respond locally
 *
 */

processShort(rx_aalap)
struct aalap_hdr *rx_aalap;
{
	char *q;
	int reqType;
	unsigned short len, newlen, getMyZone();
	struct sddp *sddp, *sddptx;
	struct atp *atp, *atptx;
	struct ddp *ddptx;

	sddp = (struct sddp *) rx_aalap->aalap_data;
	switch (sddp->ddp_type) {
	case DDP_RTMP_R: /* process locally	*/
		q = (char *) sddp->ddp_data;
		if(sddp->ddp_dstSkt == RTMP_SKT && *q == 1/* command */) {
			linkUp = TRUE;
			sendRTMP();
		}
		break;
	case DDP_NBP:
		if(sddp->ddp_dstSkt == NBP_SKT) {
			len = (ntohs(sddp->hop_and_length)&0x03ff) + 8;
			tx_aalap.aalap_frame = (u_char) FrameChar;
			tx_aalap.aalap_type = DDPType;
			tx_aalap.aalap_datalen = len;
			ddptx = (struct ddp *) tx_aalap.aalap_data;
			ddptx->hop_and_length = htons(len);
			ddptx->ddp_checksum = 0;
			ddptx->ddp_dstNet = htons(bridgeNet);
			ddptx->ddp_srcNet = htons(ourNet);
			ddptx->ddp_dstNode = bridgeNode;
			ddptx->ddp_srcNode = ourNode;
			ddptx->ddp_dstSkt = sddp->ddp_dstSkt;
			ddptx->ddp_srcSkt = sddp->ddp_srcSkt;
			ddptx->ddp_type = sddp->ddp_type;
			if((newlen = len - 13) < 0)
				newlen = 0;
			bcopy(sddp->ddp_data,ddptx->ddp_data,newlen);
			doDDPWrite(&tx_aalap);
		}
		break;
	case DDP_ATP:
		if(sddp->ddp_dstSkt == ZIP_SKT) { /* ZIP GMZ/GZL req */
			atp = (struct atp *) sddp->ddp_data;
			reqType = atp->atp_user1;
			switch (reqType) {
			case ATP_GMZ:	/* GetMyZone	*/
				tx_aalap.aalap_frame = (u_char) FrameChar;
				tx_aalap.aalap_type = SDDPType;
				sddptx = (struct sddp *) tx_aalap.aalap_data;
				sddptx->ddp_dstSkt = sddp->ddp_srcSkt;
				sddptx->ddp_srcSkt = ZIP_SKT;
				sddptx->ddp_type = DDP_ATP;
				atptx = (struct atp *) sddptx->ddp_data;
				atptx->atp_command = ATP_CMD;
				atptx->atp_bitseq = ATP_SEQ;
				/* bludgeon through alignment problems	*/
				bcopy(&atp->atp_transid,&atptx->atp_transid,2);
				atptx->atp_user1 = 0;
				atptx->atp_user2 = 0;
				atptx->atp_user3 = 0;
				atptx->atp_user4 = 1;	/* one zone name */
				q = (char *) atptx->atp_data;
				len = getMyZone(q) + 13;
				tx_aalap.aalap_datalen = len;
				sddptx->hop_and_length = htons(len);
				doAALAPWrite(&tx_aalap, SDDPType);	
				break;
			case ATP_GZL:	/* GetZoneList	*/
				len = (ntohs(sddp->hop_and_length)&0x03ff) + 8;
				tx_aalap.aalap_frame = (u_char) FrameChar;
				tx_aalap.aalap_type = DDPType;
				tx_aalap.aalap_datalen = len;
				ddptx = (struct ddp *) tx_aalap.aalap_data;
				ddptx->hop_and_length = htons(len);
				ddptx->ddp_checksum = 0;
				ddptx->ddp_dstNet = htons(bridgeNet);
				ddptx->ddp_srcNet = htons(ourNet);
				ddptx->ddp_dstNode = bridgeNode;
				ddptx->ddp_srcNode = ourNode;
				ddptx->ddp_dstSkt = sddp->ddp_dstSkt;
				ddptx->ddp_srcSkt = sddp->ddp_srcSkt;
				ddptx->ddp_type = sddp->ddp_type;
				if((newlen = len - 13) < 0)
					newlen = 0;
				bcopy(sddp->ddp_data,ddptx->ddp_data,newlen);
				doDDPWrite(&tx_aalap);
				break;
			default:
				if(debug)
				 fprintf(dbg,"Illegal request id=%d\n",reqType);
				break;
			}
		}
		break;
	case DDP_ZIP: /* Not yet implemented	*/
		break;
	case SERIALType:/* pass to a shell	*/
		processSerial(rx_aalap);
		break;
	default:
		if(debug)
			fprintf(dbg, "Unknown short DDP = %d\n",sddp->ddp_type);
		break;
	}
}

/*
 * we have had a LAP packet of SERIALType, process it ...
 *
 */

processSerial(rx_aalap)
struct aalap_hdr *rx_aalap;
{
	int startShell();
	struct sddp *sddp;
	short len;

	sddp = (struct sddp *) rx_aalap->aalap_data;
	switch (sddp->ddp_data[0]) {
		case SB_OPEN:
			if(serOpen) {
				write(p, "exit\n", 5);
				close(p);
			}
			serOpen = startShell();
			dstSkt = sddp->ddp_srcSkt;
			break;
		case SB_DATA:
			len = (ntohs(sddp->hop_and_length)&0x03ff) - 6;
			if(serOpen) write(p, sddp->ddp_data+1, len);
			serWait = 0;
			break;
		case SB_CLOSE:
			write(p, "exit\n", 5);
			close(p);
			serOpen = 0;
			break;
		case SB_XOFF:
			serWait = 1;
			break;
		case SB_XON:
			serWait = 0;
			break;
		default:
			break;
	}
}

/*
 * start up a Shell to process bytes in special DDP packets.
 *
 */

int
startShell()
{
	int on = 1;
	int pid, i;
	int funeral();
	char *line;
	char c;

	if(debug) fprintf(dbg, "Opening a Shell ...\n");

	/* find and open a pseudo TTY	*/

	p = -1;
	for(c = 'p' ; c < 's' ; c++) {
		struct stat statb;

		line = "/dev/ptyZZ";
		line[strlen("/dev/pty")] = c;
		line[strlen("/dev/ptyZ")] = '0';
		if(stat(line, &statb) < 0)
			break;
		
		for(i = 0 ; i < 16 ; i++) {
			line[strlen("/dev/ptyZ")] = "0123456789abcdef"[i];
			if((p = open(line, 2)) >= 0)
				break;
		}
		if(p >= 0)
			break;
	}

	if(p < 0) {
		if(debug) fprintf(dbg, "Failed to open pty\n");
		return(0);
	}

	if((pid = fork()) == 0) { /* in child */
		close(p);
		close(0);
		close(1);
		close(2);

		/* dissassociate from our controlling TTY	*/

		if((t = open("/dev/tty", 2)) >= 0) {
			ioctl(t, TIOCNOTTY, 0);
			setpgrp(0, 0);
			close(t);
		} 

		/* open the pseudo TTY	*/

		line[strlen("/dev/")] = 't';
		if((t = open(line, 2)) < 0) {
			if(debug) fprintf(dbg, "Failed to open tty\n");
			exit(1);
		}

		/* set nice modes	*/

		{	struct sgttyb b;

			gtty(t, &b);
			b.sg_flags = ECHO|CRMOD;
			stty(t, &b);
		}

		dup(t);	/* stdout	*/
		dup(t);	/* stderr	*/

#ifndef SECURE
		execl("/bin/login", "login", 0);
#else   SECURE
		execl("/bin/csh", "-asyncsh", 0);
#endif  SECURE

		exit(1);		/* o, oh */
	}

	/* parent */

	if(pid == -1)
		return(0);
	
	ioctl(p, FIONBIO, &on);		/* non blocking	*/

	signal(SIGTSTP, SIG_IGN);
	signal(SIGCHLD, funeral);

	return(1);
}

/*
 * write this out as a UDP encapsulated DDP packet
 *
 */
 
doDDPWrite(rx_aalap)
struct aalap_hdr *rx_aalap;
{
	int length;
	struct ap ap;
	struct ddp *ddp;
	
	ap.ldst = 0xFA;		/* magic	*/
	ap.lsrc = 0xCE;		/* magic	*/
	ap.ltype = DDPType;
	ddp = (struct ddp *) rx_aalap->aalap_data;
	length = rx_aalap->aalap_datalen;
	bcopy(rx_aalap->aalap_data, ap.dd, length);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = bridgeIP;		/* Gateway	*/
	if(bridgeIP == inet_addr("127.0.0.1"))	/* using UAB	*/
	    sin.sin_port = (word) htons(750);
	else
	    sin.sin_port = (word) htons(ddp2ipskt(ddp->ddp_dstSkt));
	length += 3;
	
	if(debug) {
		fprintf(dbg, "DDPWrite: "); 
		dumppacket(rx_aalap, 0);
	}
	if(sin.sin_port == 0) {
		if(debug) fprintf(dbg, "Couldn't find the gateway UDP port\n");
		return;	/* drop it, do nothing	*/
	}
	if(sendto(s, (caddr_t) &ap, length, 0, &sin, sizeof(sin)) != length) {
		if(debug) fprintf(dbg, "Couldn't write to the gateway\n");
		return;	/* drop it, do nothing	*/
	}
}

/*
 * write a frame out the serial port
 *
 */

doAALAPWrite(tx_aalap, type)
struct aalap_hdr *tx_aalap;
u_char type;
{
	struct aalap_trlr *trlr;
	unsigned short doCRC(), crc;
	unsigned char buf[2 * (AALAP + 3)];
	u_char *q;
	int i, j;

	tx_aalap->aalap_frame = FrameChar;
	tx_aalap->aalap_type = type;
	q = (u_char *) tx_aalap->aalap_data;
	trlr = (struct aalap_trlr *) (q + tx_aalap->aalap_datalen);
	crc = doCRC(tx_aalap, (int) tx_aalap->aalap_datalen);
	/* bludgeon through alignment problems	*/
	bcopy(&crc, &trlr->aalap_crc, 2);
	trlr->aalap_frame = FrameChar;

	if(debug) dumppacket(tx_aalap, 0);

	j = 0;
	/* FrameChar and Type		*/
	buf[j++] = tx_aalap->aalap_frame;
	buf[j++] = tx_aalap->aalap_type;

	/* body including CRC		*/
	for(i = 0 ; i < (tx_aalap->aalap_datalen + 2) ; i++, q++) {
		switch (*q) {
			case FrameChar:
			case DLEChar:
			case XONChar1:
			case XONChar2:
			case XOFFChar1:
			case XOFFChar2:
				buf[j++] = DLEChar;
				buf[j++] = *q ^ XOR;
				break;
			default:
				buf[j++] = *q;
				break;
		}
	}
	/* Trailing FrameChar		*/
	buf[j++] = trlr->aalap_frame;

	/* send it			*/
	write(1, buf, j);
}

/*
 * process serial chars from the Mac
 *
 */

doAALAPRead(rx_aalap)
struct aalap_hdr *rx_aalap;
{
	int i, j, len;
	unsigned char buf[AALAP+5];
	unsigned short doCRC();

	len = read(0, buf, AALAP+5);
	for(i = 0 ; i < len ; i++ ) {
		if(buf[i] == lastChar && buf[i] == FrameChar) /* recover */
			inFrame = FALSE;
		lastChar = buf[i];
		if(!inFrame) {
			switch (buf[i]) {
			case FrameChar:
				inFrame = TRUE;
				rx_aalap->aalap_frame = buf[i];
				rx_aalap->aalap_type = (u_char) 0;
				rx_aalap->aalap_framecomplete = FALSE;
				rx_aalap->aalap_datalen = 0;
				closing = 0;
				continue;
				break;
			case CR: 	/* 2 CR's or NL's outside the frame */
			case NL: 	/* closes the async connection. */
			case CRP:	/* CR with parity bit set */
			case NLP:	/* NL with parity bit set */
				if(++closing >= 2)
					cleanup();
				continue;
				break;
			default:
				closing = 0;
				continue;
				break;
			}
		} else { 
			closing = 0;
			/* we are in a valid frame	*/
			switch (buf[i]) {
			case FrameChar: /* end of frame	*/
		    		rx_aalap->aalap_data[rx_aalap->aalap_datalen]
					= buf[i];
		    		rx_aalap->aalap_datalen -= 2; /* CRC */
		    		if((j=doCRC(rx_aalap,rx_aalap->aalap_datalen+2))
				|| rx_aalap->aalap_datalen < 0
		    		|| rx_aalap->aalap_datalen > AALAP) {
		 	    		rx_aalap->aalap_datalen = 0;
					if(debug)fprintf(dbg,"Bogus pkt %s\n",
						(j==0) ? "Length" : "CRC");
		    		} else
			    		rx_aalap->aalap_framecomplete = TRUE;
		    		inFrame = FALSE;
		    		break;
			case DLEChar:	/* an escaped character follows */
		    		maskIt = TRUE;
				continue;
		    		break;
			case XONChar1:
			case XONChar2:
			case XOFFChar1:
			case XOFFChar2:
				if(debug) fprintf(dbg, "Received %s\n",
					(buf[i]&0x7f)==XONChar1?"XON":"XOFF");
				/* not yet implemented	*/
				continue;
		    		break;
			default:
		    		if(rx_aalap->aalap_datalen == 0
		    		&& rx_aalap->aalap_type == 0
		        	&& (buf[i]==IMType 
				 || buf[i]==URType 
				 || buf[i]==SDDPType 
				 || buf[i]==DDPType)) {
			    		rx_aalap->aalap_type = buf[i];
					continue;
			    		break;
		    		}
		    		if(maskIt) {
			    		buf[i] = buf[i] ^ XOR;
					maskIt = FALSE;
		    		}
		    		rx_aalap->aalap_data[rx_aalap->aalap_datalen]
					= buf[i];
		    		rx_aalap->aalap_datalen++;
				continue;
		    		break;
			}
		}
		if(rx_aalap->aalap_framecomplete) {
			if(debug) dumppacket(rx_aalap, 1);
			switch (rx_aalap->aalap_type) {
				case IMType:	/* we're being probed	*/
					sendUR(rx_aalap); /* always send */
					break;
				case URType:
					recvUR(rx_aalap);
					break;
				case SDDPType:	/* a short DDP packet	*/
					processShort(rx_aalap);
					break;
				case DDPType:	/* send it out on DDP	*/
					doDDPWrite(rx_aalap);
					break;
				default:	/* drop it		*/
					if(debug) fprintf(dbg,"Unknown: %02x\n",
						rx_aalap->aalap_type);
					break;
			}
			rx_aalap->aalap_framecomplete = FALSE;
			rx_aalap->aalap_datalen = 0;
		}
	}
}

/*
 * get and process DDP packet destined for the Mac
 * NB: the packets aren't really from the DDP interface
 * 	AppleTalk gateways (EG: Multigate) direct packets
 *	to the UDP port (PortOffset+node#)
 *
 * If the packet is an ATP pkt destined for the ZIP socket,
 * we have to munge the packet into a short DDP to send back
 * to the mac, otherwise it is ignored. Stupid Protocol.
 *
 */

doDDPRead(tx_aalap)
struct aalap_hdr *tx_aalap;
{
	struct ap ap;
	unsigned short junk;
	int zipped;		/* junk	*/

	tx_aalap->aalap_framecomplete = FALSE;
	tx_aalap->aalap_datalen = (short) read(s, (char *) &ap, sizeof(ap));

	if(ap.ldst == 0xFA && ap.lsrc == 0xCE && ap.ltype == 2) {
		if(ap.srcskt == ZIP_SKT && ap.type == DDP_ATP) {
			tx_aalap->aalap_datalen -= 11;
			if(tx_aalap->aalap_datalen < 0)
				tx_aalap->aalap_datalen = 0;
			bcopy(ap.dd+10, tx_aalap->aalap_data+2, 
				(int) tx_aalap->aalap_datalen);
			/* bludgeon through alignment problems	*/
			bcopy(ap.dd, &junk, 2);
			junk = ntohs(junk) - 8;
			junk = htons(junk);
			bcopy(&junk, tx_aalap->aalap_data, 2);
			zipped = TRUE;
		} else {
			tx_aalap->aalap_datalen -= 3;
			if(tx_aalap->aalap_datalen < 0)
				tx_aalap->aalap_datalen = 0;
			bcopy(ap.dd, tx_aalap->aalap_data, 
				(int) tx_aalap->aalap_datalen);
			zipped = FALSE;
		}
		tx_aalap->aalap_framecomplete = TRUE;
	} else
		tx_aalap->aalap_datalen = 0;

	if(debug) {
		fprintf(dbg,"DDPRead: "); 
		dumppacket(tx_aalap, 1);
	}

	if(tx_aalap->aalap_framecomplete) {
		if(shouldSendRTMP(tx_aalap->aalap_datalen))
			sendRTMP();
		doAALAPWrite(tx_aalap, (zipped) ? SDDPType : DDPType);
		tx_aalap->aalap_framecomplete = FALSE;
		tx_aalap->aalap_datalen = 0;
	}
}

#ifdef BROAD

doDDPRead_broad(tx_aalap)
struct aalap_hdr *tx_aalap;
{
	struct ap ap;

	tx_aalap->aalap_framecomplete = FALSE;
	tx_aalap->aalap_datalen = read(s_broad, (char *) &ap, sizeof(ap));
	if(ap.ldst == 0xFA && ap.lsrc == 0xCE && ap.ltype == 2) {
		tx_aalap->aalap_datalen -= 3;
		if(tx_aalap->aalap_datalen < 0)
			tx_aalap->aalap_datalen = 0;
		bcopy(ap.dd, tx_aalap->aalap_data, tx_aalap->aalap_datalen);
		tx_aalap->aalap_framecomplete = TRUE;
	} else
		tx_aalap->aalap_datalen = 0;

	if(debug) {
		fprintf(dbg,"DDPRead (broad): "); 
		dumppacket(tx_aalap, 1);
	}

	if(tx_aalap->aalap_framecomplete) {
		if(shouldSendRTMP(tx_aalap->aalap_datalen))
			sendRTMP();
		doAALAPWrite(tx_aalap, DDPType);
		tx_aalap->aalap_framecomplete = FALSE;
		tx_aalap->aalap_datalen = 0;
	}
}

#endif  BROAD

/*
 * read from the shell we have forked, send it to our user
 *
 */

doShellRead(tx_aalap)
struct aalap_hdr *tx_aalap;
{
	short len;
	struct sddp *sddp;

	sddp = (struct sddp *) tx_aalap->aalap_data;
	tx_aalap->aalap_framecomplete = TRUE;

	if((len = read(p, (char *)sddp->ddp_data+1, 512)) <= 0)
		return(0);

	len += 6;
	tx_aalap->aalap_datalen = len;
	sddp->hop_and_length = htons(len);
	sddp->ddp_dstSkt = dstSkt;
	sddp->ddp_srcSkt = SERIALType;
	sddp->ddp_type = SERIALType;
	sddp->ddp_data[0] = SB_DATA;

	if(debug) {
		fprintf(dbg,"ShellRead: "); 
		dumppacket(tx_aalap, 1);
	}

	if(tx_aalap->aalap_framecomplete) {
		if(shouldSendRTMP(tx_aalap->aalap_datalen))
			sendRTMP();
		doAALAPWrite(tx_aalap, SDDPType);
		tx_aalap->aalap_framecomplete = FALSE;
		tx_aalap->aalap_datalen = 0;
	}
}

/*
 * if we are about to send a packet, check to see
 * if the time to send it will make our next RTMP
 * late (Yes, this is really necessary !).
 *
 */

int
shouldSendRTMP(len)
short len;
{
	short i, ptime;

	i = sgttyb.sg_ospeed;
	if(i >= 0 && i < sizeof(speedTab)/sizeof(short)) {
		ptime = (len/speedTab[i]) + 1;
		time(&now);
		if(now+ptime > timeForRTMP) {
			timeForRTMP = now + RTMPTIME;
			return(1);
		}
	}
	return(0);
}

/*
 * Try to accomodate their wishes for a node number
 *	(this might be a re-connect)
 *
 */

u_char 
checkNode(node)
u_char node;
{
	if(ourNode != node) {
		sin.sin_port = htons(PortOffset + node);
		if(bind(s, (caddr_t) &sin, sizeof(sin), 0) < 0) {
			if(debug) fprintf(dbg, 
				"Requested node number %x in use\n", node);
			return(ourNode);
		}

		if(debug) fprintf(dbg, "Mac requested node number %x\n", node);

		/* requested node number is OK with me		*/
		ourNode = node;
#ifndef BROAD
		notify(UP);	/* tell asyncad daemon we are here	*/
#endif  BROAD
		sprintf(nbpObj, "%s#%d", getlogin(), ourNode & 0xff);
		strncpy(en.objStr.s, nbpObj, sizeof(en.objStr.s));
		strncpy(en.typeStr.s, NBPTYPE, sizeof(en.typeStr.s));
		strncpy(en.zoneStr.s, ZONE, sizeof(en.zoneStr.s));
		nbp_remove(&en); /* if one already running */
		nbp_register(&en, skt); /* synchronous */
	}
	return(ourNode);
}

/*
 * Return our network number no matter what they want
 *
 */

unsigned short 
checkNet(net)
short net;
{
	return(ourNet);
}

/*
 * return number of chars in our zone name (from atalk.local)
 * copy zone name and length byte to q (pascal string)
 *
 */

unsigned short
getMyZone(q)
char *q;
{
	int i;

	i = strlen(ourZone);
	*q = (u_char) i;
	bcopy(ourZone, q+1, i);
	return((unsigned short) i+1);
}

/*
 * read the atalk data file '/etc/atalk.local'
 *
 * for now, save effort by using a slightly modified
 * version of the CAP routine openatalkdb(). If using
 * CAP 6.0 or greater, this is all handled automagically.
 *
 */

readatalkdb(db)
char *db;
{
#ifndef CAP_6_DBM
	openatalkdb(db);	/* CAP routine in atalkdbm.c	*/
#else   CAP_6_DBM
	{ extern short lap_proto;
	  if(lap_proto == LAP_KIP)
	    openatalkdb(db);
	  else
	    openetalkdb(NULL);	/* use the default */
	}
#endif  CAP_6_DBM
	ourNode = 0;
	ourNet = ntohs(async_net);
	strncpy(ourZone, async_zone, strlen(async_zone));
	bridgeNode = bridge_node;
	bridgeNet = ntohs(bridge_net);
	bridgeIP = bridge_addr.s_addr;

	if(debug) {
		fprintf(dbg, "ourNode=%02x, ourNet=%02x, ourZone=%s\n",
			ourNode, ourNet, ourZone);
		fprintf(dbg, "bridgeNode=%02x, bridgeNet=%02x, brIP=%04x\n",
			bridgeNode, bridgeNet, ntohl(bridgeIP));
	}
}

/*
 * send an RTMP packet
 *	1. in response to a request
 *	2. synchronously every ~RTMPTIME seconds to keep the link up
 *
 */

sendRTMP()
{
	struct sddp *sddptx;
	struct rtmp *rtmp;
	unsigned short senderNet;

	rtx_aalap.aalap_frame = (u_char) FrameChar;
	rtx_aalap.aalap_datalen = 9;
	sddptx = (struct sddp *) rtx_aalap.aalap_data;
	sddptx->hop_and_length = htons(9);
	sddptx->ddp_dstSkt = RTMP_SKT;
	sddptx->ddp_srcSkt = RTMP_SKT;
	sddptx->ddp_type = DDP_RTMP_D;
	rtmp = (struct rtmp *) sddptx->ddp_data;
	senderNet = htons(ourNet);
	/* bludgeon through alignment problems	*/
	bcopy(&senderNet, &rtmp->rtmp_senderNet, 2);
	rtmp->rtmp_idLen = NBitID;
	rtmp->rtmp_senderId = bridgeNode;
	doAALAPWrite(&rtx_aalap, SDDPType);	
}


/*
 * Dump the contents of an AALAP packet
 *
 */
dumppacket(ax_aalap, dirn)
struct aalap_hdr *ax_aalap;
int dirn;
{
	struct aalap_trlr *trlr;
	char *q;
	int i, len;

	/* Heading		*/
	fprintf(dbg, "%s: len=0x%02x, pkt=", 
		(dirn) ? "IN" : "\tOUT", ax_aalap->aalap_datalen);

	q = (char *) ax_aalap->aalap_data;
	trlr = (struct aalap_trlr *) (q + ax_aalap->aalap_datalen);

	/* Frame and type	*/
	fprintf(dbg, "%02x %02x ", ax_aalap->aalap_frame, ax_aalap->aalap_type);
#ifdef ALLPACKET
	/* body including CRC	*/
	for(i = 0 ; i < (ax_aalap->aalap_datalen + 2) ; i++, q++)
		fprintf(dbg, "%02x ", (*q & 0xff));
	
	/* trailing frame	*/
	fprintf(dbg, "%02x\n", trlr->aalap_frame);
#else ALLPACKET
	len = ax_aalap->aalap_datalen + 2;
	if(len > 21) len = 21;	/* just enough to cover ATP header */
	for(i = 0 ; i < len ; i++, q++)
		fprintf(dbg, "%02x ", (*q & 0xff));
	if(len == ax_aalap->aalap_datalen+2)
		fprintf(dbg, "%02x\n", trlr->aalap_frame);
	else
		fprintf(dbg, "\n");
#endif ALLPACKET
	fflush(dbg);
}

/*
 * calculate CRC
 *	(if we want to check the CRC, set len to the data length
 *	plus 2 to include the packet CRC, the result should be 0)
 *
 * NB: this algorithm calculates the CRC in network byte order on
 * machines that are not network byte order and vice-versa.
 * Hence the crap at the end.
 *
 */

unsigned short 
doCRC(aalap, len)
struct aalap_hdr *aalap;
register int len;
{
	register int c;
	register int crc;
	register int index;
	register char *q;
	unsigned short result;

	crc = 0;
	q = (char *) &aalap->aalap_type;
	while(len-- >= 0) { /* include type */
		c = *q++;
		index = c ^ crc;
		index &= 0x0f;
		crc >>= 4;
		crc ^= crctbl[index];
		c >>= 4;
		c ^= crc;
		c &= 0x0f;
		crc >>= 4;
		crc ^= crctbl[c];
	}
	result = (unsigned short) (crc & 0xffff);
	if(result == htons(result)) 	/* on a network byte order machine */
		result = ((crc & 0xff00) >> 8) | ((crc & 0xff) << 8);
	return(result & 0xffff);
}

/*
 * unregister an NBP name
 *
 */

nbp_remove(en)
EntityName *en;
{
	return(NBPRemove(en));
}

/*
 * register an NBP name
 *
 */

nbp_register(en, skt)
EntityName *en;
int skt;
{
	nbpProto nbpr;			/* nbp proto		*/
	NBPTEntry nbpt[1];		/* table of entity names */
	
	nbpr.nbpAddress.skt = skt;
	nbpr.nbpRetransmitInfo.retransInterval = 5;
	nbpr.nbpRetransmitInfo.retransCount = 3;
	nbpr.nbpBufPtr = nbpt;
	nbpr.nbpBufSize = sizeof(nbpt);
	nbpr.nbpDataField = 1;	/* max entries */
	nbpr.nbpEntityPtr = en;
	
	return((int) NBPRegister(&nbpr,FALSE));	/* try synchronous */
}

#ifndef BROAD

/*
 * notify asyncad daemon of a change in state
 *	UP | DOWN
 *
 */

notify(state)
int state;
{
	struct ap ap;
	char hostname[64];
	struct hostent *host, *gethostbyname();

	ap.ldst = 0xC0;	/* MAGIC	*/
	ap.lsrc = 0xDE;	/* MAGIC	*/
	ap.ltype = (u_char) state;
	ap.srcskt = ourNode;
	sin_broad.sin_family = AF_INET;
	sin_broad.sin_port = htons(aaBroad);
	gethostname(hostname, sizeof(hostname));
	host = gethostbyname(hostname);
	bcopy(host->h_addr, &sin_broad.sin_addr.s_addr,
			sizeof(sin_broad.sin_addr.s_addr));

	if(debug) fprintf(dbg, "Sent %s message to: %s (%08x) on port %d\n",
			(state==UP) ? "UP" : "DOWN", hostname, 
			htonl(sin_broad.sin_addr.s_addr), 
			ntohs(sin_broad.sin_port));

	if(sendto(s, (caddr_t)&ap, 16, 0, &sin_broad,sizeof(sin_broad)) != 16) {
		if(debug) fprintf(dbg, "Couldn't tell asyncad we exist\n");
		cleanup();
	}
}

#endif  BROAD

/*
 * reset things to a natural state
 *
 */

 cleanup()
 {
	nbp_remove(&en);
#ifndef BROAD
	notify(DOWN);
#endif  BROAD
	ioctl(fd, TIOCSETP, &sgttyb);
	fchmod(fd, buf.st_mode);
	if(debug) fclose(dbg);
	if(serOpen) close(p);
	exit(1);
}

int
funeral()
{
	WSTATUS junk;
	
	serOpen = 0;
#ifdef POSIX
	while( waitpid(-1, &junk, WNOHANG) > 0 )
	  ;
#else  POSIX
#ifndef NOWAIT3
	while( wait3(&junk, WNOHANG, 0) > 0 )
	  ;
#else   NOWAIT3
	wait(&junk); /* assume there is at least one process to wait for */
#endif  NOWAIT3
#endif POSIX
	close(p);
}
