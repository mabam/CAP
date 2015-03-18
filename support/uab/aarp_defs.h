/*
 * $Author: djh $ $Date: 1994/10/10 08:56:13 $
 * $Header: /mac/src/cap60/support/uab/RCS/aarp_defs.h,v 2.2 1994/10/10 08:56:13 djh Rel djh $
 * $Revision: 2.2 $
*/

/*
 * aarp_defs.h Apple Address Resolution Protocol definitions
 *
 * Meant only for the aarp module!
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
 *  August 1988  CCKim Created
 *
*/

#define AARP_DUMP_TABLE "yes" /* turn this on */

/*
 * AARP Ethernet type
 *
*/
#ifndef ETHERTYPE_AARP
# define ETHERTYPE_AARP 0x80f3
#endif

/* probe op, define if not defined */
#ifndef ARPOP_PROBE
# define ARPOP_PROBE 3
#endif

/*
 * AARP table management
 *
*/

/* An ethertalk node address (should probably merge with aarptab) */
typedef struct aarp_entry {
  struct ethertalkaddr aae_pa;	/* protocol address */
  int aae_flags;		/* flags */
#define AARP_OKAY 0x1		/* resolved */
#define AARP_RESOLVING 0x2	/* trying to resolve this */
#define AARP_PERM 0x4		/* permanent (not used) */
#define AARP_FNAME_OKAY(f) ((f&AARP_OKAY) ? "okay " : "")
#define AARP_FNAME_RESOLVING(f) ((f&AARP_RESOLVING) ? "resolving " : "")
#define AARP_FNAME_PERM(f) ((f&AARP_PERM) ? "perm " : "")
  u_char aae_eaddr[EHRD];	/* hardware address */
  int aae_ttl;			/* time to live */
  int aae_used;			/* # of times used */
  struct aarp_entry *aae_next;	/* next in list of all */
  caddr_t aae_aarptab;		/* back pointer */
} AARP_ENTRY;


/* time to live */
#define AARP_TTL 5		/* 4*AARP_SCAN_TIME */
#define AARP_SCAN_TIME (60)	/* scan every minute  */
/* we adjust ttls if an incoming probe comes in */
/* if the ha is the same, ttl gets dropped.  if the ha */
/* is different, the ttl is smacked dead */ 
#define AARP_PROBED_SAME 1	/* set time to live to AARP_SCAN_TIME */
#define AARP_PROBED_DIFF 0	/* set time to live to zero */

/*
 * AARP probe/request managment
 *
*/
/* number of times to probe an address */
#define AARP_PROBE_TRY 15
/* timeout between probes */
#define AARP_PROBE_TIMEOUT_SEC 0
#define AARP_PROBE_TIMEOUT_USEC 100000 /* 1/10 second */
/* same for requests */
#define AARP_REQUEST_TRY 10
#define AARP_REQUEST_TIMEOUT_SEC 0
#define AARP_REQUEST_TIMEOUT_USEC 250000 /* 1/4th second */

/*
 * AARP table/interface management
 *
*/
/* number of buckets in the aarp hash table */
#define AARP_TAB_SIZE 15

/*
 * one of these are created for every "interface" open
 *
*/
typedef struct aarp_interface_handle {
  int ai_ph;			/* protocol handle */
  caddr_t ai_aarptab;		/* aarp table */
  int ai_accesses;		/* count of access to arp tabl */
#define AI_AARPTAB_EVAL_POINT 2000
#define AI_AARPTAB_REHASH_POINT 200 /* more than avg. of 2 access */
  struct ai_host_node {		/* host's node table */
    struct ethertalkaddr aihn_pa; /* our protocol address */
    int aihn_state;
#define AI_NODE_UNUSED 0	/* node not in use */
#define AI_NODE_PROBE -1	/* node in probe state */
#define AI_NODE_OKAY 1		/* node okay */
  } *ai_nodes;
  int ai_maxnode;
  int ai_numnode;		/* number of nodes in use */
  int ai_flags;			/* interface flags */
  u_char ai_eaddr[EHRD];	/* enet addr */
  /* probably should return this integer instead of the handle */
} AI_HANDLE;


/* exported procedure definitions */
export caddr_t aarp_init();
export int aarp_resolve();
export int aarp_insert();
export int aarp_acquire_etalk_node();

