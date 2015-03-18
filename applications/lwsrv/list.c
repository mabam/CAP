static char rcsid[] = "$Author: djh $ $Date: 1996/05/08 04:19:28 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/list.c,v 2.2 1996/05/08 04:19:28 djh Rel djh $";
static char revision[] = "$Revision: 2.2 $";

/*
 * list.c -  general list package-simple and key-value lists
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#include <stdio.h>
#include <netat/sysvcompat.h>
#ifdef USESTRINGDOTH
#include <string.h>
#else  USESTRINGDOTH
#include <strings.h>
#endif USESTRINGDOTH
#include "list.h"

#define	FALSE	0
#define	TRUE	1

static int listcmp();
static void sendKVTree();
static int _AddToKVTree(/* KVTree **ent, void *key, void *val,
  int (*keycmp)() */);
static KVTree *_DupKVTree(/* KVTree *lf */);
static void _FreeKVTree(/* KVTree *lp, int (*keyfree)(), int (*valfree)() */);
static void _ListKVTree(/* KVTree *kp, List *lp */);
static void *_SearchKVTree(/* KVTree *lp, void *key, int (*keycmp)() */);
char *malloc();
char *realloc();

void
AddToKVTree(ent, key, val, keycmp)
KVTree **ent;
void *key, *val;
int (*keycmp)();
{
  _AddToKVTree(ent, key, val, keycmp);
}

/*
 * Modified from "Algorithms + Data Structures = Programs", Niklaus Wirth,
 * 1976, section 4.4.7 Balanced Tree Insertion, page 220-221 (AVL).
 *
 */

#define	L_EQUILIBRATED	2
#define	LEFTSLANTED	1
#define	L_REBALANCE	0

#define	R_EQUILIBRATED	0
#define	RIGHTSLANTED	1
#define	R_REBALANCE	2

static int
_AddToKVTree(ent, key, val, keycmp)
register KVTree **ent;
void *key, *val;
int (*keycmp)();
{
  register KVTree *ent1, *ent2;
  register int cmp;
  char *malloc();

  if (*ent == NULL) {	/* not in tree, insert it */
    if ((*ent = (KVTree *)malloc(sizeof(KVTree))) == NULL)
      errorexit(1, "_AddToKVTree: Out of memory\n");
    (*ent)->left = (*ent)->right = NULL;
    (*ent)->bal = LEFTSLANTED;
    (*ent)->key = key;
    (*ent)->val = val;
    return(1);
  }
  if ((cmp = (*keycmp)(key, (*ent)->key)) == 0) {	/* match */
    (*ent)->val = val;
    return(0);
  }
  if (cmp < 0) {
    if (!_AddToKVTree(&(*ent)->left, key, val, keycmp))
      return(0);
    /* left branch has grown higher */
    switch((*ent)->bal) {
     case L_EQUILIBRATED:
      (*ent)->bal = LEFTSLANTED;
      return(0);
     case LEFTSLANTED:
      (*ent)->bal = L_REBALANCE;
      return(1);
     case L_REBALANCE:	/* rebalance */
      if ((ent1 = (*ent)->left)->bal == L_REBALANCE) {
	/* single LL rotation */
	(*ent)->left = ent1->right;
	ent1->right = *ent;
	(*ent)->bal = LEFTSLANTED;
	*ent = ent1;
      } else {
	/* double LR rotation */
	ent2 = ent1->right;
	ent1->right = ent2->left;
	ent2->left = ent1;
	(*ent)->left = ent2->right;
	ent2->right = *ent;
	(*ent)->bal = (ent2->bal == L_REBALANCE) ? L_EQUILIBRATED : LEFTSLANTED;
	ent1->bal = (ent2->bal == L_EQUILIBRATED) ? L_REBALANCE : LEFTSLANTED;
	*ent = ent2;
      }
      (*ent)->bal = LEFTSLANTED;
      return(0);
    }
  }
  if (!_AddToKVTree(&(*ent)->right, key, val, keycmp))
    return(0);
  /* right branch has grown higher */
  switch((*ent)->bal) {
   case R_EQUILIBRATED:
    (*ent)->bal = RIGHTSLANTED;
    return(0);
   case RIGHTSLANTED:
    (*ent)->bal = R_REBALANCE;
    return(1);
   case R_REBALANCE:	/* rebalance */
    if ((ent1 = (*ent)->right)->bal == R_REBALANCE) {
      /* single RR rotation */
      (*ent)->right = ent1->left;
      ent1->left = *ent;
      (*ent)->bal = RIGHTSLANTED;
      *ent = ent1;
    } else {
      /* double RL rotation */
      ent2 = ent1->left;
      ent1->left = ent2->right;
      ent2->right = ent1;
      (*ent)->right = ent2->left;
      ent2->left = *ent;
      (*ent)->bal = (ent2->bal == R_REBALANCE) ? R_EQUILIBRATED : RIGHTSLANTED;
      ent1->bal = (ent2->bal == R_EQUILIBRATED) ? R_REBALANCE : RIGHTSLANTED;
      *ent = ent2;
    }
    (*ent)->bal = RIGHTSLANTED;
    return(0);
  }
}

