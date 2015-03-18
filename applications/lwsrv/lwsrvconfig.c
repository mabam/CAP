static char rcsid[] = "$Author: djh $ $Date: 1996/09/10 14:33:00 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/lwsrvconfig.c,v 2.3 1996/09/10 14:33:00 djh Rel djh $";
static char revision[] = "$Revision: 2.3 $";

/*
 * lwsrvconfig - auxiliary program for testing configuration files
 *   and creating printer description databases
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/file.h>
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#include <netat/compat.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else  /* USESTRINGDOTH */
# include <strings.h>
#endif /* USESTRINGDOTH */
#ifdef NEEDFCNTLDOTH
# include <sys/fcntl.h>
#endif NEEDFCNTLDOTH
#ifndef NONDBM
# include <ndbm.h>
#else NONDBM
# include <dbm.h>
#endif NONDBM
#include "list.h"
#include "packed.h"
#include "parse.h"

char *myname;

#define	F_GLOBAL	(1 << F_GLOBAL_BIT)
#define	F_GLOBAL_BIT	0
#define	F_HASARG	(1 << F_HASARG_BIT)
#define	F_HASARG_BIT	1
#define	hasarg(f)	((f) & F_HASARG)
#define	isglobal(f)	((f) & F_GLOBAL)
#define	sf(a,g)		(((a) << F_HASARG_BIT) | ((g) << F_GLOBAL_BIT))

struct descrip {
  char *name;
  short flags;
};

static KVTree **argname;
static struct argnamebuf {
  char *arg;
  struct descrip d;
} argnamebuf[] = {
  {"-A", {"DSC", sf(1, 0)}},
  {"-C", {"LPRCommand", sf(1, 1)}},
#ifdef LPRARGS
  {"-L", {"LPRArgument", sf(1, 0)}},
#endif LPRARGS
  {"-N", {"DontCollect", sf(0, 0)}},
#ifdef PASS_THRU
  {"-P", {"PassThru", sf(0, 0)}},
#endif PASS_THRU
#ifdef NeXT
  {"-R", {"NeXTResolution", sf(1, 0)}},
#endif NeXT
  {"-S", {"SingleFork", sf(0, 1)}},
  {"-T", {"TranScriptOption", sf(1, 0)}},
#ifdef LWSRV_AUFS_SECURITY
  {"-X", {"AUFSSecurity", sf(1, 1)}},
#endif LWSRV_AUFS_SECURITY
#ifdef LW_TYPE
  {"-Y", {"AppleTalkType", sf(1, 0)}},
#endif LW_TYPE
  {"-a", {"ProcsetDir", sf(1, 0)}},
  {"-d", {"Debug", sf(1, 1)}},
  {"-e", {"Alloweexec", sf(0, 0)}},
  {"-f", {"FontFile", sf(1, 0)}},
  {"-h", {"SuppressBanner", sf(0, 0)}},
  {"-k", {"NoChecksum", sf(0, 0)}},
  {"-l", {"LogFile", sf(1, 1)}},
  {"-p", {"PrinterQueue", sf(1, 0)}},
  {"-q", {"QueryFile", sf(1, 1)}},
  {"-r", {"KeepSpoolFile", sf(0, 0)}},
  {"-t", {"TraceFile", sf(1, 0)}},
  {"-v", {"Verbose", sf(0, 1)}},
  {NULL, NULL},
};
static char *createdb = NULL;
static int datafd;
long dataoffset = 0;
#ifndef NONDBM
static DBM *db;
#endif NONDBM
static char *dbname = NULL;
static KVTree **_default;
static int firstargs = 1;
static int firstprinter = 1;
static char firsttime[127];
static char *headerstr = NULL;
static char null[] = "";
static int oldformat = 0;
static char *printername;
static int verbose = 0;

int doargs();

static void addback(/* int argc, char **argv */);
static void display(/* char *str, KVTree **kp */);
static void displaytemplates();
static void docreatedb();
static KVTree **initargname();
static int getflags(/* char *str */);
static char *keyname(/* char *str */);
static char *quotestr(/* char *str */);
static void setfirsttime(/* char *str */);
static void storedata(/* char *name, int n, char *ptr, int size */);
static unshort *ushortdup(/* int i */);
static char *untab(/* char *str */);
static void usage();

