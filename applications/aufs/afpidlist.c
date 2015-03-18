/*
 * $Author: djh $ $Date: 1996/06/18 10:49:40 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpidlist.c,v 2.3 1996/06/18 10:49:40 djh Rel djh $
 * $Revision: 2.3 $
 *
 */

/*
 * tool that prints out the contents of the AUFS fixed directory ID database
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifdef FIXED_DIRIDS

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>

#include "../../lib/afp/afpidaufs.h"

void
print_tree(id, prefix)
sdword id;
char *prefix;
{
	char me [MAXPATHLEN];
	sdword parent;
	char *name;
	int num_children;
	sdword *children;
	datum key, data;
	int i;

	if (id != rootEid) { /* root a bit different, because name is "/" */
	  strcpy(me, prefix);
	  strcat(me, "/");
	} else
	  strcpy(me, "");

	key = num_datum(id);
#ifdef USE_GDBM
	data = gdbm_fetch(db, key);
#else  /* USE_GDBM */
	data = dbm_fetch(db, key);
#endif /* USE_GDBM */

	if (data.dptr == NULL) {
	  printf("**Error** id %d not found (prefix '%s')\n", id, prefix);
	  return;
	}

	if (extract_entry(data,&name,&parent,&num_children,&children) < 0) {
	  printf("**Error** had probs with data for id %d(prefix '%s')\n",
	    id, prefix);
	  return;
	}

	if (id == rootEid)
	  printf("RootEid = %d (+%d)\n", id, num_children);
	else {
	  strcat(me, name);
	  printf("%d = (%d) '%s' (+%d)\n", id, parent, me, num_children);
	}

	children = copy_children(children, num_children);

#ifdef USE_GDBM
	free(data.dptr);
#endif /* USE_GDBM */

	for (i = 0; i < num_children; i++)
	  print_tree(children[i], me);

	free(children);
}
	

main(argc, argv)
int argc;
char *argv[];
{
	int ret;

	if ((ret = open_dbase(1)) < 0) {
	  fprintf(stderr, "Can't open database %s (%d,%d)\n",
	      aufsDbName, ret, errno);
	  exit(-1);
	}

	print_tree(rootEid, NULL);
	close_dbase();
	exit(0);
}
#else  FIXED_DIRIDS
#include <stdio.h>
main()
{
	printf("afpidlist: not compiled with -DFIXED_DIRIDS\n");
}
#endif FIXED_DIRIDS
