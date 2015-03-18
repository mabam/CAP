/*
 * $Author: djh $ $Date: 91/02/15 21:07:18 $
 * $Header: afpgc.c,v 2.1 91/02/15 21:07:18 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * afpgc.c - Appletalk Filing Protocol General Cache Manager
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 * 
 *  Apr 3, 1987     Schilit	Created.
 *
 */

/*
 * General Cache Routines.
 *
 * These routines may be used for building a cache based on LRU
 * replacement.
 *
 * The caller controls the cache by supplying routines for loading,
 * purging, comparing, and validating entries.
 *
 * The cache is initialized and the size is defined by a call to
 * GCNew, all the other work is performed by the lookup routine,
 * GCLocate.  See the comments on those routines for calling
 * conventions.
 *
 */

/*
 * GCNew - create a new cache
 * GCScan - locate cache entry
 * GCLocate - locate cache entry, loading if necessary
 * GCFlush - flush cache
 * GCGet - get cache entry
 * GCAdd - add cache entry

*/
  
#include "afpgc.h"			/* include defs for cache */

#define TRUE 1
#define FALSE 0

/*
 * GCHandle *GCNew(size,int (*valid)(), int (*comp)(),
 *		   char *(*load)(), void (*purge)());
 *
 * Create a new cache of size 'size' and define the interface routines
 * for validating, comparing, loading, and purging entries.
 *
 *  int valid(ce) 	- returns TRUE if entry is valid.
 *  int comp(ce,key) 	- compare and return 0 if equal.
 *  char *load(key) 	- load an entry (allocate stg and/or read from disk).
 *  void purge(ce)  	- purge an entry (release stg & write to disk).
 *  void flush(ce)	- flush entry (write to disk)
 *
 *
 * The datatype of a cache entry is defined by the *result* of load().
 * This datatype is stored in the cache and passed to purge(), valid()
 * and as the first argument to comp().
 *
 * A second datatype may be defined for the lookup key.  This is the
 * argument to GCLocate and is sent to comp() to compare against an
 * existing cache entry.  Load() accepts a key and returns the cache
 * entry for that key.
 *
 */
  
GCHandle *
GCNew(size,cvalid,ccomp,cload,cpurge,cflush)
int size;
int (*cvalid)();
int (*ccomp)();
char *(*cload)();
void (*cpurge)();
void (*cflush)();
{
  GCHandle *gch;
  int i;
  
  gch = (GCHandle *) malloc(sizeof(GCHandle));
  gch->gch_clock = 0;
  gch->gch_size = size;
  gch->gch_valid = cvalid;		/* validation routine */
  gch->gch_comp = ccomp;		/* compare routine */
  gch->gch_load = cload;		/* loading routine */
  gch->gch_purge = cpurge;		/* release routine */
  gch->gch_flush = cflush;		/* write or update */
  gch->gch_lru = (int *) malloc(sizeof(int)*size);
  gch->gch_ents = (GCUData **) malloc(sizeof(GCUData *) * (size));
  for (i=0; i < size; i++)
    gch->gch_lru[i] = -1;
  return(gch);
}

/*
 * boolean GCScan(GCHandle *gch, GCUData *key, int *idx)
 *
 * Scan the cache (gch) looking for key.  
 *
 * Returns TRUE if the entry was found in the cache, idx is the index
 * of the entry.
 *
 * Returns FALSE if no entry was found in the cache, idx is a free
 * entry (may have called user's purge).
 *
 */

/* private */ int
GCScan(gch, key, idx)
GCHandle *gch;
GCUData *key;
int *idx;
{
  register GCUData **ent = gch->gch_ents;
  register int *lru = gch->gch_lru;
  register int i;
  register int mi = 0;
  register int mf = -1;

  for (i=0; i < gch->gch_size; i++) {	/* scan cache for entry and min */
    if (lru[i] < 0)			/* entry in cache? */
      mf = i;				/* no, remember free entry */
    else {
      if ((*gch->gch_comp)(ent[i],key))	/* compare */
	if ((*gch->gch_valid)(ent[i])) { /* match, see if entry is valid */
	  lru[i] = gch->gch_clock++;	/* found matching valid entry */
	  *idx = i;			/* here is the cache index */
	  return(TRUE);			/* aready in cache return TRUE */
	} else {
	  mf = i;			/* invalid and matching, so reuse it */
	  break;			/* break out of for loop */
	}
      if (lru[i] < lru[mi])		/* no match, check for min entry */
	mi = i;				/*  if so remember */
    }
  }

  /* Miss. cache scan is over without locating the desired entry. */
  /* if we did not find a free entry then free the min lru found */
  /* and load the cache with the desired entry. */

  if (mf < 0) {				/* did we find a free entry */
    (*gch->gch_purge)(ent[mi]);		/* no, free the min entry */
    mf = mi;				/* now here is a slot */
  }
  *idx = mf;				/* set index */
  return(FALSE);
}

/*
 * char *GCLocate(GCHandle *gch, char *key)
 *
 * Locate the entry matching 'key' in the cache 'gch' by calling
 * the comparision routine comp() for each cache entry.
 *
 * If no entry is found, or the matching entry fails the user
 * specified valid() check, then add a new cache entry by calling
 * the load() procedure.
 *
 * If the cache is full, then replacement is required and the
 * user specified purge() is called to release the LRU entry.
 *
 */

char *
GCLocate(gch,key)
GCHandle *gch;
char *key;
{
  int idx;

  if (GCScan(gch,key,&idx))		/* scan for entry */
    return(gch->gch_ents[idx]);		/*  found it, so return */
  gch->gch_lru[idx] = gch->gch_clock++;	/* else free entry, set clock */
  gch->gch_ents[idx] = (*gch->gch_load)(key); /* load it in */
  return(gch->gch_ents[idx]);		/* and return it */
}

/*
 * flush is called when OSFlush gets called -- could and should be
 * done without releasing cache entries by using user defined flush
 * routine: in other words, it is a prime opportunity to scan for "bad"
 * items
 *
 * allows passing of userdata (single long)
 *
 */
void
GCFlush(gch, udata)
GCHandle *gch;
unsigned long udata;
{
  int i;
  char **ent = gch->gch_ents;
  int *lru = gch->gch_lru;

  for (i=0; i < gch->gch_size; i++) 	/* scan cache for entry and min */
    if (lru[i] >= 0)			/* entry in cache? */
      if ((*gch->gch_valid)(ent[i]))	/* yes... entry valid? */
	(*gch->gch_flush)(ent[i], udata); /*  yes.. then flush  */

}

/*
 * GCUData *GCGet(GCHandle *gch, int idx)
 *
 * Return the cache entry at index idx.
 *
 */

GCUData *
GCGet(gch,idx)
GCHandle *gch;
int idx;
{
  return(gch->gch_ents[idx]);
}

/*
 * int GCAdd(GCHandle *gch, GCUData *udata)
 *
 * Add the entry udata to the cache.  Returns cache index.
 *
 * Adding an entry may cause user's flush routine to be called
 * if the cache is full.
 *
 */

int 
GCAdd(gch, udata)
GCHandle *gch;
GCUData *udata;
{
  int idx;

  if (GCScan(gch,udata,&idx))		/* scan for entry in cache */
    return(idx);			/* found it, oh well.. */
  gch->gch_lru[idx] = gch->gch_clock++;	/* else free entry, set clock */
  gch->gch_ents[idx] = udata;		/* store entry */
  return(idx);				/* and return index */
}
