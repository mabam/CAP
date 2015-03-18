static char rcsid[] = "$Author: djh $ $Date: 1993/09/28 08:24:19 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/aarp.c,v 2.6 1993/09/28 08:24:19 djh Rel djh $";
static char revision[] = "$Revision: 2.6 $";

/*
 * aarp.c - AppleTalk Address Resolution Protocol handler
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
 *  April 3, 1988  CCKim Created
 *  Aug 26,  1988  Moved into separate module from ethertalk
 *  April 28,1991  djh Added Phase 2 support
 *
*/

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#ifdef pyr
#include <netinet/in_ether.h>
#else  pyr
#include <netinet/if_ether.h>
#endif pyr

#include <netat/appletalk.h>
#include "proto_intf.h"
#include "ethertalk.h"
#include "aarp_defs.h"
#include "aarp.h"		/* double checks aarp.h */
#include "hash.h" 
#include "log.h"

#define LOG_LOG 0
#define LOG_BASE 1
#define LOG_LOTS 9

private int aarptab_compare();
private u_int aarp_compress();
private caddr_t aarptab_new();
private AARP_ENTRY *aarptab_get();
private int aarptab_delete();

export caddr_t aarptab_init();
export AARP_ENTRY *aarptab_find();
export caddr_t aarp_init();
export int aarp_get_host_addr();
#ifdef PHASE2
export int aarp_set_host_addr();
#endif PHASE2
export int aarp_resolve();
export int aarp_insert();
export int aarp_acquire_etalk_node();
#ifdef sony_news
int aarp_listener();
#else  sony_news
private aarp_listener();
#endif sony_news
export int aarp_inited = 0;

private AARP_ENTRY *aarp_find_free_etalk_node();
private probe_and_request_driver();
private void aarp_probed();
private void aarp_reply();
private void aarp_request();
private struct ai_host_node *host_node();

/* pointers to host ethernet and broadcast addresess */
#ifdef PHASE2
private u_char b_eaddr[EHRD] = {0x09, 0x00, 0x07, 0xff, 0xff, 0xff};
#else  PHASE2
private u_char b_eaddr[EHRD] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#endif PHASE2

/* list of all the node ids: DUMP ONLY  */
private AARP_ENTRY *aarptab_node_list;

private AARP_ENTRY *aarptab_free_list;


private int m_aarptab = 0;
private int m_probe = 0;
private int f_probe = 0;


/* compare two ethertalk node addresses */
private int
aarptab_compare(k,node)
struct ethertalkaddr *k;
AARP_ENTRY *node;
{
#ifdef PHASE2
  if (node)
      return(memcmp(k, &node->aae_pa, ETPL));
  else
	  return -1;
#else  PHASE2
  return(k->node - node->aae_pa.node);
#endif PHASE2
}

/* compress an ethertalk node addresses */
private u_int
aarptab_compress(k)
struct ethertalkaddr *k;
{
#ifdef PHASE2
  /* ZZ */
#endif PHASE2
  return(k->node);
}

/* create a new arp table entry based upon the ethertalk node */
private caddr_t
aarptab_new(k)
struct ethertalkaddr *k;
{
  AARP_ENTRY *en;

  if (aarptab_free_list) {
    en = aarptab_free_list;	/* get first node */
    aarptab_free_list = en->aae_next; /* and unlink from free list */
    /* if dump, don't need to link into dump table, already there */
  } else {
    if ((en = (AARP_ENTRY *)malloc(sizeof(AARP_ENTRY))) == NULL)
      return(NULL);
    m_aarptab++;
  }
  en->aae_next = aarptab_node_list;
  aarptab_node_list = en;
  en->aae_flags = 0;		/* clear flags */
  en->aae_used = 0;		/* clear used flag */
  en->aae_ttl = 0;		/* clear ttl */
  bcopy(k, &en->aae_pa, sizeof(*k)); /* copy in node address */
  return((caddr_t)en);
}

/* initial hash table for aarp table */
export caddr_t
aarptab_init()
{
  static caddr_t hashtab = NULL;

  if (aarp_inited)
    return(hashtab);

  hashtab = h_new(HASH_POLICY_CHAIN, HASH_TYPE_MULTIPLICATIVE,
			  AARP_TAB_SIZE,
			  aarptab_compare, aarptab_new, aarptab_compress, 
			  NULL, NULL, NULL);
  return(hashtab);
}

/* see if we need to try to speed up hash lookups */
aarptab_speedup(aih)
AI_HANDLE *aih;
{
  struct hash_statistics *s;
  int nbkts;

  aih->ai_accesses = 0;		/* clear */
  s = h_statistics(aih->ai_aarptab); /* get stats */
  if (s->hs_davg < AI_AARPTAB_REHASH_POINT)
    return;			/* nothing to do */
  logit(LOG_BASE, "aarp: %d buckets, %d entries",
      s->hs_buckets, s->hs_used);
  logit(LOG_BASE, "aarp: arp hash table average lookup distance %d.%02d",
      s->hs_davg / 100, s->hs_davg % 100);
  /* possibly delete old entries */
  if ((s->hs_buckets/2) > s->hs_used) {
    logit(LOG_BASE, "aarp: buckets half filled, reconsider hash function");
    return;
  } else {
    nbkts = s->hs_buckets*2;
  }
  logit(LOG_BASE, "aarp: rehashing table from %d buckets to %d buckets",
      s->hs_buckets, nbkts);
  h_rehash(aih->ai_aarptab, nbkts);
}

