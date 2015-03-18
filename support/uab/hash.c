static char rcsid[] = "$Author: djh $ $Date: 1994/10/10 08:56:13 $";
static char rcsident[] = "$Header: /mac/src/cap60/support/uab/RCS/hash.c,v 2.4 1994/10/10 08:56:13 djh Rel djh $";
static char revision[] = "$Revision: 2.4 $";

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

static char columbia_copyright[] = "Copyright (c) 1988 by The Trustees of \
Columbia University in the City of New York";

#include <stdio.h>
#include <sys/types.h>
#include <netat/sysvcompat.h>
#include "hash.h"

#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE 1
#endif
#ifndef PRIVATE
# define PRIVATE static
#endif


/*
 * holds and describes a hash table
 *
 * ht_policy: policy on collisions (cf hash.h)
 * ht_cfunc: takes (key, data) and returns true if they are equal
 * ht_afunc: takes a key and returns the item from higher up
 * ht_cpfunc: should compress the key to a integer -- only required if
 *    no hash function has been provided
 * ht_hfunc: takes (M, data) as arguments - returns hash index
 *    M is the sizeof(int)*8 - log of the table size if the hashing
 *     type is multiplicative
 * ht_hfunc2: is the secondary hashing function for double hashing
 * ht_chainops: chaining function other than linked list
 * ht_stats: describes performance of hash table
 * ht_type: hash function type
 * ht_buckets: pointer to the hash table buckets
 *
*/
typedef struct htable {
  int ht_M;			/* # of hash table buckes */
  int ht_logM;			/* M or log M (for certain hash types) */
  int ht_policy;		/* hashing policies for collision */
  /* alway call with passed key first, data item second */
  int (*ht_cfunc)();		/* comparision function */
  caddr_t (*ht_afunc)();	/* allocate function */
  u_int (*ht_cpfunc)();		/* compression function */
  u_int (*ht_hfunc)();		/* hashing function */
  u_int (*ht_hfunc2)();		/* secondary hashing function */
  struct hash_bucket_list_ops *ht_chainops; /* chain functions */
  int ht_type;			/* hash type */
  struct hash_statistics ht_stats; /* statisitics */
  caddr_t *ht_buckets;		/* actual hash table */
} HTABLE;

/* some hash functions */
PRIVATE u_int hash_multiplicative();
PRIVATE u_int hash_division();
PRIVATE u_int hash2_multiplicative();
PRIVATE u_int hash2_division();

/* list operations */
PRIVATE caddr_t list_find();
PRIVATE caddr_t list_insert();
PRIVATE int list_delete();
PRIVATE caddr_t list_get();

/* basic hash bucket chaining with an ordered link list */
PRIVATE struct hash_bucket_list_ops hash_chain_by_list = {
  list_find,
  list_insert,
  list_delete,
  list_get
};

/* table of primary hashfunctions */
PRIVATE u_int (*hash_funcs[HASH_TYPE_NUM])() = {
  NULL,
  hash_division,
  hash_multiplicative,
};

/* table of secondary hash functions */
PRIVATE u_int (*hash_funcs2[HASH_TYPE_NUM])() = {
  NULL,				/* own */
  hash2_division,
  hash2_multiplicative,
};

/* free a hash table - free_func gets called to free data */
void
h_free(ht, free_func)
HTABLE *ht;
int (*free_func)();
{
  caddr_t *bt;
  caddr_t p;
  int M, i, policy;
  caddr_t (*get_func)();
  
  M = ht->ht_M;			/* get # of entries */
  bt = ht->ht_buckets;		/* get buckets */
  ht->ht_buckets = NULL;	/* just in case... */
  if (ht->ht_chainops)
    get_func = ht->ht_chainops->hlo_get;
  policy = ht->ht_policy;
  if (bt == NULL)
    return;
  for (i = 0; i < M; i++) {
    if (bt[i] == NULL)
      continue;
    switch (policy) {
    case HASH_POLICY_CHAIN:
      if (get_func == NULL)
	break;
      while ((p = (*get_func)(&bt[i])))
	if (free_func)
	  (*free_func)(p);
      break;
    default:
	if (free_func)
	  (*free_func)(bt[i]);
    }
  }
  free((caddr_t)bt);			/* free old table */
  free((caddr_t)ht);
}

