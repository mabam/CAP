/*
 *	timelord - UNIX Macintosh time server daemon
 *
 *	22/05/89	djh@munnari.oz
 *
 * Copyright (c) 1990, the University of Melbourne
 *
 */

/* general junk		*/

#define	ZONE		"*"		/* NBP Zone		*/
#define MACSERVER	"TimeLord"	/* NBP Name		*/
#define TIME_OFFSET	0x7c25b080	/* Mac Time 00:00:00 GMT Jan 1, 1970 */
#define LOGFILE		"/dev/null"

/* available commands	*/

#define GETTIME		0

/* error messages	*/

#define	ENOPERM		10
#define ENOLOGIN	11
#define	ENONE		12
