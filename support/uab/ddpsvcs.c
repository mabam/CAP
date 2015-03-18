static char rcsid[] = "$Author: djh $ $Date: 1992/07/30 09:50:38 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/ddpsvcs.c,v 2.3 1992/07/30 09:50:38 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * ddpsvcs.c - simple ddp router
 *
 * Some ddp services
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
 *
 * Edit History:
 *
 *  September, 1988  CCKim Created
 *
*/

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <sys/types.h>
#include <netat/appletalk.h>
#include <netat/abnbp.h>
#include <netinet/in.h>

#include "gw.h"

export void ddp_svcs_init();
export void ddp_svcs_start();
private int echo_handler();
private int nbp_handler();
private int nbp_ptrs();
private void nbp_bridge_request_handler();
#ifdef dontdothisrightnow
private void nbp_lookup_handler();
private void nbp_lookup_reply_handler();
export int nbp_lookup();
export int nbp_register();
export int nbp_delete();
export int nbp_tickle();
#endif

/*
 * glue
 *
*/

export void
ddp_svcs_start()
{
}

export void
ddp_svcs_init()
{
  ddp_open(echoSkt, echo_handler);
  ddp_open(nbpNIS, nbp_handler);
}

/*
 * echo service
 *
*/
private boolean
echo_handler(port, ddp, data, datalen)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  byte e_cmd;			/* echo command */
  DDP rddp;			/* reply ddp header */

  if (ddp->type != ddpECHO)	/* dump it */
    return(TRUE);

  e_cmd = *data;			/* first byte is command */
  switch (e_cmd) {
  case echoRequest:
    break;
  case echoReply:
    return(FALSE);		/* still needs forwarding */
  }
  *data = echoReply;		/* establish as reply */
  rddp.dstNet = ddp->srcNet;	/* respond to sender */
  rddp.dstNode = ddp->srcNode;
  rddp.dstSkt = ddp->srcSkt;
  rddp.srcSkt = echoSkt;	/* set socket */
  rddp.type = ddpECHO;		/* and type */
  ddp_output(NULL, &rddp, data, datalen);
  return(TRUE);			/* we handled it */
}

/*
 * handle incoming nbp packet
 *
 *
*/
private boolean
nbp_handler(port, ddp, data, datalen)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  word control;

  if (ddp->type != ddpNBP)	/* dump it */
    return(TRUE);

  if (datalen < nbpMinSize)
    return(TRUE);		/* say we handled it! */
  control = *data >> 4;		/* get nbp control part */
  switch (control) {
  case nbpBrRq:			/* bridge request for lookup */
    nbp_bridge_request_handler(port, ddp, data, datalen);
    return(TRUE);
  case nbpLkUp:			/* nbp lookup */
    break;
  case nbpLkUpReply:		/* nbp lookup reply */
    return(FALSE);		/* for now*/
  /* nbp extended codes */
  case nbpRegister:		/* register a name */
  case nbpDelete:		/* delete a name */
  case nbpTickle:		/* let them know socket is alive and well */
    break;
  case nbpStatusReply:		/* status on register/delete */
  case nbpLookAside:		/* Lookup via NIS */
    return(FALSE);		/* for now */
  }
  return(FALSE);		/* for now */
}

/*
 * given a pointer to an NBP packet, return pointers to the three
 * elements of the nbp entity.  assumes there is only one there.
 *
 * returns the valid length of nbp packet
*/
private int
nbp_ptrs(nbp, obj, typ, zon)
NBP *nbp;
byte **obj;
byte **typ;
byte **zon;
{
  byte *o, *t, *z;
  int ol, tl, zl;
  o = nbp->tuple[0].name;
  ol = *o;
  if (ol >= ENTITYSIZE)
    return(0);
  t = o+ol+1;			/* get ptr to type */
  tl = *t;			/* get type length */
  if (tl >= ENTITYSIZE)
    return(0);
  z = t+tl+1;			/* get ptr to zone */
  zl = *z;
  if (zl >= ENTITYSIZE)
    return(0);
  *obj = o, *typ = t, *zon = z;
  /* add 3 for lengths, add 1 for enum, and addtrblock */
  return(ol+tl+zl+nbpMinSize);
}

