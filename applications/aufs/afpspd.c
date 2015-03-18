/*
   Speedup routines

   Author: Dan Oscarsson    (Dan@dna.lth.se)
*/

#include <sys/param.h>
#ifndef _TYPES
 /* assume included by param.h */
# include <sys/types.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netat/appletalk.h>
#include <netat/compat.h>
#include "afps.h" 

#ifdef STAT_CACHE

struct statobj {
	char * NAME;
	struct stat STAT;
	int FN,RS;
	time_t	VAL_TIME;
};

#define VAL_OK 1
#define VAL_NO 0
#define VAL_INVALID 2

#define MAX_LEVEL 20

private struct statobj STATS[MAX_LEVEL+1];

private time_t CUR_TIME;
private struct timeval *Sched_Now = NULL; /* pointer to scheduler clock */


#define EXP_TIME 120;

private char CUR_DIR_STR[1024];
private char * CUR_DIR = NULL;
private int CUR_DIR_LEN;
private char CUR_STAT_STR[1024];
private int CUR_STAT_LEVEL = 0;

/*
 * The following is used as a sentinel in a statobj to mark invalid
 * entries.  It is stored in the st_nlink field in the STAT substructure.
 *
 */

#define BAD_LINK	0x7fff

OSStatInit()
{
	register int P;

	for (P = 0; P <= MAX_LEVEL; P++) {
		STATS[P].NAME = NULL;
		STATS[P].STAT.st_nlink = BAD_LINK;
		STATS[P].FN = VAL_INVALID;
		STATS[P].RS = VAL_INVALID;
	}

	getschedulerclock(&Sched_Now);
	time(&CUR_TIME);	/* since scheduler has not run yet */
}

OScd(path)
	char * path;
{
	if (CUR_DIR != NULL && strlen(path) == CUR_DIR_LEN-1 && strncmp(path,CUR_DIR,CUR_DIR_LEN-1) == 0)
		return;
	if (chdir(path) < 0)
		return;
	CUR_DIR = CUR_DIR_STR;
	strcpy(CUR_DIR,path);
	strcat(CUR_DIR,"/");
	CUR_DIR_LEN = strlen(CUR_DIR);
	if (DBOSI)
		printf("OScd=%s\n",CUR_DIR);
}

cwd_stat(path,buf)
	char * path;
	struct stat *buf;
{
	if (CUR_DIR != NULL && strncmp(path,CUR_DIR,CUR_DIR_LEN) == 0) {
		if (DBOSI)
			printf("OScwd_stat=%s\n",path+CUR_DIR_LEN);
		return(stat(path+CUR_DIR_LEN,buf));
	}
	if (DBOSI)
		printf("OScwd_stat=%s\n",path);
	return(stat(path,buf));
}

cwd_open(path,flags,mode)
	char * path;
	int flags,mode;
{
	if (CUR_DIR != NULL && strncmp(path,CUR_DIR,CUR_DIR_LEN) == 0) {
		if (DBOSI)
			printf("OScwd_open=%s\n",path+CUR_DIR_LEN);
		return(open(path+CUR_DIR_LEN,flags,mode));
	}
	if (DBOSI)
		printf("OScwd_open=%s\n",path);
	return(open(path,flags,mode));
}

char * cwd_path(path)
	char * path;
{
	if (CUR_DIR != NULL && strncmp(path,CUR_DIR,CUR_DIR_LEN) == 0) {
		if (DBOSI)
			printf("OScwd_path=%s\n",path+CUR_DIR_LEN);
		return path+CUR_DIR_LEN;
	}
	if (DBOSI)
		printf("OScwd_path=%s\n",path);
	return path;
}

private release_stats(K)
	int K;
{
	register int P;

	if (DBOSI)
		printf("release_stats=%d of %d\n",K,CUR_STAT_LEVEL);

	for (P = K; P <= CUR_STAT_LEVEL; P++) {
		STATS[P].STAT.st_nlink = BAD_LINK;
		STATS[P].FN = VAL_INVALID;
		STATS[P].RS = VAL_INVALID;
		STATS[P].NAME = NULL;
	}
	if (K < CUR_STAT_LEVEL)
		CUR_STAT_LEVEL = K;
}

private int EXPIRE_REQ;

private expire_stats()
{
	register int K;

	if (!EXPIRE_REQ)
		return;
	EXPIRE_REQ = 0;
	for (K = 0; K <= CUR_STAT_LEVEL; K++) {
		if (STATS[K].STAT.st_nlink == BAD_LINK)
			continue;
		if (STATS[K].VAL_TIME <= CUR_TIME) {
			if (DBOSI)
				printf("expired=%d(%s) t=%d\n",
					K, STATS[K].NAME,
					STATS[K].VAL_TIME - CUR_TIME);
			STATS[K].STAT.st_nlink = BAD_LINK;
			STATS[K].FN = VAL_INVALID;
			STATS[K].RS = VAL_INVALID;
			STATS[K].NAME = NULL;
		}
	}
}

