/*
 * $Author: djh $ $Date: 91/02/15 22:49:05 $
 * $Header: abpp.c,v 2.1 91/02/15 22:49:05 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * abpp - Unix Printer Access Protocol calls
 *
 * A set of routines which emulate the unix read/write calls.
 * The calls are always synchronous.
 *
 * Note: these aren't much use unless you fork off since
 * a blocking read will pretty much kill you
 * 
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July  5, 1986    CCKim	Created.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <netat/appletalk.h>


/*
 * ppopen
 * 
 * open pap socket
 *
 * note: does not return until connection is opened.
 *
*/
int
ppopen(cno, lwname, flowq, pstatus)
int *cno;
char *lwname;
int flowq;
int (*pstatus)();
{
  int err;
  PAPStatusRec statusbuff;
  int rcomp;

  while ((err = PAPOpen(cno, lwname, flowq, &statusbuff, &rcomp)) != noErr) {
    (*pstatus)(&statusbuff.StatusStr[1]);
    sleep(5);			/* wait five seconds */
  }
  do {
    abSleep(4*5, TRUE);		/* wait 2 seconds */
    (*pstatus)(&statusbuff.StatusStr[1]);
  } while (rcomp > 0);
  return(rcomp);
}


/*
 * ppread
 * 
 * Read from the LaserWriter
 *
 * Note: buf is assume to be PAPSegSize * flowquantum returned by the
 * laserWriter.  To be safe, use  512 (PAPSegSize) * 8 (the maximum flow
 * quantum)
 *
 * returns number of bytes or zero to indicate eof
 *
*/
ppread(cno, buf)
int cno;
char *buf;
{
  int rcomp;
  int rlen;
  static int eof = 0;
  int err;

  if (eof)			/* check for eof on previous call */
    return(0);
  do {
    if ((err = PAPRead(cno, buf, &rlen, &eof, &rcomp)) != noErr)
      return(err);
    do { abSleep(4, TRUE); } while (rcomp > 0);
    if (rcomp > 0)
      return(rcomp);
  } while (rlen == 0 && !eof);

  if (eof && rlen == 0)
    return(0);			/* zero length indicates eof */
  return(rlen);
}

/*
 * ppwrite - write to laserWriter
 *
 * eof - true if we are to send eof to the laserwriter 
 *
*/
ppwrite(cno, buf, blen, eof)
int cno;
char *buf;
int blen;
{
  int wcomp;
  int err;

  err = PAPWrite(cno, buf, blen, eof, &wcomp); /* send eof */
  if (err != noErr)
    return(err);
  do { abSleep(4, TRUE); } while (wcomp > 0);
  return(err);
}
