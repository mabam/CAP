/*
 * $Author: djh $ $Date: 91/02/15 23:07:47 $
 * $Header: mpxddp.h,v 2.1 91/02/15 23:07:47 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * demultipexor/multiplexor interface
 *
 *  used to send packets to/from processes from a central process that
 * handles incoming ddp packets from an interface that can only be
 * attached by a single process 
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

#ifndef _MPX_DDP_INCLUDED
#define _MPX_DDP_INCLUDED "yes"
/* 
 * demuliplexing module interface point
 *
*/

struct mpxddp_module {
  char *mpx_name;		/* name of module (usually transport type) */
  char *mpx_key;		/* key for specification purposes  */
  int (*mpx_init)();		/* init routine */
  int (*mpx_grab)();		/* used to grab ddp sockets */
  int (*mpx_send_ddp)();	/* send ddp routine */
  int (*mpx_havenode)();	/* mark node known */
  int (*mpx_havenet)();		/* network & bridge (if nec) known */
  int (*mpx_havezone)();	/* zone known */
};

/*
 * mpx_init
 *
 * initialization
 *
 * int (*mpx_init)() - returns handle (hdl) for later use
 *
*/

/*
 * mpx_grab
 *
 *  mpx_grab should "grab" the specified ddp socket (in its own way)
 *  and forward packets received upon that ddp socket to (s)ddp_router
 *
 * int (*mpx_grab)(hdl, skt);
*/

/*
 * mpx_send_ddp
 *
 * used by the mpx process to send ddp packets to clients
 *
 * (*mpx_send_ddp)(hdl, DDP *ddp, caddr_t data, int data_len)
 */

/*
 *  used by multiplexing process to send back info on the current ddp world
 *
 * Ordering will always be: havenode, havenet, havezone
 * 
 * (*mpx_havenode)(hdl,byte node)
 * (*mpx_havenet)(hdl,word net, byte bridgenode)
 * (*mpx_havezone)(hdl,pstr zone)
 *
*/

#endif /* INCLUDE THIS FILE */
