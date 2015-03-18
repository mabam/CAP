/*
 * $Author: djh $ $Date: 1992/03/07 11:03:35 $
 * $Header: /mac/src/cap60/support/capd/RCS/capd.kas.c,v 2.1 1992/03/07 11:03:35 djh Rel djh $
 * $Revision: 2.1 $
 */

#include <netat/appletalk.h>

/*
 * CAPD dummy run routine for Kernel AppleTalk
 *
 */

void
run()
{
	exit(0);
}

/*
 * identify LAP method being used
 *
 */

short
capdIdent()
{
	return(LAP_KERNEL);
}
