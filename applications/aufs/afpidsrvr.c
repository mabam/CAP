/*
 * $Author: djh $ $Date: 1996/06/19 10:32:13 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpidsrvr.c,v 2.5 1996/06/19 10:32:13 djh Rel djh $
 * $Revision: 2.5 $
 *
 */

/*
 * Server to provide AUFS fixed directory ID database write facilities
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifdef FIXED_DIRIDS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
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
#include <assert.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#include "../../lib/afp/afpidaufs.h"

int verbose = 0;
int disconnect = 1;
char *log = NULL;
int continue_clean;

extern char *optarg;
extern int optind, opterr;

int queries; /* request socket */
int bound_queries = 0; /* whether to delete socket or not */
void create_queries();

void session();
void set_signal();
void do_disconnect();
void clean_entries();

void open_database();

void
fatal(message)
char *message;
{
	if (message != NULL)
	  fprintf(stderr, "%s:", message);
	if (errno > 0) {
	  fprintf(stderr, " %d: ", errno);
	  perror("");
	} else
	  putc('\n', stderr);
#ifdef USE_GDBM
	if (db != NULL)
	  gdbm_close(db);
#else  /* USE_GDBM */
	if (db != NULL)
	  dbm_close(db);
#endif /* USE_GDBM */
	if (bound_queries)
	  unlink(aufsSockname);
	exit(-1);
}

void
fatal2(message, a1)
char *message, *a1;
{
	fprintf(stderr, "afpidsrvr: fatal error: %s%s :", message, a1);
 	fatal(NULL);
}

void
clean_exit(n)
int n;
{
#ifdef USE_GDBM
	if (db != NULL)
	  gdbm_close(db);
#else  /* USE_GDBM */
	if (db != NULL)
	  dbm_close(db);
#endif /* USE_GDBM */
	if (bound_queries)
	  unlink(aufsSockname);
	exit(n);
}	

main(argc, argv)
char *argv[];
{
	int c;
	extern char *optarg;
	int doClean = 0;

	while ((c = getopt(argc, argv, "vtcl:")) != -1) {
	  switch (c) {
	    case 'v':
	      verbose = 1;
	      break;
	    case 't':
	      disconnect = 0;
	      break;
	    case 'l':
	      log = optarg;
	      break;
	    case 'c':
	      doClean = 1;
	      break;
	    case '?':
	      fprintf(stderr,
		"usage: afpidsrvr [-c] [-v] [-t] [-l logfile]\n");
	      exit(-1);
	  }
	}

	set_signal();

	if (disconnect)
	  do_disconnect();

	if (log) {
	  int fd;

	  if ((fd = open(log, O_WRONLY|O_APPEND|O_CREAT)) < 0)
	    fd = open("/dev/null", O_WRONLY);

	  if (fd >= 0) {
#ifndef NODUP2
	    dup2(fd, 2);
#else NODUP2
	    close(2);
	    dup(fd);
#endif NODUP2
	    close(fd);
	  }
	}

	init_aufsExt();

	open_database();

	create_queries();

	if (doClean)
	do
	  clean_entries();
	while (continue_clean);

#ifdef USE_GDBM
	gdbm_close(db);
	db = NULL;
#endif /* USE_GDBM */

	{
	  time_t now;

	  time(&now);
	  fprintf(stderr, "Starting afpidsrvr with %s at %s",
	    aufsDbName, ctime(&now));
	}

	session();
	/* NOTREACHED */
}

/*
 * create queries "pipe" entry
 *
 */

void
create_queries()
{
	struct sockaddr *addr;
	int addrlen;

	if ((queries = query_socket()) < 0)
	  fatal("Problem creating socket");

	query_addr(&addr, &addrlen);
	if (bind(queries, addr, addrlen) < 0)
	  fatal2("Problem binding socket ", aufsSockname);
	bound_queries = 1;
	
#ifdef linux
	chmod(aufsSockname, 0666);
#endif /* linux */

	if (listen(queries, 5) < 0)
	  fatal("Listen");

	return;
}