void
AddToList(lp, item)
register List *lp;
void *item;
{
  if (lp->n >= lp->lmax && (lp->list = (void **)realloc((char *)lp->list,
   (lp->lmax += LISTDELTA) * sizeof(void *))) == NULL)
    errorexit(1, "AddToList: Out of memory\n");
  lp->list[lp->n++] = item;
}

KVTree **
CreateKVTree()
{
  register KVTree **kp;

  if ((kp = (KVTree **)malloc(sizeof(KVTree *))) == NULL)
    errorexit(1, "AddToKVTree: Out of memory\n");
  *kp = NULL;
  return(kp);
}

void
CatList(to, from)
register List *to;
List *from;
{
  register int n;
  register void **vp;

  for (n = from->n, vp = from->list; n > 0; n--)
    AddToList(to, *vp++);
}

List *
CreateList()
{
  register List *lp;
  static char errstr[] = "CreateList: Out of memory\n";

  if ((lp = (List *)malloc(sizeof(List))) == NULL)
    errorexit(1, errstr);
  if ((lp->list = (void **)malloc((lp->lmax = LISTDELTA) * sizeof(void *)))
   == NULL)
    errorexit(2, errstr);
  lp->n = 0;
  return(lp);
}

KVTree **
DupKVTree(lf)
register KVTree **lf;
{
  register KVTree **kp;

  if ((kp = (KVTree **)malloc(sizeof(KVTree *))) == NULL)
    errorexit(1, "DupKVTree: Out of memory\n");
  *kp = *lf ? _DupKVTree(*lf) : NULL;
  return(kp);
}

static KVTree *
_DupKVTree(lf)
register KVTree *lf;
{
  register KVTree *lt;

  if ((lt = (KVTree *)malloc(sizeof(KVTree))) == NULL)
    errorexit(1, "_DupKVTree: Out of memory\n");
  lt->bal = lf->bal;
  lt->key = lf->key;
  lt->val = lf->val;
  lt->left = lf->left ? _DupKVTree(lf->left) : NULL;
  lt->right = lf->right ? _DupKVTree(lf->right) : NULL;
  return(lt);
}

List *
DupList(lf)
register List *lf;
{
  register List *lt;
  register void **vp;
  register int i;

  lt = CreateList();
  for (i = lf->n, vp = lf->list; i > 0; i--)
    AddToList(lt, *vp++);
  return(lt);
}

void
FreeKVTree(lp, keyfree, valfree)
KVTree **lp;
int (*keyfree)(), (*valfree)();
{
  if (*lp)
    _FreeKVTree(*lp, keyfree, valfree);
  free((char *)lp);
}

static void
_FreeKVTree(lp, keyfree, valfree)
register KVTree *lp;
int (*keyfree)(), (*valfree)();
{
  if (lp->left)
    _FreeKVTree(lp->left, keyfree, valfree);
  if (lp->right)
    _FreeKVTree(lp->right, keyfree, valfree);
  if (keyfree)
    (*keyfree)(lp->key);
  if (valfree)
    (*valfree)(lp->val);
  free((char *)lp);
}

void
FreeList(lp, itemfree)
List *lp;
int (*itemfree)();
{
  register void **vp;
  register int i;

  if (itemfree)
    for (i = lp->n, vp = lp->list; i > 0; i--)
      (*itemfree)(*vp++);
  free((char *)lp->list);
  free((char *)lp);
}

List *
ListKVTree(kp)
KVTree **kp;
{
  register List *lp;

  lp = CreateList();
  if (*kp)
    _ListKVTree(*kp, lp);
  return(lp);
}

static void
_ListKVTree(kp, lp)
register KVTree *kp;
register List *lp;
{
  if (kp->left)
    _ListKVTree(kp->left, lp);
  AddToList(lp, kp);
  if (kp->right)
    _ListKVTree(kp->right, lp);
}

void *
SearchKVTree(lp, key, keycmp)
KVTree **lp;
void *key;
int (*keycmp)();
{
  return(*lp ? _SearchKVTree(*lp, key, keycmp) : NULL);
}

static void *
_SearchKVTree(lp, key, keycmp)
KVTree *lp;
register void *key;
int (*keycmp)();
{
  register int cmp;

  if ((cmp = (*keycmp)(key, lp->key)) == 0)
    return(lp->val);
  if (cmp > 0)
    return(lp->right ? _SearchKVTree(lp->right, key, keycmp) : NULL);
  else
    return(lp->left ? _SearchKVTree(lp->left, key, keycmp) : NULL);
}

void *
SearchList(lp, item, itemcmp)
register List *lp;
register void *item;
int (*itemcmp)();
{
  register void **ip = lp->list;
  register int i, low, high, cmp;

  low = 0;
  high = lp->n - 1;
  while (low <= high) {
    i = (low + high) / 2;
    if ((cmp = (*itemcmp)(ip[i], item)) == 0)
      return(ip[i]);
    if (cmp > 0)
      high = i - 1;
    else
      low = i + 1;
  }
  return(NULL);
}

void
SortList(lp, itemcmp)
register List *lp;
int (*itemcmp)();
{
  qsort((char *)lp->list, lp->n, sizeof(void *), itemcmp);
}

int
StringSortItemCmp(a, b)
char **a, **b;
{
  return(strcmp(*a, *b));
}

errorexit(status, str)
int status;
char *str;
{
  fputs(str, stderr);
  exit(status);
}
