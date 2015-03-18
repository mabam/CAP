/*
 * $Author: djh $ $Date: 1996/06/19 10:51:19 $
 * $Header: /mac/src/cap60/applications/aufs/RCS/afpidtool.c,v 2.3 1996/06/19 10:51:19 djh Rel djh $
 * $Revision: 2.3 $
 *
 */

/*
 * Tell the AUFS fixed directory ID sever to do some basic things
 *
 * John Forrest <jf@ap.co.umist.ac.uk>
 *
 */

#ifdef FIXED_DIRIDS

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#include "../../lib/afp/afpidaufs.h"

int verbose = 0;

void
fatal(message)
char *message;
{
	if (message != NULL)
	  fprintf(stderr, "%s:", message);
	if (errno > 0)
	  perror("");
	else
	  putc('\n', stderr);
	exit(-1);
}

void
fatal2(message, a1)
char *message, *a1;
{
	fprintf(stderr, "afpidtool: fatal error: %s%s :", message, a1);
 	fatal(NULL);
}

void
getcdwd(path, result)
char *path, *result;
{
	char command [256];
	FILE *pwd;

	sprintf(command, "cd '%s'; pwd\n", path);
	if ((pwd = popen(command, "r")) == NULL)
	  strcpy(result, "");
	else {
	  fgets(result, MAXPATHLEN, pwd);
	  pclose(pwd);
	  if (result[0] != '/') 
	    strcpy(result, "");
	  result[strlen(result)-1] = '\0'; /* remove trailing lf */
	}
}

void
doFullPath(path, fullPath)
char *path, *fullPath;
{
	char *ptr;

	if (path[0] == '\0') 
#ifdef SOLARIS
	  getcwd(fullPath, MAXPATHLEN);
#else  /* SOLARIS */
	  getwd(fullPath);
#endif /* SOLARIS */
	else
	  if (is_directory(path)) 
	    getcdwd(path, fullPath);
	  else
	    if ((ptr = (char *)rindex(path, '/')) != NULL) {
	      *ptr = '\0';
	      doFullPath(path, fullPath);
	      strcat(fullPath, "/");
	      strcat(fullPath, ptr+1);
	    } else { /* directory name only */
	      doFullPath("", fullPath);
	      strcat(fullPath, "/");
	      strcat(fullPath, path);
	    }
}	

char *fullpath(path)
char *path;
{
	char temp[MAXPATHLEN];
	char fullPath[MAXPATHLEN];

	if (is_directory(path))
	  getcdwd(path, fullPath);
	else {
	  strcpy(temp, path);
	  doFullPath(path, fullPath);
	}

	return string_copy(fullPath);
}

main(argc, argv)
int argc;
char *argv[];
{
	int c;
	extern char *optarg;
	extern optind;
	char *arg1, *arg2;
	sdword id1, id2;

	int res;	

	while ((c = getopt(argc, argv, "vn:a:m:d:r:cN:A:D:M:R:")) != -1) {
	  switch (c) {
            case 'v':
	      verbose = 1;
	      break;
	    case 'n': case 'a':
	      arg1 = fullpath(optarg);
	      if (verbose)
	        fprintf(stderr, "New %s\n", arg1);
	      if (send_new(arg1) < 0)
	        fatal("Sending new");
	      break;
	    case 'N': case 'A':
	      id1 = atoi(optarg);
	      arg1 = argv[optind++];
	      if (verbose)
	        fprintf(stderr, "New %d/%s\n", id1, arg1);
	      if (send_new_id(id1,arg1) < 0)
	        fatal("Sending new");
	      break;
	    case 'd':
	      arg1 = fullpath(optarg);
	      if (verbose)
	        fprintf(stderr, "Delete %s\n", arg1);
	      if (send_delete(arg1) < 0)
	        fatal("Sending delete");
	      break;
	    case 'D':
	      id1 = atoi(optarg);
	      if (verbose)
	        fprintf(stderr, "Delete %d\n", id1);
	      if (send_delete_id(id1) < 0)
	        fatal("Sending delete");
	      break;
	    case 'm':
	      arg1 = fullpath(optarg);
	      arg2 = fullpath(argv[optind++]);
	      if (verbose)
	        fprintf(stderr, "Move %s -> %s\n", arg1, arg2);
	      if (send_move(arg1, arg2) < 0)
	        fatal("Sending move");
	      break;
	    case 'M':
	      id1 = atoi(optarg);
	      arg1 = argv[optind++];
	      id2  = atoi(argv[optind++]);
	      arg2 = argv[optind++];
	      if (verbose)
	        fprintf(stderr, "Move %d/%s -> %s\n", 
	          id1, arg1, id2, arg2);
	      if (send_move_id(id1, arg1, id2, arg2) < 0)
	        fatal("Sending move");
	      break;
	    case 'r':
	      arg1 = fullpath(optarg);
	      arg2 = argv[optind++];
	      if (verbose)
	        fprintf(stderr, "Rename %s -> %s\n", arg1, arg2);
	      if (send_rename(arg1, arg2) < 0)
	        fatal("Sending rename");
	      break;
	    case 'R':
	      id1 = atoi(optarg);
	      arg1 = argv[optind++];
	      if (verbose)
	        fprintf(stderr, "Rename %d -> %s\n", id1, arg1);
	      if (send_rename_id(id1, arg1) < 0)
	        fatal("Sending rename");
	      break;
	    case 'c':
	      if (verbose)
	        fprintf(stderr, "Clean\n");
	      if (send_clean() < 0)
	        fatal("Sending clean");
	      break;
            case '?':
              fprintf(stderr,
		"usage: afpidtool [-v] [-n path] [-d path] [-m from to]\n");
	      exit(-1);
          }
	}
	exit(0);
}
#else  FIXED_DIRIDS
#include <stdio.h>
main()
{
	printf("afpidtool: not compiled with -DFIXED_DIRIDS\n");
}
#endif FIXED_DIRIDS
