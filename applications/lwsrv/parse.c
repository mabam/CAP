static char rcsid[] = "$Author: djh $ $Date: 1995/08/28 10:38:35 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/parse.c,v 2.1 1995/08/28 10:38:35 djh Rel djh $";
static char revision[] = "$Revision: 2.1 $";

/*
 * parse.c -  read a configuration file and parse the information
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/file.h>
#include <netat/sysvcompat.h>
#include <netat/compat.h>
#ifdef USESTRINGDOTH
#include <string.h>
#else  /* USESTRINGDOTH */
#include <strings.h>
#endif /* USESTRINGDOTH */
#ifdef NEEDFCNTLDOTH
#include <sys/fcntl.h>
#endif /* NEEDFCNTLDOTH */
#ifndef NONDBM
# include <ndbm.h>
#else /* NONDBM */
# include <dbm.h>
#endif /* NONDBM */
#include "list.h"
#include "parse.h"

typedef unsigned short unshort;

char datasuffix[] = ".dat";
char includename[] = "-";
char keywords_key[] = "keywords\0001.0.0";
KVTree **_printers;
char specialOpts[] = "LT";
KVTree **thequery;

extern char *myname;
extern List *optionlist;
extern List *printerlist;

static char *keywords = NULL;
static KVTree **pass1;
#ifndef NONDBM
static DBM *printerdb;
#endif NONDBM
static int printerfd;

char *malloc();

static void Construct();
static char *FetchData();
static void Include();
static void _Include();
static List *UnpackList();
static List *Unpackdb();
static void _configargs();
static void setargs();

static void
Construct(name, kp, lp)
char *name;
register KVTree **kp;
List *lp;
{
  register void **vp;
  register int i;
  register void *np;

  for (vp = (void **)AddrList(lp), i = NList(lp); i > 0; i -= 2) {
    if (strcmp((char *)*vp, includename) == 0) {	/* include */
      Include(name, (char *)*++vp, kp);
      vp++;
      continue;
    }
    np = *vp++;
    if (isOption(np) && isSpecialOpt(np)) {
      if ((lp = (List *)SearchKVTree(kp, np, strcmp)) == NULL) {
        lp = CreateList();
        AddToKVTree(kp, np, lp, strcmp);
      }
      AddToList(lp, *vp++);
    } else
      AddToKVTree(kp, np, *vp++, strcmp);
  }
}

static char *
FetchData(name, n)
char *name;
register int n;
{
  register char *ptr, *cp;
  register int size;
  datum key, data;
  Location loc;

  key.dptr = name;
  key.dsize = n;
#ifndef NONDBM
  data = dbm_fetch(printerdb, key);
#else NONDBM
  data = fetch(key);
#endif NONDBM
  if (!data.dptr)
    return(NULL);
  if (data.dsize != sizeof(Location)) {
    fprintf(stderr, "%s: location data for %s is wrong size\n",
     myname, name);
    exit(1);
  }
  bcopy((char *)data.dptr, (char *)&loc, sizeof(Location));
  if (loc.magic != MagicNumber || (size = loc.size) <= 0) {
    fprintf(stderr, "%s: location data mismatch for %s\n", myname, name);
    exit(1);
  }
  if (lseek(printerfd, loc.offset, L_SET) < 0) {
    fprintf(stderr, "%s: unable to position in data file for %s\n",
     myname, name);
    exit(1);
  }
  if ((cp = ptr = malloc(size)) == NULL) {
    fprintf(stderr, "%s: out of memory for %s\n", myname, name);
    exit(1);
  }
  while (size > 0) {
    if ((n = read(printerfd, cp, size)) < 0) {
      fprintf(stderr, "%s: read error on %s\n", myname, name);
      exit(1);
    }
    if (n == 0) {
      fprintf(stderr, " %s: premature end of file on %s\n", myname, name);
      exit(1);
    }
    size -= n;
    cp += n;
  }
  return(ptr);
}

static void
Include(name, cp, kp)
char *name, *cp;
KVTree **kp;
{
  register KVTree **ip;
  register List *lp;
  register char *dp;

  if (ip = (KVTree **)SearchKVTree(_printers, (void *)cp, strcmp)) {
    _Include(kp, *ip);
    return;
  }
  /* not in printer list, so try pass1 */
  if (lp = (List *)SearchKVTree(pass1, (void *)cp, strcmp)) {
    ip = CreateKVTree();
    /*
     * We add the new (empty) tree into master printer tree before
     * constructing it so as to prevent infinite include loops.
     */
    AddToKVTree(_printers, (void *)cp, ip, strcmp);
    Construct(cp, ip, lp);
    if (*ip)
      _Include(kp, *ip);
    NList(lp) = 0;	/* mark the list as already processed */
    return;
  }
  /* not in pass1, so try database */
  if (keywords && (dp = FetchData(cp, strlen(cp)))) {
    lp = Unpackdb(dp);
    ip = CreateKVTree();
    /*
     * We add the new (empty) tree into master printer tree before
     * constructing it so as to prevent infinite include loops.
     */
    AddToKVTree(_printers, cp, ip, strcmp);
    Construct(cp, ip, lp);
    if (*ip)
      _Include(kp, *ip);
    return;
  }
  fprintf(stderr, "%s: %s: Can't include %s\n", myname, name, cp);
  exit(1);
}

static void
_Include(kp, ip)
KVTree **kp;
register KVTree *ip;
{
  register List *lp;

  if (ip == NULL)
    return;
  if (isOption(ip->key) && isSpecialOpt(ip->key) &&
   (lp = (List *)SearchKVTree(kp, ip->key, strcmp))) {
    CatList(lp, (List *)ip->val);
  } else
    AddToKVTree(kp, ip->key, ip->val, strcmp);
  if (ip->left)
    _Include(kp, ip->left);
  if (ip->right)
    _Include(kp, ip->right);
}

