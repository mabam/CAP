/*
 * $Date: 91/02/15 21:21:06 $
 * $Header: macros.h,v 2.1 91/02/15 21:21:06 djh Rel $
 * $Log:	macros.h,v $
 * Revision 2.1  91/02/15  21:21:06  djh
 * CAP 6.0 Initial Revision
 * 
 * Revision 1.1  90/03/17  22:06:49  djh
 * Initial revision
 * 
 *
 *	macros.h
 *
 *	Make nice macros work on 4.2BSD systems
 *
 *	Asynchronous AppleTalk for CAP UNIX boxes.
 *	David Hornsby, djh@munnari.oz, August 1988
 *	Department of Computer Science.
 *	The University of Melbourne, Parkville, 3052
 *	Victoria, Australia.
 *
 * Copyright 1988, The University of Melbourne.
 *
 * You may use, modify and redistribute BUT NOT SELL this package provided
 * that all changes/bug fixes are returned to the author for inclusion.
 *
 */

#ifndef NBBY
#define	NBBY	8		/* number of bits in a byte */
#endif

#ifndef NFDBITS
#define NFDBITS	(sizeof(int) * NBBY)	/* bits per mask */
#endif

#ifndef FD_SET
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#endif

#ifndef FD_CLR
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#endif

#ifndef FD_ISSET
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#endif

#ifndef FD_ZERO
#define FD_ZERO(p)	bzero(p, sizeof(*(p)))
#endif
