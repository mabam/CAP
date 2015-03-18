/*
 * $Date: 91/03/14 14:24:51 $
 * $Header: notify.h,v 2.2 91/03/14 14:24:51 djh Exp $
 * $Log:	notify.h,v $
 * Revision 2.2  91/03/14  14:24:51  djh
 * Revision for CAP.
 * 
 * Revision 1.1  91/01/10  01:04:54  djh
 * Initial revision
 * 
 *
 * djh@munnari.OZ.AU, 06/05/90
 * Copyright (c) 1991, The University of Melbourne
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of Melbourne makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netat/appletalk.h>

#define NUMNBPENTRY	200		/* maximum users we can look up	*/
#define MAXZONES	200		/* maximum number of zones	*/
#define MAXUSERS	100		/* maximum users to send to	*/
#define TIME_OFFSET	0x7c25b080	/* Mac Time 10:00:00 01/01/70	*/
#define MACUSER		"macUser"	/* NBP type registration	*/
#define OURZONE		"*"		/* Zone for this host		*/
#define DDPPKTSIZE	586		/* maximum bytes for a DDP pkt	*/
#define ICONFLAG	0x80		/* indicates ICON included	*/
#define ICONSIZE	128		/* mac ICONs are 128 bytes	*/
#define ICONOFFSET	0404		/* into a ResEDIT resource file */
#define ICONFILE	".myicon"	/* the ResEDIT file		*/

#define MSGMAIL		30
#define MSGWALL		31
#define MSGTO		32

#define MSGTOARG	64		/* an internal flag		*/
