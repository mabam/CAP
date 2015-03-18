/*
 * $Author: djh $ $Date: 91/02/15 21:07:27 $
 * $Header: afpgc.h,v 2.1 91/02/15 21:07:27 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * afpgc.c - Appletalk Filing Protocol General Cache Manager Definitions
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
  
/* GCHandle is the general cache manager's handle on a cache */

typedef char GCUData;

typedef struct {
  int gch_clock;			/* the current cache clock */
  int gch_size;				/* the size of the cache */
  int (*gch_valid)();			/* valid function */
  int (*gch_comp)();			/* compare function */
  char *(*gch_load)();			/* load function */
  void (*gch_purge)();			/* free function */
  void (*gch_flush)();			/* write to disk function */
  int  *gch_lru;			/* clocks for each entry */
  GCUData **gch_ents;			/* data for each entry */
} GCHandle;

/* given a pointer cache pointer and an index, returns an entry */
#define GCidx2ent(gch,idx) ((gch)->gch_ents[(idx)])

GCHandle *GCNew();			/* create a new cache */
GCUData *GCLocate();			/* locate an entry in the cache */
void GCFlush();				/* flush the cache */

GCUData *GCGet();			/* direct access get by index  */
int GCAdd();				/* direct access add to cache */

#define NOGCIDX -1			/* NULL cache index  */