/* find an aarp table entry given an ethertalk node address */
export AARP_ENTRY *
aarptab_find(aih, pa)
AI_HANDLE *aih;
struct ethertalkaddr *pa;
{
  AARP_ENTRY *r;
  if ((r = (AARP_ENTRY *)h_member(aih->ai_aarptab, pa))) {
#ifdef AARP_DUMP_TABLE
    r->aae_used++;		/* used once more */
#endif
    if (aih->ai_accesses++ > AI_AARPTAB_EVAL_POINT)
      aarptab_speedup(aih);
  }
  return(r);
}

/* find an aarp table entry given an ethertalk node address */
/* if not found, return a new entry */
private AARP_ENTRY *
aarptab_get(aih, pa)
AI_HANDLE *aih;
struct ethertalkaddr *pa;
{
  AARP_ENTRY *r;
  int d, b;

  r = (AARP_ENTRY *)h_operation(HASH_OP_INSERT,aih->ai_aarptab,pa,-1,-1,&d,&b);
  if (r == NULL)
    return(NULL);
  if (r->aae_flags == 0) {
#ifdef PHASE2
    logit(LOG_LOTS, "New AARP mapping for node %d/%d.%d",
		r->aae_pa.node, r->aae_pa.dummy[1], r->aae_pa.dummy[2]);
#else  PHASE2
    logit(LOG_LOTS, "New AARP mapping for node %d", r->aae_pa.node);
#endif PHASE2
    r->aae_aarptab = aih->ai_aarptab;	/* remember */
  } else {
    if (aih->ai_accesses++ > AI_AARPTAB_EVAL_POINT)
      aarptab_speedup(aih);
  }
#ifdef AARP_DUMP_TABLE
  r->aae_used++;		/* bump */
#endif
  return(r);
}

#ifdef notdef
/* delete an aarp table entry  */
private int
aarptab_delete(aih, aa)
AI_HANDLE *aih;
AARP_ENTRY *aa;
{

  aa->aae_flags = 0;		/* not okay */
#ifdef PHASE2
  logit(LOG_LOTS, "deleting arp entry: node %d/%d.%d",
 		 aa->aae_pa.node, aa->aae_pa.dummy[1], aa->aae_pa.dummy[2]);
#else  PHASE2
  logit(LOG_LOTS, "deleting arp entry: node %d", aa->aae_pa.node);
#endif PHASE2
  en = (AARP_ENTRY *)h_delete(aih->ai_aarptab, aa->aae_pa.node);
  if (en != aa) {
    logit(LOG_BASE, "when deleting arp entry, freed entry was %x, wanted %x",
	en, aa);
  }
  aa->aae_next = aarptab_free_list;
  aarptab_free_list = aa;	/* mark */
}
#endif notdef

/* scan arp table and see if anything needs going */
aarptab_scan()
{
  struct timeval tv;
  AARP_ENTRY *n, *np, *en, *t;

  np = NULL, n = aarptab_node_list;
  while (n) {
    if (n->aae_ttl <= 0) {
#ifdef PHASE2
      logit(LOG_LOTS, "deleting arp entry: node %d/%d.%d",
		n->aae_pa.node, n->aae_pa.dummy[1], n->aae_pa.dummy[2]);
#else  PHASE2
      logit(LOG_LOTS, "deleting arp entry: node %d", n->aae_pa.node);
#endif PHASE2
      en = (AARP_ENTRY *)h_delete(n->aae_aarptab, &n->aae_pa);
      if (en != n) {
	logit(LOG_BASE,"when deleting arp entry, freed entry was %x, wanted %x",
	    en, n);
      }
      if (np)
	np->aae_next = n->aae_next; /* unlink */
      else
	aarptab_node_list = n->aae_next; /* at head of list */
      /* link onto free list */
      t = n->aae_next;		/* remember next */
      n->aae_next = aarptab_free_list;
      aarptab_free_list = n;	/* mark */      /* delete it */
      n = t;			/* advance n */
      continue;			/* and skip regular advance */
    } else
      n->aae_ttl--;		/* dec. ttl */
    np = n;			/* remeber previous */
    n = n->aae_next;		/* advance n */
  }
  tv.tv_sec = AARP_SCAN_TIME;
  tv.tv_usec = 0;
  relTimeout(aarptab_scan, 0, &tv, TRUE);
}