void
do_exit()
{
	clean_exit(-1);
}

/*
 * force clean exit
 *
 */

void
set_signal()
{
	if (signal(SIGHUP, SIG_IGN)!=SIG_IGN)
	  signal(SIGHUP, do_exit);
	if (signal(SIGINT, SIG_IGN)!=SIG_IGN)
	  signal(SIGINT, do_exit);
	if (signal(SIGTERM, SIG_IGN)!=SIG_IGN)
	  signal(SIGTERM, do_exit);
}

void
create_database()
{
	datum initial_key, initial;
	datum root, root_datum;

#ifdef USE_GDBM
	if ((db = gdbm_open(aufsDbName, 2048, GDBM_WRCREAT, 0644, 0L)) == NULL)
	  fatal("Creating db");
#else  /* USE_GDBM */
	if ((db = dbm_open(aufsDbName, O_RDWR|O_CREAT, 0644)) == NULL)
	  fatal("Creating db");
#endif /* USE_GDBM */

	rootEid = valid_id(); /* any will do */
	root_datum = new_entry(aufsRootName, (sdword)0);
	root = num_datum(rootEid);
#ifdef USE_GDBM
	if (gdbm_store(db, root, root_datum, GDBM_REPLACE) < 0)
	  fatal("Writing root record");
#else  /* USE_GDBM */
	if (dbm_store(db, root, root_datum, DBM_REPLACE) < 0)
	  fatal("Writing root record");
#endif /* USE_GDBM */

	initial_key.dptr = "I";
	initial_key.dsize = strlen(initial_key.dptr);
	initial = create_init(aufsDbVersion, rootEid);
#ifdef USE_GDBM
	if (gdbm_store(db, initial_key, initial, GDBM_REPLACE) < 0)
	  fatal("Writing root record");
#else  /* USE_GDBM */
	if (dbm_store(db, initial_key, initial, DBM_REPLACE) < 0)
	  fatal("Writing root record");
#endif /* USE_GDBM */

	flush_database();
}


void
open_database()
{
	datum initial_key, initial;
	char *version;

	initial_key.dptr = "I";
	initial_key.dsize = strlen(initial_key.dptr);

#ifdef USE_GDBM
	db = gdbm_open(aufsDbName, 2048, GDBM_READER, 0644, 0L);
#else  /* USE_GDBM */
	db = dbm_open(aufsDbName, O_RDWR, 0644);
#endif /* USE_GDBM */
	if (db == NULL && errno == ENOENT)
	  create_database();
	if (db == NULL)
	  fatal("Opening db");

#ifdef USE_GDBM
	initial = gdbm_fetch(db, initial_key);
#else  /* USE_GDBM */
	initial = dbm_fetch(db, initial_key);
#endif /* USE_GDBM */
	if (initial.dptr == NULL)
	  fatal("Suspect Database");
	if (extract_init(initial, &version, &rootEid) < 0)
	  fatal("Problem with initial record");
	if (strcmp(version, aufsDbVersion) != 0)
	  fatal2("Incompatible d/b, can't deal with version ", version);

#ifdef USE_GDBM
	free(initial.dptr);
#endif /* USE_GDBM */

	return;
}

void
add_entry(entry)
char *entry;
{
	int ret;

	if (!is_directory(entry))
	  return;

	if ((ret = lookup_path(entry, NULL, NULL, 1)) <= 0) 
	  fprintf(stderr, "Failed to create '%s' (%d, %d)\n",
	    entry, ret, errno);

	return;
}

void
delete_entry_id();

void
add_entry_id(parent, rest)
sdword parent;
char *rest;
{
	int ret;
	sdword id;
	char *entry;

	if ((ret = lookup_path_id(parent, rest, &id, NULL, 1)) <= 0) {
	  fprintf(stderr, "Failed to create %d/'%s' (%d, %d)\n", 
	    parent, rest, ret, errno);
	  return;
	}
	if (verbose)
	  fprintf(stderr, "Created entry %d/'%s' (%d, %d) id = %d\n",
		parent, rest, ret, errno, id);
		  
	entry = equiv_path(id);
	if (!is_directory(entry)) /* check existance in restrospect! */
	  delete_entry_id(id);
	flush_database();

	return;
}

