/* "$Author: djh $ $Date: 1995/08/28 11:10:14 $" */
/* "$Header: /mac/src/cap60/applications/lwsrv/RCS/fontlist.h,v 2.2 1995/08/28 11:10:14 djh Rel djh $" */
/* "$Revision: 2.2 $" */

/*
 * fontlist - UNIX AppleTalk spooling program: act as a laserwriter
 *   handles simple font list - assumes we can place in a file
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986,1987 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  Created sept 5, 1987 by cck from lwsrv
 *
 */

#ifndef LWSRV8
typedef struct Font_List {		/* fontlist as read from fonts file */
  struct Font_List *fl_next;		/*  pointer to next font name */
  char *fl_name;			/*  the name itself */
} FontList;
boolean LoadFontList();
#else /* LWSRV8 */
List *LoadFontList();
#endif /* LWSRV8 */

void SendFontList();
