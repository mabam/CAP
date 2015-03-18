/*
 * $Author: djh $ $Date: 1991/05/29 12:48:21 $
 * $Header: /mac/src/cap60/support/uab/RCS/ethertalk.h,v 2.2 1991/05/29 12:48:21 djh Rel djh $
 * $Revision: 2.2 $
*/

/*
 * Ethertalk definitions
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
 *  August 1988  CCKim Created
 *  April  1991  djh   Added Phase 2 support
 *
*/

/* format of an ethertalk appletalk address */
struct ethertalkaddr {
  byte dummy[3];		/* should be "network" */
  byte node;			/* appletalk node # */
};

#ifndef ETHERTYPE_APPLETALK
# define ETHERTYPE_APPLETALK 0x809b
#endif

#define ETPL 4			/* ethertalk protocol address length */

