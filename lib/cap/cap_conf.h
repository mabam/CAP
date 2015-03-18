/*
 * $Author: djh $ $Date: 91/02/15 22:49:45 $
 * $Header: cap_conf.h,v 2.1 91/02/15 22:49:45 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * cap_conf.h - site configuration file
 *
 * This file is to contain parameters individual sites might like to
 * change
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * Edit History:
 *
 *  July  6, 1986    CCKim	Created.
 *  July  9, 1986    CCKim	Convert to "bridge" philosophy
 *				Add some params
 *
 */

/* timeout parameters for PAP */
#define PAPOPENTIMEOUT  sectotick(2) /* spec says 2 seconds, use */
				       /* 3 seconds- o.w. see */
				       /* anomalous behavior */
#define PAPOPENRETRY	5	/* as spec'ed */

#define PAPCLOSETIMEOUT sectotick(5)
#define PAPCLOSERETRY 3

#define PAPCONNECTIONTIMEOUT sectotick(120) /* two minutes */
#define PAPTICKLETIMEOUT sectotick(60) /* one minute - half connection */
				       /* timeout */

#define PAPREADTIMEOUT sectotick(15) /* 15 second retry (a little long */
				     /* by my standards) */
#define PAPREADRETRIES 255	/* retry count of infinity */

#define PAPARBITRATIONTIME sectotick(2)	/* two seconds */