/*
 * take an nbp packet and handle it if it is a nbp BrRq
 *
*/
private void
nbp_bridge_request_handler(port, ddp, data, datalen)
PORT_T port;			/* incoming port */
DDP *ddp;
byte *data;
int datalen;
{
  struct route_entry *re;
  int tuplelen;
  byte *obj, *typ, *zon;	/* pointers to parts of entity name */
  byte *zonep;			/* nbp zone to lookup */
  NBP nbp;
  DDP sddp;
  
  bcopy((caddr_t)data,(caddr_t)&nbp,sizeof(NBP)>datalen?datalen:sizeof(NBP));
  nbp.control = nbpLkUp;
  /* if bad tuplelen, then drop */
  if ((tuplelen = nbp_ptrs(&nbp, &obj, &typ, &zon)) <= 0)
    return;
  /* zone of "=" is invalid */
  if (zon[0] == 1 && zon[1] == nbpEquals)
    return;
  /* 0 length zone or "*" qualifies as this zone */
  if (zon[0] == 0 || (zon[0] == 1 && zon[1] == nbpStar)) {
    int t;

    t = zon[0];
    re = route_find(ddp->srcNet == 0 ? PORT_DDPNET(port) : ddp->srcNet);
    if (re == NULL)		/* can't rewrite zone name */
      return;
    if (re->re_zonep == NULL)	/* bad zone? */
      return;
    zonep = re->re_zonep;
    /* copy it in, plenty of space */
    bcopy((caddr_t)zonep, (caddr_t)zon, (int)(1+(*zonep)));
    tuplelen += (*zonep- t); /* fix length */
  } else {
    if ((zonep = zone_find(zon, FALSE)) == NULL)
      return;
  }
  /* fixup and figure out zone */
  sddp = *ddp;
  sddp.checksum = 0;
  sddp.length = htons(tuplelen + ddpSize);
  for (re = route_list_start(); re; re = route_next(re)) {
    if (!re->re_state || re->re_zonep != zonep)
      continue;
    sddp.dstNode = DDP_BROADCAST_NODE;
    sddp.dstNet = re->re_ddp_net;
    logit(LOG_LOTS, "nbp lkup net %d.%d for zone %s",
	nkipnetnumber(re->re_ddp_net), nkipsubnetnumber(re->re_ddp_net),
	zonep+1);
    ddp_output(NULL, &sddp, (byte *)&nbp, tuplelen);
  }
  return;
}

#ifdef dontdothisrightnow
/*
 * handle an incoming nbp lookup request
 *
*/
private void
nbp_lookup_handler(port, ddp, data, datalen)
PORT_T port;
DDP *ddp;
byte *data;
int datalen;
{
  int tuplelen;
  byte *obj, *typ, *zon;
  NPB nbp;

  bcopy((caddr_t)data,(caddr_t)&nbp,sizeof(NBP)>datalen?datalen:sizeof(NBP));
  /* if bad tuplelen, then drop */
  if ((tuplelen = nbp_ptrs(&nbp, &obj, &typ, &zon)) <= 0)
    return;
  /* zone specified is not myzone */
  if (!(zon[0] == 0 || (zon[0] == 1 && zon[1] == '*'))) {
    /* check to see if zone specified is same as port's zone */
    byte *zp;

    if ((zp = zone_find(zon, FALSE)) == NULL) /* find it */
      return;			/* wasn't there */
    if (zp != port->p_zonep)	/* check the canonical zone name */
      return;			/* bad, return */
  }
  /* check our tables and rspond */
  {
    EntityName *en;
  }
}

private void
nbp_lookup_reply_handler()
{
}

export int
nbp_lookup()
{
}

export int
nbp_register()
{
  /* check by lookup first? */
  /* insert into our tables */
}

export int
nbp_delete()
{
  /* delete from our tables */
}

export int
nbp_tickle()
{
  /* reset timer */
}

#endif /* NOTDONE */