main(argc, argv)
int argc;
char **argv;
{
  register int c;
  register KVTree **kp;
  register List *lp;
  register FILE *fp;
  extern FILE *yyin;
  extern char *optarg;
  extern int optind;

  if (myname = rindex(*argv, '/'))
    myname++;
  else
    myname = *argv;
  while ((c = getopt(argc, argv, "c:l:ov?")) != EOF) {
    switch(c) {
     case 'c':
      createdb = optarg;
      break;
     case 'l':
      dbname = optarg;
      break;
     case 'o':
      oldformat++;
      break;
     case 'v':
      verbose++;
      break;
     case '?':
     default:
      usage();
    }
  }
  if (createdb && dbname)
    usage();
  if (optind < argc) {
    if ((argc - optind) > 1)
      usage();
    if ((fp = fopen(argv[optind], "r")) == NULL) {
      fprintf(stderr, "%s: Can't open %s\n", myname, argv[optind]);
      exit(1);
    }
  } else
    fp = stdin;
  yyin = fp;				/* set for flex(1) */
  argname = initargname();
  initkeyword(fp);
  umask(0133);
  if (yyparse())
    exit(1);
  if (!dbname && libraryfile)
    dbname = libraryfile;
  if (createdb)
    docreatedb();
  else {
    _default = thequery = CreateKVTree();
    setfirsttime(specialOpts);
    configargs(dbname);
  }
  exit(0);
}

static void
addback(argc, argv)
register int argc;
register char **argv;
{
  register void *n;
  register List *lp;
  register int f;

  while (argc-- > 0) {
    f = getflags(*argv);
    if (hasarg(f)) {
      n = (void *)*argv++;
      if (isSpecialOpt(n)) {
	if (firsttime[Option(n)]) {
	  firsttime[Option(n)] = 0;
	  if (lp = (List *)SearchKVTree(thequery, n, strcmp)) {
	    if (thequery == _default) {
	      AddToList(lp, (void *)*argv);
	    } else {
	      lp = DupList(lp);
	      AddToList(lp, (void *)*argv);
	      AddToKVTree(thequery, n, (void *)lp, strcmp);
	    }
	  } else {
	    lp = CreateList();
	    AddToList(lp, (void *)*argv);
	    AddToKVTree(thequery, n, (void *)lp, strcmp);
	  }
        } else {
          lp = (List *)SearchKVTree(thequery, n, strcmp);
          AddToList(lp, (void *)*argv);
        }
      } else
	AddToKVTree(thequery, n, (void *)*argv, strcmp);
      argv++;
      argc--;
    } else
      AddToKVTree(thequery, (void *)*argv++, NULL, strcmp);
  }
}

/*
 * doargs would normally set the arguments in lwsrv.  We intercept this
 * to do our printing.
 */
doargs(argc, argv)
int argc;
char **argv;
{
  register int n, i, f;
  register char **a;

  argc--;
  argv++;
  if (firstargs && strcmp(*argv, "-n") != 0) {	/* options */
    firstargs = 0;
    if (verbose) {
      addback(argc, argv);
      display("Options", thequery);
    } else {
      n = argc;
      a = argv;
      i = 0;
      while (n-- > 0) {
	f = getflags(*a);
	if (isglobal(f))
	  i++;
	if (hasarg(f)) {
	  a++;
	  n--;
	}
	a++;
      }
      if (i > 0) {
	printf("Options = %s", i > 1 ? "(\n" : null);
	n = argc;
	a = argv;
	while (n-- > 0) {
	  f = getflags(*a);
	  if (isglobal(f)) {
	    printf("%s%s", i > 1 ? "\t" : null, keyname(*a));
	    if (hasarg(f)) {
	      printf(" %s;\n", quotestr(*++a));
	      n--;
	    } else
	      fputs(";\n", stdout);
	    if (i == 1)
	      break;
	  } else {
	    if (hasarg(f)) {
	      a++;
	      n--;
	    }
	  }
	  a++;
	}
	if (i > 1)
	  fputs(");\n\n", stdout);
	else
	  putchar('\n');
      }
      addback(argc, argv);
    }
  } else if (strcmp(*argv, "-n") == 0) {
    /* setup thequery so setargs will insert the queries in the right one */
    firstargs = 0;
    if (thequery != _default)
      FreeKVTree(thequery, NULL, NULL);
    thequery = DupKVTree(_default);
    if (verbose && firstprinter) {	/* print the templates */
      firstprinter = 0;
      displaytemplates();
    }
    setfirsttime(specialOpts);
    printername = *++argv;
  } else {	/* actually do the printer */
    addback(argc, argv);
    display(printername, thequery);
  }
}