/* setup a new hash table, returns handle for hash table */
caddr_t
h_new(policy, hashtype, M, cfunc, afunc, cpfunc, hfunc, hfunc2, chainops)
int policy;
int hashtype;			/* hash type */
int M;
int (*cfunc)();			/* comparision function */
caddr_t (*afunc)();			/* allocate function */
u_int (*cpfunc)();		/* compression function */
u_int (*hfunc)();		/* hash function */
u_int (*hfunc2)();		/* secondary hash function */
struct hash_bucket_list_ops *chainops;
{
  HTABLE *htable;

  if (cfunc == NULL) {		/* required! */
    fprintf(stderr, "hash table create: no compare function\n");
    return(NULL);
  }
  if (!HASH_TYPE_VALID(hashtype)) {
    fprintf(stderr, "hash table create: invalid type %d\n", hashtype);
    return(NULL);
  }
  if (hashtype == HASH_TYPE_OWN && hfunc == NULL) {
  fprintf(stderr, "hash table create: must give hash function when own set\n");
  return(NULL);
  }
  if (!HASH_POLICY_VALID(policy)) {
    fprintf(stderr, "hash table create: invalid policy %d\n", policy);
    return(NULL);
  }
  if (M <= 0) {
    fprintf(stderr, "hash table create: invalid hash table size %d\n", M);
    return(NULL);
  }
  if ((htable = (HTABLE *)malloc(sizeof(HTABLE))) == NULL)
    return(NULL);
  htable->ht_policy = policy;
  htable->ht_cfunc = cfunc;
  htable->ht_afunc = afunc;
  htable->ht_hfunc = hash_funcs[hashtype];
  if (htable->ht_policy == HASH_POLICY_DOUBLE_HASH)
    htable->ht_hfunc2 = hash_funcs2[hashtype];
  else
    htable->ht_hfunc2 = NULL;
  /* override std. hash functions if specified */
  if (hfunc)
    htable->ht_hfunc = hfunc;
  if (hfunc2)
    htable->ht_hfunc2 = hfunc2;
  htable->ht_cpfunc = cpfunc;
  htable->ht_chainops = chainops ? chainops : &hash_chain_by_list;
  htable->ht_type = hashtype;
  bzero(&htable->ht_stats, sizeof(htable->ht_stats));
  htable->ht_stats.hs_buckets = M;
  htable->ht_M = 0;		/* assume these */
  htable->ht_buckets = NULL;
  return((caddr_t)h_redefine(htable,HASH_POLICY_OLD,HASH_TYPE_OLD, M,
			     NULL, NULL,NULL,NULL, NULL, NULL));
}

/* redefine an existing hash table, will rehash by creating new set of */
/* buckets and killing off old set */
caddr_t
h_redefine(ht, policy, hashtype, M, cfunc, afunc, cpfunc, hfunc, hfunc2,
	   chainops)
HTABLE *ht;
int policy;			/* hashing policy */
int hashtype;			/* hashing type */
int M;				/* size */
int (*cfunc)();			/* comparision function */
caddr_t (*afunc)();		/* allocate function */
u_int (*hfunc)();		/* hash function */
u_int (*hfunc2)();		/* secondary hash function */
u_int (*cpfunc)();		/* compression function */
struct hash_bucket_list_ops *chainops;
{
  int logM, oldM, i, oldPolicy;
  struct hash_bucket_list_ops *oldChainOps;
  caddr_t *bt, *nbt;
  caddr_t p;

  if (!HASH_TYPE_VALID(hashtype) && hashtype != HASH_TYPE_OLD) {
    fprintf(stderr, "hash table create: invalid type %d\n", hashtype);
    return(NULL);
  }
  if (!HASH_POLICY_VALID(policy) && policy != HASH_POLICY_OLD) {
    fprintf(stderr, "hash table create: invalid policy %d\n", policy);
    return(NULL);
  }
  if (M <= 0)			/* zero means base on old */
    M = ht->ht_M;
  if (hashtype == HASH_TYPE_OLD)
    hashtype = ht->ht_type;	/* get old */
  logM = 0;
  switch (hashtype) {
  case HASH_TYPE_MULTIPLICATIVE:
    i = M >> 1;
    M = 1;
    logM = 0;
    while (i) {			/* while M is still about */
      i >>= 1;			/* divide by 2 */
      M <<= 1;			/* multiply by 2 */
      logM++;
    }
    break;
  case HASH_TYPE_DIVISION:
    M += (1 - (M%2));		/* make odd */
    /* scale up M so it isn't relatively prime for these small primes */
    /* c.f. Fundamental of Data Structures, Horowitz and Sahni, pp. 461 */
    while (!((M%3) && (M%5) && (M%7) && (M%11) && (M%13) && (M%17)&&(M%19)))
      M+=2;
    break;
  default:
    break;
  }
  if (M <= ht->ht_M)		/* nothing to do */
    return((caddr_t)ht);
  if (logM == 0) {		/* no logM?  figure it */
    int t = M>>1;		/* get M */
    do {
      logM++;			/* int log M to 1 */
      t >>= 1;			/* divide by 2 */
    } while (t);
  }
  bt = ht->ht_buckets;		/* get buckets */
  oldM = ht->ht_M;
  oldPolicy = ht->ht_policy;
  oldChainOps = ht->ht_chainops;