void
add_entry_fid(parent, rest)
sdword parent;
char *rest;
{
	int ret;
	sdword id;
	char *entry;

	if ((ret = lookup_path_id(parent, rest, &id, NULL, 1)) <= 0) {
	  fprintf(stderr, "Failed to create %d/'%s' (%d, %d)\n", 
	    parent, rest, ret, errno);
	  return;
	}
	entry = equiv_path(id);
	if (!is_file(entry)) /* check existance in restrospect! */
	  delete_entry_id(id);
	flush_database();

	return;
}

void
delete_entry(entry)
char *entry;
{
	sdword id, parent;
	datum data;

	if (is_directory(entry) || is_file(entry))
	  return;

	if (lookup_path(entry, &id, &data, 0) > 0){
	  (void) extract_entry(data, NULL, &parent, NULL, NULL);
	  do_delete_entry(id);
	  delete_child(parent, id);
	  flush_database();
	}

	return;
}

void
delete_entry_id(id)
sdword id;
{
	sdword parent;
	datum data;
	char *entry = equiv_path(id);

	if (is_directory(entry) || is_file(entry))
	  return;

	data = get_datum(id);
	if (data.dptr == NULL)
	  return;
	if (extract_entry(data, NULL, &parent, NULL, NULL) < 0)
	  return;
	do_delete_entry(id);
	delete_child(parent, id);
	flush_database();

	return;
}

void
delete_entry_fid(id)
sdword id;
{
	sdword parent;
	datum data;
	char *entry = equiv_path(id);

	if (is_file(entry))
	  return;

	data = get_datum(id);
	if (data.dptr == NULL)
	  return;
	if (extract_entry(data, NULL, &parent, NULL, NULL) < 0)
	  return;
	do_delete_entry(id);
	delete_child(parent, id);
	flush_database();

	return;
}

void
move_entry(from, to)
char *from, *to;
{
	datum data;
	sdword id, from_parent, to_parent;
	char to_directory[MAXPATHLEN];
	char *new_name;
	char *name;
	char *ptr;

	assert(to[0] == '/');

	if (is_directory(from) || !is_directory(to))
	  if (is_file(from) || !is_file(to))
	    return;

	if (lookup_path(from, &id, &data, 0) <= 0) {
	  /* did not know previously! */
	  add_entry(to);
	  return;
	}

	if (lookup_path(to, NULL, NULL, 0) > 0) /* exists already! */
	  return;

	extract_entry(data, &name, &from_parent, NULL, NULL);

	name = string_copy(name); /* just in case it gets clobbered */

	ptr = rindex(to, '/');
	strcpy(to_directory, "");
	strncat(to_directory, to, ptr-to);
	new_name = ptr+1;

	assert(new_name[0] != '\0');
	/* will happen for trailing /, so be careful */

	lookup_path(to_directory, &to_parent, NULL, 1);

	fprintf(stderr, "Moving %d/%s to %d/%s\n",
	  from_parent, name, to_parent, new_name);

	if (from_parent != to_parent) {
	  /* NB it may be quicker to try compare the two strings */
	  delete_child(from_parent, id);
	  add_child(to_parent, id);
	}

	data = get_datum(id); /* get again - in case overwritten */
	data = modify_parent(data, to_parent);
	if (strcmp(name, new_name) != 0) /* name change too */
	  data = modify_name(data, new_name);
	store_datum(id, data, DBM_REPLACE);
	flush_database();

	return;
}

