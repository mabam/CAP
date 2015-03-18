/*
 * $Author: djh $ $Date: 1996/06/18 10:46:58 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afpidaufs.h,v 2.2 1996/06/18 10:46:58 djh Rel djh $
 * $Revision: 2.2 $
 *
 */

/*
 * AUFS fixed directory IDs
 *
 * joint declarations for libafp.a
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifndef _afpidaufs_h
#define _afpidaufs_h

#include <sys/types.h>
#include <sys/socket.h>
#ifdef USE_GDBM
#include <gdbm.h>
#define DBM_INSERT	GDBM_INSERT
#define DBM_REPLACE	GDBM_REPLACE
#else  /* USE_GDBM */
#include <ndbm.h>
#endif /* USE_GDBM */

#include "afpidnames.h"

#ifndef	LAP_ETALK /* not included before */
#include <netat/appletalk.h>
#endif	LAP_ETALK

/* query_socket: create socket for use for queries */
int query_socket( /* void */ );

/* connect_query: connect created socket to query entry */
int connect_query( /* int sock */ );

/* query_addr: setup pointer to query socket, and give socket length */
void query_addr (/* sockaddr **addr, int *len */);

/* is_directory: return true if a directory of this name exists */
int is_directory ( /* char *name */ );

/* number_datum: returns properly setup datum for an external num */
datum num_datum ( /* sdword num */ );

/* num_from_datum: returns numerical value in number datum */
sdword num_from_datum ( /* datum dat */ );

/* new_entry: create virgin directory entry - assume exists */
datum new_entry ( /* char *name, sdword parent */ );

/* modify: modify the given entry as appropriate - given new */
datum modify_name ( /* datum entry, char *new_name */ );
datum modify_parent ( /* datum entry, sdword parent */ );
datum modify_children ( /* datum entry, int num_children, 
			   sdword *children */ );

/* create_entry: create entry datum */
datum create_entry ( /* char *name, sdword parent,
			int num_children, sdword *children */ );

/* extract_entry: get data from entry. Returns neg on error.
   NB. if NULL is given as actual param, will not return that
   particular field. */
int extract_entry ( /* datum entry, char **name, sdword *parent,
		       int *num_children, sdword **children */ );

/* copy_children: take copy of children (eg as returned by 
   extract_children). Needed because areas from db are static. */
sdword *copy_children (/* sdword *children, int num_children */);

/* lookup_path: if "create" then create elements as needs (from server only).
   Return positive on found, neg on error, 0 if not found. Set id and data if found. */
int lookup_path (/* char *path, sdword *id, data data, int create */);

/* lookup_path_id: similar, but from already known parent */
int lookup_path_id 
	(/* sdword parent, char *remaining, sdword *id, data data, int create */);

/* get_datum: get datum for an id. result.dptr == NULL if not found */
datum get_datum ( /* sdword id */ );

/* store_datum: write value for id to d/b */
int store_datum ( /* sdword id, datum data, int flags */ );

/* do_delete_entry: delete entry data, but not the record in its parent. */
void do_delete_entry ( /* sdword id */);

/* add_child: add a child to its parents record */
int add_child (/* sdword id, sdword child */);

/* delete_child: delete a child from its parents record */
int delete_child (/* sdword id, sdword child */);

/* equiv_path. Given id, return equiv filename, NULL if not found */
char *equiv_path (/* sdword id */);

/* create_init: create initial datum contents */
datum create_init ( /* char *version, sdword rootEid */ );

/* extract_init: extract initial datum contents */
int extract_init ( /* datum initial, char **version, sdword *rootEid */ );

/* valid_id: returns random external id in valid range */
sdword valid_id( /* void */ );

/* init_aufsExt: initialise library */
void init_aufsExt ( /* void */ );

/* open_dbase: open database for reading, and check version if first */
int open_dbase ( /* int first */ );

/* close_dbase: close database */
void close_dbase ( /* void */ );

/* flush_database: force write - used in server only */
void flush_database( /* void */ );

/* poll_db: if db has been changed since open, close and reopen.
            returns 0 if has changed, pos if no change, neg on error.
	    if force, reopen anyway.
 */
int poll_db ( /* int force */ );

/* amAufsExt: 1 in server, 0 in clients */
int amAufsExt ( /* void */);

/* string_copy: general string copy routine. Should not
   really be here! */
char *string_copy ( /* char * */ );

/* send_X: send commands to the server. Neg result on error. */
int send_new ( /* char *path */);
int send_new_id ( /* sdword parent, char *remaining */);
int send_delete ( /* char *path */);
int send_delete_id ( /* sdword id */ );
int send_move ( /* char *from, char *to */);
int send_move_id ( /* sdword from_parent, char *old_name,
		      sdword to_parent, char*new_name */);
int send_rename ( /* char *old_path, char *new_name */);
int send_rename_id ( /* sdword id, char *new_name */ );
int send_clean ( /* void */ );

#define is_init_datum(dat) (dat.dsize == 2 && dat.dptr[0]=='I' && dat.dptr[1] == '\0')
#define is_num_datum(dat) (dat.dptr[0]=='N')

extern char * aufsSockname;
extern char * aufsDbName;
extern char * aufsDbVersion;
#ifdef USE_GDBM
extern GDBM_FILE db;
#else  /* USE_GDBM */
extern DBM *db;
#endif /* USE_GDBM */
extern sdword rootEid;
extern char * aufsRootName;

/* The following are intended to be called from aufs */

/* initialise - call before other routines */
void aufsExtInit 	( /* void */ );

sdword aufsExtEDir 	( /* char *path */ );
sdword aufsExtEDirId 	( /* sdword parent, char *name */ );
char *aufsExtPath	( /* sdword edir */ );
void aufsExtDel 	( /* char *path */ );
void aufsExtDelId	( /* sdword edir */ );
void aufsExtMove 	( /* char *from, *to */ );
void aufsExtMoveId	( /* char *origPath, sdword newParent, 
			     char *newName */ );
void aufsExtMoveIds	( /* sdword orig, sdword newParent, 
			    char *newName */ );
void aufsExtRename	( /* char *origPath, char *newName */ );
void aufsExtRenameId	( /* sdword edir, char *newName */ );
sdword aufsExtLookForId	( /* sdword parent, char *name */ ); /* -1 if not found */

#endif /* _afpidaufs_h */
