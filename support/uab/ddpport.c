static char rcsid[] = "$Author: djh $ $Date: 91/03/13 20:38:01 $";
static char rcsident[] = "$Header: ddpport.c,v 2.2 91/03/13 20:38:01 djh Exp $";
static char revision[] = "$Revision: 2.2 $";

/*
 * ddpport.c - ddp port manager
 *
 * Handles the ddp port which sits on the interface boundary between
 * DDP and LAP.  In addition, it carries the "local delivery" or
 * demuxing information necessary to pass data to individual modules
 *
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
 *  Sept 4, 1988  CCKim Created
 *
*/

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/param.h>
#ifndef _TYPES
# include <sys/types.h>
#endif
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>

#include <netat/appletalk.h>
#include <netat/compat.h>
#include "ddpport.h"
#include "log.h"

private PORT *port_list = NULL;

#define LOG_HIGH 1
#define LOG_STD 2

/*
 * Establish an open port.
 *  ddp_net, ddp_node - ddp network/node to associate with this port
 *   (0 if not seeded) 
 *  zone - zone if any (null if not)
 *  node - node id of this port (required)
 *  local_handle - local data handle for caller
 *  sendrtn - send data routine
 *  map_fromddp - map ddp net, node to lap node
 *  map_laptoddp - map inverse (not used)
 *  demuxer - demux structure that describes the local delivery mechanism
 *
*/
export PORT_T
port_create(ddp_net, ddp_node, zone, node, flags, local_handle,
	    sendrtn, map_fromddp, map_laptoddp, demuxer)
word ddp_net;
word ddp_node;
byte *zone;
NODE *node;
int flags;
caddr_t local_handle;
int (*sendrtn)();
NODE *(*map_fromddp)();
int (*map_laptoddp)();
struct mpxddp_module *demuxer;
{
  struct route_entry *re;
  PORT *port;

  if ((port = (PORT *)malloc(sizeof(PORT))) == NULL) {
    logit(LOG_HIGH|L_UERR, "malloc for port failed");
    return(NULL);
  }
  port->p_next = NULL;
  port->p_flags = flags|PORT_ISCONNECTED; /* mark here */
  port->p_ddp_node = ddp_node;	/* mark ddp node */
  port->p_ddp_net = 0;		/* mark as unknown for now */
  port->p_zonep = NULL;		/* mark as unknown for now */
  port->p_id = node;
  port->p_local_data = local_handle;
  port->p_send_if = sendrtn;
  port->p_map_lap_if = map_fromddp;
  port->p_map_ddp_if = map_laptoddp;
  port->p_mpx = NULL;
  if (ddp_net) {
    port_net_ready(port, ddp_net, NULL); /* this will set ddp_net properly */
    if (route_add_host_entry(port, ddp_net, zone) < 0) {
      logit(LOG_HIGH,"***couldn't add host route for port %d, net %d.%d",
	  port, nkipnetnumber(ddp_net), nkipnetnumber(ddp_net));
    }
    if (zone)			/* if zone, mark that too */
      port_zone_known(port, zone);
  }
  /* wait until after we set zone, etc if we did */
  if (demuxer) {
    if (!port_setdemuxer(port, demuxer))
      logit(LOG_HIGH, "couldn't initialize local divery via %s on port %d\n",
	  demuxer->mpx_name,port);
  }
  /* may need to call a lower level grab routine (e.g. if working with */
  /* kernel ddp) */
  return(port);
}

/*
 * set and initialize local delivery: can be called later if not set
 * in port create.
 * returns true,false
 *
*/
export
port_setdemuxer(port, mpx)
PORT_T port;
struct mpxddp_module *mpx;
{
  int i;

  if (mpx == NULL)
    return(TRUE);
  
  if (!mpx->mpx_init || (i = mpx->mpx_init()) < 0) /* init multiplexor */
    return(FALSE);
  if (mpx->mpx_grab)		/* if socket grabber */
    ddp_reopen(mpx->mpx_grab, i, NULL);	/* then call it */
  port->p_mpx_hdl = i;		/* remember handle */
  /* port is ready already! */
  if (mpx->mpx_havenode)	/* always mark node (must be known) */
    (*mpx->mpx_havenode)(i, port->p_ddp_node);
  /* if netready, send down net */
  if ((port->p_flags & PORT_NETREADY) && mpx->mpx_havenet)
      (*mpx->mpx_havenet)(i, port->p_ddp_net, port->p_ddp_node);
  /* if portready then send down zone (==> netready) */
  if ((port->p_flags & PORT_ISREADY) && mpx->mpx_havezone)    
    (*mpx->mpx_havezone)(i, port->p_zonep);
  port->p_mpx = mpx;
  return(TRUE);
}

/*
 * get the head of the "lap" interface port list
 *
*/
export PORT_T
port_list_start()
{
  return(port_list);
}

/*
 * port's net is ready, set vars: return TRUE if was ready
 *
 * (Is this really sufficient?)
 *
*/
export boolean
port_net_ready(port, net, sid, zone)
PORT_T port;			/* port */
word net;			/* ddp network for port */
NODE *sid;			/* node the information came from */
byte *zone;			/* zone */
{
  if (port == NULL) {		/* how did this happen? */
    logit(LOG_HIGH, "port null in port_net_ready");
    return(FALSE);
  }
  if (port->p_ddp_net == 0) {
    port->p_ddp_net = net;
    port->p_next = port_list;	/* link into list */
    port_list = port;
    port->p_flags |= PORT_NETREADY; /* mark sure marked ready */
    /* set net & bridge info (bridge is ourselves) */
    if (port->p_mpx && port->p_mpx->mpx_havenet)
      (*port->p_mpx->mpx_havenet)(port->p_mpx_hdl, net, port->p_ddp_node);
    return(FALSE);
  }
  if (port->p_ddp_net != net) {
    logit(LOG_STD, "***net mismatch: port %d's net is %d.%d, received %d.%d",
	port, nkipnetnumber(port->p_ddp_net),
	nkipsubnetnumber(port->p_ddp_net),
	nkipnetnumber(net), nkipsubnetnumber(net));
    if (sid)
      logit(LOG_STD, "offending node: %s",
	  node_format(sid));
  } 
  return(TRUE);
}

/*
 * zone acquired, returns TRUE, FALSE
 *
*/
export boolean
port_zone_known(port,zone)
PORT_T port;
byte *zone;
{
  if (port == NULL) {		/* how did this happen? */
    logit(LOG_HIGH, "port null in port_zone_ready");
    return(FALSE);
  }
  if ((port->p_flags & PORT_NETREADY) == 0)
    return(FALSE);
  port->p_flags |= PORT_ISREADY; /* mark sure marked ready */
  /* zone storage is kept */
  port->p_zonep = zone;		/* remember it */
  if (port->p_mpx && port->p_mpx->mpx_havezone)
    (*port->p_mpx->mpx_havezone)(port->p_mpx_hdl, zone);
  return(TRUE);
}