void
move_entry_id(from_parent, name, to_parent, new_name)
sdword from_parent, to_parent;
char *name, *new_name;
{
	datum data;
	sdword id;
	char *ptr;


	if (lookup_path_id(from_parent, name, &id, NULL, 0) <= 0) {
	  /* did not know previously! */
	  add_entry_id(from_parent, name);
	  return;
	}

	if (lookup_path_id(to_parent, new_name, NULL, NULL, 0) > 0)
	  /* exists already! */
	  return;

	if (is_directory(equiv_path(id))||is_file(equiv_path(id)))
	  return;

	if (from_parent == to_parent) {
	  data = get_datum(id); /* get here in case corrupted earlier */
	  data = modify_name(data, new_name);
	  store_datum(id, data, DBM_REPLACE);
	} else {
	  delete_child(from_parent, id);
	  add_child(to_parent, id);
	  data = get_datum(id); /* get here in case corrupted earlier */
	  data = modify_parent(data, to_parent);
	  if (strcmp(name, new_name) != 0) /* name change too */
	    data = modify_name(data, new_name);
	  store_datum(id, data, DBM_REPLACE);
	}
	if (! is_directory(equiv_path(id)) && ! is_file(equiv_path(id)))
	  delete_entry_id(id); /* does not actually exist */
	else
	  flush_database();

	return;
}

void
rename_entry_id(id, new_name)
sdword id;
char *new_name;
{
	datum data;
	int ret;

	if (verbose)
		fprintf(stderr, "rename_entry_id: %d -> %s\n",
				id, new_name);
	data = get_datum(id);
	if (data.dptr == NULL) {
	  if (verbose)
	    fprintf(stderr, "Unknown id: %d\n", id);
	  /* did not know previously! */
	  return;
	}
	data = modify_name(data, new_name);
	if (ret = store_datum(id, data, DBM_REPLACE) < 0)
		if (verbose)
			fprintf(stderr, "rename_entry_id: store returned %d\n",
					ret);

	if (! is_directory(equiv_path(id)) && ! is_file(equiv_path(id))) {
	  if (verbose)
		fprintf(stderr, "rename_entry_id: Ooops! %s doesn't exist!\n",
			equiv_path(id));
	  delete_entry_id(id); /* does not actually exist */
	} else
	  flush_database();

	return;
}

void
rename_entry(path, new_name)
char *path, *new_name;
{
	datum data;
	sdword id;
	char *ptr;

	if (lookup_path(path, &id, &data, 0) <= 0) {
	  /* did not know previously! */
	  return;
	}
	data = modify_name(data, new_name);
	store_datum(id, data, DBM_REPLACE);

	if (! is_directory(equiv_path(id)) && ! is_file(equiv_path(id)))
	  delete_entry_id(id); /* does not actually exist */
	else
	  flush_database();

	return;
}

void
clean_entries()
{
	/* currently no-op */
	continue_clean = 0;

	return;
}

