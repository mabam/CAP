/*
 * $Author: djh $ $Date: 1996/06/19 10:27:21 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afpidgnrl.c,v 2.5 1996/06/19 10:27:21 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * AUFS fixed directory IDs
 *
 * general library functions
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifdef FIXED_DIRIDS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#ifdef USE_GDBM
#include <gdbm.h>
#else  /* USE_GDBM */
#include <ndbm.h>
#endif /* USE_GDBM */
#include <malloc.h>
#include <sys/time.h>
#include <sys/param.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH
#include <assert.h>

#include "afpidaufs.h"

/* #define DEBUG 0 */

/* globals */
char *aufsSockname = SOCKNAME;
char *aufsDbName = DBNAME;
char *aufsDbVersion = DBVERSION;
char *aufsRootName = ROOTNAME;
sdword rootEid;
#ifdef USE_GDBM
GDBM_FILE db = NULL;
#else  /* USE_GDBM */
DBM *db = NULL;
#endif /* USE_GDBM */

char *
string_copy(str)
char *str;
{
	char *new = (char *)malloc(strlen(str)+1);

	strcpy(new, str);
	return(new);
}

sdword *
copy_children(children, num_children)
sdword *children;
int num_children;
{
	sdword *temp = (sdword*)calloc(num_children, sizeof(sdword));

	bcopy(children, temp, num_children*sizeof(sdword));
	return(temp);
}

int
query_socket()
{
	return(socket(AF_UNIX, SOCK_STREAM, 0));
}

void
query_addr(addr, len)
struct sockaddr **addr; int *len;
{
	static struct sockaddr_un un_addr;

	un_addr.sun_family = AF_UNIX;
	sprintf((char *)un_addr.sun_path, "%s", aufsSockname);
	*addr = (struct sockaddr *)&un_addr;
	*len = strlen(un_addr.sun_path) + 1 + sizeof(un_addr.sun_family);
}

int
is_directory(path)
char *path;
{
	struct stat statbuf;

	if (stat(path, &statbuf) < 0)
	  return(0);

	return(S_ISDIR(statbuf.st_mode));
}

int
is_file(path)
char *path;
{
	struct stat statbuf;

	if (stat(path, &statbuf) < 0)
	  return(0);

	return(S_ISREG(statbuf.st_mode));
}

datum
num_datum(num)
/* note only does one at a time! */
sdword num;
{
	datum temp;
	static unsigned char data[1+sizeof(sdword)];

	data[0] = 'N';
	bcopy((char *)&num, (char *)&data[1], sizeof(sdword));
	temp.dsize = 1+sizeof(sdword);
	temp.dptr = (char *)&data[0];

	return(temp);
}

sdword
num_from_datum(dat)
datum dat;
{
	sdword temp;

	if (dat.dptr == NULL || dat.dptr[0] != 'N')
	  return(-1);

	bcopy(&dat.dptr[1], (char *)&temp, sizeof(temp)); 

	return(temp);
}

/*
 * format is <parent><num children>{<children>}<name> 
 * all but <name> are sdwords. name is null terminated string
 *
 */

static sdword sdtemp[MAXPATHLEN/sizeof(sdword)]; /* should be sufficient ! */
static char *temp = (char *)sdtemp;

datum
new_entry(name, parent)
char *name;
sdword parent;
{
	datum new;

	sdtemp[0] = parent;
	sdtemp[1] = 0;		/* no children */
	strcpy((char *)&sdtemp[2], name);
	new.dptr = temp;
	new.dsize = 2*sizeof(sdword) + strlen(name)+1;

	return(new);
}

datum
create_entry(name, parent, num_children, children)
char *name;
sdword parent; 
int num_children;
sdword *children;
{
	datum new;

	sdtemp[0] = parent;
	sdtemp[1] = num_children;		/* no. children */
	bcopy((char *)children,(char *)&sdtemp[2],num_children*sizeof(sdword));
	strcpy((char *)&sdtemp[2+num_children], name);
	new.dptr = temp;
	new.dsize = (2+num_children)*sizeof(sdword) + strlen(name)+1;

	return(new);
}

