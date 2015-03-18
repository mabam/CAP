/*
 * $Author: djh $ $Date: 1996/06/19 10:14:39 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afpidaufs.c,v 2.5 1996/06/19 10:14:39 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * AUFS fixed directory IDs
 *
 * These routines provide the external <-> path mechanism for
 * aufs itself. As well as the direct approach there is a backup
 * mechanism using a direct table. This is not very efficient,
 * and should not be used normally. It is intended to avoid
 * crashing aufs if the server falls over for some reason.
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifdef FIXED_DIRIDS

#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#include "afpidaufs.h"
#include "../../applications/aufs/afps.h"

typedef struct {
	sdword ext;
	char *path;
} Xtable;

#define TimeOut (15) /* time to wait for answer from server */

static Xtable *extras;
static int num_extras;
static int db_open;
static int server_probs;

static
void check_db()
{
	if (db_open) {
	  int temp = poll_db(FALSE);

	  if (temp < 0) {
	    db_open = 0;
	    if (DBDIR)
	      logit(0, "Can't poll database %d", temp);
	  }
	}
}

void
aufsExtInit()
{
	int temp;

	init_aufsExt();

	if ((temp = open_dbase(TRUE)) >= 0) {
	  db_open = 1;
	} else {
	  db_open = 0;
	  logit(0, "Can't open database %d",temp);
	}
	server_probs = 0;
	extras = NULL;
#ifdef USE_GDBM
	close_dbase();
#endif /* USE_GDBM */
}

static int
existing_entry(id)
sdword id;
{
	datum contents;

	contents = get_datum(id);
	return(contents.dptr != NULL);
}

static sdword
add_to_extras(path)
char *path;
{
	for ( ; ; ) {
	  int found = 0;
	  sdword try = valid_id();

	  if (db_open && existing_entry(try))  /* first check in database */
	    continue;

	  if (extras != NULL) { /* and check for dup in Xtras */
	    Xtable *ptr;
	    int found = 0;

	    for (ptr = extras; ptr->path != NULL; ptr++) {
	      if (ptr->ext == try) {
	        found = 1;
	        break;
	      }
	    }
	    if (found)
	      continue;
	  }

	  /* have unique id! */

	  if (extras == NULL) {
	    num_extras = 2;			
	    extras = (Xtable *)calloc(num_extras, sizeof(*extras));
	  } else {
	    num_extras += 1;
	    extras = (Xtable *)realloc(extras, num_extras*sizeof(*extras));
	    extras[num_extras-1] = extras[num_extras-2];
	  }
	  extras[num_extras-2].ext = try;
	  extras[num_extras-2].path = (char *)malloc(strlen(path)+1);
	  strcpy(extras[num_extras-2].path, path);
	  return(try);
	}
	/* NOTREACHED */
}

sdword
aufsExtFindId (path)
char *path;
{
	int i, res;

	if (db_open)
	  check_db();

	if (db_open) {
	  sdword id;

	  /* have we got an existing entry ? */
	  if (lookup_path(path, &id, NULL, 0) > 0)
	    return(id);
	}

	/*
	 * No, return an error
	 */
	return (-1);
}

void
aufsExtExchange(apath, bpath)
char *apath, *bpath;
{
	sdword	aID, bID;
	char	t_path[MAXPATHLEN];
	char	*t_ptr;
	char	*tname = ".txxx";

	/*
	 * Get the ID's
	 */
	aID = aufsExtFindId(apath);
	bID = aufsExtFindId(bpath);
	if (aID >= 0 && bID >= 0) {
		/*
		 * Both have fileID's -- need to do three renames
		 */
		strcpy(t_path, apath);
		t_ptr = (char *)strrchr(t_path, '/');
		if (t_ptr) {
			t_ptr++;
			strcpy(t_ptr, tname);
		}
		aufsExtRename(apath, t_path);
		aufsExtRename(bpath, apath);
		aufsExtRename(t_path, bpath);
	} else if (aID >= 0) {
		/*
		 * Only aID has a filID -- rename to bpath
		 */
		aufsExtRename(apath, bpath);
	} else if (bID >= 0) {
		/*
		 * Only bID has a filID -- rename to apath
		 */
		aufsExtRename(bpath, apath);
	}
	return;
}

sdword
aufsExtEDir(path)
char *path;
{
	int i, res;

	if (db_open)
	  check_db();

	if (db_open) {
	  sdword id;

	  /* have we got an existing entry ? */
	  if (lookup_path(path, &id, NULL, 0) > 0)
	    return(id);

	  /* ask the server to create a new record - even if having problems */
	  if ((res = send_new(path)) >= 0 && server_probs < 5) {
	    for (i = 0; i < TimeOut*4; i++) {
	      int poll;

	      abSleep(1, FALSE); /* wait 0.25s! */
	      poll = poll_db( i%(4*4) == 0 ); /* force poll each 4 s */
	      if (poll < 0) { /* lost dbase */
	        logit(0, "Lost database %d errno=%d",poll,errno);
	        db_open = 0;
	        break;
	      } else {
	        if (poll == 0) /* db has changed */
	          if (lookup_path(path, &id, NULL, 0) > 0)
	            return(id);
	      }
	    }
	  } else {
	    logit(0, "Send_new failed with %d errno=%d", res, errno);
	    server_probs++;
	  }
	}

	if (DBDIR) 
	  logit(0, "Lost connection to aufsExt server?");

	return(add_to_extras(path));
}

