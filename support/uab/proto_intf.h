/*
 * $Author: djh $ $Date: 1995/08/30 14:11:46 $
 * $Header: /mac/src/cap60/support/uab/RCS/proto_intf.h,v 2.2 1995/08/30 14:11:46 djh Rel djh $
 * $Revision: 2.2 $
*/

/*
 * protocol interface header:
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
 *
 * Edit History:
 *
 *  August 1988  CCKim Created
 *
*/

/* call first (double call okay) */
export int pi_setup();
/* call with protocol (network order), device name (e.g. qe), device */
/* unit number, return < 0 on error, > 0 protocol handle */
export int pi_open(/* int protocol, char * dev, int devno */);
/* get ethernet address, ea is pointer to place to return address */
export int pi_get_ethernet_address(/* int edx, u_char *ea */);
/* returns TRUE if interface tap can see its own broadcasts (or they */
/* are delivered by system */
export int pi_delivers_self_broadcasts();
/* close a protocol handle */
export int pi_close(/* int edx */);
/* establishes a listener to be called when data ready on */
/* protocol,interface.  (*listener)(socket, arg, eh) */
export int pi_listener(/* int edx, int (*listener), caddr_t arg */);
/* like read */
export int pi_read(/* int edx, caddr_t buf, int bufsize */);
/* like readv */
export int pi_readv(/* int edx, struct iovec iov[], int iovlen */ );
/* like write */
export int pi_write(/* int idx, caddr_t buf, int bufsize */);
/* like writev */
export int pi_writev(/* int edx, struct iovec iov[], int iovlen */ );

#define EHRD 6			/* ethernet hardware address length */

/* much like struct ether_header, but we know what is here -- can be */
/* variable on systems */
struct ethernet_addresses {
  u_char daddr[EHRD];
  u_char saddr[EHRD];
  u_short etype;
};

#define MAXOPENPROT 30		/* arb. number */