int
extract_entry(entry, name, parent, num_children, children)
datum entry;
char **name;
sdword *parent; 
int *num_children;
sdword **children;
{
	if (entry.dptr == NULL)
	  return(-1);

	if (entry.dptr == temp) { /* we know we are word aligned! */
	  if (parent != NULL)
	    *parent = sdtemp[0];
	  if (num_children != NULL)
	    *num_children = sdtemp[1];
	  if (children != NULL)
	    *children = &sdtemp[2];
	  if (name != NULL)
	    *name = (char *)&sdtemp[2 + *num_children];
	} else {
	  int num;
	  sdword *area = (sdword *)entry.dptr;

	  bcopy((char *)&area[1], (char *)&num, sizeof(sdword));
	  if (parent != NULL)
	    bcopy((char *)&area[0], (char *)parent, sizeof(sdword));
	  if (num_children != NULL)
	    *num_children = num;
	  if (children != NULL) { /* need to ensure this is word aligned! */
	    bcopy((char *)&area[2], temp, num*sizeof(sdword));
	    *children = sdtemp;
	  }
	  if (name != NULL)
	    *name = (char *)&area[2 + num];
	}

	return(0);
}

datum
modify_name(entry, new_name)
datum entry;
char *new_name;
{
	int num_children;

	assert(entry.dptr != NULL);

	if (entry.dptr != temp) { /* don't overwrite db */
	  bcopy(entry.dptr, temp, entry.dsize);
	  entry.dptr = temp;
	}
	num_children = sdtemp[1];
	strcpy((char *)&sdtemp[2+num_children], new_name);
	entry.dsize = (2+num_children)*sizeof(sdword)+strlen(new_name)+1;

	return(entry);
}

datum
modify_parent(entry, parent)
datum entry;
sdword parent;
{
	assert(entry.dptr != NULL);

	if (entry.dptr != temp) { /* don't overwrite db */
	  bcopy(entry.dptr, temp, entry.dsize);
	  entry.dptr = temp;
	}
	sdtemp[0] = parent;
	/* entry.dsize remains unchanged */

	return(entry);
}

datum
modify_children(entry, num_children, children)
datum entry;
int num_children;
sdword *children;
{
	sdword *from;
	sdword *temp_area = NULL;
	datum new;
	char *name;
	int old_num;

	assert(entry.dptr != NULL);

	if (entry.dptr == temp) { 
	  /* we are going to use temp to create new area! */
	  temp_area = (sdword *)malloc(entry.dsize);
	  bcopy(entry.dptr, temp_area, entry.dsize);
	  from = temp_area;
	} else
	  from = (sdword *)entry.dptr;

	bcopy((char *)&from[0],(char *)&sdtemp[0],sizeof(sdword)); /* parent */
	bcopy((char *)&from[1], (char *)&old_num, sizeof(sdword));
	sdtemp[1] = num_children;
	bcopy(children, &sdtemp[2], num_children*sizeof(sdword));
	name = (char *)&from[2+old_num];
	strcpy((char *)&sdtemp[2+num_children], name);
	
	new.dsize = (num_children+2)*sizeof(sdword)+strlen(name)+1;
	new.dptr = temp;

	if (temp_area != NULL)
	  free(temp_area);

	return(new);
}

/*
 * note we always place the db version as first field, even though
 * it is a bit of a pain to extract the other data - since it is
 * not alligned properly
 *
 */

datum
create_init(version, rootEid)
char *version;
sdword rootEid;
{
	datum new;
	int len = strlen(version);

	strcpy(temp, version);
	bcopy((char *)&rootEid, &temp[len+1], sizeof(sdword));
	new.dptr = temp;
	new.dsize = len+1+sizeof(sdword);

	return(new);
}

