/*
 * $Author: djh $ $Date: 91/02/15 23:04:00 $
 * $Header: instappl.c,v 2.1 91/02/15 23:04:00 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * instappl - install an unix generated resource file into a aufs volume
 *  no real need to do this for data forks since you can just copy the
 *  file in.  (Would be nice to have something that diddles with finder
 *  information though).
 *
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia
 *  University in the City of New York.
 *
 * Edit History:
 *
 *  March 1987 	CCKim		Created.
 *
 */

char copyright[] = "Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the City of New York";

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <netat/appletalk.h>
#include <netat/macfile.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else
# include <strings.h>
#endif
#ifdef NEEDFCNTLDOTH
# include <fcntl.h>
#endif

usage()
{
  fprintf(stderr, "Usage: instappl <flgs> <application> <directory> [dest]\n");
  fprintf(stderr, "	  -c<Creator> -t<type> [note: truncated to 4 each]\n");
  fprintf(stderr, "	  -l - locked\t-m multi-user\t-i invisibile\n");
  exit(1);
}

main(argc, argv)
char **argv;
int argc;
{
  char *name, *path, *dir;
  char *creator = "????";	/* def creator unknown */
  char *type = "APPL";		/* def file type is application */
  FileInfo fi;
  int c, ff;
  struct timeval tvp[2];
  struct timezone tzp;
  extern char *optarg;
  extern int optind;

  if (argc < 3)
    usage();

  ff = 0;
  while ((c = getopt(argc, argv, "c:t:lmir")) != EOF) 
    switch (c) {
    case 'c':
      creator = optarg;
      break;
    case 't':
      type = optarg;
      break;
    case 'm':
      ff |= FI_ATTR_MUSER;	/* multiuser */
      break;
    case 'r':
    case 'l':
      ff |= FI_ATTR_READONLY;	/* readonly */
      break;
    case 'i':
      ff |= FI_ATTR_INVISIBLE;		/* invisible */
      break;
    case '?':
      usage();
      break;
    }
  if (argc - optind < 2)
    usage();
  
  path = argv[optind++];
  dir = argv[optind++];
  name = NULL;
  if (optind < argc)
    name = argv[optind++];
  if (optind != argc)
    usage();
  if (name == NULL) {
    if ((name = rindex(path,'/')) == NULL)
      name = path;
    else name++;
  }
#ifdef DEBUG  
  printf("copy %s to %s as %s, %s, %s\n",path,dir,name,creator,type);
#endif
  bzero(&fi, sizeof(fi));
  bcopy(type,fi.fi_fndr, 4);
  bcopy(creator,fi.fi_fndr+4, 4);
  fi.fi_attr = ff;
#define COMMENT "Installed via AppleShare by INSTAPPL"
  fi.fi_comln = sizeof(COMMENT);
  fi.fi_magic1 = FI_MAGIC1;
  fi.fi_magic = FI_MAGIC;
  fi.fi_version = FI_VERSION;
  fi.fi_bitmap = FI_BM_MACINTOSHFILENAME;
  strncpy((char *)fi.fi_macfilename, name, sizeof(fi.fi_macfilename)-1);
  bcopy(COMMENT, fi.fi_comnt, sizeof(COMMENT));
  writefinder(name, dir, &fi);
  writedata(name, dir);
  writeres(name, path, dir);
#ifndef NOUTIMES
  gettimeofday(&tvp[0], &tzp);
  tvp[1] = tvp[0];
  utimes(dir, tvp);
#endif
}

char endup[1024];

writefinder(name, dir, fi)
char *name, *dir;
FileInfo *fi;
{
  int fd;

  sprintf(endup, "%s/.finderinfo/%s", dir, name);
  if ((fd = open(endup, O_WRONLY|O_CREAT|O_TRUNC, 0662)) < 0) {
    perror("writefinder: open");
    exit(1);
  }
  write(fd, fi, sizeof(FileInfo));
  close(fd);
}

writedata(name, dir)
char *name, *dir;
{
  int fd;
  sprintf(endup, "%s/%s", dir, name);
  if ((fd = open(endup, O_WRONLY|O_CREAT|O_TRUNC, 0662)) < 0) {
    perror("writedata: open");
    exit(1);
  }
  close(fd);
}

char buf[1024];
writeres(name, path, dir)
char *name, *path, *dir;
{
  int len, fd2, fd;
  sprintf(endup, "%s/.resource/%s", dir, name);
  if ((fd = open(endup, O_WRONLY|O_CREAT|O_TRUNC, 0662)) < 0) {
    perror("writeres: open");
    exit(1);
  }
  if ((fd2 = open(path, O_RDONLY)) < 0) {
    perror("writeres: open");
    exit(1);
  }
  do {
    len = read(fd2, buf, sizeof(buf));
    if (len > 0)
      write(fd, buf, len);
  } while (len > 0);
  if (len < 0)
    perror("read");
  close(fd);
}
