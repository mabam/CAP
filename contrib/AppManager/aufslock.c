static char rcsid[] = "$Author: djh $ $Date: 1993/11/23 09:01:24 $";
static char rcsident[] = "$Header: /mac/src/cap60/contrib/AppManager/RCS/aufslock.c,v 2.3 1993/11/23 09:01:24 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * aufslock <filename> [ <byte> ]
 *
 * Add an advisory shared lock to a single byte of <filename> (mark it busy).
 * This is a program to assist in testing the CAP/AUFS Application Manager.
 * NB: <filename> must specify a path to the resource fork of the Application.
 *
 * Copyright (c) 1991, The University of Melbourne
 * djh@munnari.OZ.AU
 * September 1991
 * August 1993
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef apollo
# include <netat/fcntldomv.h>
#endif apollo

main(argc, argv)
int argc;
char *argv[];
{
	int fd, i;
	void dolock();

	if (argc < 2 || argc > 3) {
	  printf("usage: %s <filename> [ <byte> ]\n", argv[0]);
	  exit(1);
	}

	if ((fd = open(argv[1], O_RDONLY, 0644)) < 0) {
	  perror("read()");
	  exit(1);
	}

	printf("Locking %s", argv[1]);
	dolock(fd, (argc == 3) ? atoi(argv[2]) : -1);

	/* hang around to keep the fd open */

	for (;;)
	  sleep(3600);
}

void
dolock(fd, byten)
int fd;
int byten;
{
	int i, qty;
	struct flock flck;

	if (byten == -1) {
	  for (i = 1; i <= 128 ; i++) {
	    flck.l_type = F_WRLCK;
	    flck.l_whence = SEEK_SET;
	    flck.l_start = i+4;
	    flck.l_len = 1;
	    if (fcntl(fd, F_GETLK, &flck) == -1) {
	      printf("lock test failed at %d\n", i);
	      exit(1);
	    }
	    if (flck.l_type == F_UNLCK) {
	      byten = i;
	      break;
	    }
	  }
	  if (i > 128) {
	    printf("no free locks\n");
	    exit(1);
	  }
	}

	flck.l_type = F_RDLCK;
	flck.l_whence = SEEK_SET;
	flck.l_start = byten+4;
	flck.l_len = 1;

	if (fcntl(fd, F_SETLK, &flck) == -1) {
	  printf("FAIL @%d\n", byten);
	  close(fd);
	  exit(1);
	}
	printf(" (byte %d)\n", byten);
}
