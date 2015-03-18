/*
 * (re)build CAP desktop files .IDeskTop and .ADeskTop
 *
 * Copyright (c) 1993, The University of Melbourne.
 * All Rights Reserved. Permission to publicly redistribute this
 * package (other than as a component of CAP) or to use any part
 * of this software for any purpose, other than that intended by
 * the original distribution, *must* be obtained in writing from
 * the copyright owner.
 *
 * djh@munnari.OZ.AU
 * 15 February 1993
 * 30 November 1993
 *
 * Refer: "Inside Macintosh", Volume 1, page I-128 "Format of a Resource File"
 *
 * $Author: djh $
 * $Revision: 2.2 $
 *
 */

#include "dt.h"

/* variables */

int fdi, fda;
int debug = 0;
int quiet = 0;
int writing = 0;
int totfiles = 0;
int idesk = 0, adesk = 0;
long iconlen, bndllen, freflen;
short bndlnum, frefnum;
long bndloffset, frefoffset;
long listoffset;
u_char icon[1024];

struct adt adt;
struct idt idt;
struct finfo finfo;
struct rhdr rhdr;
struct rmap rmap;
struct tlist tlist;
struct rlist rlist;
struct bhdr bhdr;
struct bdata bdata;
struct bids bids;
struct fdata fdata;

struct icn icn[8] = {
  "", 0, 0,
  "ICN#", 0, 0,
  "icl4", 0, 0,
  "icl8", 0, 0,
  "ics#", 0, 0,
  "ics4", 0, 0,
  "ics8", 0, 0,
  "", 0, 0
};

