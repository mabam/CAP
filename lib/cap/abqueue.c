/*
 * $Author: djh $ $Date: 1993/09/28 08:24:19 $
 * $Header: /mac/src/cap60/lib/cap/RCS/abqueue.c,v 2.2 1993/09/28 08:24:19 djh Rel djh $
 * $Revision: 2.2 $
*/

/*
 * abqueue - routines for queueing and dequeueing
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  July  1, 1986    Schilit    Created
 *
 */

/*
 * The following queuing code is structured such that the VAX
 * queue instructions is used if "VAX" is defined.
 *
*/

#include <netat/abqueue.h>

#ifndef VAX			/* if not a VAX need queue routines */

/* 
 * remque(QElemPtr elem)
 *
 * Remove "elem" from queue.
 *
 * NB: libc routine of same name is used on VAX systems.
 *
*/

remque(elem)
QElemPtr elem;
{
  (elem->q_back)->q_forw = elem->q_forw;
  (elem->q_forw)->q_back = elem->q_back;
}

/*
 * insque(QElemPtr elem,pred)
 *
 * Insert "elem" before "pred" in queue.
 *
 * NB: libc routine of same name is used on VAX systems.
 *
*/

insque(elem,pred)
QElemPtr elem,pred;
{
  elem->q_forw = pred->q_forw;
  elem->q_back = pred;
  pred->q_forw = elem;
  (elem->q_forw)->q_back = elem;
}

#endif				/* not VAX */

/*
 * q_tail(QElemPtr *head,ntail)
 *
 * q_tail adds "ntail" to the rear of the queue specified by "*head"
 *
 * If the queue does not exist (i.e. "head" points to a NIL) then one
 * is created.
 *
*/

void
q_tail(head,ntail)
QElemPtr *head;			/* pointer to the head of the queue */
QElemPtr ntail;
{
  if (*head == NILQ) {		/* is this an empty queue? */
    *head = ntail;		/* yes, first item is now head */
    ntail->q_back =		/* set forward and back pointers */
      ntail->q_forw = ntail;	/* list is circular! */
  } else 
    insque(ntail,(*head)->q_back);
}

/*
 * q_head(QElemPtr *head,nhead) 
 *
 * q_head adds "nhead" to the front of the queue specified by "head."
 *
 * If the queue does not exist (i.e. "head" points to a NIL) then one
 * is created.
 *
*/
 
void
q_head(head,nhead)
QElemPtr *head;			/* pointer to the head of the queue */
QElemPtr nhead;
{

  if (*head == NILQ) {		/* is this an empty queue? */
    nhead->q_back =		/* set forward and back pointers */
      nhead->q_forw = nhead;	/* list is circular! */
  } else
    insque(nhead,(*head)->q_back);
  *head = nhead;		/* yes, first item is now head */
}

/*
 * q_elem inserts item after prev in the list.  If the list is empty
 * or no prev is specified, then the q_head is used to create a new
 * list or insert at the head respectively
 *
*/
void
q_elem(head, prev, item)
QHead head;
QElemPtr prev;
QElemPtr item;
{
  if (*head == NILQ || prev == NILQ)
    q_head(head, item);
  else
    insque(prev, item);
}


/*
 * QElemPtr dq_tail(QElemPtr *head)
 *
 * dq_tail removes the last entry on the queue pointed to by "*head"
 * and returns a pointer to the entry removed.
 *
 * If we remove the last entry, then "*head" is set to NIL.
 *
*/
 
QElemPtr
dq_tail(head)
QElemPtr *head;
{
  QElemPtr tail;

  if (*head == NILQ)		/* if no queue then  */
    return(NILQ);		/*  return NIL */

  tail = (*head)->q_back;	/* pick up the tail */
  if (tail == *head) {		/* is this the last element? */
    *head = NILQ;		/* yes, set the head to NIL */
  } else
    remque(tail);
  return(tail);
}



/*
 * QElemPtr dq_head(QElemPtr *head)
 *
 * dq_head removes the first entry on the queue pointed to by "*head"
 * and returns a pointer to the entry removed.
 *
 * If we remove the last entry, then "*head" is set to NIL.
 *
*/

QElemPtr
dq_head(head)
QHead head;
{
  QElemPtr nhead,ohead;

  if (*head == NILQ)		/* if no queue then  */
    return(NILQ);		/*  return NIL */

  ohead = *head;		/* dereference and remember head */
  nhead = (*head)->q_forw;	/* this is the new head */
  if (nhead == ohead) {		/* is this the last element? */
    nhead = NILQ;		/* yes, will set the head to NIL */
  } else
    remque(ohead);		/* remove head */
  *head = nhead;		/* update head pointer */
  return(ohead);		/* return head of list */
}


/*
 * remove element from list, setting head to null if list becomes empty
 *
*/

void
dq_elem(head,item)
QHead head;
QElemPtr item;
{
  if (*head == NILQ)
    return;			/* can't remove from empty list! */

  if (*head == item)		/* removing head? */
    dq_head(head);		/* get rid of it nicely then */
  else 
    remque(item);
}


/*
 * QElemPtr q_mapf(QElemPtr head,int (*filter)())
 *
 * q_mapf maps across all entries in the queue pointed to by "head"
 * calling the "filter" routine with a pointer to each QElem.  If the 
 * filter routine returns TRUE then q_mapf returns with the current
 * QElemPtr.  If filter never returns TRUE then NILQ is returned.
 *
 * q_mapf processes the list head to tail.
 *
*/ 

QElemPtr
q_mapf(head,filter,arg)
QElemPtr head;
int (*filter)();
void *arg;
{
  QElemPtr step;

  if (head == NILQ)
    return(NILQ);

  step = head;
  do {
    if ((*filter)(step,arg))
      return(step);
    step = step->q_forw;
  } while (step != head);
  return(NILQ);
}

/*
 * QElemPtr q_mapb(QElemPtr head,int (*filter)())
 *
 * q_mapb is the same as q_mapf except the list is processed tail to
 * front.
 *
*/
 
QElemPtr
q_mapb(head,filter,arg)
QElemPtr head;
int (*filter)();
void *arg;
{
  QElemPtr step;

  if (head == NILQ)
    return(NILQ);

  step = head;
  do {
    step = step->q_back;
    if ((*filter)(step,arg))
      return(step);
  } while (step != head);
  return(NILQ);
}


/*
 * QElemPtr q_map_min(QElemPtr head,int (*filter)())
 *
 * map over the elements, returning the element with who filter reports
 * as having the smallest value
 *
 * bug: executes filter on head twice
*/
 
QElemPtr
q_map_min(head,filter,arg)
QElemPtr head;
int (*filter)();
void *arg;
{
  QElemPtr step;
  QElemPtr rv;
  int minval, curval;

  if (head == NILQ)
    return(NILQ);

  step = head;
  minval = (*filter)(step, arg); /* assume */
  rv = step;
  do {
    step = step->q_back;
    if ((curval = (*filter)(step,arg)) < minval) {
      minval = curval;
      rv = step;
    }
  } while (step != head);
  return(rv);
}