  if ((nbt = (caddr_t *)calloc((u_int)M, sizeof(caddr_t))) == NULL) {
    fprintf(stderr, "hash table create: no memory for %d element table\n",M);
    return(NULL);		/* return */
  }
  ht->ht_buckets = nbt;	/* save new bucket table */
  ht->ht_M = M;		/* assume these */
  ht->ht_logM = logM;
  ht->ht_stats.hs_buckets = M; /* mark # of buckets */
  ht->ht_policy = (policy == HASH_POLICY_OLD) ? oldPolicy : policy;
  if (afunc)
    ht->ht_afunc = afunc;
  if (cfunc)
    ht->ht_cfunc = cfunc;
  if (ht->ht_type != hashtype && hashtype != HASH_TYPE_OLD) {
    ht->ht_hfunc = hash_funcs[hashtype];
    if (ht->ht_policy == HASH_POLICY_DOUBLE_HASH)
      ht->ht_hfunc2 = hash_funcs2[hashtype];
    else
      ht->ht_hfunc2 = NULL;
  }
  /* always reset if given */
  if (hfunc)
    ht->ht_hfunc = hfunc;
  if (hfunc2)
    ht->ht_hfunc2 = hfunc2;
  if (cpfunc)
    ht->ht_cpfunc = cpfunc;
  if (chainops)
    ht->ht_chainops = chainops;
  ht->ht_type = hashtype;
  {
    struct hash_statistics *s = &ht->ht_stats;
    /* no longer valid */
    s->hs_used = s->hs_davg = s->hs_dsum = s->hs_dmax = 0;
    /* no longer valid */
    s->hs_lnum = s->hs_lsum = s->hs_lavg = 0;
    /* cum. statistics stay */
  }
  /* rehash if new table */
  if (bt) {
    afunc = ht->ht_afunc;	/* save */
    ht->ht_afunc = NULL;		/* turn off for a bit */
    for (i = 0; i < oldM; i++) {
      if (bt[i]) {
	switch (oldPolicy) {
	case HASH_POLICY_CHAIN:
	  while ((p = (*oldChainOps->hlo_get)(&bt[i])))
	    h_insert(ht, p);
	  break;
	default:
	  h_insert(ht, bt[i]);
	}
      }
    }
    ht->ht_afunc = afunc;	/* turn back on */
    free((caddr_t)bt);		/* free old table */
  }
  return((caddr_t)ht);
}

/* update hash TABLE statistics: generally, these are off for chain */
/* and when there are deletes done */ 
PRIVATE int
update_hash_table_stats(s, distance, updown)
struct hash_statistics *s;
int distance;
int updown;
{
  if (distance > s->hs_dmax) /* new maximum distance */
    s->hs_dmax = distance;
  s->hs_dsum += distance;	/* bump sum of distances */
  s->hs_used += updown;
  if (s->hs_used)
    s->hs_davg = (100*s->hs_dsum) / s->hs_used; /* scale it */
  else
    s->hs_davg = 0;
  return(s->hs_davg);
}

/* update lookup statisitics */
PRIVATE int
update_hash_lookup_stats(s, distance)
struct hash_statistics *s;
int distance;
{
  s->hs_lsum += distance;	/* bump sum of distances */
  s->hs_lnum++;			/* bump number of distances */
  s->hs_clsum += distance;	/* same for cum. */
  s->hs_clnum++;
  s->hs_lavg = (100*s->hs_lsum) / s->hs_lnum; /* save 2 decimal points */
  return(s->hs_lavg);
}

