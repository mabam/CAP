/*
 * $Author: djh $ $Date: 91/02/15 22:47:37 $
 * $Header: ablap.c,v 2.1 91/02/15 22:47:37 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * ablap.c - Link Access Protocol
 *
 * As of CAP 5.0, this module has been dropped.  It is included for
 * reference.  It never really made sense with DDP encapsulation anyway
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * Edit History:
 *
 *  June 19, 1986    Schilit	Created.
 *  July  2, 1986    Schilit	Most is not yet implemented
 *  July  5, 1986    CCKim      Most will never be implemented
 *  March 1988       CCKim      DUMP IT!
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netat/appletalk.h>

/* 
 * lapp - table of LAP protocol handlers.
 *
*/

struct {
  ProcPtr pproc;		/* LAP protocol handler */
} lapp[lapMaxProto];		/* number of protocols */


typedef struct {
  LAP lap;
  char data[lapMaxData];
} ABpkt;			/* for lap_read */

ABpkt rdpkt;

/*
 * OSErr LAPOpenProtocol(int type, ProcPtr proto)
 *
 * LAPOpenProtocol adds the LAP protocol type specified in "type"
 * to the protocol table.  If you provide a pointer to a protocol
 * handler in "proto" then each frame with a LAP protocol type of
 * "type" will be sent to that handler.
 *
 * If "proto" is NILPROC then the default handler will be used for
 * receiving frames with a LAP protocol type of "type."  In this
 * case you must call LAPRead to provide the default protocol handler
 * with buffer for storing the data.  If however you have written your
 * own protocol handler and "proto" points to it, your protocol handler
 * will be responsible for receiving the frame and it is not necessary
 * to call LAPRead.
 *
*/ 

OSErr
LAPOpenProtocol(type,proto)
int type;
ProcPtr proto;
{
  int defLAPProto();

  if (type < 0 || type > 127) {	/* type in range? */
    fprintf(stderr,"LAPOpenProtocol: type (%d) not in range 1..127\n",type);
    exit(1);
  }
  lapp[type].pproc = (proto == NILPROC) ? defLAPProto : proto;
  return(noErr);
}

/*
 * OSErr LAPCloseProtocol(int type)
 *
 * LAPCloseProtocol removes the protocol specified by "type" from
 * the LAP protocol handler table.
 *
*/

OSErr
LAPCloseProtocol(type)
int type;
{
  if (type < 2 || type > 127) {	/* type in range? */
    fprintf(stderr,"LAPCloseProtocol: type (%d) not in range 3..127\n",type);
    exit(1);
  }
  lapp[type].pproc = NILPROC;	/* no more handler */
}

/*
 * OSErr LAPRead(abRecPtr abr,int async)
 *
 * LAPRead receives a frame from another node.
 *
*/

OSErr
LAPRead(abr,async)
abRecPtr abr;
int async;
{
  fprintf(stderr,"LAPRead NYI\n");
}

OSErr
LAPRdCancel(abr)
abRecPtr abr;
{
  fprintf(stderr,"LAPRead NYI\n");
}

/*
 * OSErr LAPWrite(abRecPtr abr)
 *
 * LAPWrite send a frame to another node.  Currently this does
 * not work because of the limitations in the kinetics bridge 
 * box.
 *
*/

OSErr
LAPWrite(abr)
abRecPtr abr;
{
  fprintf(stderr,"LAPWrite NYI\n");
}

/*
 * lap_read(int fd)
 *
 * Read the data off the net and dispatch to the appropriate
 * protocol handler.
 *
*/
 
lap_read(fd)
int fd;
{
  struct iovec iov[2];
  int lpt,len;

  iov[0].iov_base = (caddr_t) &rdpkt.lap;
  iov[0].iov_len = lapSize;
  iov[1].iov_base = (caddr_t) rdpkt.data;
  iov[1].iov_len = lapMaxData;
  if ((len = abread(fd,&iov[0],2)) < 0) /* do the read */
    return(len);		/* failed, return now... */
  lpt = rdpkt.lap.type;		/* get the protocol type */
  if (lpt < 1 || lpt > 127)
    return(-1);			/* ugh... */
  if (lapp[lpt].pproc == NILPROC) /* any protocol handler? */
    return(-1);			/* no, ugh again */
  return((*lapp[lpt].pproc)(rdpkt.data,len-lapSize)); /* call handler */
}
  
/*
 * defLAPProto(int fd)
 *
 * defLAPProto is not used.
 *
*/ 

defLAPProto(fd)
int fd;
{
  fprintf(stderr,"defLAPProto NYI %d\n",fd);
}