int
extract_init(initial, version, rootEid)
datum initial;
char **version;
sdword *rootEid;
{
	int len;

	if (initial.dptr == NULL)
	  return(-1);

	if (version != NULL)
	  *version = initial.dptr;
	if (rootEid != NULL) {
	  len = strlen(initial.dptr);
	  bcopy(&initial.dptr[len+1], (char *)rootEid, sizeof(sdword));
	}
	return(1);
}

#ifdef USE_GDBM
static char ftchtmp[MAXPATHLEN+32];
#endif /* USE_GDBM */

datum
get_datum(id)
sdword id;
{
	datum key,entry;

	key = num_datum(id);
#ifdef USE_GDBM
	if (db != NULL)
	  gdbm_close(db);
	entry.dptr = NULL;
	{ int tries = 0;
	  while ((db = gdbm_open(aufsDbName, 2048, GDBM_READER, 0644, 0L)) == NULL) {
	    if (++tries > 60)
	      return(entry);
	    usleep(250000);
	  }
	}
	entry = gdbm_fetch(db, key);
	if (entry.dptr != NULL && entry.dsize <= sizeof(ftchtmp)) {
	  /* simulate ndbm static data areas */
	  bcopy(entry.dptr, ftchtmp, entry.dsize);
	  free(entry.dptr);
	  entry.dptr = ftchtmp;
	}
	gdbm_close(db);
	db = NULL;
#else  /* USE_GDBM */
	entry = dbm_fetch(db, key);
#endif /* USE_GDBM */
#ifdef DEBUG
	fprintf(stderr, "Read for id %d datum (%d,%x)\n", 
	    id, entry.dsize, entry.dptr);
#endif DEBUG
	return(entry);
}

int
store_datum(id, entry, flags)
sdword id;
datum entry;
int flags;
{
	int ret;
	datum key;

	key = num_datum(id);
#ifdef USE_GDBM
	if (db != NULL)
	  gdbm_close(db);
	{ int tries = 0;
	  while ((db = gdbm_open(aufsDbName, 2048, GDBM_WRITER, 0644, 0L)) == NULL) {
	    if (++tries > 60)
	      return(-1);
	    usleep(250000);
	  }
	}
	ret = gdbm_store(db, key, entry, flags);
	gdbm_close(db);
	db = NULL;
#else  /* USE_GDBM */
	ret = dbm_store(db, key, entry, flags);
#endif /* USE_GDBM */
#ifdef DEBUG
	fprintf(stderr, "Write for id %d datum (%d,%x) gave %d\n", 
	  id, entry.dsize, entry.dptr, ret);
#endif DEBUG
	flush_database();
	return(ret);
}

void
delete_datum(id)
sdword id;
{
	datum key;

	key = num_datum(id);
#ifdef USE_GDBM
	if (db != NULL)
	  gdbm_close(db);
	{ int tries = 0;
	  while ((db = gdbm_open(aufsDbName, 2048, GDBM_WRITER, 0644, 0L)) == NULL) {
	    if (++tries > 60)
	      return;
	    usleep(250000);
	  }
	}
	gdbm_delete(db, key);
	gdbm_close(db);
	db = NULL;
#else  /* USE_GDBM */
	dbm_delete(db, key);
#endif /* USE_GDBM */
}
	
static
int find_equiv(id, buffer)
sdword id;
char *buffer;
{
	if (id == rootEid) {
	  strcpy(buffer, ""); /* equiv path copes with special cases */
	  return(1);
	} else {
	  datum contents;
	  char *name;
	  sdword parent;
	  int ans;

	  contents = get_datum(id);
	  if (contents.dptr == NULL) {
	    fprintf(stderr, "find_equiv: get_datum returns NULL\n");
	    return(-1);
	  }

	  if (extract_entry(contents, &name, &parent, NULL, NULL) < 0) {
#ifdef DEBUG
	    fprintf(stderr, "find_equiv: extract_entry returns -1\n");
#endif DEBUG
	    return(-1);
	  }

	  name = string_copy(name); /* avoid probs with recursion */

	  ans = find_equiv(parent, buffer); /* recurse */

	  if (ans >= 0) {
	    strcat(buffer, "/");
	    strcat(buffer, name);
	  } 
#ifdef DEBUG
	  else
	    fprintf(stderr, "find_equiv: find_equiv returns -1\n");
#endif DEBUG

	  free(name);

	  return(ans);
	}
}
	  

