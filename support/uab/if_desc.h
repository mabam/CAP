/*
 * $Author: djh $ $Date: 91/02/15 23:07:39 $
 * $Header: if_desc.h,v 2.1 91/02/15 23:07:39 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * if_desc.h - interface description
 *
 *  describes parameters for a interface
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
 *  Sept 8, 1988  CCKim Created
 *
*/

#ifndef _IF_DESC_INCLUDED
#define _IF_DESC_INCLUDED "yes"
# include "mpxddp.h"
/*
 * description of a supported LAP type 
 *
*/
typedef struct lap_description {
  char *ld_name;		/* name of lap  */
  char **ld_key;		/* array of keywords to use for lap type */
  int ld_wants_data;		/* needs more than key */
  int (*ld_init_routine)();	/* (id, async) */
  int (*ld_stats_routine)();	/* (fd, id) */
  int (*ld_dump_routine)();	/* (fd, id) */
} LDESC_TYPE;

/*
 * an interface description (port description)
 *
 * Call ld_init_routine with this (+ async flag if you want to run
 *   async if possible) 
 * All fields except id_ifuse are not touched by ld_init_routine
*/
typedef struct interface_description {
  struct lap_description *id_ld; /* lap description */
  char *id_intf;		/* interface name */
  int id_intfno;		/* interface # */
  struct mpxddp_module *id_local; /* local delivery */
  int id_isabridge;		/* flag */
  int id_network;		/* network number (net order) */
  byte *id_zone;		/* zone name (pstring) */
  struct interface_description *id_next; /* next in list */
  caddr_t id_ifuse;		/* interface use */
} IDESC_TYPE;
#endif