/* hash table operation: delete, insert, find */
caddr_t
h_operation(what, ht, key, idx, idx2, d, b)
int what;
HTABLE *ht;
caddr_t key;
int idx;			/* preliminary index (-1 if none) */
int idx2;			/* secondary index (-1 if none) */
int *d;				/* return distance ? */
int *b;				/* return bucket # */
{
  int sidx, t;
  int distance;
  u_int cpkey;			/* compress version of key */
  caddr_t *bp;			/* bucket pointer */
  caddr_t *pbp = NULL;		/* previous bucket pointer for delete */
  caddr_t data = NULL;

  /* blather */
  if (ht == NULL || HASH_OP_INVALID(what))
    return(NULL);
  if (idx < 0) {
    if (ht->ht_cpfunc) {
      cpkey = (*ht->ht_cpfunc)(key);
      idx = (*ht->ht_hfunc)(ht->ht_M, ht->ht_logM, cpkey);
    } else
      idx = (*ht->ht_hfunc)(ht->ht_M, ht->ht_logM, key);
  }
  sidx = idx;
  if (ht->ht_buckets == NULL) {
    fprintf(stderr, "No buckets for hash table!  (Possibly using a freed \
 hash table handle)\n");
    return(NULL);
  }
  bp = &ht->ht_buckets[idx];	/* start */
  distance = 0;
  if (b)
    *b = sidx;

  if (ht->ht_policy == HASH_POLICY_CHAIN) {
    caddr_t hint, hint2;

    /* distance should be updated */
    data = (*ht->ht_chainops->hlo_find)(*bp,key,ht->ht_cfunc,
					&distance, &hint, &hint2);
    switch (what) {
    case HASH_OP_DELETE:
      if (!data)
	break;
      /* key */
      /* ignore error (should not happen!) */
      (void)(*ht->ht_chainops->hlo_delete)(bp, key, &distance, hint, hint2);
      update_hash_table_stats(&ht->ht_stats, -distance, -1);
      break;
    case HASH_OP_MEMBER:
      if (data)
	t = update_hash_lookup_stats(&ht->ht_stats, distance);
      break;
    case HASH_OP_INSERT:
      if (data) {
	t = update_hash_lookup_stats(&ht->ht_stats, distance);
	break;
      }
      data= (*ht->ht_chainops->hlo_insert)(bp,key,ht->ht_afunc,
					   &distance, hint,hint2);
      update_hash_table_stats(&ht->ht_stats, distance, 1);
      break;
    }
    if (d)
      *d = distance;
    return(data);
  }

  do {
    if (*bp == NULL) {
      switch (what) {
      case HASH_OP_DELETE:		/* finished delete */
	break;
      case HASH_OP_MEMBER:
	data = NULL;
	break;
      case HASH_OP_INSERT:
	/* left with insert */
	data = ht->ht_afunc ? (*ht->ht_afunc)(key) : key;
	*bp = data;
	update_hash_table_stats(&ht->ht_stats, distance, 1);
	break;
      }
      if (d)
	*d = distance;
      return(data);
    } else {
      switch (what) {
      case HASH_OP_DELETE:
	/* if we haven't found an key to delete, try to find it */
	if (!pbp) {
	  if ((*ht->ht_cfunc)(key, *bp) == 0) {
	    data = *bp;		/* save return key */
	    *bp = NULL;		/* clear out this bucket */
	    pbp = bp;		/* remember this bucket */
	    update_hash_table_stats(&ht->ht_stats, -distance, -1);
	  }
	} else {
	  /* delete old distance */
	  update_hash_table_stats(&ht->ht_stats, -distance, -1);
	  /* insert new distance */
	  update_hash_table_stats(&ht->ht_stats, distance-1, 1);
	  *pbp = *bp;		/* move bucket */
	  *bp = NULL;		/* clear out this bucket */
	  pbp = bp;		/* remember this bucket */
	}
      default:
	if ((*ht->ht_cfunc)(key, *bp) == 0) {
	  t = update_hash_lookup_stats(&ht->ht_stats, distance);
	  if (d)
	    *d = distance;
	  return(*bp);		/* done */
	}
      }
    }
    if (idx2 < 0 && ht->ht_hfunc2)
      if (ht->ht_cpfunc) 
	idx2 = (*ht->ht_hfunc2)(ht->ht_M, ht->ht_logM, idx, cpkey);
      else
	idx2 = (*ht->ht_hfunc2)(ht->ht_M, ht->ht_logM, idx, key);
    distance++;
    idx += idx2 > 0 ? idx2 : 1; /* bump index */
    bp++;			/* advance bucket pointer */
    if (idx >= ht->ht_M) {	/* need to wrap around */
      idx %= ht->ht_M;		/* push index about */
      bp = &ht->ht_buckets[idx]; /* and reset buckets */
    }
  } while (sidx != idx);
  return(NULL);
}

