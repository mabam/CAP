/*
 * $Author: djh $ $Date: 91/02/15 21:05:43 $
 * $Header: afpavl.c,v 2.1 91/02/15 21:05:43 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * afpavl.c - Appletalk Filing Protocol AVL Tree Management
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 *
 * Edit History:
 *
 *  Mar 30, 1987     Schilit	Created, based on older versions
 *
 *
 */


#include <stdio.h>
#include "afpavl.h"

/*
 * The compare routines for the following MUST be used as:
 *    compare(userdata, node->userdata)
 *  This is because some of the callers may want to "cheat" on copying
 *  data and only send the key as the userdata.  Since AVLInsert returns
 *  the AVLNode, the user data there can be modified after the fact
 *  if the userdata passed is not quite what we wanted (was only a search
 *  key)
 */
#define TRUE 1
#define FALSE 0

AVLNode *
AVLLookup(r,udata,compar)
AVLNodePtr r;				/* root, start of search  */
AVLUData *udata;			/* check for this user data */
int (*compar)();			/* comparison routine */
{
  int cmp;

  while (r != NILAVL) {			/* until we runnout...  */
    cmp = (*compar)(udata,r->b_udata); /* compare the keys */
    if (cmp < 0)			/* key < r */
      r = r->b_l;			/* move left */
    else if (cmp > 0)			/* key > r */
      r = r->b_r;			/* move right */
    else				/* key = r */
      return(r);		/* return the node */
  }
  return(NULL);			/* not found, return null */
}

AVLNode *
AVLNew(udata)
AVLUData *udata;
{
  AVLNode *new;

  new = (AVLNode *) malloc(sizeof(AVLNode));
  new->b_bf = 0;			/* init balance factor */
  new->b_l =				/* and left, right pointer */
    new->b_r = NILAVL;			/*  fields in new node */
  new->b_udata = udata;			/* install user data */
  return(new);				/* and return new ptr */
}

/*
 * int AVLInsert(AVLNode **root, AVLUData *udata, int (*compar)())
 *
 * Create a new node with the user's data attached to it (AVLUData).
 * Use the compar routine to decide where this entry goes in the 
 * tree.  Always returns user data of node inserted or found.
 *
 * Rewrote to match closer to Knuth version: cf. Searching and Sorting, 
 * The Art of Computer Programming, Volume 3, pp. 455-457.
 *
 */

AVLNode *
AVLInsert(root,udata,compar)
AVLNode **root;				/* handle on root node */
AVLUData *udata;			/* user data for new node */
int (*compar)();			/* comparison routine */
{
  AVLNodePtr t,s,p,q,r;
  int bf,cmp;				/* balance factor */

  if ((p = *root) == NILAVL) {		/* check for empty tree */
    *root = AVLNew(udata);		/* if so then insert and */
    return(*root);			/*  return... */
  }

/*
 * p move down the tree to locate the insertion
 * point for new.
 *
 * s is the node to rebalance around: the closest node to new which
 * has a balance factor different from zero.  t is the father of s.
 * If t stays null then we want to rebalance around "root"
 */

  /* A1 */
  t = NILAVL;
  s = p;

  do {
    /* A2 */
    if ((cmp = (*compar)(udata,p->b_udata)) == 0)
      return(p);
    if (cmp < 0) {			/* comparison base */
      /* A3 */
      if ((q = p->b_l) == NILAVL) {	/* go left unless NIL */
	p->b_l = q = AVLNew(udata);	/* if nil, then insert */
	break;				/* and goto rest of code */
      }
    } else {				/* cmp > 0 */
      /* A4 */
      if ((q = p->b_r) == NILAVL) {	/* go right unless NIL */
	p->b_r = q = AVLNew(udata);
	break;
      }
    }
    if (q->b_bf != 0) {
      t = p;				/* new rebalance point */
      s = q;
    }
    p = q;
  } while (1);

  /* A6 */
  /* adjust the balance factors of nodes on path from next of s to q */

				/* remember bf at balance point */
  bf = ((*compar)(udata,s->b_udata) < 0) ? -1 : 1;
  if (bf < 0)				/* take the first step... */
    r = p = s->b_l;			/* to get past the balance point */
  else
    r = p = s->b_r;

  /* follow p down to q and set each balance factor to 1 or -1.
   * no addition is required since balance factors of all nodes after
   * s are zero (s is the closest non zero node).
   */

  while (p != q) {			/* follow the path from p to new */
					/* set balance factors for nodes */
    cmp = (*compar)(udata,p->b_udata);
#ifdef DEBUG
    if (cmp == 0) {			/* p == q !! */
      fprintf(stderr, "Can't happen p == q in AVLInsert A6\n");
      exit(9999);
    }
#endif
    p->b_bf = cmp < 0 ? -1 : 1;		/* adjust */
    p = cmp < 0 ? p->b_l : p->b_r;	/* and follow correct path */
  }

  /* A7 */
  /* check if the tree is unbalanced */
  
  if (s->b_bf == 0) {			/* at balance point check bf */
    s->b_bf = bf;			/* was balanced now only 1 off */
    return(q);				/*  tree is ok (return(q)) */
  }

  if (s->b_bf == -bf) {			/* at balance point did we improve? */
    s->b_bf = 0;			/* yes, set balance factor to 0 */
    return(q);				/* and return (q - new node ) */
  }

  /* balance factor went to -2 or +2, rebalance the tree */
  if (r->b_bf == bf) {			/* single rotation */
    p = r;
    if (bf < 0) {
      s->b_l = r->b_r;
      r->b_r = s;
    } else {
      s->b_r = r->b_l;
      r->b_l = s;
    }
    s->b_bf = r->b_bf = 0;		/* subtree is now balanced */
  } else {
    /* double rotation */
    if (bf < 0) {
      p = r->b_r;
      r->b_r = p->b_l;
      p->b_l = r;
      s->b_l = p->b_r;
      p->b_r = s;
    } else {
      p = r->b_l;
      r->b_l = p->b_r;
      p->b_r = r;
      s->b_r = p->b_l;
      p->b_l = s;
    }
    if (p->b_bf == 0)
      s->b_bf = r->b_bf = 0;
    else {
      if (p->b_bf == bf) {		/* b(p) = a */
	s->b_bf = -bf;
	r->b_bf = 0;
      } else {				/* b(p) = -a */
	s->b_bf = 0;
	r->b_bf = bf;
      }
    }
    p->b_bf = 0;
  }
  
  /* A10 */
  /* Finishing touch: s points to the new root and t points to the
   * father of the old root of the rebalanced subtree.  Make t point
   * to the head of the balanced subtree.
   */

  if (t == NILAVL)			/* rebalance around the root node? */
    *root = p;				/* yes, set a new root */
  else {
    if (s == t->b_r)			/* did we rebalance on right? */
      t->b_r = p;			/* yes, then set new subtree there */
    else
      t->b_l = p;			/* else rebalanced on left */
  }
  return(q);				/* return new node */
}

