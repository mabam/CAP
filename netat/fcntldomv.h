
/*
 * fcntldomv.h - Definitions for hidden fcntl calls in HP/Apollo Domain BSD 4.3.
 *
 * The F_SETLK and F_GETLK fcntl calls are not documented for HP/Apollo Domain
 * BSD 4.3 environment, but apparently exist and work.  Below are the 
 * necessary definitions as defined in the SysV environment.  
 *
 * These lock calls are only needed if the APPLICATION_MANAGER and/or 
 * DENYREADWRITE options are enabled in m4.features.  The APPLICATION_MANAGER's
 * features (inihibiting finder copying and limiting the number of simultaneous
 * executions of applications) appear to work with these hidden calls.
 * Interestingly enough, these features appear to work even when the managed
 * applications reside on another node's disk, even though the Domain Sys V
 * documentation for fcntl says that record locking works only on the
 * station local to the call.
 *
 * Darrell Skinner <skinner@lpmi.polytechnique.fr> Oct 1993
 *
 */

#define		F_GETLK		7	/* Get file lock */
#define		F_SETLK		8	/* Set file lock */
#define		F_RDLCK		01	/* Shared or Read lock. */
#define		F_WRLCK		02	/* Exclusive or Write lock. */
#define		F_UNLCK		03	/* Unlock. */

struct flock {
	short	l_type;		/* Type of lock. */
	short	l_whence;	/* Flag for starting offset. */
	off_t	l_start;	/* Relative offset in bytes. */
	off_t	l_len;		/* Size; if 0 then until EOF. */
	short	l_sysid;
	short   l_pid;		/* Process ID of the lock owner. */
};
