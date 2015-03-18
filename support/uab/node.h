/*
 * $Author: djh $ $Date: 91/02/15 23:07:48 $
 * $Header: node.h,v 2.1 91/02/15 23:07:48 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * node.h - defines a "node" (as per RTMP id)
 *
 * will eventually be the header file for a node manager
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
 *  Sept 4, 1988  CCKim Created
 *
*/

#ifndef _NODE_INCLUDED
#define _NODE_INCLUDED "yes"
/*
 * NODE, struct node_addr
 * 
 * describes a node address which can be up to 256 bits in length
 * (theoretically).  This would be the "lap" level node id which need
 * not bear any relationship to the ddp node.
 *
*/

#define MAXNODEBYTE 8
#define MAXNODEBIT 256

typedef struct node_addr {
  int n_bytes;			/* number of bytes */
  int n_size;			/* size of node */
  byte n_id[MAXNODEBYTE];	/* node number on that port */
} NODE;

#endif /* FILE INCLUDED */