static void
docreatedb()
{
  register KVTree **keys;
  register PackedChar *ckeys;
  register PackedChar *pc;
  register PackedShort *ps;
  register void **v2, **v3, **vp;
  register int i, j, k;
  register unshort *sp, *sp2;
  register char *name;
  char buf[BUFSIZ];

  if (verbose)
    printf("Creating database %s\n", createdb);
  strcpy(buf, createdb);
  strcat(buf, ".dir");
  unlink(buf);
  strcpy(buf, createdb);
  strcat(buf, ".pag");
  unlink(buf);
#ifndef NONDBM
  if ((db = dbm_open(createdb, O_RDWR | O_CREAT, 0755)) == NULL) {
    fprintf(stderr, "%s: Can't create database %s\n", myname, createdb);
    exit(1);
  }
#else NONDBM
  if ((i = creat(buf, 0755)) < 0) {
    fprintf(stderr, "%s: Can't create %s\n", myname, buf);
    exit(1);
  }
  if ((i = creat(buf, 0755)) < 0) {
    fprintf(stderr, "%s: Can't create %s\n", myname, buf);
    exit(1);
  }
  if (dbminit(creatdb) < 0) {
    fprintf(stderr, "%s: Can't open database %s\n", myname, createdb);
    exit(1);
  }
#endif NONDBM
  strcpy(buf, createdb);
  strcat(buf, datasuffix);
  unlink(buf);
  if ((datafd = open(buf, O_WRONLY | O_CREAT, 0755)) < 0) {
    fprintf(stderr, "%s: Can't create %s\n", myname, buf);
    exit(1);
  }
  keys = CreateKVTree();
  ckeys = CreatePackedChar();
  for (i = NList(printerlist), vp = AddrList(printerlist); i > 0; i -= 2) {
    name = (char *)*vp++;
    if (verbose)
      printf("\tCreating template %s\n", name);
    pc = CreatePackedChar();
    /*
     * Since a zero means the a string value is null, we "use up"
     * the zero so that it can never occur in our data (except to mean
     * that the string is NULL
     */
    AddToPackedChar(pc, null);
    ps = CreatePackedShort();
    AddToPackedShort(ps, (j = NList(*vp) / 2));
    sp = AllocatePackedShort(ps, 2 * j);
    for (v2 = AddrList(*vp); j > 0; j--) {
      if (verbose)
        printf("\t\tAdding keyword %s\n", untab((char *)*v2));
      if ((sp2 = (unshort *)SearchKVTree(keys, *v2, strcmp)) == NULL) {
        k = AddToPackedChar(ckeys, (char *)*v2);
        sp2 = ushortdup(k);
        AddToKVTree(keys, *v2, (void *)sp2, strcmp);
      }
      *sp++ = *sp2;
      if (StringVal(*v2)) {
        v2++;
	*sp++ = *v2 ? AddToPackedChar(pc, (char *)*v2) : 0;
      } else {
        v2++;
	k = NList(*v2);
	*sp++ = AddToPackedShort(ps, k);
	for (v3 = AddrList(*v2); k > 0; k--)
	  AddToPackedShort(ps, AddToPackedChar(pc, (char *)*v3++));
      }
      v2++;
    }
    j = (NPackedShort(ps) + 1) * sizeof(unshort);
    k = NPackedChar(pc) * sizeof(char);
    if ((sp = (unshort *)malloc(j + k)) == NULL)
      errorexit(1, "docreatedb: Out of memory\n");
    *sp = j;
    bcopy((char *)AddrPackedShort(ps), (char *)(sp + 1),
     NPackedShort(ps) * sizeof(unshort));
    bcopy((char *)AddrPackedChar(pc), (char *)sp + j, k);
    storedata(name, strlen(name), (char *)sp, j + k);
    vp++;
  }
  if (verbose)
    printf("Adding keywords to database %s\n", createdb);
  storedata(keywords_key, KEYWORDSKEYSIZE, AddrPackedChar(ckeys),
   NPackedChar(ckeys) * sizeof(char), db);
#ifndef NONDBM
  dbm_close(db);
  if (verbose)
    printf("Closing database %s\n", createdb);
#else NONDBM
  /*
   * There does not seem to be a way to close the old dbm file!
   */
#endif NONDBM
}