sdword
aufsExtEDirId(parent, rest)
sdword parent;
char *rest;
{
	int i, res;

	if (db_open)
	  check_db();

	if (db_open) {
	  sdword id;

	  /* have we got an existing entry ? */
	  if (lookup_path_id(parent, rest, &id, NULL, 0) > 0) {
	    if (DBDIR)
	  	logit(0, "aufsExtEDirId for %s is %d", rest, id);
	    return(id);
	  }

	  /* ask the server to create a new record - even if having problems */
	  if ((res = send_new_id(parent, rest)) >= 0 && server_probs < 5) {
	    for (i = 0; i < TimeOut*4; i++) {
	      int poll;

	      abSleep(1, FALSE); /* wait 0.25s! */
	      poll = poll_db( i%(4*4) == 0 ); /* force poll each 4 s */
	      if (poll < 0) { /* lost dbase */
      	        logit(0, "Lost database %d errno=%d",poll,errno);
	        db_open = 0;
	        break;
	      } else {
	        if (poll == 0) { /* db has changed */
	          if (lookup_path_id(parent, rest, &id, NULL, 0) > 0) {
		    if (DBDIR)
			logit(0, "aufsExtEDirId created new id for %s:%d", rest, id);
	            return(id);
		  }
	        }
	      }
	    }
	  } else {
	    logit(0, "Send_new failed with %d errno=%d", res, errno);
	    server_probs++;
	  }
	}

	if (DBDIR) 
	  logit(0, "Lost connection to aufsExt server?");

	{ /* should never really get here, but just in case ... */
	  char *str = equiv_path(parent);
	  char *temp = (char *)malloc(strlen(str) + strlen(rest) + 2);
	  sdword id;

	  strcpy(temp, str);
	  if (strlen(str) > 1)
	    strcat(temp, "/");
	  strcat(temp, rest);
	  id = add_to_extras(temp);
	  free(temp);
	  return(id);
	}
}

char *
aufsExtPath(edir)
sdword edir;
{
	char *ans = NULL;

	if (db_open)
	  check_db();

	if (db_open) 
	  ans = equiv_path(edir);

	if (ans == NULL && extras != NULL) { /* lookup in extras */
	  Xtable *ptr;

	  for (ptr = extras; ptr->path != NULL; ptr++) 
	    if (ptr->ext == edir) {
	      ans = ptr->path;
	      break;
	    }
	}
	return(ans);
}

void
aufsExtDel(path)
char *path;
{
	int found = 0;

	if (db_open)
	  check_db();

	if (db_open) {
	  sdword id;
	  if (lookup_path(path, &id, NULL, 0) > 1) {
	    found = 1;
	    send_delete_id(id);
	  }
	}

	if (! found && extras != NULL) { /* lookup in extras */
	  Xtable *ptr;

	  for (ptr = extras; ptr->path != NULL; ptr++) {
	    if (ptr->ext == -1)
	      continue;
	    if (strcmp(path, ptr->path) == 0) {
	      free(ptr->path);
	      ptr->ext = -1;
	      break;
	    }
	  }
	}
}

void
aufsExtDelId(edir)
sdword edir;
{
	int found = 0;

	if (db_open)
	  check_db();

	if (db_open) {
	  if (existing_entry(edir)) {
	    found = 1;
	    send_delete_id(edir);
	  }
	}

	if (! found && extras != NULL) { /* lookup in extras */
	  Xtable *ptr;

	  for (ptr = extras; ptr->path != NULL; ptr++) {
	    if (ptr->ext == -1)
	      continue;
	    if (ptr->ext == edir) {
	      free(ptr->path);
	      ptr->ext = -1;
	      break;
	    }
	  }
	}
}

sdword
aufsExtEFileId(parent, rest)
sdword parent;
char *rest;
{
	int i, res;

	if (db_open)
	  check_db();

	if (db_open) {
	  sdword id;

	  /* have we got an existing entry ? */
	  if (lookup_path_id(parent, rest, &id, NULL, 0) > 0)
	    return(id);

	  /* ask the server to create a new record - even if having problems */
	  if ((res = send_new_fid(parent, rest)) >= 0 && server_probs < 5) {
	    for (i = 0; i < TimeOut*4; i++) {
	      int poll;

	      abSleep(1, FALSE); /* wait 0.25s! */
	      poll = poll_db( i%(4*4) == 0 ); /* force poll each 4 s */
	      if (poll < 0) { /* lost dbase */
      	        logit(0, "Lost database %d errno=%d",poll,errno);
	        db_open = 0;
	        break;
	      } else {
	        if (poll == 0) { /* db has changed */
	          if (lookup_path_id(parent, rest, &id, NULL, 0) > 0)
	            return(id);
	        }
	      }
	    }
	  } else {
	    logit(0, "Send_new failed with %d errno=%d", res, errno);
	    server_probs++;
	  }
	}

	if (DBDIR) 
	  logit(0, "Lost connection to aufsExt server?");

	return(-1);
}