static List *
UnpackList(sp, c)
register unshort *sp;
register char *c;
{
  register List *lp;
  register int n;

  lp = CreateList();
  for (n = *sp++; n > 0; n--)
    AddToList(lp, c + *sp++);
  return(lp);
}

static List *
Unpackdb(c)
register char *c;
{
  register unshort *sp, *s;
  register char *cp;
  register List *lp;
  register int n;

  lp = CreateList();
  s = (unshort *)c;
  /*
   * The first unshort is offset to string section.  s is now pointing to
   * beginning of packed ushorts
   */
  c += *s++;
  /*
   * The first unshort is the number of pairs in the list.  Then each following
   * pair of ushorts points to a name string and the value, which may be another
   * string or another list.
   */
  sp = s;
  for (n = *sp++; n > 0; n--) {
    AddToList(lp, cp = keywords + *sp++);
    if (StringVal(cp))	/* value is just a string */
      AddToList(lp, *sp == 0 ? NULL : c + *sp);
    else		/* value is another list */
      AddToList(lp, UnpackList(s + *sp, c));
    sp++;
  }
  return(lp);
}

void
configargs(dbname)
char *dbname;
{
  register KVTree **kp;
  register List *lp;
  register void **vp;
  register int i;
  register char *cp;
  KVTree **_default;
  char buf[BUFSIZ];

  if (dbname) {
#ifndef NONDBM
    if ((printerdb = dbm_open(dbname, O_RDONLY, 0)) == NULL) {
      fprintf(stderr, "%s: Can't open dbm %s\n", myname, dbname);
      exit(1);
    }
#else NONDBM
    if (dbminit(dbname) < 0) {
      fprintf(stderr, "%s: Can't open dbm %s\n", myname, dbname);
      exit(1);
    }
#endif NONDBM
    strcpy(buf, dbname);
    strcat(buf, datasuffix);
    if ((printerfd = open(buf, O_RDONLY, 0)) < 0) {
      fprintf(stderr, "%s: Can't open data file %s%s\n", myname, dbname,
       datasuffix);
      exit(1);
    }
    if ((keywords = FetchData(keywords_key, KEYWORDSKEYSIZE)) == NULL) {
      fprintf(stderr, "%s: %s does not contain printer templates\n",
       myname, dbname);
      exit(1);
    }
  }
  if (optionlist) {
    _default = CreateKVTree();
    Construct("Default", _default, optionlist);
    if (NList(optionlist) > 0)
      setargs(NULL, _default);
    FreeKVTree(_default, NULL, NULL);
  }
  _printers = CreateKVTree();
  /* Pass 1 */
  pass1 = CreateKVTree();
  for (vp = AddrList(printerlist), i = NList(printerlist); i > 0; i -= 2){
    cp = (char *)*vp++;
    AddToKVTree(pass1, (void *)cp, *vp++, strcmp);
  }
  /* Pass 2 */
  for (vp = AddrList(printerlist), i = NList(printerlist); i > 0; vp++, i -= 2) {
    if (NList(*vp) <= 0)	/* already processed */
      continue;
    kp = CreateKVTree();
    cp = (char *)*vp++;
    /*
     * We add the new (empty) tree into master printer tree before
     * constructing it so as to prevent infinite include loops.
     */
    AddToKVTree(_printers, (void *)cp, kp, strcmp);
    Construct(cp, kp, (List *)*vp);
  }
  if (*_printers)
    _configargs(*_printers);

#ifndef NONDBM
  if (printerdb)
    dbm_close(printerdb);
#else NONDBM
  /*
   * There does not seem to be a way to close the old dbm file!
   */
#endif NONDBM
}

static void
_configargs(kp)
register KVTree *kp;
{
  if (kp == NULL)
    return;
  if (kp->left)
    _configargs(kp->left);
  if (SearchKVTree(kp->val, "-p", strcmp))
    setargs(kp->key, kp->val);
  if (kp->right)
    _configargs(kp->right);
}

static void
setargs(name, kp)
char *name;
register KVTree **kp;
{
  register int i, j;
  register List *lp, *lk;
  register void **vp;
  extern int optind;

  lp = CreateList();
  AddToList(lp, myname);	/* simulating argc, argv, so this is argv[0] */
  if (name) {
    /*
     * Just do "-n name" to force thequery to be set to the correct value.
     */
    AddToList(lp, "-n");
    AddToList(lp, name);
    AddToList(lp, NULL);	/* Just in case */
    optind = 1;
    doargs(NList(lp) - 1, AddrList(lp));
    FreeList(lp, NULL);
    lp = CreateList();
    AddToList(lp, myname);
  }
  lk = ListKVTree(kp);
  for (kp = (KVTree **)AddrList(lk), i = NList(lk); i > 0; kp++, i--) {
    if (!isOption((*kp)->key))
      break;
    if (isSpecialOpt((*kp)->key)) {
      for (j = NList((*kp)->val), vp = AddrList((*kp)->val); j > 0; j--) {
        AddToList(lp, (*kp)->key);
        AddToList(lp, *vp++);
      }
    } else {
      AddToList(lp, (*kp)->key);
      if ((*kp)->val)
        AddToList(lp, (*kp)->val);
    }
  }
  if (i > 0) {
    while (i-- > 0) {
      AddToKVTree(thequery, (*kp)->key, (*kp)->val, strcmp);
      kp++;
    }
  }
  FreeList(lk, NULL);
  if (NList(lp) > 1) {
    AddToList(lp, NULL);	/* Just in case */
    optind = 1;
    doargs(NList(lp) - 1, AddrList(lp));
  }
  FreeList(lp, NULL);
}
