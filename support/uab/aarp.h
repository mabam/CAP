/*
 * $Author: djh $ $Date: 91/02/15 23:07:10 $
 * $Header: aarp.h,v 2.1 91/02/15 23:07:10 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * aarp.h Apple Address Resolution Protocol interface
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
 *  September 1988  CCKim Created
 *
*/

/*
 * intializes aarp for this interface: upto maxnode nodes for this interface 
*/
caddr_t aarp_init( /* char *dev, int devno, int maxnode */);

/*
 * returns a node address for this interface.  index specifies which
 *  one (can have multiple)
*/
int aarp_get_host_addr(/*struct ethertalkaddr *pa, int index*/);


/*
 * resolves a ethertalk node address into an ethertalk address 
 * returns pointer to address in eaddr
 * args: caddr_t ah, struct ethertalkaddr *pa,
 *       boolean wantbr, u_char **eaddr
 *
*/ 
int aarp_resolve();

/*
 * inserts the a mapping into the aarp table
 *
 * if flag is "true" then will override any mappings already there
 *  [used by ethertalk for gleaning]
 * 
 * args: caddr_t ah, u_char ha[6], struct ethertalkaddr *pa, int flag
 *
*/
int aarp_insert();

/*
 * acquire an ethertalk node address 
 * tries to acquire the ethertalk node address specfied in node.
 * calls back with the host address index: 
 *     (*callback)(callback_arg, index)
 *
 * args: caddr_t ah, struct ethertalkaddr *node, int (*callback)(),
 *       caddr_t callback_arg
 *
*/
int aarp_acquire_etalk_node();