main(argc, argv)
int argc;
char *argv[];
{
	int c;
	extern char *optarg;
	extern int optind, opterr;
	void enumerate();

	opterr = 0;

	while ((c = getopt(argc, argv, "qdw")) != -1) {
	  switch (c) {
	    case 'd':
	      debug = 1;
	      break;
	    case 'w':
	      writing = 1;
	      break;
	    case 'q':
	      quiet = 1;
	      break;
	    case '?':
	      usage();
	      break;
	  }
	}

	if (optind >= argc)
	  usage();

	if (chdir(argv[optind]) < 0) {
	  perror(argv[optind]);
	  exit(1);
	}

	if (writing) {
	  if ((fdi = open(IFILE, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0) {
	    perror(IFILE);
	    exit(1);
	  }
	  if ((fda = open(AFILE, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0) {
	    perror(AFILE);
	    exit(1);
	  }
	}

	enumerate(".");		/* handle recursively */

	if (writing) {
	  close(fdi);
	  close(fda);
	}

	printf("\n");
	printf("wrote %d .ADeskTop entries\n", adesk);
	printf("wrote %d .IDeskTop entries\n", idesk);
	printf("from a total of %d files\n", totfiles);
	if (adesk == 0 && idesk == 0)
	  printf("(nothing written to .?DeskTop, did you use the -w flag ?)\n");
}

/*
 * print a usage message and exit
 *
 */

usage()
{
	  fprintf(stderr, "usage: builddt [ -d ] [ -w ] [ -q ] volname\n");
	  exit(1);
}

/*
 * check files in directory 'dir'
 *
 */

void
enumerate(dir)
char *dir;
{
	DIR *dirp;
	int dirlen;
	struct stat sbuf;
	char path[MAXPATHLEN];
	char resource[MAXPATHLEN];
	char finderinfo[MAXPATHLEN];
#ifdef SOLARIS
	struct dirent *dp, *readdir();
#else  SOLARIS
	struct direct *dp, *readdir();
#endif SOLARIS
	void makeEntry();

	if ((dirp = opendir(dir)) == NULL) {
	  perror(dir);
	  return;
	}
	
	strcpy(path, dir);
	dirlen = strlen(path);

	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	  if (skip(dp->d_name))
	    continue;
	  strcpy(path+dirlen, "/");
	  strcpy(path+dirlen+1, dp->d_name);
	  if (stat(path, &sbuf) < 0)
	    continue;
	  if (S_ISDIR(sbuf.st_mode)) {
	    enumerate(path);
	    continue;
	  }
	  /* must be a file then */
	  sprintf(resource, "%s/%s/%s", dir, RSRCF, dp->d_name);
	  sprintf(finderinfo, "%s/%s/%s", dir, FNDRI, dp->d_name);
	  if (access(resource, F_OK) < 0)
	    continue;
	  if (access(finderinfo, F_OK) < 0)
	    continue;
	  strcpy(path, dir);
	  if (path[0] == '.' && path[1] == '\0')
	    strcpy(path, "./");
	  else
	    path[dirlen] = '\0';
	  makeEntry(path, dp->d_name, resource, finderinfo);
	  if (debug)
	    printf("\n");
	}

	closedir(dirp);

	return;
}

/*
 * skip subdirectory and file names we aren't interested in
 *
 */

int
skip(name)
char *name;
{
	if (strcmp(name, ".") == 0)
	  return(1);
	if (strcmp(name, "..") == 0)
	  return(1);
	if (strcmp(name, FNDRI) == 0)
	  return(1);
	if (strcmp(name, RSRCF) == 0)
	  return(1);
	if (strcmp(name, AFILE) == 0)
	  return(1);
	if (strcmp(name, IFILE) == 0)
	  return(1);
	if (strcmp(name, "afpvols") == 0)
	  return(1);
	if (strcmp(name, ".afpvols") == 0)
	  return(1);
	if (strcmp(name, "afpfile") == 0)
	  return(1);
	if (strcmp(name, ".afpfile") == 0)
	  return(1);

	return(0);
}

/*
 * grope around in the resource file ... :-(
 *
 */

void
makeEntry(path, file, resource, finderinfo)
char *path, *file, *resource, *finderinfo;
{
	int h;
	int i;
	int j;
	int k;
	int fd;
	char *p;
	long len;
	short ilen;
	long offset;
	short numres;
	char *ic, *findIcon();
	struct fdata *ff, *findFREF();

	totfiles++;

	if (!quiet)
	  printf("Checking file \"%s%s%s\"\n", path+2,
	   (*(path+2) == '\0') ? "" : "/", file);

	if ((fd = open(finderinfo, O_RDONLY, 0644)) < 0) {
	  perror(finderinfo);
	  return;
	}
	if (read(fd, &finfo, sizeof(finfo)) != sizeof(finfo)) {
	  if (debug) printf("\"%s\": too small, ignoring\n", finderinfo+2);
	  close(fd);
	  return;
	}
	close(fd);

	/* handle .ADeskTop file (Application Mapping) */

	if (bcmp(finfo.fi.ftype, "APPL", 4) == 0) {
	  bcopy(finfo.fi.creat, adt.creat, 4);
	  bzero(adt.userb, 4);
	  adt.magic = htonl(MAGIC);
	  adt.dlen = htonl(strlen(path+2)+1); /* ignore leading "./" */
	  adt.flen = htonl(strlen(file)+1); /* but include trailing null */

	  if (debug) {
	    printf("creat");
	    printsig(adt.creat);
	    printf("userb");
	    printsig(adt.userb);
	    printf("path :%s: ", path+2);
	    printf("file :%s:\n", file);
	  }

	  if (writing) {
	    write(fda, &adt, sizeof(adt));
	    write(fda, path+2, ntohl(adt.dlen));
	    write(fda, file, ntohl(adt.flen));
	    adesk++;
	  }
	}

	/* handle .IDeskTop file (ICON/creator/type mapping) */

	if (debug) {
	  printf("ftype");
	  printsig(finfo.fi.ftype);
	  printf("icon ID %d ", (short)ntohs(finfo.fi.iconID));
	  printf("path :%s: ", path+2);
	  printf("file :%s: ", file);
	  printf("\n");
	}

	if ((fd = open(resource, O_RDONLY, 0644)) < 0) {
	  perror(resource);
	  return;
	}

	/* get resource header */
	if (read(fd, &rhdr, sizeof(rhdr)) != sizeof(rhdr)) {
	  if (debug) printf("\"%s\": too small, ignoring\n", resource+2);
	  close(fd);
	  return;
	}

	/* get resource map */
	if (lseek(fd, ntohl(rhdr.rmapOffset), SEEK_SET) < 0) {
	  perror("SEEK_SET");
	  close(fd);
	  return;
	}
	if (read(fd, &rmap, sizeof(rmap)) != sizeof(rmap)) {
	  if (debug) printf("\"%s\": too small, ignoring\n", resource+2);
	  close(fd);
	  return;
	}

	/* file pointer now at start of resource map + sizeof(rmap) */

	if (lseek(fd, ntohs(rmap.listOffset)-sizeof(rmap), SEEK_CUR) < 0) {
	  perror("SEEK_CUR");
	  close(fd);
	  return;
	}
	
	listoffset = ntohl(rhdr.rmapOffset) + ntohs(rmap.listOffset);

	/* get number of resource types */
	if (read(fd, &numres, 2) != 2) {
	  if (debug) printf("failed to read resource type list qty\n");
	  close(fd);
	  return;
	}

	numres = ntohs(numres) + 1;

	if (debug)
	  printf("found %d distinct resource type%s\n",
	    numres, (numres == 1) ? "" : "s");

	/* (in)sanity check */
	if (numres > 1000) {
	  if (debug) printf("that's a lot of resources, probably bogus\n");
	  close(fd);
	  return;
	}

	bndlnum = frefnum = 0;
	for (j = 0; j < 8; j++)
	  icn[j].iconnum = icn[j].iconoffset = 0;

	for (j = 0; j < numres; j++) {
	  /* read resource type list */
	  if (read(fd, &tlist, sizeof(tlist)) != sizeof(tlist)) {
	    if (debug) printf("bad resource type list\n");
	    close(fd);
	    return;
	  }

	  if (debug) {
	    printf("type");
	    printsig(tlist.rtype);
	    printf("num %d off %d\n",
	      ntohs(tlist.rnum)+1, ntohs(tlist.roff));
	  }

	  if (bcmp(tlist.rtype, "BNDL", 4) == 0) {
	    bndlnum = ntohs(tlist.rnum)+1;
	    bndloffset = ntohs(tlist.roff);
	  }
	  /* check for icon family */
	  for (h = 1; h < 7; h++) {
	    if (bcmp(tlist.rtype, icn[h].ityp, 4) == 0) {
	      icn[h].iconnum = ntohs(tlist.rnum)+1;
	      icn[h].iconoffset = ntohs(tlist.roff);
	    }
	  }
	  if (bcmp(tlist.rtype, "FREF", 4) == 0) {
	    frefnum = ntohs(tlist.rnum)+1;
	    frefoffset = ntohs(tlist.roff);
	  }
	}

	/* check for BNDL/ICN#/FREF resources */
	if (bndlnum == 0 || icn[1].iconnum == 0 || frefnum == 0) {
	  if (debug) printf("no BNDL/ICN#/FREF resources\n");
	  close(fd);
	  return;
	}

	/* handle each BNDL present */
	for (j = 0; j < bndlnum; j++) {
	  /* move to NBDL reference list */
	  if (lseek(fd, listoffset+bndloffset, SEEK_SET) < 0) {
	    perror("SEEK_SET");
	    close(fd);
	    return;
	  }
	  if (read(fd, &rlist, sizeof(rlist)) != sizeof(rlist)) {
	    if (debug) printf("bundle reference list too small\n");
	    close(fd);
	    return;
	  }

	  if (debug) {
	    printf("found BNDL rsrc ID %d ", (short)ntohs(rlist.rsrcID));
	    printf("offset %d\n", (long)ntohl(rlist.attrOff) & 0xffffff);
	  }

	  offset = ntohl(rhdr.rdataOffset)+(ntohl(rlist.attrOff) & 0xffffff);

	  /* move to beginning of BNDL resource data */
	  if (lseek(fd, offset, SEEK_SET) < 0) {
	    perror("SEEK_SET");
	    close(fd);
	    return;
	  }
	  /* get BNDL resource length */
	  if (read(fd, &bndllen, 4) != 4) {
	    if (debug) printf("failed to read BNDL resource length\n");
	    close(fd);
	    return;
	  }

	  bndllen = ntohl(bndllen);

	  /* get BNDL resource header */
	  if (read(fd, &bhdr, sizeof(bhdr)) != sizeof(bhdr)) {
	    if (debug) printf("bundle header too small\n");
	    close(fd);
	    return;
	  }

	  bndllen -= sizeof(bhdr);

	  if (debug) {
	    printf("signature");
	    printsig(bhdr.creat);
	    printf("version %d ", (short)ntohs(bhdr.version));
	    printf("numtypes %d\n", (short)ntohs(bhdr.numType)+1);
	  }

	  for (i = 0; i < ntohs(bhdr.numType)+1; i++) {
	    /* get BNDL resource data */
	    if (read(fd, &bdata, sizeof(bdata)) != sizeof(bdata)) {
	      if (debug) printf("bundle data too short\n");
	      close(fd);
	      return;
	    }

	    if (debug) {
	      printf("type");
	      printsig(bdata.btype);
	      printf("has %d ID(s)\n", (short)ntohs(bdata.numID)+1);
	    }

	    for (k = 0; k < ntohs(bdata.numID)+1; k++) {
	      /* scan BNDL ID array */
	      if (read(fd, &bids, sizeof(bids)) != sizeof(bids)) {
		if (debug) printf("bundle data too short\n");
		close(fd);
		return;
	      }

	      if (debug)
		printf("local ID %d, actual ID %d\n",
		  (short)ntohs(bids.localID), (short)ntohs(bids.actualID));

	      if ((ff = findFREF(fd, ntohs(bids.localID))) != NULL) {
	        if (bcmp(bdata.btype, "ICN#", 4) == 0) {
	          /* check icon family */
	          for (h = 1; h < 7; h++) {
	            if ((ic=findIcon(fd,h,ntohs(bids.actualID),&ilen))!=NULL){
	              idt.magic = htonl(MAGIC);
	              idt.isize = htonl(ilen);
		      bcopy(bhdr.creat, idt.creat, 4);
		      bcopy(ff->ftype, idt.ftype, 4);
		      idt.itype = h;
	              idt.pad1 = 0;
		      bzero(idt.userb, 4);
	              idt.pad2[0] = 0;
	              idt.pad2[1] = 0;

		      if (debug) {
		        printf("creator");
		        printsig(idt.creat);
		        printf("type");
		        printsig(idt.ftype);
		        printf("found '%s' of length %d\n", icn[h].ityp, ilen);
		        printicn(ic, h, ilen);
		      }

		      if (writing) {
		        write(fdi, &idt, sizeof(idt));
		        write(fdi, ic, ilen);
		        idesk++;
		      }
		    }
	          }
	        }
	      }
	    }
	  }
	}

	close(fd);

	return;
}

/*
 * find the icon with the specified resource number
 * and type (ICN#, icl4, icl8, ics#, ics4, ics8),
 * return a pointer to the icon data and it's length
 *
 */

char *
findIcon(fd, typ, rsrcID, len)
int fd;
int typ;
short rsrcID;
short *len;
{
	int i;
	long offset;
	long curpos;
	struct rlist rlist;

	*len = 0;

	/* keep current file pointer position */
	if ((curpos = lseek(fd, 0, SEEK_CUR)) >= 0) {
	  /* move to start of icon resource reference list */
	  if (lseek(fd, listoffset+icn[typ].iconoffset, SEEK_SET) >= 0) {
	    for (i = 0; i < icn[typ].iconnum; i++) {
	      /* get resource reference list for each icon */
	      if (read(fd, &rlist, sizeof(rlist)) == sizeof(rlist)) {
	        if (debug)
	          printf("found '%s' rsrc ID %d offset %d\n",
		    icn[typ].ityp, (short)ntohs(rlist.rsrcID),
	            (long)ntohl(rlist.attrOff) & 0xffffff);
	        if ((short)ntohs(rlist.rsrcID) == rsrcID)
	          break;
	      }
	    }
	    if (i < icn[typ].iconnum) {
	      offset = ntohl(rhdr.rdataOffset)+(ntohl(rlist.attrOff)&0xffffff);
	      /* move to beginning of icon resource data */
	      if (lseek(fd, offset, SEEK_SET) >= 0) {
	        /* get icon resource length */
	        if (read(fd, &iconlen, 4) == 4) {
	          if ((iconlen = ntohl(iconlen)) <= sizeof(icon)) {
		    /* read icon data */
	            if (read(fd, icon, iconlen) == iconlen) {
		      /* restore original pointer */
	              lseek(fd, curpos, SEEK_SET);
	              *len = iconlen;
	              return((char *)icon);
		    }
		  }
		}
	      }
	    }
	  }
	}
	lseek(fd, curpos, SEEK_SET);
	return(NULL);
}

/*
 * find the FREF with the specified local ID
 *
 */

struct fdata *
findFREF(fd, localID)
int fd;
short localID;
{
	int i;
	long offset;
	long curpos;
	struct rlist rlist;

	/* keep current file pointer position */
	if ((curpos = lseek(fd, 0, SEEK_CUR)) >= 0) {
	  for (i = 0; i < frefnum; i++) {
	    /* move to start of FREF resource reference list */
	    if (lseek(fd, listoffset+frefoffset+i*sizeof(rlist),SEEK_SET)>=0){
	      /* get resource reference list for each FREF */
	      if (read(fd, &rlist, sizeof(rlist)) == sizeof(rlist)) {
	        if (debug) {
	          printf("found FREF rsrc ID %d ", (short)ntohs(rlist.rsrcID));
	          printf("offset %d\n", (long)ntohl(rlist.attrOff) & 0xffffff);
	        }
	        offset=ntohl(rhdr.rdataOffset)+(ntohl(rlist.attrOff)&0xffffff);
	        /* move to beginning of FREF resource data */
	        if (lseek(fd, offset, SEEK_SET) >= 0) {
	          /* get FREF resource length */
	          if (read(fd, &freflen, 4) == 4) {
	            if ((freflen = ntohl(freflen)) >= sizeof(fdata)) {
		      /* read FREF data */
	              if (read(fd, &fdata, sizeof(fdata)) == sizeof(fdata)) {
			if (ntohs(fdata.localID) != localID)
			  continue;
		        /* restore original pointer */
	                lseek(fd, curpos, SEEK_SET);
	                return(&fdata);
		      }
		    }
		  }
	        }
	      }
	    }
	  }
	}
	lseek(fd, curpos, SEEK_SET);
	return(NULL);
}
