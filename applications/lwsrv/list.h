/* "$Author: djh $ $Date: 1995/08/28 10:38:35 $" */
/* "$Header: /mac/src/cap60/applications/lwsrv/RCS/list.h,v 2.1 1995/08/28 10:38:35 djh Rel djh $" */
/* "$Revision: 2.1 $" */

/*
 * list.h -  general list package-simple and key-value lists
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#ifndef _LIST_H_
#define	_LIST_H_

#define	LISTDELTA	10

typedef struct KVTree {
    struct KVTree *left;
    struct KVTree *right;
    void *key;
    void *val;
    unsigned char bal;		/* for AVL tree balancing */
} KVTree;
typedef struct List {
    int n;
    int lmax;
    void **list;
} List;

#define	AddrList(lp)	(((List *)(lp))->list)
#define	NList(lp)	(((List *)(lp))->n)

void AddToKVTree(/* KVTree **lp, void *key, void *val, int (*keycmp)() */);
void AddToList(/* List *lp, void *item */);
void CatList(/* List *to, List *from */);
KVTree **CreateKVTree();
List *CreateList();
KVTree **DupKVTree(/* KVTree **lf */);
List *DupList(/* List *lf */);
void FreeKVTree(/* KVTree **lp, int (*keyfree)(), int (*valfree)() */);
void FreeList(/* List *lp, int (*itemfree)() */);
List *ListKVTree(/* KVTree **kp */);
void *SearchKVTree(/* KVTree **lp, void *key, int (*keycmp)() */);
void *SearchList(/* List *lp, void *item, int (*itemcmp)() */);
void SortList(/* List *lp, int (*itemcmp)() */);
int StringSortItemCmp(/* char **a, char **b */);

#endif /* _LIST_H_ */