/* return hash statistics */
struct hash_statistics *
h_statistics(h)
HTABLE *h;
{
  return(&h->ht_stats);
}


/* for linked list */
struct hash_chain_item {
  struct hash_chain_item *hci_next; /* pointer to next item in chain */
  caddr_t hci_data;		/* pointer to data */
};

/*
 * hint == previous(hint2)
 *  hint2 is the match node or node whose data is > than current
 *
*/
PRIVATE caddr_t
list_find(h, key, cmp, distance, hint, hint2)
struct hash_chain_item *h;
caddr_t key;
int (*cmp)();
int *distance;
struct hash_chain_item **hint;
struct hash_chain_item **hint2;
{
  struct hash_chain_item *hp = NULL;
  int d,c;

  *distance = 0;		/* no distnace */
  *hint = NULL;			/* mark no hint */
  *hint2 = NULL;
  if (h == NULL)
    return(NULL);
  for (d = 0 ; h ; h = h->hci_next) {
    if ((c = (*cmp)(key, h->hci_data)) >= 0)
      break;
    d++;
    hp = h;
  }
  if (distance)
    *distance = d;
  if (hint2)
    *hint2 = h;
  if (hint)
    *hint = hp;
  return(c == 0 ? h->hci_data : NULL);
}

/*
 * insert item into chain.  hint is from the lookup and helps us insert
 * distance is from lookup too (we could choose to change)
 *
 * hint == previous(hint2)
 *  hint2 is the match node or node whose data is > than current
 * return 0 on success, -1 on failure.
 *
 */
/*ARGSUSED*/
PRIVATE caddr_t
list_insert(head, key, alloc, distance, hint, hint2)
caddr_t *head;
caddr_t key;
caddr_t (*alloc)();
int *distance;
struct hash_chain_item *hint;
struct hash_chain_item *hint2;
{
  struct hash_chain_item *h;

  h = (struct hash_chain_item *)malloc(sizeof(struct hash_chain_item));
  if (h == NULL)
    return(NULL);
  h->hci_data = alloc ? (*alloc)(key) : key;
  h->hci_next = hint2;
  if (hint)
    hint->hci_next = h;
  else
    *head = (caddr_t)h;
  return(h->hci_data);
}

/*
 * assumes a find has been done, hint is set by find and item exists
 *  in the list
 * head - head of list
 * item - data (unused)
 * hint - previous node to one that contains item
 * distance - distance to update (not done) (may be deleted)
 *
*/
/*ARGSUSED*/
PRIVATE int
list_delete(head, key, distance, hint, hint2)
caddr_t *head;
caddr_t key;
int *distance;			/* not used */
struct hash_chain_item *hint;
struct hash_chain_item *hint2;
{
  /* trust our input: two things could be wrong, first */
  /* hint2 == NULL ==> nothing to delete */
  /* hint2 != "key" ==> item not in list */
  if (hint == NULL) {
    *head = (caddr_t)hint2->hci_next;	/* remove */
    free((caddr_t)hint2);
    return(TRUE);
  }
  hint->hci_next = hint2->hci_next; /* unlink */
  free((caddr_t)hint2);		/* get rid of node */
  return(TRUE);
}

/* gets first item on list and returns data, freeing up node */
PRIVATE caddr_t
list_get(h)
struct hash_chain_item **h;
{
  struct hash_chain_item *n;
  caddr_t d;

  if (h == NULL || *h == NULL)
    return(NULL);
  n = *h;			/* get item */
  *h = n->hci_next;		/* and remove */
  d = n->hci_data;
  free((caddr_t)n);
  return(d);
}

/* do hash division method */
/*ARGSUSED*/
PRIVATE u_int
hash_division(M, logM, idx)
int M;
int logM;
u_int idx;
{
  return(idx % M);
}

/* will work will with M if M-2,M are twin primes */
/*ARGSUSED*/
PRIVATE u_int
hash2_division(M, logM, hidx, idx)
int M;
int logM;
u_int hidx;
u_int idx;
{
  return(1 + (idx % (M-2)));
}

