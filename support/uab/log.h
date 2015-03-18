/*
 * $Author: djh $ $Date: 91/02/15 23:07:44 $
 * $Header: log.h,v 2.1 91/02/15 23:07:44 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * log.h - simple logging facility header file
 *
 *
 * Copyright (c) 1988 by The Trustees of Columbia University 
 *   in the City of New York.
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
 *   Aug, 1988 CCKim created
 *
*/

/* logging flags */
#define L_UERR 0x20		/* want unix error message */
#define L_EXIT 0x10		/* exit after logging */
#define L_LVL 0xf		/* debug levels */
#define L_LVLMAX 15		/* maximum level */