#ifdef notdef
AVLNode *
AVLLookN(r,udata,n,comp)
AVLNode *r;
AVLUData *udata;
int *n;
int (*comp)();
{
  AVLNode *s;
  int cmp;

  while (r != NILAVL) {			/* until we runnout...  */
    cmp = (*comp)(udata,r->b_udata);	/* compare the keys */
    if (cmp < 0)			/* key < r */
      r = r->b_l;			/* move left */
    else if (cmp > 0)			/* key > r */
      r = r->b_r;			/* move right */
    else				/* key = r */
      break;				/* found a match */
  }
  if (r == NILAVL)			/* check if runnout */
    return(NILAVL);			/* yes... then not found */
  if (--(*n) <= 0)			/* no... found something, decr n */
    return(r);				/*  it was the one we wanted */
  s = AVLLookN(r->b_l,udata,n,comp);	/* else check left subtree */
  if (s != NILAVL)			/* found it there? */
    return(s);				/*  return the node */
  return(AVLLookN(r->b_r,udata,n,comp)); /* else check right subtree */
}

/*
 * AVLLookupNth(AVLNode *root, AVLUData *udata, int n, int (*compare)())
 *
 * Call the specified comparison routine to locate the n'th node
 * that matches user data.  The primary key used by compare must
 * match the primary key used by AVLInsert.
 *
 */

AVLUData *
AVLLookupNth(r,udata,n,comp)
AVLNodePtr r;
AVLUData *udata;
int n;
int (*comp)();
{
  AVLNode *s,*f;			/* subtree */
  int cmp;

  f = NILAVL;
  while (r != NILAVL && f == NILAVL) {	/* locate first matching */
    cmp = (*comp)(udata,r->b_udata);	/* compare the keys */
    if (cmp < 0)			/* key < r */
      r = r->b_l;			/* move left */
    else if (cmp > 0)			/* key > r */
      r = r->b_r;			/* move right */
    else				/* key = r */
      f = r;				/*  found it set f exits loop */
  }
  if (f == NILAVL)
    return((AVLUData *) 0);		/* nothing found */
  s = AVLLookN(f,udata,&n,comp);	/* call locator routine */
  if (s != NILAVL)			/* did we find it? */
    return(s->b_udata);			/* yes, return user data */
  return((AVLUData *) 0);		/* return failure */
}

#endif

void
AVLMapTree(root,pnode,uhdl)
AVLNodePtr root;
void (*pnode)();
char *uhdl;
{
  if (root == NILAVL)
    return;
  (*pnode)(root->b_udata,uhdl);		/* call with udata */
  if (root->b_l != NILAVL)
    AVLMapTree(root->b_l,pnode,uhdl);	/* do left nodes */
  if (root->b_r != NILAVL)
    AVLMapTree(root->b_r,pnode,uhdl);	/* do right nodes */
}
