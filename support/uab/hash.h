/*
 * $Author: djh $ $Date: 91/02/15 23:07:37 $
 * $Header: hash.h,v 2.1 91/02/15 23:07:37 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * hash.h - external definitions for hash.c - generalized hashing function
 *
 *  written by Charlie C. Kim
 *     Academic Networking, Communications and Systems Group
 *     Center For Computing Activities
 *     Columbia University
 *   September 1988
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
 *  Sept 5, 1988  CCKim Created
 *  Sept 6, 1988  CCKim Finished: level 0
 *
*/

#ifndef _HASH_HEADER_INCLUDED
#define _HASH_HEADER_INCLUDED "yes"

/* hash table operations recognized */
#define HASH_OP_DELETE 2
#define HASH_OP_MEMBER 1 
#define HASH_OP_INSERT 0
#define HASH_OP_NUM 3

#define HASH_OP_VALID(n) ((n) < HASH_OP_NUM && (n) >= HASH_OP_INSERT)
#define HASH_OP_INVALID(n) ((n) >= HASH_OP_NUM || (n) < HASH_OP_INSERT)

/* hashing collision policies */
#define HASH_POLICY_OLD -1	/* old for redefine */
#define HASH_POLICY_CHAIN 0	/* chain on collisions */
#define HASH_POLICY_LINEAR_PROBE 1 /* linear probes on collisions */
#define HASH_POLICY_DOUBLE_HASH 2 /* double hash on collisions */

/* given a hash policy, returns true if it is a valid policy */
#define HASH_POLICY_VALID(n) ((n) <= HASH_POLICY_DOUBLE_HASH && \
			      (n) >= HASH_POLICY_CHAIN)
#define HASH_POLICY_LINKSNEEDED(n) ((n) == HASH_POLICY_CHAIN)

/* hash function types */
#define HASH_TYPE_OLD -1	/* old for redefine */
#define HASH_TYPE_OWN 0		/* our own */
#define HASH_TYPE_DIVISION 1	/* division method */
#define HASH_TYPE_MULTIPLICATIVE 2 /* multiplicative method */
/* for multiplicative mode: try for fibonacci */
/* only valid for 32 bit machines! */
#define A_MULTIPLIER 2630561063 /* gotta figure out a good one */

#define HASH_TYPE_NUM 3

#define HASH_TYPE_VALID(n) ((n) < HASH_TYPE_NUM && \
  (n) >= HASH_TYPE_OWN)


/*
 * structure to allow operations other than a linear list off a hash
 * bucket in the chain policy
 *
*/
struct hash_bucket_list_ops {
  caddr_t (*hlo_find)();	/* find a member */
  caddr_t (*hlo_insert)();	/* insert a member (returns ptr to */
				/* data) */
  int (*hlo_delete)();		/* delete a member */
  caddr_t (*hlo_get)();		/* get any member and remove from list */
};

/* averages are fixed decimal with 2 digits after the decimal */
/* they are kept in case we want to do something when average */
/* distances get too large */
struct hash_statistics {
  int hs_buckets;		/* number of buckets in table */
  /* describes # of entries in chain */
  int hs_used;			/* # of buckets filled */
  /* describes table (not accurate for chain policy) */
  int hs_davg;			/* average distance from hash index */
  int hs_dsum;			/* sum of distances from hash index */
  int hs_dmax;			/* maximum distance from hash index */
  /* describes lookup patterns (describes distance into linear table */
  /* if the policy is chain */
  int hs_lnum;			/* remember number of lookups */
  int hs_lsum;			/* sum of lookup distances */
  int hs_lavg;			/* average lookup distance */
  /* cumulative for lookup patterns (describes overall efficiency) */
  int hs_clnum;			/* remember number of lookups */
  int hs_clsum;			/* sum of lookup distances */
};

/* function declarations */
caddr_t h_new();		/* create new table */
caddr_t h_operation();		/* hash operations */
struct hash_statistics *h_statistics(); /* returns stats on a table */
void h_free();			/* free a table */
/* must specify policy and type */
caddr_t h_redefine();		/* redefine operating parameters for a */
				/* hash table, forces a rehash */
#define h_rehash(ht,M) (h_redefine((ht),HASH_POLICY_OLD,HASH_TYPE_OLD,\
				   (M),NULL,NULL,NULL,NULL,NULL,NULL))
/* call hash operation for these */
#define h_member(ht,key) (h_operation(HASH_OP_MEMBER,(ht),(key),-1,-1,\
				      NULL,NULL))
#define h_insert(ht,key) (h_operation(HASH_OP_INSERT,(ht),(key),\
					  -1,-1, NULL, NULL))
#define h_delete(ht,key) (h_operation(HASH_OP_DELETE,(ht),(key),-1,-1,\
				       NULL,NULL))


#endif /* _HASH_HEADER_INCLUDED */
