/*
 * $Date: 91/02/15 21:21:05 $
 * $Header: atalkdbm.c,v 2.1 91/02/15 21:21:05 djh Rel $
 * $Log:	atalkdbm.c,v $
 * Revision 2.1  91/02/15  21:21:05  djh
 * CAP 6.0 Initial Revision
 * 
 * Revision 1.2  91/01/04  22:43:52  djh
 * We don't need this file if used with CAP 6.0, add some
 * defines to suit.
 * 
 * Revision 1.1  90/03/17  22:06:08  djh
 * Initial revision
 * 
 *
 * From atalkdbm.c, Revision 1.13, Charlie Kim/CAP distribution.
 * mods for Async AppleTalk: djh@munnari.oz, August, 1988
*/

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netat/sysvcompat.h>
#include <netat/appletalk.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else
# include <strings.h>
#endif

#ifndef CAP_6_DBM

/*
 * The following globals are exported to the rest of CAP.
 */
u_short	this_net = 0, bridge_net = 0, nis_net = 0, async_net; /* djh */
u_char	this_node, bridge_node,nis_node;
char	this_zone[34], async_zone[34];	/* djh */
struct in_addr bridge_addr;

#ifndef	TAB
# define TAB "/etc/atalk.local"
#endif	TAB
#ifndef CONFIGDIR
# define CONFIGDIR "/etc"
#endif

#include <netdb.h>

static int opened = 0;
#define HAVE_ZONE -1		/* our zone was set */
#define CONFIGURED 1		/* set when configured */

/*
 * Set zone name - sets alternate atalk configuration file: atalk.<zonename>
 * 
 *
*/
zoneset(zonename)
char *zonename;
{
  strncpy(this_zone, zonename, 32);
  opened = HAVE_ZONE;
}

/*
 * get base configuration
 *
*/
openatalkdb(name)
char *name;
{
  FILE *fp;
  int a, b;
  char fn[255];
  char line[256], st[64];
  int linecnt = 0;
  char zonename[34];		/* temp */
  char bridge_name[64];
  char *p, *p2, c;

  if (opened == CONFIGURED)
    return;

  if (name == NULL) {
    if (opened == HAVE_ZONE) {
      strcpy(fn, CONFIGDIR);
      strcat(fn,"/atalk.");
      a = strlen(fn);		/* find where we are */
      p = fn+a;			/* move to end */
      a = 0;
      while ((c=this_zone[a++]) != '\0')
	*p++ = (isascii(c) && isalpha(c) && isupper(c)) ? tolower(c) : c;
      *p = '\0';			/* tie string */
    } else 
      strcpy(fn, TAB);
  } else
    strcpy(fn, name);

  if ((fp = fopen(fn, "r")) == NULL) {
    perror(fn);
    exit(1);
  }

  while (fgets(line, sizeof line, fp) != NULL) {
    linecnt++;			/* remember which line */
    if (line[0] == '#' || line[0] == '\n')
      continue;
    if (this_net == 0) {
      if (sscanf(line, "%s %d %s", st, &b, zonename) != 3) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad node id format: %s", line);
	exit(1);
      }
      this_net = htons(atnetshort(st));
      this_node = (b);
      /* convert zonename: __ means _ and "_" means space */
      for (p = zonename, p2=this_zone; *p != '\0'; ) {
	if (*p == '_') {
	  p++;
	  if (*p == '_') {
	    *p2++ = '_';
	    p++;		/* skip */
	  } else
	    *p2++ = ' ';
	} else
	  *p2++ = *p++;		/* just copy the byte */
      }
      continue;
    }
    if (bridge_net == 0) {
      if (sscanf(line, "%s %d %s", st, &b, bridge_name) != 3) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad bridge id format: %s", line);
	exit(1);
      }
      bridge_net = htons(atnetshort(st));
      bridge_node = (b);
      if (name_toipaddr(bridge_name, &bridge_addr) < 0) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr,"openatalkdb: bridge \"%s\" is unknown: %s\n",
		bridge_name,line);
	exit(1);
      }
      continue;
    }
    if (nis_net == 0) {
      if (sscanf(line, "%s %d", st, &b) != 2) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad NIS server id format: %s", line);
	exit(1);
      }
      nis_net = htons(atnetshort(st));
      nis_node = (b);
      continue;
    }
    /* djh */
    if (async_net == 0) {
      if (sscanf(line, "%s %s", st, zonename) != 2) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad async id format: %s", line);
	exit(1);
      }
      async_net = htons(atnetshort(st));
      /* convert zonename: __ means _ and "_" means space */
      for (p = zonename, p2=async_zone; *p != '\0'; ) {
	if (*p == '_') {
	  p++;
	  if (*p == '_') {
	    *p2++ = '_';
	    p++;		/* skip */
	  } else
	    *p2++ = ' ';
	} else
	  *p2++ = *p++;		/* just copy the byte */
      }
      continue;
    }
    /* end djh */
  }
  if (this_net == 0 || bridge_net == 0) {
    fprintf(stderr, "openatalkdb: %s: node or bridge identification missing\n",
	    fn);
    exit(1);
  }
  if (nis_net == 0) {		/* usual case */
    nis_net = this_net;
    nis_node = this_node;
  }
  opened++;
  fclose(fp);
}

/*
 * Get a short number or address.
 */
atnetshort(st)
register char *st;
{
  register char *cp;

  if ((cp = index(st, '.')) == 0)
    return (atoi(st));
  *cp++ = 0;
  return ((atoi(st)<<8) | atoi(cp));
}


static int
name_toipaddr(name, ipaddr)
char *name;
struct in_addr *ipaddr;
{
  struct hostent *host;

  if (isdigit(name[0])) {
    if ((ipaddr->s_addr = inet_addr(name)) == -1)
      return(-1);
    return(0);
  }
  if ((host = gethostbyname(name)) == 0)
    return(-1);
  bcopy(host->h_addr, (caddr_t)&ipaddr->s_addr, sizeof(ipaddr->s_addr));
}

#endif CAP_6_DBM