static void
storedata(name, n, ptr, size)
char *name;
register int n;
register char *ptr;
register int size;
{
  register char *cp;
  datum key, data;
  Location loc;

  key.dptr = name;
  key.dsize = n;
  loc.magic = MagicNumber;	/* to identify a real lwsrvconfig database */
  loc.offset = dataoffset;
  loc.size = size;
  data.dptr = (char *)&loc;
  data.dsize = sizeof(Location);
#ifndef NONDBM
  if (dbm_store(db, key, data, DBM_REPLACE) < 0) {
    fprintf(stderr, "%s: Failed to insert %s into database %s \n", myname,
     name, createdb);
    exit(1);
  }
#else NONDBM
  if (store(key, data) < 0) {
    fprintf(stderr, "%s: Failed to insert %s into database %s \n", myname,
     name, createdb);
    exit(1);
  }
#endif NONDBM
  dataoffset += size;
  while (size > 0) {
    if ((n = write(datafd, ptr, size)) <= 0) {
      fprintf(stderr, "%s: write error on %d%s\n", myname, createdb,
       datasuffix);
      exit(1);
    }
    size -= n;
    ptr += n;
  }
}

static void
display(name, kp)
char *name;
register KVTree **kp;
{
  register List *lp;
  register char **cp;
  register int n, i;

  lp = ListKVTree(kp);
  if (headerstr) {
    fputs(headerstr, stdout);
    headerstr = NULL;
  }
  printf("%s = ", quotestr(name));
  kp = (KVTree **)AddrList(lp);
  if ((n = NList(lp)) == 1) {
    fputs(untab((char *)(*kp)->key), stdout);
    if (ListVal((*kp)->key)) {
      cp = (char **)AddrList((*kp)->val);
      if ((i = NList((*kp)->val)) == 1)
	printf(" %s;\n", quotestr(*cp));
      else if (isSpecialOpt((*kp)->key)) {
	printf(" %s;\n", quotestr(*cp));
	while (--i > 0) {
	  fputs(untab((char *)(*kp)->key), stdout);
	  printf(" %s;\n", quotestr(*++cp));
	}
      } else {
	fputs(" (\n", stdout);
	while (i--) {
	  printf("\t%s,\n", quotestr(*cp));
	  cp++;
	}
	fputs(");\n", stdout);
      }
    } else {
      if ((*kp)->val)
	printf(" %s;\n", quotestr((char *)(*kp)->val));
      else
	fputs(";\n", stdout);
    }
  } else {
    fputs("(\n", stdout);
    for (;n-- > 0; kp++) {
      printf("\t%s", untab((char *)(*kp)->key));
      if (ListVal((*kp)->key)) {
	cp = (char **)AddrList((*kp)->val);
	if ((i = NList((*kp)->val)) == 1)
	  printf(" %s;\n", quotestr(*cp));
	else if (isSpecialOpt((*kp)->key)) {
	  printf(" %s;\n", quotestr(*cp));
	  while (--i > 0) {
	    printf("\t%s", untab((char *)(*kp)->key));
	    printf(" %s;\n", quotestr(*++cp));
	  }
	} else {
	  fputs(" (\n", stdout);
	  while (i--) {
	    printf("\t\t%s,\n", quotestr(*cp));
	    cp++;
	  }
	  fputs("\t);\n", stdout);
	}
      } else {
	if ((*kp)->val)
 	  printf(" %s;\n", quotestr((char *)(*kp)->val));
	else
	  fputs(";\n", stdout);
      }
    }
    fputs(");\n", stdout);
  }
}