char *
equiv_path(id)
sdword id;
{
	static char buffer[MAXPATHLEN]; /* used for returned string */
	int result;

	if (id == rootEid)
	  return(aufsRootName);

	buffer[0] = '\0'; /* start with null string in buffer */

	result = find_equiv(id, buffer);

#ifdef DEBUG
	fprintf(stderr, "Looking up %d gave %s\n", id, /* debug*/
	    (result < 0) ? NULL : buffer);
#endif DEBUG
	return((result < 0) ? NULL : buffer);
}

#define null_string(str) (str == NULL || str[0] == '\0')

static char *
next_ele(path, name)
char *path, *name;
{
	char *ptr;

	if (null_string(path)) { /* given "/" to start with ? */
	  strcpy(name, "");
	  return(NULL);
	}

	ptr = index(path, '/');
	if (ptr == NULL) {
	  strcpy(name, path);
	  return(NULL);
	}
	  
	name[0] = '\0'; /* use strncat to overcome occational bug */
	strncat(name, path, ptr-path);

	return(ptr+1);   /* rest */
}

/*
 * add child entry to record
 *
 */

int
add_child(id, child)
sdword id;
sdword child;
{
	datum entry;
	sdword *children, *new_children;
	int num_children;
	int ret;

	if (child <= 0 || id <= 0)
	  return(0);

#ifdef DEBUG
        fprintf(stderr, "adding child %d to %d\n", child, id);
#endif DEBUG
		  
	entry = get_datum(id);
	if (entry.dptr == NULL) /* must assume it exists */
	  return(-1);
	if (extract_entry(entry,NULL,NULL,&num_children,&children) < 0)
	  return(-1);
	new_children = (sdword *)calloc(num_children+1, sizeof(sdword));
	bcopy(children, new_children, num_children*sizeof(sdword));
	num_children += 1;
	new_children[num_children-1] = child;
	entry = modify_children(entry, num_children, new_children);
	if (ret = store_datum(id, entry, DBM_REPLACE) < 0) {
#ifdef DEBUG
	  fprintf(stderr, "add_child: store datum failed for id %d (%d)\n",
		id, ret);
#endif DEBUG
	}
	free(new_children);
	return(1);
}

/*
 * delete child entry to record
 *
 */

int
delete_child(id, child)
sdword id;
sdword child;
{
	datum entry;
	sdword *children, *new_children;
	int num_children;
	int i,new_num;

	if (child <= 0 || id <= 0)
	  return(0);

	entry = get_datum(id);
	if (entry.dptr == NULL) /* must assume it exists */
	  return(-1);
	if (extract_entry(entry,NULL,NULL,&num_children,&children) < 0)
	  return(-1);
	/* too big won't hurt */
	new_children = copy_children(children, num_children);
	bcopy(children, new_children, num_children*sizeof(sdword));
	for (i=0,new_num=0; i<num_children; i++) {
	  if (children[i] != child)
	    new_children[new_num++] = children[i];
	}
	entry = modify_children(entry, new_num, new_children);
	(void)store_datum(id, entry, DBM_REPLACE);
	free(new_children);
	return(1);
}

	
static
sdword new_entries(parent, path)
sdword parent;
char *path;
{
	char name[128];
	char *remaining;
	datum entry;
	sdword id, child;

	if (null_string(path))
	  return(0);

	remaining = next_ele(path, name);

#ifdef DEBUG
	fprintf(stderr, "Creating %d/%s\n", parent, name);
#endif DEBUG

	entry = new_entry(name, parent);

	for ( ; ; ) { /* look for new id */
	  id = valid_id();
	  if (store_datum(id, entry, DBM_INSERT) >= 0)
	    break;
	}

	child = new_entries(id, remaining); /* recurse */

	(void)add_child(id, child); /* not sure what to do with error */

	return(id);
}