/*
 * initialize AARP module
 *
 * arguments are: dev, devno of the interface
 *   maxnodes specfies the maximum number of nodes wanted on this
 *     interface (usually one)
 *
 * returns pointer to a AI_HANDLE - as caddr_t
 *
*/
caddr_t
aarp_init(dev, devno, maxnodes)
char *dev;
int devno;
int maxnodes;
{
  int aarp_listener();
  AI_HANDLE *aih = NULL;
  static int here_before = 0;
  int ph = -1;

  if (!here_before) {
    struct timeval tv;

    here_before = 1;
    aarptab_node_list = NULL;	/* clear */
    aarptab_free_list = NULL;
    tv.tv_sec = AARP_SCAN_TIME;
    tv.tv_usec = 0;
    relTimeout(aarptab_scan, 0, &tv, TRUE);
  }

#ifdef sony_news
  ph = devno;
#else  sony_news
#ifdef AARPD
  if ((ph = pi_open(ETHERTYPE_AARP, -1, dev, devno)) < 0) {
#else AARPD
  if ((ph = pi_open(ETHERTYPE_AARP, dev, devno)) < 0) {
#endif AARPD
    logit(L_UERR|LOG_LOG,"pi_open: aarp_init");
    return(NULL);
  }
#endif sony_news

  if ((aih = (AI_HANDLE *)malloc(sizeof(AI_HANDLE))) == NULL)
    goto giveup;
  if ((aih->ai_nodes = (struct ai_host_node *)
       malloc(sizeof(struct ai_host_node)*maxnodes)) == NULL)
    goto giveup;

  /* clear host node table */
  bzero(aih->ai_nodes, sizeof(struct ai_host_node)*maxnodes);
  aih->ai_flags = 0;		/* clear flags for interface */
  aih->ai_numnode = 0;		/* no nodes yet */
  aih->ai_maxnode = maxnodes;	/* max # of nodes (usually 1) */
  aih->ai_ph = ph;
  /* get ethernet address */
  if ((pi_get_ethernet_address(ph, aih->ai_eaddr)) < 0)
    goto giveup;

#ifdef sony_news
  logit(LOG_BASE,"Ethernet address is %02x:%02x:%02x:%02x:%02x:%02x",
#else  sony_news
  logit(LOG_BASE,"Ethernet address for %s%d is %02x:%02x:%02x:%02x:%02x:%02x",
      dev, devno, 
#endif sony_news
      aih->ai_eaddr[0], aih->ai_eaddr[1], aih->ai_eaddr[2],
      aih->ai_eaddr[3], aih->ai_eaddr[4], aih->ai_eaddr[5]);

  /* setup arp table */
  if ((aih->ai_aarptab = aarptab_init()) == NULL)
    goto giveup;
  aih->ai_accesses = 0;		/* no access to table yet */

#ifndef sony_news
  /* establish listener now */
  pi_listener(aih->ai_ph, aarp_listener, (caddr_t)aih);
#endif sony_news

  aarp_inited = 1;

  return((caddr_t)aih);
 giveup:
  if (ph >= 0)
    pi_close(ph);
  if (aih) {
    if (aih->ai_nodes)
      free(aih->ai_nodes);
    free(aih);
  }
  return(NULL);
}

/*
 * get a host node address
 *
*/
export int
aarp_get_host_addr(aih, pa, which)
AI_HANDLE *aih;
struct ethertalkaddr *pa;
int which;
{
  if (aih->ai_numnode > which &&
      aih->ai_nodes[which].aihn_state == AI_NODE_OKAY) {
    *pa = aih->ai_nodes[which].aihn_pa; /* copy it */
    /* should we count pa node address? */
#ifdef PHASE2
    return(4);
#else  PHASE2
    return(1);
#endif PHASE2
  }
  return(-1);
}

#ifdef PHASE2
/*
 * set a host node address
 *
*/
export int
aarp_set_host_addr(aih, pa)
AI_HANDLE *aih;
struct ethertalkaddr *pa;
{
  int i = aih->ai_numnode;
  struct ai_host_node *an = aih->ai_nodes;

  for ( ; i ; an++, i--)
    if (an->aihn_state == AI_NODE_OKAY)
      an->aihn_pa = *pa;

  if (i == 0)
  	return(0);

  return(-1);
}
#endif PHASE2

/*
 * returns -1: can't be resolved
 *          0: resolving, come back later
 *          1: resolved
 *
 * Resolves the ethernet address of the specified destination.
 * Special cases are:
 *   dstnode == 0xff -> broadcast
 *   srcnode != ourselves - error
 *   dstnode == ourselves - our hw addr (maybe should be error if we
 *        can't send to ourselves)
 * In general, algorithm is:
 *  check internal table.  If entry is valid and if the ttl is valid
 *  return the address from the table
 * If not in the table or ttl is bad, then do a request -- however,
 *  the current packet WILL drop since the aarp request will not block
 *
*/
export int
aarp_resolve(aih, tpa, wantbr, eaddr)
AI_HANDLE *aih;
struct ethertalkaddr *tpa;
int wantbr;
u_char **eaddr;
{
  struct timeval *tv;
  AARP_ENTRY *aa;

  if (wantbr) {
    *eaddr = b_eaddr;	/* want to broadcast */
    return(1);
  }

  /* allow target to be ourselves */
  if (host_node(aih, tpa, AI_NODE_OKAY)) {
    *eaddr = aih->ai_eaddr;
    return(1);
  }

  /* find the node in the table */
  if ((aa = aarptab_find(aih, tpa)) != NULL && aa->aae_flags & AARP_OKAY) {
    if (aa->aae_flags & (AARP_PERM)) {
      *eaddr = aa->aae_eaddr;
      return(1);
    }
    if (aa->aae_ttl > 0) {
      *eaddr = aa->aae_eaddr;	/* yes, return valid */
      return(1);
    }
  }
  /* query on interface */
  aarp_request(aih, tpa);
  return(0);
}

/*
 * call with hardware address, protocol address
 * flag is TRUE if insert is "hard" FALSE o.w.
 *  -- A HARD insert will smash what is there
 *  -- A SOFT insert will return FALSE if the ha's don't match and
 *       invalidate the cache entry
 * If the insert succeeds, then the ttl is bumped to the normal level
 *
 * Note: aarp_insert performs the gleaning functions of the aarp spec.
*/
export int
aarp_insert(aih, ha, pa, hard)
AI_HANDLE *aih;
u_char *ha;
struct ethertalkaddr *pa;
int hard;
{
  AARP_ENTRY *aa;
  struct timeval *tv;

  /* check to make sure ha isn't broadcast (or multicast)! */
  if (bcmp(ha, b_eaddr, EHRD) == 0)
    return(FALSE);
#ifdef PHASE2
  if (ha[0] == 0x09 && ha[1] == 0x00 && ha[2] == 0x07 && ha[3] == 0x00)
    return(FALSE); /* zone multicast address */
  logit(LOG_LOTS,"Got mapping for %d/%d.%d",
			pa->node, pa->dummy[1], pa->dummy[2]);
#else  PHASE2
  logit(LOG_LOTS, "Got mapping for %d", pa->node);
#endif PHASE2
  dumpether(LOG_LOTS, "  Address", ha);

  aa = aarptab_get(aih, pa); /* find it or get a new one */
  if (aa->aae_flags & AARP_OKAY) {
    if (aa->aae_flags & (AARP_PERM)) /* don't reset these */
      return(FALSE);
    if (hard)
#ifdef PHASE2
      logit(LOG_LOTS, "Resetting an old mapping for node %d/%d.%d",
			pa->node, pa->dummy[1], pa->dummy[2]);
#else  PHASE2
      logit(LOG_LOTS, "Resetting an old mapping for node %d", pa->node);
#endif PHASE2
  }
  /* if arp entry is in cache and we aren't doing a hard update */
  /* and the hardware addresses don't match, don't do an update */
  if ((aa->aae_flags & AARP_OKAY) && !hard) {
    if (bcmp((caddr_t)aa->aae_eaddr, (caddr_t)ha, EHRD) != 0) {
      logit(LOG_BASE, "AARP RESET - ethernet address mismatch");
#ifdef PHASE2
      logit(LOG_BASE,"node number is %d/%d.%d",
			pa->node, pa->dummy[1], pa->dummy[2]);
#else  PHASE2
      logit(LOG_BASE,"node number is %d", pa->node);
#endif PHASE2
      dumpether(LOG_BASE, "incoming address", ha);
      dumpether(LOG_BASE, "cached address",aa->aae_eaddr);
      aa->aae_flags &= ~AARP_OKAY; /* there we go */
      return(FALSE);
    }
  } else {
    bcopy((caddr_t)pa, (caddr_t)&aa->aae_pa, sizeof(aa->aae_pa));
    bcopy((caddr_t)ha, (caddr_t)aa->aae_eaddr, EHRD); /* copy in mapping */
    aa->aae_flags = AARP_OKAY;	/* reset resolving flag if set too */
  }
  aa->aae_ttl = AARP_TTL;	/* reset */
  return(TRUE);
}

/*ARGSUSED*/
#ifdef sony_news
aarp_listener(fd, aih, proth, buf, len)
#else  sony_news
private
aarp_listener(fd, aih, proth)
#endif sony_news
int fd;				/* dummy */
AI_HANDLE *aih;
int proth;
#ifdef sony_news
u_char *buf;
int len;
#endif sony_news
{
  struct ethertalkaddr *dpa, *spa;
  byte *sha;
  struct ether_arp arp;

  struct ai_host_node *hn;

  /* note, we use read protocol because we cannot trust incoming etalk */
  /* addresses from DLI on Ultrix if sent to broadcast */
#ifdef sony_news
  bcopy(buf, &arp, sizeof(arp));
#else  sony_news
  if (pi_read(aih->ai_ph, &arp, sizeof(arp)) < 0)
    return;
#endif sony_news

  if (ntohs(arp.arp_hrd) != ARPHRD_ETHER || /* not ethernet? */
      ntohs(arp.arp_pro) != ETHERTYPE_APPLETALK || /* not appletalk? */
      arp.arp_hln != EHRD ||	/* wrong hrdwr length */
      arp.arp_pln != ETPL) {	/* wrong protocol length? */
    logit(5, "AARP drop!: %d %d %d %d",
	arp.arp_hrd, arp.arp_pro, arp.arp_hln, arp.arp_pln);
    return;
  }

  dpa = (struct ethertalkaddr *)arp.arp_tpa; /* reformat */
  spa = (struct ethertalkaddr *)arp.arp_spa; /* reformat */
  /* copy these because sunos4.0 they are struct's rather than arrays */
  /* and we need to take the address and hate the way the warning */
  /* messages come up, besides saves us a bit on dereferncing */
#if defined(sony_news) || defined(__osf__)
  sha = (byte *)&(arp.arp_sha[0]);
#else  sony_news
  sha = (byte *)&arp.arp_sha;
#endif sony_news

  /* check dummy bytes? */
  /* this is the first one of these, so ...  the reason we have an & */
  /* in front of an array is that under sunos4.0, arp_sha, etc. are */
  /* now structs */
  if (bcmp((caddr_t)sha, (caddr_t)aih->ai_eaddr, EHRD) == 0) {
    logit(LOG_LOTS, "Packet from self");
    return;
  }
  if (host_node(aih, spa, AI_NODE_OKAY)) {
#ifdef PHASE2
    logit(LOG_BASE, "Address conflict %d/%d.%d\n",
			spa->node, spa->dummy[1], spa->dummy[2]);
#else  PHASE2
    logit(LOG_BASE, "Address conflict %d\n", spa->node);
#endif PHASE2
  }

  switch (ntohs(arp.arp_op)) {
  case ARPOP_PROBE:
    if ((hn = host_node(aih, spa, 0))) {
      switch (hn->aihn_state) {
      case AI_NODE_OKAY:
	aarp_reply(aih, &hn->aihn_pa, &arp); /* defend! */
	break;
      case AI_NODE_PROBE:
	hn->aihn_state = AI_NODE_UNUSED;
	break;
      default:
	break;
      }
      break;
    }
    aarp_probed(aih, sha, spa, arp); /* check over entry since we got probe */
    break;
  case ARPOP_REQUEST:
    if (host_node(aih, spa, 0))	/* drop if request from one of */
      break;			/* our address */
    /* probably should be more discriminating here */
    /* insert new entry */
    aarp_insert(aih, sha, (struct ethertalkaddr *)arp.arp_spa, TRUE); 
    if ((hn = host_node(aih, dpa, AI_NODE_OKAY)))
      aarp_reply(aih, &hn->aihn_pa, &arp);
    break;
  case ARPOP_REPLY:
    /* check to see that dpa isn't one we are probing */
    /* if it is, then things are bad for that potential addr */

    if ((hn = host_node(aih, dpa, AI_NODE_PROBE))) {
      hn->aihn_state = AI_NODE_UNUSED;
      break;
    }
    /* drop if source was self (whether ready or not) */
    /* don't need check before here because replies are not broadcast, */
    /* so why would we get it :-) */
    if (host_node(aih, spa, 0))
      break;
    /* mark in our tables */
    aarp_insert(aih, sha, spa, TRUE);
    break;
  }
}

/* AARP PROBE: some hooks in the listener too */
/*
 * initialize and start AARP probe process
 *
*/
struct aarp_aphandle {
  AI_HANDLE *aaph_aih;		/* protocol handle */
  AARP_ENTRY *aaph_ae;		/* request: point to node handle */
  struct ai_host_node *aaph_node; /* probe: which node # probing */
  struct ether_arp aaph_arp;	/* arp */
  int (*aaph_callback)();	/* callback on success on request/probe */
  caddr_t aaph_callback_arg;	/* and initial argument */
  int aaph_callback_result;	/* this is the result */
  int aaph_tries;		/* tries to do */
};

/*
 * acquire an ethertalk node 
 * doesn't try new node address 
 * callback is (*callback)(callback_arg, result)
 *  where result < 0 if couldn't
 *        result >= 0 ==> index of host node address for get host addr
 *
 * return: > 0 - bad node
 *         = 0 - okay
 *         < 0 - internal error
 *
*/

/* probe state */
export int
aarp_acquire_etalk_node(aih, initial_node, callback, callback_arg)
AI_HANDLE *aih;
struct ethertalkaddr *initial_node;
int (*callback)();		/* callback routine */
caddr_t callback_arg;		/* arg for callback */
{
  struct aarp_aphandle *probe;
  struct ai_host_node *ahn;
  int whichnode = -1;
  int i;
    
  /* bad node */
  if (initial_node->node == 0 || initial_node->node == 255)
    return(1);
#ifdef PHASE2
  if (initial_node->node == 254)	/* reserved */
    return(1);
#endif PHASE2
  if (aarptab_find(aih, initial_node) || host_node(aih, initial_node, 0)) {
    /* callback ? */
    return(1);
  }

  /* get probe structure */
  probe = (struct aarp_aphandle *)malloc(sizeof(struct aarp_aphandle));
  if (probe == NULL)
    return(-1);
  m_probe++;
  for (ahn = aih->ai_nodes, i = 0; i < aih->ai_numnode; i++, ahn++) {
    if (ahn->aihn_state)	/* node in use or in probe */
      continue;
    whichnode = i;		/* got a good one, ahn set */
    break;
  }
  if (whichnode < 0) {		/* no node allocate? */
    if (aih->ai_numnode >= aih->ai_maxnode) /* can't */
      return(-1);
    whichnode = aih->ai_numnode++; /* next node */
    ahn = &aih->ai_nodes[whichnode]; /* get node */
  }

  /* initialize probe arp packet */
  bzero(&probe->aaph_arp, sizeof(probe->aaph_arp));
  probe->aaph_arp.arp_hrd = htons(ARPHRD_ETHER);
  probe->aaph_arp.arp_pro = htons(ETHERTYPE_APPLETALK);
  probe->aaph_arp.arp_op = htons(ARPOP_PROBE);
  probe->aaph_arp.arp_hln = EHRD;
  probe->aaph_arp.arp_pln = ETPL;
#if defined(sony_news) || defined(__osf__)
  bcopy((caddr_t)aih->ai_eaddr, (caddr_t)&(probe->aaph_arp.arp_sha[0]), EHRD);
#else  sony_news
  bcopy((caddr_t)aih->ai_eaddr, (caddr_t)&probe->aaph_arp.arp_sha, EHRD);
#endif sony_news

  bcopy((caddr_t)initial_node, (caddr_t)probe->aaph_arp.arp_spa, ETPL);
  bcopy((caddr_t)initial_node, (caddr_t)probe->aaph_arp.arp_tpa, ETPL);

  probe->aaph_tries = AARP_PROBE_TRY;

  probe->aaph_callback = callback;
  probe->aaph_callback_arg = callback_arg;

  probe->aaph_aih = aih;	/* handle for interface */
  probe->aaph_ae = NULL;	/* mark as none */

  probe->aaph_callback_result = whichnode; /* mark */
  probe->aaph_node = ahn;	/* remember it */
  ahn->aihn_state = AI_NODE_PROBE; /* mark in probe state */
  ahn->aihn_pa = *initial_node;	/* copy pa */

  /* do first probe */
  probe_and_request_driver(probe);
  return(0);			/* okay, we are trying! */
}

/* main probe and request driver */
private
probe_and_request_driver(aphandle)
struct aarp_aphandle *aphandle;
{
  register AARP_ENTRY *enp;

  struct timeval tv;
  struct ai_host_node *ahn;
  AI_HANDLE *aih = aphandle->aaph_aih;
#ifdef PHASE2
  char abuf[sizeof(struct ether_arp)+8];
#endif PHASE2

  /* either of these can be running or both - both really doesn't */
  /* make much sense though */
  enp = aphandle->aaph_ae;		/* get pointer to node handle */
  ahn = aphandle->aaph_node;

  if (enp && ahn) {
    logit(LOG_LOG, "internal error: probe and request set in aarp driver\n");
    logit(LOG_LOG|L_EXIT, "fatal: please report\n");
  }

  if (enp && (enp->aae_flags & AARP_RESOLVING) == 0)
    goto dofree;
  if (ahn && ahn->aihn_state == AI_NODE_UNUSED) {
    /* tell caller that probe for the node address failed */
    if (aphandle->aaph_callback)
      (*aphandle->aaph_callback)(aphandle->aaph_callback_arg, -1);
    goto dofree;
  }
  if (aphandle->aaph_tries <= 0) { /* nothing left to do? */
    /* if we are out of tries, then node has been acquired, hurrah! */
    if (ahn) {
      ahn->aihn_state = AI_NODE_OKAY;
    }
    if (enp) {
      enp->aae_flags &= ~AARP_RESOLVING; 
    }
    if (aphandle->aaph_callback)
      (*aphandle->aaph_callback)(aphandle->aaph_callback_arg,
				 aphandle->aaph_callback_result);
    goto dofree;
  }
  /* set timeout to next try */
  aphandle->aaph_tries--;
  if (enp) {
    /* should decay based on number of tries */
    tv.tv_sec = AARP_REQUEST_TIMEOUT_SEC;
    tv.tv_usec = AARP_REQUEST_TIMEOUT_USEC;
  }
  if (ahn) {
    /* set timeout to next try */
    tv.tv_sec = AARP_PROBE_TIMEOUT_SEC;
    tv.tv_usec = AARP_PROBE_TIMEOUT_USEC;
  }
#ifdef PHASE2
  /* make room for the rest of the ELAP header */
  bcopy(&aphandle->aaph_arp, abuf+8, sizeof(struct ether_arp));
  if (pi_write(aih->ai_ph, abuf, sizeof(abuf), b_eaddr) < 0) 
#else  PHASE2
#ifdef sony_news
  if (pi_write(aih->ai_ph,&aphandle->aaph_arp, sizeof(struct ether_arp),
		    b_eaddr, ETHERTYPE_AARP) < 0) 
#else  sony_news
  if (pi_write(aih->ai_ph,&aphandle->aaph_arp, sizeof(struct ether_arp),
		    b_eaddr) < 0) 
#endif sony_news
#endif PHASE2
    logit(LOG_BASE|L_UERR, "pi_write failed: aarp driver");
  /* setup timeout to next (relative timeout) */
  relTimeout(probe_and_request_driver, aphandle, &tv, TRUE);
  return;
dofree:
  while (remTimeout(probe_and_request_driver, aphandle))
    /* remove while some on queue: NULL STATEMENT */;
  /* address was resolving, nothing left */
  free((caddr_t)aphandle);
  f_probe++;
}


/* 
 * got a probe on the specified ha, pa pair and pa is not one of ours
 * 
 * mark the mapping pair as suspect.  If the specified ha is
 * different, then mark it as very suspect.  In both cases, the
 * "suspect" flag is moving down the ttl of the mapping.
 *
*/
private void
aarp_probed(aih, ha, pa, arp)
AI_HANDLE *aih;
u_char *ha;
struct ethertalkaddr *pa;
struct ether_arp *arp;
{
  AARP_ENTRY *aa;
  struct timeval *tv;
  int nttl;

  if ((aa = aarptab_find(aih, pa)) == NULL) /* find entry */
    return;			/* nothing */
  if ((aa->aae_flags & AARP_OKAY) == 0) /* nothing to do? */
    return;			/* ;right */
  /* get reduction factor on ttl */
  nttl = (bcmp((caddr_t)ha, (caddr_t)aa->aae_eaddr, EHRD) == 0) ?
    AARP_PROBED_SAME : AARP_PROBED_DIFF;
  /* update ttl to lessor of new or old */
  if (nttl < aa->aae_ttl)
    aa->aae_ttl = nttl;
}

/*
 * reply to an arp request packet
 *
 * note, smashes the arp packet
 *
*/
private void
aarp_reply(aih, pa, arp)
AI_HANDLE *aih;
struct ethertalkaddr *pa;
struct ether_arp *arp;
{
  caddr_t sha, tha;
#ifdef PHASE2
  char abuf[sizeof(struct ether_arp)+8];
#endif PHASE2

#if defined(sony_news) || defined(__osf__)
  sha = (caddr_t)&(arp->arp_sha[0]);	/* need & because can be struct */
  tha = (caddr_t)&(arp->arp_tha[0]);
#else  sony_news
  sha = (caddr_t)&arp->arp_sha;	/* need & because can be struct */
  tha = (caddr_t)&arp->arp_tha;
#endif sony_news

  bcopy(sha, tha, EHRD);
  bcopy((caddr_t)arp->arp_spa, (caddr_t)arp->arp_tpa, ETPL);
  arp->arp_op = htons(ARPOP_REPLY);
  bcopy((caddr_t)aih->ai_eaddr, sha, EHRD);
  /* copy in our node */
  bcopy((caddr_t)pa, (caddr_t)arp->arp_spa, ETPL);
#ifdef PHASE2
  bcopy(arp, abuf+8, sizeof(struct ether_arp));
  if (pi_write(aih->ai_ph, abuf, sizeof(abuf), tha) < 0) {
#else  PHASE2
#ifdef sony_news
  if (pi_write(aih->ai_ph, arp, sizeof(*arp), tha, ETHERTYPE_AARP) < 0) {
#else  sony_news
  if (pi_write(aih->ai_ph, arp, sizeof(*arp), tha) < 0) {
#endif sony_news
#endif PHASE2
    logit(LOG_LOG|L_UERR, "etsend");
  }
}

/*
 * do an aarp request
 * - doesn't check that target is ourselves
*/
private void
aarp_request(aih, tpa)
AI_HANDLE *aih;
struct ethertalkaddr *tpa;
{
  AARP_ENTRY *aa;
  struct aarp_aphandle *ap;
#ifdef PHASE2
  int forcePROBE = tpa->dummy[0];
  /* can you say 'hack' */
  tpa->dummy[0] = 0;
#endif PHASE2

  if ((aa = aarptab_get(aih, tpa)) == NULL) /* get new */
    return;
  if (aa->aae_flags & (AARP_RESOLVING))
    return;
  /* bad condition */
  if (aa->aae_flags & (AARP_PERM))
    return;
  ap = (struct aarp_aphandle *)malloc(sizeof(struct aarp_aphandle));
  m_probe++;
  aa->aae_pa = *tpa;
  aa->aae_flags |= AARP_RESOLVING;
  aa->aae_flags &= ~AARP_OKAY;	/* make sure! (can be) */

  /* establish arp packet */
  bzero(&ap->aaph_arp, sizeof(ap->aaph_arp));
#ifdef PHASE2
  ap->aaph_arp.arp_op = htons((forcePROBE) ? ARPOP_PROBE : ARPOP_REQUEST);
#else  PHASE2
  ap->aaph_arp.arp_op = htons(ARPOP_REQUEST);
#endif PHASE2
  ap->aaph_arp.arp_hrd = htons(ARPHRD_ETHER);
  ap->aaph_arp.arp_pro = htons(ETHERTYPE_APPLETALK);
  ap->aaph_arp.arp_hln = EHRD;
  ap->aaph_arp.arp_pln = ETPL;
#ifdef notdef
  bcopy((caddr_t)snode->aae_eaddr, (caddr_t)&ap->aaph_arp.arp_sha, EHRD);
  bcopy((caddr_t)&snode->aae_pa, (caddr_t)ap->aaph_arp.arp_spa,
	sizeof(struct ethertalkaddr));
#else
#if defined(sony_news) || defined(__osf__)
  bcopy((caddr_t)aih->ai_eaddr, (caddr_t)&(ap->aaph_arp.arp_sha[0]), EHRD);
#else  sony_news
  bcopy((caddr_t)aih->ai_eaddr, (caddr_t)&ap->aaph_arp.arp_sha, EHRD);
#endif sony_news
  /* grab any source protocol address (on right interface already!) */
  { int i = aih->ai_numnode;
    struct ai_host_node *an = aih->ai_nodes;

    for ( ; i ; an++, i--)
      if (an->aihn_state == AI_NODE_OKAY) {
	bcopy((caddr_t)&an->aihn_pa, (caddr_t)ap->aaph_arp.arp_spa,
	      sizeof(struct ethertalkaddr));
	break;
      }
    if (i == 0)			/* no host node! drop request for now */
      return;
  }
#endif
  bcopy((caddr_t)tpa, (caddr_t)ap->aaph_arp.arp_tpa, sizeof(*tpa));
#ifdef PHASE2
  if (forcePROBE)
    bcopy((caddr_t)tpa, (caddr_t)ap->aaph_arp.arp_spa, sizeof(*tpa));
#endif PHASE2

  ap->aaph_ae = aa;		/* remember this for later */
  ap->aaph_tries = AARP_REQUEST_TRY;
  ap->aaph_node = NULL;		/* clear this */
  ap->aaph_aih = aih;
  ap->aaph_callback = NULL;

  probe_and_request_driver(ap);
}


/*
 * See if the specified ethertalk address "n" is a host address
 * return true if it is, false o.w.
 *
 * if flag is set, then it indicates that we only want to check against
 *  host nodes in the state flag is set to.
 *
*/
private struct ai_host_node *
host_node(aih, n, flag)
AI_HANDLE *aih;
struct ethertalkaddr *n;
{
  register int i = aih->ai_numnode;
  register struct ai_host_node *ai_nodes = aih->ai_nodes;

  for ( ; i  ; ai_nodes++, i--)
    if (ai_nodes->aihn_state) {
      if (flag && ai_nodes->aihn_state != flag)
	continue;
#ifdef PHASE2
      if (ai_nodes->aihn_pa.node
       && ai_nodes->aihn_pa.dummy[1] == n->dummy[1]
       && ai_nodes->aihn_pa.dummy[2] == n->dummy[2]
       && ai_nodes->aihn_pa.node == n->node)
	return(ai_nodes);
#else  PHASE2
      if (ai_nodes->aihn_pa.node && ai_nodes->aihn_pa.node == n->node)
	return(ai_nodes);
#endif PHASE2
    }
  return(NULL);
}


export
aarp_dump_stats(fd, ah)
FILE *fd;
AI_HANDLE *ah;
{
  struct hash_statistics *s = h_statistics(ah->ai_aarptab);

  fprintf(fd, " aarp hash table statistics, %d nodes in use\n",
	  s->hs_used);
  fprintf(fd, "\t%d lookups since last rehash, average distance %.02f\n",
	  s->hs_lnum, s->hs_lnum ? ((float)s->hs_lsum / s->hs_lnum) : 0.0);
  fprintf(fd, "\t%d lookups total, average distance %.02f\n",
	  s->hs_clnum, s->hs_clnum ? ((float)s->hs_clsum / s->hs_clnum) : 0.0);
  putc('\n', fd);
  fprintf(fd, " %d aarptab entries allocated\n", m_aarptab);
  fprintf(fd, " %d probe/request handles, %d freed\n", m_probe, f_probe);
  putc('\n', fd);
}

export
aarp_dump_tables(fd, aih)
FILE *fd;
AI_HANDLE *aih;
{
  AARP_ENTRY *n;

#ifdef AARP_DUMP_TABLE
  fprintf(fd," ARP table dump for interface\n");
  for (n = aarptab_node_list; n ; n = n->aae_next) {
    if (aih->ai_aarptab != n->aae_aarptab || n->aae_flags == 0)
      continue;
    fprintf(fd,
       "  node %d, %02x:%02x:%02x:%02x:%02x:%02x, used %d flags %s%s%sttl %d\n",
	    n->aae_pa.node, 
	    n->aae_eaddr[0], n->aae_eaddr[1], n->aae_eaddr[2], 
	    n->aae_eaddr[3], n->aae_eaddr[4], n->aae_eaddr[5],
	    n->aae_used,
	    AARP_FNAME_OKAY(n->aae_flags),
	    AARP_FNAME_RESOLVING(n->aae_flags),
	    AARP_FNAME_PERM(n->aae_flags), n->aae_ttl);
  }
  putc('\n', fd);
#endif
}
