/*
 * $Author: djh $ $Date: 91/02/15 21:05:50 $
 * $Header: afpavl.h,v 2.1 91/02/15 21:05:50 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * afpavl.h - Appletalk Filing Protocol AVL Tree Management definitions
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

typedef char AVLUData;		/* baggage for user */

typedef struct avl_node {
  short  b_bf;			/* balance factor 0, 1, or -1 */
  struct avl_node *b_l;		/* left child */
  struct avl_node *b_r;		/* right child */
  AVLUData *b_udata;		/* user data (baggage) */
} AVLNode, *AVLNodePtr;

#define NILAVL ((AVLNodePtr) 0)

AVLNode *AVLInsert();
AVLNode *AVLLookup();
void AVLPrintTree();
void AVLMapTree();