int
lookup_path_id(parent, path, id, dat, create)
sdword parent;
char *path; /* relative to parent */
sdword *id;
datum *dat;
int create; /* server only */
{
	sdword current = parent;
	char name[128];
	char *remaining;
	sdword *children = NULL, child;
	int num_children;
	datum data, child_data;
	int indx;
	char *child_name;

	data = get_datum(parent);
	if (data.dptr == NULL)
	  return(-2); /* help!! Lost something !! */

	remaining = next_ele(path, name);

	if (null_string(name)) { /* were after parent */
	  if (dat)
	    *dat = data;
	  if (id)
	    *id = parent;
	  return(1);
	}

	redo:
	for ( ; ; ) {
#ifdef DEBUG
	  fprintf(stderr, "Looking up %d/%s\n", current, name); /* debug */
#endif DEBUG
	  if (extract_entry(data, NULL, NULL, &num_children, &children) < 0)
	      return(-1); /* error */
	  /* take copy in dbm overwrites original area */
	  children = copy_children(children, num_children);
	  indx = 0;
	  while (indx < num_children) {
	    child = children[indx++];
	    child_data = get_datum(child);
	    if (child_data.dptr == NULL)
	      continue; /* error - missing child */
	    if (extract_entry(child_data, &child_name, NULL, NULL, NULL) < 0)
	      continue; /* error, no name? */
	    if (strcmp(name, child_name) == 0) { /* found it ! */
	      if (null_string(remaining)) {
	        if (id)
	          *id = child;
	        if (dat)
	          *dat = child_data;
	        free(children);
	        return(1);
	      } else {
	        free(children);
	        path = remaining;
	        remaining = next_ele(remaining, name);
	        current = child;
	        data = child_data;
	        goto redo;
	      }
	    }
	  }
	  /* if we get this far, something is missing */
	  free(children);
	
	  /* something missing */
	  if (!create || !amAufsExt()) 
	    return(0);
	
	  /* assume we are in the server */

	  child = new_entries(current, path);
	  if (add_child(current, child) < 0)
	    return(-1);
	  flush_database();

	  /* having created object, loop back to it. Perhaps not the most
	     efficient way, but seems reliable */
	  data = get_datum(current);
	  if (data.dptr == NULL) /* should never happen */
	    return(-2);
	}
}

int
lookup_path(path, id, dat, create)
char *path;
sdword *id;
datum *dat;
int create; /* server only */
{
	char *remaining;


	if (null_string(path))
	  remaining = NULL;
	else
	  remaining = path+1; /* skip '/' */

	return(lookup_path_id(rootEid, remaining, id, dat, create));
}

void
do_delete_entry(id)
sdword id;
{
	datum data;
	sdword *children;
	int num_children;
	int i;

	data = get_datum(id);
	if (data.dptr == NULL)
	  return;
	if (extract_entry(data, NULL, NULL, &num_children, &children) >= 0) {
	  children = copy_children(children, num_children);
	  for (i=0; i < num_children ; i++)
	    do_delete_entry(children[i]);
	  free(children);
	}
	delete_datum(id);
}

#define MINID 200		 /* The first so many have special meanings. */
#define MAXID ((1<<30)-1)	 /* 2^30-1, guessed that this is in range. */

sdword
valid_id()
{
#ifdef USERAND
	sdword id = rand()%(MAXID-MINID)+MINID;
#else  /* USERAND */
	sdword id = random()%(MAXID-MINID)+MINID;
#endif /* USERAND */

	return(id);
}

#undef MINID
#undef MAXID

void
init_aufsExt()
{
#ifdef USERAND
	srand(time(NULL));
#else  /* USERAND */
	srandom((int)time(NULL));
#endif /* USERAND */
}
#else  FIXED_DIRIDS
int afpdid_dummy_3; /* keep loader happy */
#endif FIXED_DIRIDS