static void
move_in_extras(from, to)
char *from, *to;
{
	if (extras) { /* go through and change any */
	  Xtable *ptr;

	  for (ptr = extras; ptr->path != NULL; ptr++) {
	    if (ptr->ext == -1)
	      continue;
	    if (strcmp(from, ptr->path) == 0) {
	      free(ptr->path);
	      ptr->path = string_copy(to);
	      break;
	    }
	  }
	}
}

static void
rename_in_extras(path, new_name)
char *path, *new_name;
{
	char temp [MAXPATHLEN];
	char *ptr;

	strcpy(temp, path);
	if ((ptr = (char *)rindex(temp, '/')) == NULL)
	  return; /* not a real path */
	strcpy(ptr+1, new_name);
	move_in_extras(path, temp);
}

static void
rename_id_in_extras(id, new_name)
sdword id;
char *new_name;
{
	char temp [MAXPATHLEN];
	char *p;

	if (extras) { /* go through and change any */
	  Xtable *ptr;

	  for (ptr = extras; ptr->path != NULL; ptr++) {
	    if (ptr->ext == -1)
	      continue;
	    if (ptr->ext == id) {
	      strcpy(temp, ptr->path);
	      if ((p = (char *)rindex(temp, '/')) == NULL)
	        return; /* not a real path */
	      strcpy(p+1, new_name);
	      free(ptr->path);
	      ptr->path = string_copy(temp);
	      break;
	    }
	  }
	}
}

	
void
aufsExtMove(from, to)
char *from, *to;
{
	send_move(from, to); /* send anyway */

	if (extras)
	  move_in_extras(from, to);
}

void
aufsExtMoveIds(orig, newParent, newName)
sdword orig, newParent;
char *newName;
{
	datum data;
	sdword parent;
	char *name;

	if (db_open)
	  check_db();

	if (db_open) {
	  data = get_datum(orig);
	  if (data.dptr != NULL) {
	    if (extract_entry(data, &name, &parent, NULL, NULL) < 0)
	      return;
	    send_move_id(parent, name, newParent, newName);
	    return;
	  }
	}

	/* try to move the extras - note move this only and not children */
	
	{
	  char *newParent_name = aufsExtPath(newParent);
	  char *temp = (char *)malloc(strlen(newParent_name)+strlen(newName)+2);

	  strcpy(temp, newParent_name);
	  if (strlen(newParent_name) > 1)
	    strcat(temp, "/");
	  strcat(temp, newName);

	  move_in_extras(aufsExtPath(orig), temp);

	  free(temp);
	}
}

void
aufsExtMoveId(origPath, newParent, newName)
sdword newParent;
char *newName, *origPath;
{
	datum data;
	sdword parent;
	char *name;
	
	if (db_open)
	  check_db();

	if (db_open) {
	  if (lookup_path(origPath, NULL, &data, 0) > 0) {
	    if (extract_entry(data, &name, &parent, NULL, NULL) < 0)
	      return;
	    send_move_id(parent, name, newParent, newName);
	    return;
	  }
	}

	/* otherwise not sure what to do, try falling back on aufsExtMove! */

	{
	  char *newParent_name = aufsExtPath(newParent);
	  char *temp = (char *)malloc(strlen(newParent_name)+strlen(newName)+2);

	  strcpy(temp, newParent_name);
	  if (strlen(newParent_name) > 1)
	    strcat(temp, "/");
	  strcat(temp, newName);

	  aufsExtMove(origPath, temp);

	  free(temp);
	}
}

void
aufsExtRename(path, newName)
char *path, *newName;
{
	sdword id;

	if (db_open)
	  check_db();

	if (db_open) {
	  if (lookup_path(path, &id, NULL, 0) > 0) {
	    send_rename_id(id, newName);
	    return;
	  }
	}

	/* try to move the extras - note rename this only and not children */

	rename_in_extras(path, newName);
}

void
aufsExtRenameId(edir, newName)
sdword edir; char *newName;
{
	datum data;
	int found = 0;
	char *name;

	if (db_open)
	  check_db();

	if (db_open) {
	  data = get_datum(edir);
	  if (data.dptr != NULL) {
	    send_rename_id(edir, newName);
	    return;
	  }
	}

	/* try to move the extras - note rename this only and not children */

	rename_id_in_extras(edir, newName);
}
#else  FIXED_DIRIDS
int afpdid_dummy_1; /* keep loader happy */
#endif FIXED_DIRIDS