static void
displaytemplates()
{
  register List *lp;
  register KVTree **kp;
  register int n;

  lp = ListKVTree(_printers);
  headerstr = "\nTemplates:\n";
  for (kp = (KVTree **)AddrList(lp), n = NList(lp); n > 0; kp++, n--) {
    if (SearchKVTree((*kp)->val, "-p", strcmp) == NULL)
      display((*kp)->key, (*kp)->val, 0);
  }
  headerstr = "\nPrinters:\n";
  FreeList(lp, NULL);
}

static KVTree **
initargname()
{
  register KVTree **kp;
  register struct argnamebuf *ap;

  kp = CreateKVTree();
  for (ap = argnamebuf; ap->arg; ap++)
    AddToKVTree(kp, (void *)ap->arg, (void *)&ap->d, strcmp);
  return(kp);
}

static int
getflags(str)
char *str;
{
  register struct descrip *sp;

  if ((sp = (struct descrip *)SearchKVTree(argname, str, strcmp)) == NULL) {
    fprintf(stderr, "%s: Unknown lwsrv argument %s\n", myname, str);
    exit(1);
  }
  return(sp->flags);
}

static char *
keyname(str)
char *str;
{
  register struct descrip *sp;

  if (strcmp(str, includename) == 0)
    return("Include");
  if (oldformat)
    return(str);
  return((sp = (struct descrip *)SearchKVTree(argname, str, strcmp)) ?
   sp->name : str);
}

#define	DQUOTE	(1 << 0)
#define	SQUOTE	(1 << 1)

static char *
quotestr(str)
register char *str;
{
  static char buf[256];
  register char *fp, *tp, *bp;
  register int quote;

  if (!str)
    return("");
  quote = 0;
  if (index(str, '"'))
    quote |= DQUOTE;
  if (index(str, '\''))
    quote |= SQUOTE;
  if (!quote && !index(str, ' ') && !index(str, '\t')) {
    bp = str;
    if (index(bp, '\n')) {
      strcpy(bp = buf, str);
      if (fp = index(bp, '\n'))
	*fp = 0;
    }
    if (strlen(bp) == 2 && *bp == '-' && isalpha(bp[1])) {
      sprintf(buf, "\"-%c\"", bp[1]);
      bp = buf;
    }
    return(bp);
  }
  switch(quote) {
   case 0:
   case (DQUOTE | SQUOTE):
    quote = '"';
    break;
   default:
    quote = (quote & SQUOTE) ? '"' : '\'';
    break;
  }
  fp = str;
  tp = buf;
  *tp++ = quote;
  while (*fp) {
    if (*fp == '\n') {
      fp++;
      continue;
    }
    if (*fp == quote)
      *tp++ = quote;
    *tp++ = *fp++;
  }
  *tp++ = quote;
  *tp = 0;
  return(buf);
}

static void
setfirsttime(str)
register char *str;
{
  while (*str)
    firsttime[*str++] = 1;
}

static unshort *
ushortdup(i)
int i;
{
  register unshort *sp;

  if ((sp = (unshort *)malloc(sizeof(unshort))) == NULL)
    errorexit(1, "ushortdup: Out of memory\n");
  *sp = i;
  return(sp);
}

#if (!(defined(SOLARIS)||defined(bsdi)||defined(__NetBSD__)||defined(__FreeBSD__)||defined(linux)))
char *
strdup(str)
char *str;
{
  register char *cp;

  if ((cp = (char *)malloc(strlen(str) + 1)) == NULL)
    errorexit(1, "strdup: Out of memory\n");
  strcpy(cp, str);
  return(cp);
}
#endif /* SOLARIS || bsdi || __NetBSD__ */

static void
usage()
{
  fprintf(stderr, "Usage: %s [-l] dbname [-o] [-v] [file]\n", myname);
  fprintf(stderr, "       %s -c newdbname [-o] [-v] [file]\n", myname);
  exit(1);
}

static char *
untab(str)
char *str;
{
  register char *cp, *bp;
  register int i;
  static char buf[256];

  if (!(bp = index(str, '\t')))
    return(keyname(str));
  strncpy(buf, str, (i = bp - str));
  buf[i] = 0;
  if ((cp = keyname(str)) != str)
    strcpy(buf, cp);
  strcat(buf, " ");
  strcat(buf, quotestr(bp + 1));
  return(buf);
}