/* handle multiplicative method - hopefully the multiplier gives us */
/* good range */
/*ARGSUSED*/
PRIVATE u_int
hash_multiplicative(M, logM, idx)
int M;
int logM;
u_int idx;
{
  return(((u_int)((u_int)idx*(u_int)A_MULTIPLIER)>>(8*sizeof(int)-logM)));
}

/* the r more bits -- should be indepdent of the first r bits */
/*ARGSUSED*/
PRIVATE u_int
hash2_multiplicative(M, logM, hidx, idx)
int M;
int logM;
u_int hidx;
u_int idx;
{
  return(((u_int)((u_int)idx*(u_int)A_MULTIPLIER)>>(8*sizeof(int)-logM-logM)|1) );
}

#ifdef TESTIT
/* test program */
u_int
docomp(data)
char *data;
{
  u_int j;
  j = 0;
  while (*data)
    j = ((j + *data++) >> 1) | j<<31;
  return(j);
}

char *
alloc_func(p)
char *p;
{
  char *d = (caddr_t)malloc(strlen(p) + 1);
  strcpy(d, p);
  return(d);
}

dumpstats(msg, s)
char *msg;
struct hash_statistics *s;
{
  printf("%s\n\t %d bkts used, avg dist = %d.%02d, max dist = %d\n",
	 msg,
	 s->hs_used, s->hs_davg/100, s->hs_davg % 100,
	 s->hs_dmax);
}

main()
{
  HTABLE *hpc, *hplp, *hpdh;
  extern strcmp();
  char buf[BUFSIZ];
  int b, d, op;
  char *p;

#define X 16 

  hpc = (HTABLE *)h_new(HASH_POLICY_CHAIN, HASH_TYPE_DIVISION, X,
			strcmp, alloc_func, docomp, NULL, NULL, NULL);
  hplp = (HTABLE *)h_new(HASH_POLICY_LINEAR_PROBE,
			 HASH_TYPE_MULTIPLICATIVE, X, strcmp,
			 alloc_func, docomp, NULL, NULL, NULL);
  hpdh = (HTABLE *)h_new(HASH_POLICY_DOUBLE_HASH,
			 HASH_TYPE_MULTIPLICATIVE, X, strcmp,
			 alloc_func, docomp, NULL, NULL, NULL);
  while (gets(buf) != NULL) {
    p = buf+1;
    switch (buf[0]) {
    case '+':
      printf("INSERT %s\n", buf+1);
      op = HASH_OP_INSERT;
      break;
    case '-':
      printf("DELETE %s\n", buf+1);
      op = HASH_OP_DELETE;
      break;
    case ' ':
      printf("FIND %s\n", buf+1);
      op = HASH_OP_MEMBER;
      break;
    default:
      op = HASH_OP_INSERT;
      p = buf;
    }
    if ((h_operation(op, hpc, p, -1, -1, &d, &b)))
      printf("chain: %s at distance %d from bucket %d\n", p, d,b);
    else
      printf("chain hash table overflow or item not in table\n");
    if ((h_operation(op, hplp, p, -1, -1, &d, &b)))
      printf("linear probe: %s at distance %d from bucket %d\n", p, d,b);
    else
      printf("linear probe hash table overflow or item not in table\n");
    if ((h_operation(op, hpdh, p, -1, -1, &d, &b)))
      printf("double hash: %s at distance %d from bucket %d\n", p, d,b);
    else
      printf("double hash table overflow or item not in table\n");
  }
  dumpstats("double hash with multiplicative hash", h_statistics(hpdh));
  h_redefine(hpdh, HASH_POLICY_CHAIN,HASH_TYPE_DIVISION, X, NULL,
	     NULL, NULL, NULL,NULL,NULL);
  dumpstats("redefine above as chain with division hash", h_statistics(hpdh));
  h_redefine(hpdh, HASH_POLICY_LINEAR_PROBE,HASH_TYPE_MULTIPLICATIVE,
	     X, NULL,NULL,NULL,NULL,NULL,NULL);
  dumpstats("redefine above as linear probe with multiplicative hash",
	    h_statistics(hpdh));
  dumpstats("chain with division hash", h_statistics(hpc));
  dumpstats("linear probe with multiplicative hash", h_statistics(hplp));
  h_free(hpdh, free);
}
#endif