void
session()
{
	struct sockaddr addr;
	int addrlen;
	int sock;
	char buff[2*MAXPATHLEN+3];
	char command, arg1[MAXPATHLEN], arg2[MAXPATHLEN];
	sdword id1, id2;
	int count;
	int prob, args, fileID;

	for (;;) {
	  fileID = 0;
	  addrlen = sizeof(struct sockaddr);
	  if ((sock = accept(queries, &addr, &addrlen)) < 0)
	    fatal("Accept");
	  if ((count = recv(sock, buff, 2*MAXPATHLEN+3, 0)) < 0)
	    fatal("Recv");
	  buff[count] = '\0';
	  if (verbose && count > 0)
	    fprintf(stderr, "Received: '%s'\n", buff);

	  prob = 0;

	  if (buff[0] != '\0') {
	    command = buff[0];
	    switch (command) { /* first decode arguments */
	      case 'A': case 'D':
	        args = sscanf(buff+1, "%[^\277]\277", arg1);
	        break;
	      case 'M':
	        args = sscanf(buff+1, "%[^\277]\277%[^\277]\277", arg1, arg2);
	        break;
	      case 'R':
	        args = sscanf(buff+1, "%[^\277]\277%[^\277]\277", arg1, arg2);
	        break;
	      case 'a':
	        args = sscanf(buff+1, "%d\277%[^\277]\277", &id1, arg1);
	        break;
	      case 'f':
	        args = sscanf(buff+1, "%d\277%[^\277]\277", &id1, arg1);
	        break;
	      case 'd':
	        args = sscanf(buff+1, "%d\277", &id1);
	        break;
	      case 'm':
	        args = sscanf(buff+1, "%d\277%[^\277]\277%d\277%[^\277]\277", 
	          &id1, arg1, &id2, arg2);
	        break;
	      case 'r':
	        args = sscanf(buff+1, "%d\277%[^\277]\277", &id1, arg1);
	        break;
	    }

	    switch (command) {
	      case 'A':
		if (args < 1 || arg1[0] != '/')
		  prob = 1;
	        else
		  add_entry(arg1);
	        break;
	      case 'a':
		if (args < 2)
		  prob = 1;
	        else
		  add_entry_id(id1, arg1);
	        break;
	      case 'f':
		if (args < 2)
		  prob = 1;
	        else
		  add_entry_fid(id1, arg1);
	        break;
	      case 'D':
		if (args < 1 || arg1[0] != '/')
		  prob = 1;
	        else
		  delete_entry(arg1);
	        break;
	      case 'd':
		if (args < 1)
		  prob = 1;
	        else
		  delete_entry_id(id1);
	        break;
	      case 'M':
		if (args < 2 || arg1[0] != '/' || arg2[0] != '/')
	          prob = 1;
	        else
		  move_entry(arg1, arg2);
	        break;
	      case 'm':
	        if (args < 4)
	          prob = 1;
	        else
		  move_entry_id(id1, arg1, id2, arg2);
	        break;
	      case 'R':
		if (args < 2 || arg1[0] != '/')
		  prob = 1;
	        else
		  rename_entry(arg1, arg2);
	        break;
	      case 'r':
		if (args < 2)
		  prob = 1;
	        else
		  rename_entry_id(id1, arg1);
	        break;
	      case 'C':
		clean_entries();
	        break;
	      default:
		prob = 1;
		break;
	    }
	  }
	  if (prob)
	    fprintf(stderr, "Bad command '%s'\n", buff);
	  close(sock);
	}

	return;
}

/*
 * disassociate
 *
 */

void
do_disconnect()
{
	if (fork())
	  _exit(0); 
	{
	  int f;

	  for (f = 0; f < 10; f++)
	    (void) close(f);
	}
	(void) open("/", 0);
#ifndef NODUP2
	(void) dup2(0, 1);
	(void) dup2(0, 2);
#else  NODUP2
	(void)dup(0);		/* for slot 1 */
	(void)dup(0);		/* for slot 2 */
#endif NODUP2
#ifndef POSIX
#ifdef TIOCNOTTY
	{
	  int t;

	  if ((t = open("/dev/tty", 2)) >= 0) {
	    ioctl(t, TIOCNOTTY, (char *)0);
	    (void) close(t);
	  }
	}
#endif TIOCNOTTY
#ifdef xenix5
	/*
	 * USG process groups:
	 * The fork guarantees that the child is not a process group leader.
	 * Then setpgrp() can work, whick loses the controllong tty.
	 * Note that we must be careful not to be the first to open any tty,
	 * or it will become our controlling tty.  C'est la vie.
	 *
	 */
	setpgrp();
#endif xenix5
#else  POSIX
	(void) setsid();
#endif POSIX
}

/*
 * These are here to ensure we pick up these
 * versions for the server, and not those in lib_client.c
 *
 */

int
amAufsExt()
{
	return(1);
}

/*
 * flush_database: force any outstanding writes
 *
 */

void
flush_database()
{
#ifndef USE_GDBM
	extern void open_database( /* void */ );

	dbm_close(db);
	open_database();
#endif /* USE_GDBM */

	return;
}
#else  FIXED_DIRIDS
#include <stdio.h>
main()
{
	printf("afpidsrvr: not compiled with -DFIXED_DIRIDS\n");
}
#endif FIXED_DIRIDS
