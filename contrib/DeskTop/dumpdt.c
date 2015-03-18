/*
 * dump CAP desktop files .IDeskTop and .ADeskTop
 *
 * Copyright (c) 1993-1997, The University of Melbourne.
 * All Rights Reserved. Permission to publicly redistribute this
 * package (other than as a component of CAP) or to use any part
 * of this software for any purpose, other than that intended by
 * the original distribution, *must* be obtained in writing from
 * the copyright owner.
 *
 * djh@munnari.OZ.AU
 * 15 February 1993
 * 10 August   1997
 *
 * Refer: "Inside Macintosh", Volume 1, page I-128 "Format of a Resource File"
 *
 * $Author: djh $
 * $Revision: 2.1 $
 *
 */

#include "dt.h"

struct adt adt;
struct idt idt;

u_char last_creat[4] = { 0, 0, 0, 0 };
u_char last_ftype[4] = { 0, 0, 0, 0 };

main(argc, argv)
int argc;
char *argv[];
{
	int len;
	int fd, i;
	u_char zero[4];
	u_char icon[1024];
	char path[MAXPATHLEN], file[MAXPATHLEN];

	if (argc != 2) {
	  fprintf(stderr, "usage: dumpdt volname\n");
	  exit(1);
	}

	if (chdir(argv[1]) < 0) {
	  perror(argv[1]);
	  exit(1);
	}

	bzero(zero, sizeof(zero));

	if ((fd = open(AFILE, O_RDONLY, 0644)) < 0) {
	  perror(AFILE);
	  exit(1);
	}

	printf("APPL Mappings from %s/%s\n\n", argv[1], AFILE);

	for ( ; ;  ) {
	  if (read(fd, &adt, sizeof(adt)) < sizeof(adt))
	    break;

	  if (ntohl(adt.magic) != MAGIC) {
	    fprintf(stderr, "bad magic in %s/%s\n", argv[1], AFILE);
	    break;
	  }

	  printf("creat");
	  printsig(adt.creat);
	  if (bcmp(adt.userb, zero, sizeof(zero))) {
	    printf("userb");
	    printsig(adt.userb);
	  }

	  path[0] = '\0';
	  file[0] = '\0';

	  if (read(fd, path, ntohl(adt.dlen)) != ntohl(adt.dlen)) {
	    fprintf(stderr, "bad path length %d\n", ntohl(adt.dlen));
	    break;
	  }
	  if (read(fd, file, ntohl(adt.flen)) != ntohl(adt.flen)) {
	    fprintf(stderr, "bad file length %d\n", ntohl(adt.dlen));
	    break;
	  }
	  if (path[0] != '\0')
	    strcat(path, "/");
	  strcat(path, file);
	  printf("file :%s:", path);
	  if (access(path, F_OK))
	    printf(" (not found)");
	  printf("\n");
	}

	close(fd);

	if ((fd = open(IFILE, O_RDONLY, 0644)) < 0) {
	  perror(IFILE);
	  exit(1);
	}

	printf("\nIcon Family Mappings from %s/%s\n\n", argv[1], IFILE);

	for ( ; ; ) {
	  if (read(fd, &idt, sizeof(idt)) < sizeof(idt))
	    break;

	  if (ntohl(idt.magic) != MAGIC) {
	    fprintf(stderr, "bad magic in %s/%s\n", argv[1], IFILE);
	    break;
	  }

	  if (last_creat[0] != '\0'
	   && (bcmp(last_creat, idt.creat, 4) != 0
	    || bcmp(last_ftype, idt.ftype, 4) != 0))
	    printf("\n\n");

	  bcopy(idt.creat, last_creat, 4);
	  bcopy(idt.ftype, last_ftype, 4);

	  printf("creat");
	  printsig(idt.creat);
	  printf("ftype");
	  printsig(idt.ftype);
	  printf("itype %d ", idt.itype);
	  if (bcmp(idt.userb, zero, sizeof(zero))) {
	    printf("userb");
	    printsig(idt.userb);
	  }
	  printf("\n");
	  if ((len = read(fd, icon, ntohl(idt.isize))) != ntohl(idt.isize)) {
	    fprintf(stderr, "bad icon length %d\n", ntohl(idt.isize));
	    break;
	  }
	  printicn(icon, idt.itype, len);
	}

	close(fd);
}