private struct statobj * locate_statobj(path)
	char * path;
{
	register char *NEW, *OLD;
	register char *TMP;
	register int K;
	char *REPLACE;

	if (DBOSI)
		printf("locate_statobj: path '%s'\n", path);

	/* get the path and make sure it is an absolute one */
	NEW = path;
	if (*NEW++ != '/')
		return(NULL);

	/* get rid of old information */
	expire_stats();

	/* record what we are replacing */
	REPLACE = CUR_STAT_STR;

	/* loop thru each path component and check against current */
	for(K=0;;) {
		/* strip leading slashes */
		while (*NEW == '/')
			NEW++;

		/* handle root */
		if (K == 0) {
			if (*NEW == '\0') {
				if (DBOSI)
					printf("locate_statobj: root\n");
				STATS[0].NAME = "<root>";
				return(&STATS[0]);
			}
			/* something more */
			K++;
		}

		/* do we have info for this level? */
		if (K > CUR_STAT_LEVEL)
			break;	/* nope */

		/* do we have a name for this level */
		if ((OLD = STATS[K].NAME) == NULL)
			break;	/* nope */

		/* record buffer name */
		REPLACE = OLD;

		/* compare name */
		TMP = NEW;
		while (*OLD && *OLD == *TMP) {
			OLD++;
			TMP++;
		}

		/* did we end cleanly? */
		if (*OLD != '\0')
			break;	/* nope */

		/* did NEW path end? */
		if (*TMP == '\0') {
			/* yes */
			if (DBOSI)
				printf("locate_statobj: found %d: '%s'\n",
					K,STATS[K].NAME);
			return(&STATS[K]);
		}

		/* did NEW path end on slash? */
		if (*TMP != '/')
			break;	/* nope */

		/* about to loop, see if too many levels */
		if (++K >= MAX_LEVEL) {
			if (DBOSI)
				printf("locate_statobj: too many '%s'\n",
					path);
			return(NULL);
		}

		/* loop again */
		REPLACE = ++OLD;
		NEW = TMP;
	}

	/* add components */
	release_stats(K);

	OLD = REPLACE;
	for (;;) {
		/* strip leading slashes */
		while (*NEW == '/')
			NEW++;

		/* make sure we have something */
		if (*NEW == '\0') {
			/* must be trailing slash */
			if (DBOSI)
				printf("locate_statobj: trailing / in '%s'\n",
					path);
			return(NULL);
		}

		/* record and copy component */
		STATS[K].NAME = OLD;

		while (*NEW && *NEW != '/')
			*OLD++ = *NEW++;
		*OLD++ = '\0';

		/* record new stat level */
		CUR_STAT_LEVEL = K;

		/* are we done? */
		if (*NEW == '\0') {
			/* yep */
			if (DBOSI)
				printf("locate_statobj: return %d: '%s'\n",
					K,STATS[K].NAME);
			return(&STATS[K]);
		}

		/* about to loop, see if too many levels */
		if (++K >= MAX_LEVEL) {
			if (DBOSI)
				printf("locate_statobj: too many (add) '%s'\n",
					path);
			return(NULL);
		}

		/* loop again */
	}

	/* NEVER REACHED */
}
	
OSstat(path,buf)
	char * path;
	struct stat *buf;
{
	struct statobj * CE;

	if (DBOSI)
		printf("OSstat=%s\n",path);

	CE = locate_statobj(path);
	if (CE != NULL) {
		if (CE->STAT.st_nlink != BAD_LINK) {
			if (DBOSI)
				printf("OSstat=cache path\n");
			bcopy(&CE->STAT,buf,sizeof(struct stat));
			return 0;
		}
		if (cwd_stat(path,buf) == 0) {
			if (DBOSI)
				printf("OSstat=caching path\n");
			bcopy(buf,&CE->STAT,sizeof(struct stat));
			CE->VAL_TIME = CUR_TIME+EXP_TIME;
			return 0;
		}
		if (DBOSI)
			printf("OSstat=no cache\n");
		return -1;
	}
	if (DBOSI)
		printf("OSstat=not cached\n");
	return cwd_stat(path,buf);
}

OSfinderinfo(path)
	char * path;
{
	char pp[MAXPATHLEN];
	struct stat S;
	struct statobj * CE;

	CE = locate_statobj(path);
	if (CE != NULL) {
		if (CE->FN != VAL_INVALID)
			return CE->FN;
		strcpy(pp,path);
		toFinderInfo(pp,"");
		if (cwd_stat(pp,&S) < 0 || !S_ISDIR(S.st_mode))
			CE->FN = VAL_NO;
		else
			CE->FN = VAL_OK;
		return CE->FN;
	}
	strcpy(pp,path);
	toFinderInfo(pp,"");
	if (cwd_stat(pp,&S) < 0 || !S_ISDIR(S.st_mode))
		return 0;
	else
		return 1;
}

OSresourcedir(path)
	char * path;
{
	char pp[MAXPATHLEN];
	struct stat S;
	struct statobj * CE;

	CE = locate_statobj(path);
	if (CE != NULL) {
		if (CE->RS != VAL_INVALID)
			return CE->RS;
		strcpy(pp,path);
		toResFork(pp,"");
		if (cwd_stat(pp,&S) < 0 || !S_ISDIR(S.st_mode))
			CE->RS = VAL_NO;
		else
			CE->RS = VAL_OK;
		return CE->RS;
	}
	strcpy(pp,path);
	toResFork(pp,"");
	if (cwd_stat(pp,&S) < 0 || !S_ISDIR(S.st_mode))
		return 0;
	else
		return 1;
}

OSflush_stat()
{
	CUR_TIME = Sched_Now->tv_sec;
	if (DBOSI)
		printf("OSflush_stat\n");
	release_stats(0);
}

OSstat_cache_update()
{
	CUR_TIME = Sched_Now->tv_sec;
	EXPIRE_REQ = 1;
}

#endif STAT_CACHE
