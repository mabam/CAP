static char rcsid[] = "$Author: djh $ $Date: 1995/08/28 11:10:14 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/papstream.c,v 2.2 1995/08/28 11:10:14 djh Rel djh $";
static char revision[] = "$Revision: 2.2 $";

/*
 * papstream - UNIX AppleTalk: simple stream handling for pap connections
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986,1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Created sept 5, 1987 by cck from lwsrv
 *
 */

#include <stdio.h>
#include <netat/appletalk.h>
#include "papstream.h"

extern char *myname;

void
p_clreof(p)
PFILE *p;
{
  p->p_flg &= ~P_IOEOF;
  p->p_cnt = 0;
}

/*
 * PFILE *p_opn(int cno)
 *
 * Initialize a stream interface for the PAP connection specified by cno.
 * All buffers are dynamically allocated.  Use p_cls() to close the stream
 * and deallocate buffers.
 *
 */

PFILE *
p_opn(cno, bufsiz)
int cno;
int bufsiz;
{
  PFILE *p;

  p = (PFILE *)malloc(sizeof(PFILE));	/* allocate io block */
  p->p_buf = (byte *)malloc(bufsiz);	/* pointer to input buffer */
  p->p_bufsiz = bufsiz;
  p->p_cno = cno;		/* save pap connection number */
  p->p_cnt = 0;			/* count in buffer is zero */
  p->p_flg = 0;			/* no flags are set */
  return(p);			/* return io block */
}

/*
 * void p_cls(PFILE *p)
 *
 * Close the PAP stream; deallocate buffers and do a PAPClose.
 *
 */

void
p_cls(p)
PFILE *p;
{
  PAPClose(p->p_cno,TRUE);		/* close out connection */
  free((char *)p->p_buf);		/* release input buffer */
  free((char *)p);			/* release io block */
}

/*
 * void p_write(PFILE *p, char *buf, int len, int sendeof)
 *
 * Write len characters from buf to the PAP connection specified by p.
 * If sendeof is TRUE then pass along the EOF indicator to the remote
 * client.
 *
 */

void
p_write(p,buf,len,sendeof)
PFILE *p;
char *buf;
int len,sendeof;
{
  int err,cmp;
#ifdef LWSRV8
  import boolean tracing;
#endif /* LWSRV8 */

  err = PAPWrite(p->p_cno,buf,len,sendeof,&cmp);
#ifdef LWSRV8
  if (tracing)
    tracewrite(buf, len);
#endif /* LWSRV8 */
  if (err != noErr) {
    p->p_flg |= P_IOCLS;
    fprintf(stderr,"%s: p_write error %d on write to Connection %d\n",
     myname, err, p->p_cno);
  } else
    while (cmp > 0)			/* wait for completion */
      abSleep(100,TRUE);		/* returns on event anyway */
  if (cmp != noErr)
    p->p_flg |= P_IOCLS;
}
  
/*
 * int p_fillbuf(PFILE *p)
 *
 * Called by p_getc to refill the input buffer for a PAP stream connection.
 * Returns the next character read or EOF.
 *
 */

int
p_fillbuf(p)
PFILE *p;
{
  int peof, pcmp;
  
  if (p_iscls(p))
    return(EOF);

  /* if end of file set and nothing left in buffer */
  if (p_eof(p) && p->p_cnt <= 0) {
    /* return eof after clearing */
    p_clreof(p);
    return(EOF);
  }
  
  if (PAPRead(p->p_cno,p->p_buf,&p->p_cnt,&peof,&pcmp) != noErr) {
    p->p_flg |= P_IOCLS;
    return(EOF);
  }

  while (pcmp > 0)		/* wait until completion */
    abSleep(100,TRUE);		/* returns upon packet event */

#ifdef notdef /* removed by somebody once, when, who or why ? */
  if (pcmp != noErr) {
    fprintf(stderr,"%s: p_fillbuf PAPRead error %d\n", myname, pcmp);
    p->p_flg |= P_IOCLS;
    /* always return eof in this case */
    return(EOF);		/* what else to do? */
  }
#endif notdef

  if (peof) {
    p->p_flg |= P_IOEOF;
    if (p->p_cnt <= 0) {	/* another way to get eof... */
      p_clreof(p);
      return(EOF);
    }
  }

  p->p_ptr = p->p_buf;		/* reset pointer to start of buffer */
  return(p_getc(p));
}
	 
/*
 * int p_getc(PFILE *p)
 *
 * Read a character from the PAP stream specified by p.  Return the
 * character or EOF.
 *
 */

#ifndef p_getc
int p_getc(p)
PFILE *p;
{
  if (--(p->p_cnt) >= 0) {
    if ((p->p_cnt % ((p->p_bufsiz/RFLOWQ)/2)) == 0)
      abSleep(0,TRUE);			/* make sure protocol runs */
    return(*p->p_ptr++);
  } else
    return(p_fillbuf(p));
}  
#endif
