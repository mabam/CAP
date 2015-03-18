/*
 * $Author: djh $ $Date: 1996/06/18 10:46:58 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afpidclnt.c,v 2.5 1996/06/18 10:46:58 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * AUFS fixed directory IDs
 *
 * library functions used by client programs only
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifdef FIXED_DIRIDS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
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

#include "afpidaufs.h"

static time_t openTime;
static char *pagFile = NULL;

int
connect_query(sock)
int sock;
{
	struct sockaddr *addr;
	int addrlen;

	query_addr(&addr, &addrlen);
	return(connect(sock, addr, addrlen));
}

time_t
getMtime(file)
char *file;
{
	struct stat buf;
	
	if (stat(file, &buf) < 0)
	  return(0);

	return(buf.st_mtime);
}

void
setPagFile()
{
	pagFile = (char *)malloc(strlen(aufsDbName)+4+1);
	strcpy(pagFile, aufsDbName);
#ifndef USE_GDBM
	strcat(pagFile, ".pag");
#endif  /* USE_GDBM */
}


int
open_dbase(first)
int first;
{
	datum initial_key, initial;
	char *version;

	if (db != NULL)
	  return(1);

	if (pagFile == NULL)
	  setPagFile();

	openTime = getMtime(pagFile); /* do before for safety! */

#ifdef USE_GDBM
	{ int tries = 0;
	  while ((db = gdbm_open(aufsDbName, 2048, GDBM_READER, 0644, 0L)) == NULL) {
	    if (++tries > 60)
	      return(-1);
	    usleep(250000);
	  }
	}
#else  /* USE_GDBM */
	if ((db = dbm_open(aufsDbName, O_RDONLY, 0644)) == NULL)
	  return(-1);
#endif /* USE_GDBM */

	if (first) {
	  initial_key.dptr = "I";
	  initial_key.dsize = strlen(initial_key.dptr);
#ifdef USE_GDBM
	  initial = gdbm_fetch(db, initial_key);
#else  /* USE_GDBM */
	  initial = dbm_fetch(db, initial_key);
#endif /* USE_GDBM */
	  if (initial.dptr == NULL) {
	    close_dbase();
	    return(-1);
	  }
	  if (extract_init(initial, &version, &rootEid) < 0) {
	    close_dbase();
	    return(-1);
	  }			
	  if (strcmp(version, aufsDbVersion) != 0) {
	    close_dbase();
	    return(-2);
	  }
#ifdef USE_GDBM
	  if (initial.dptr != NULL)
	    free(initial.dptr);
#endif /* USE_GDBM */
	}
	return(1);
}

void
close_dbase()
{
	if (db == NULL)
	  return;
#ifdef USE_GDBM
	gdbm_close(db);
#else  /* USE_GDBM */
	dbm_close(db);
#endif /* USE_GDBM */
	db = NULL;
	return;
}

int
poll_db(force)
int force;
{
	if (pagFile == NULL)
	  setPagFile();

	if (force || getMtime(pagFile) > openTime) {
	  int res;

	  close_dbase();
	  res = open_dbase(FALSE);
	  return((res >= 0) ? 0 : res);
	}

	return(1);
}

static int
send_command(command)
char *command;
{
	int sock;
	int n;

	if ((sock = query_socket()) < 0)
	  return(-2);

	if (connect_query(sock) < 0) {
	  close(sock);
	  return(-3);
	}

	n = send(sock, command, strlen(command), 0);
	close(sock);

	return(n);
}

int
send_new(path)
char *path;
{
	char command[MAXPATHLEN+3];

	sprintf(command, "A%s\277", path);
	return(send_command(command));
}

int
send_new_id(parent, remaining)
sdword parent;
char *remaining;
{
	char command[MAXPATHLEN+32];

	sprintf(command, "a%d\277%s\277", parent, remaining);
	return(send_command(command));
}	

int
send_new_fid(parent, remaining)
sdword parent;
char *remaining;
{
	char command[MAXPATHLEN+32];

	sprintf(command, "f%d\277%s\277", parent, remaining);
	return(send_command(command));
}	
 
int
send_delete(path)
char *path;
{
	char command[MAXPATHLEN+3];

	sprintf(command, "D%s\277", path);
	return(send_command(command));
}

int
send_delete_id(id)
sdword id;
{
	char command[32];

	sprintf(command, "d%d\277", id);
	return(send_command(command));
}

int
send_move(from, to)
char *from, *to;
{
	char command[2*MAXPATHLEN+4];

	sprintf(command, "M%s\277%s\277", from, to);
	return(send_command(command));
}

int
send_move_id(from_parent, old_name, to_parent, new_name)
sdword from_parent, to_parent;
char *old_name, *new_name;
{
	char command[2*MAXPATHLEN+32]; /* should be sufficient */

	sprintf(command, "m%d\277%s\277%d\277%s\277", from_parent,
	  old_name, to_parent, new_name);
	return(send_command(command));
}

int
send_rename(orig_path, new_name)
char *orig_path, *new_name;
{
	char command[2*MAXPATHLEN+4];

	sprintf(command, "R%s\277%s\277", orig_path, new_name);
	return(send_command(command));
}

int
send_rename_id(id, new_name)
sdword id;
char *new_name;
{
	char command[MAXPATHLEN+32];

	sprintf(command, "r%d\277%s\277", id, new_name);
	return(send_command(command));
}

int
send_clean()
{
	char command[32];

	sprintf(command, "C");
	return(send_command(command));
}

int
amAufsExt()
{
	return(0);
}

/*
 * never called, but needed for linking
 *
 */

void
flush_database()
{
}
#else  FIXED_DIRIDS
int afpdid_dummy_2; /* keep loader happy */
#endif FIXED_DIRIDS
