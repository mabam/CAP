/*
 * $Date: 1996/06/18 10:48:17 $
 * $Header: /mac/src/cap60/lib/cap/RCS/atalkdbm.c,v 2.13 1996/06/18 10:48:17 djh Rel djh $
 * $Revision: 2.13 $
 *
 * mods for async appletalk support, /etc/etalk.local for EtherTalk and
 * changes for quoted zone names: djh@munnari.OZ.AU, 27/11/90
 * Phase 2 support: djh@munnari.OZ.AU, 28/04/91
 *
 */

#include <stdio.h>
#include <ctype.h>
#if (defined(SOLARIS) || defined(linux))
#include <unistd.h>
#endif SOLARIS || linux
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netat/sysvcompat.h>
#include <netat/appletalk.h>
#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH
#include <sys/file.h>
#include "atalkdbm.h"

#ifndef	TAB
# define TAB "/etc/atalk.local"
#endif	TAB
#ifndef	ETAB
# define ETAB "/etc/etalk.local"
#endif	ETAB
#ifndef CONFIGDIR
# define CONFIGDIR "/etc"
#endif CONFIGDIR

#include <netdb.h>

static int opened = 0;
#define HAVE_ZONE -1		/* our zone was set */
#define CONFIGURED 1		/* set when configured */

static int name_toipaddr();

char *linep;
int linecnt = 0;

/* export these to the rest of CAP */

u_short	this_net=0,	bridge_net=0,	nis_net=0,	async_net=0;
u_char	this_node=0,	bridge_node=0,	nis_node=0;
u_char	this_zone[34],	async_zone[34],	interface[50];

#ifdef PHASE2
u_short net_range_start = 0, net_range_end = 0xfffe;
#endif PHASE2

struct	in_addr bridge_addr;

char enet_device[50];
char enet_zone[34];
char netnumber[64];
int argsread;
      
/*
 * GetMyAddr(AddrBlock)
 * set up AddrBlock to my address.
 * Use rather than directly referring to my_node and my_net.
 *
 */

GetMyAddr(addr)
AddrBlock *addr;
{
  addr->net = this_net;
  addr->node = this_node;
}

SetMyAddr(addr)
AddrBlock *addr;
{
  this_net = nis_net = addr->net;
  this_node = nis_node = addr->node;
}

/* similarly for nis */

GetNisAddr(addr)
AddrBlock *addr;
{
  addr->net = nis_net;
  addr->node = nis_node;
}

SetNisAddr(addr)
AddrBlock *addr;
{
  nis_net = addr->net;
  nis_node = addr->node;
}

/*
 * Set zone name - sets alternate atalk configuration file: atalk.<zonename>
 * 
 *
 */

zoneset(zonename)
u_char *zonename;
{
  strncpy((char *)this_zone, (char *)zonename, 32);
  opened = HAVE_ZONE;
}

/*
 * get base configuration from our config file (default "/etc/atalk.local").
 * Use this info as a "hint" for Phase I EtherTalk networks with no router.
 * Use to select "default zone" for Phase II EtherTalk (maybe eventually).
 * This file need not exist for use with EtherTalk but is mandatory for KIP.
 *
 * Fix the format of this file as follows ...
 *
 *		mynet		mynode		myzone 
 *		bridgenet	bridgenode	bridgeIP
 *		nisnet		nisnode
 *		asyncnet	asynczone
 *
 * If the optional line 4 async info isn't required, line 3 is also optional
 * (a default NIS line has nisnet=mynet and nisnode=mynode). Comment lines
 * start with a '#', blank lines are ignored. Zone names can now be quoted
 * (with " or ') to contain spaces, the '_' -> ' ' mapping garbage is gone.
 *
 */

openatalkdb(name)
char *name;
{
  FILE *fp;
  int a, c;
  char fn[255];
  char line[256], st[64];
  char bridge_name[64];
  char *p;

  if (opened == CONFIGURED)
    return;

  if (name == NULL) {
    if (opened == HAVE_ZONE) {
      strcpy(fn, CONFIGDIR);
      strcat(fn,"/atalk.");
      a = strlen(fn);		/* find where we are */
      p = fn+a;			/* move to end */
      a = 0;
      while ((c=(int)this_zone[a++]) != '\0')
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

  this_node = bridge_node = nis_node = 0;
  this_net = bridge_net = nis_net = async_net = 0;
  *this_zone = *async_zone = *interface = 0;
  bzero(&bridge_addr, sizeof(struct in_addr));

  while (fgets(line, sizeof(line), fp) != NULL) {
    linecnt++;	/* remember which line */
    if (line[0] == '#' || line[0] == '\n')
      continue;
    if ((p = (char *)index(line, '\n')) != NULL)
      *p = '\0';
    linep = line;

    /* backward compatability with rutgers native ethertalk */
    if ((argsread = sscanf(line, "%s %s %s", enet_device,
     enet_zone, netnumber)) >= 2 && strncmp(enet_device, "enet", 4) == 0) {
      if (argsread == 3) {
	this_net = htons(atnetshort(netnumber));
	if ( this_net <= 0 || this_net >= 65280 ) {
	  fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	  fprintf(stderr, "openatalkdb: invalid network number: %d\n",this_net);
	  exit(1);
	}
      }
      strncpy((char *)interface, enet_device,50);
      strncpy((char *)this_zone, enet_zone, 34);
      opened = CONFIGURED;
      fclose(fp);
      return;
    }

    if (this_net == 0) {
      getfield(st, sizeof(st), 0);
      this_net = htons(atnetshort(st));
      getfield(st, sizeof(st), 0);
      this_node = atoi(st) & 0xff;
      getfield((char *)this_zone, sizeof(this_zone), 0);

      /*
       * let node "0" stand for last byte of IP address
       * Lennart Lovstrand 890430
       *
       */
      if (this_node == 0) {
	struct hostent *he, *gethostbyname();
	char name[100];

	(void) gethostname(name, sizeof(name));
	if ((he = gethostbyname(name)) == NULL) {
	  perror(name);
	  exit(2);
	}
	/* if only one interface */
	if (he->h_addr_list[1] == NULL)
	  /* get the last byte for node number */
	  this_node = (unsigned char) he->h_addr[3];
      }

      if (this_net == 0 || this_node == 0 || *this_zone == '\0') {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad format/missing information\n");
	exit(1);
      }
      continue;
    }

    if (bridge_net == 0) {
      getfield(st, sizeof(st), 0);
      bridge_net = htons(atnetshort(st));
      getfield(st, sizeof(st), 0);
      bridge_node = atoi(st);
      getfield(bridge_name, sizeof(bridge_name), 0);
      if (bridge_net == 0 || bridge_node == 0 || *bridge_name == '\0') {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad format/missing information\n");
	exit(1);
      }
      if (name_toipaddr(bridge_name, &bridge_addr) < 0) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr,"openatalkdb: bridge \"%s\" is unknown: %s\n",
		bridge_name,line);
	exit(1);
      }
      continue;
    }
    if (nis_net == 0) {
      getfield(st, sizeof(st), 0);
      nis_net = htons(atnetshort(st));
      getfield(st, sizeof(st), 0);
      nis_node = atoi(st);
      if (nis_net == 0 || nis_node == 0) {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad format/missing information\n");
	exit(1);
      }
      continue;
    }
    if (async_net == 0) {
      getfield(st, sizeof(st), 0);
      async_net = htons(atnetshort(st));
      getfield((char *)async_zone, sizeof(async_zone), 0);
      if (async_net == 0 || *async_zone == '\0') {
	fprintf(stderr, "openatalkdb: in %s, error at line %d\n",fn,linecnt);
	fprintf(stderr, "openatalkdb: bad format/missing information\n");
	exit(1);
      }
      continue;
    }
  }
  if (nis_net == 0) {		/* usual case */
    nis_net = this_net;
    nis_node = this_node;
  }
  opened++;
  fclose(fp);
}

/*
 * Manage EtherNet network information file (default "/etc/etalk.local").
 * If the file exists, it is read and the information it contains
 * is copied to the relevant variables (this_net, this_node etc.). 
 * If the file does not exist, it is created and "/etc/atalk.local" is
 * used to set default values. Use editetalkdb() to alter values once set.
 *
 */

openetalkdb(name)
char *name;
{
  FILE *fp, *fopen();
  char line[256], st[64];
  char fn[256], bridge_name[64];
  char *p, c;

  if (name == NULL)
    strncpy(fn, ETAB, sizeof(fn));
  else
    strncpy(fn, name, sizeof(fn));

  if ((fp = fopen(fn, "r")) == NULL) {	/* etalk file doesn't exist */
    if (access(TAB, R_OK) == 0) {
    	openatalkdb(TAB);		/* set some defaults */
    } else {
        this_node = bridge_node = nis_node = 0;
        this_net = bridge_net = nis_net = async_net = 0;
        *this_zone = *async_zone = *interface = 0;
        bzero(&bridge_addr, sizeof(struct in_addr));
    }
    etalkdbupdate(fn);			/* create it ... */
  } else {
    linecnt = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
      linecnt++;				/* remember which line */
      if (line[0] == '#' || line[0] == '\n')	/* ignore comments */
        continue;
      if ((p = (char *)index(line, '\n')) != NULL)
        *p = '\0';
      matchvariable(line);
    }
    fclose(fp);
  }
}

/*
 * alter the variable ...
 */

editetalkdb(str)
char *str;
{
  matchvariable(str);
}

/*
 * write out the values that we have
 *
 */

extern short lap_proto;

etalkdbupdate(name)
char *name;
{
  long now;
  FILE *fp, *fopen();
  char fn[256], *ctime(), *inet_ntoa(), *prZone();
#ifdef ISO_TRANSLATE
  void cMac2ISO(), cISO2Mac();
#endif ISO_TRANSLATE

  if (name == NULL)
    strncpy(fn, ETAB, sizeof(fn));
  else
    strncpy(fn, name, sizeof(fn));

  if ((fp = fopen(fn, "w")) != NULL) {
    time(&now);
    fprintf(fp, "#\n# EtherTalk dynamic configuration data\n#\n");
#ifdef ISO_TRANSLATE
    fprintf(fp, "# Using ISO character translation\n#\n");
#endif ISO_TRANSLATE
    fprintf(fp, "# Last update:\t%s#\n", ctime(&now));
    if (lap_proto == LAP_MKIP)
      fprintf(fp, "# Generated by UAB\n#\n");
    if (lap_proto == LAP_ETALK)
#ifdef PHASE2
      fprintf(fp, "# Generated by Native EtherTalk (Phase 2)\n#\n");
#else  PHASE2
      fprintf(fp, "# Generated by Native EtherTalk (Phase 1)\n#\n");
#endif PHASE2
    if (lap_proto == LAP_KERNEL)
      fprintf(fp, "# Generated by Kernel AppleTalk\n#\n");
    if (*interface != '\0')
      fprintf(fp, "%-12s\t\"%s\"\n", ETH_INTERFACE, interface);
#ifdef PHASE2
    fprintf(fp, "%-12s\t%2d.%02d\n", ETH_NET_START,
      ((ntohs(net_range_start) >> 8) & 0xff), (ntohs(net_range_start) & 0xff));
    fprintf(fp, "%-12s\t%2d.%02d\n", ETH_NET_END,
      ((ntohs(net_range_end) >> 8) & 0xff), (ntohs(net_range_end) & 0xff));
#endif PHASE2
    fprintf(fp, "%-12s\t%2d.%02d\n", ETH_THIS_NET,
      ((ntohs(this_net) >> 8) & 0xff), (ntohs(this_net) & 0xff));
    fprintf(fp, "%-12s\t%d\n", ETH_THIS_NODE, (this_node & 0xff));
#ifdef ISO_TRANSLATE
    cMac2ISO(this_zone);
    fprintf(fp, "%-12s\t\"%s\"\n", ETH_THIS_ZONE, (char *)this_zone);
    cISO2Mac(this_zone);
#else  ISO_TRANSLATE
    fprintf(fp, "%-12s\t\"%s\"\n", ETH_THIS_ZONE, prZone((char *)this_zone));
#endif ISO_TRANSLATE
    fprintf(fp, "%-12s\t%2d.%02d\n", ETH_BRIDGE_NET,
      ((ntohs(bridge_net) >> 8) & 0xff), (ntohs(bridge_net) & 0xff));
    fprintf(fp, "%-12s\t%d\n", ETH_BRIDGE_NODE, (bridge_node & 0xff));
    fprintf(fp, "%-12s\t%s\n", ETH_BRIDGE_IP, inet_ntoa(bridge_addr));
    fprintf(fp, "%-12s\t%2d.%02d\n", ETH_NIS_NET,
      ((ntohs(nis_net) >> 8) & 0xff), (ntohs(nis_net) & 0xff));
    fprintf(fp, "%-12s\t%d\n", ETH_NIS_NODE, (nis_node & 0xff));
    fprintf(fp, "%-12s\t%2d.%02d\n", ETH_ASYNC_NET,
      ((ntohs(async_net) >> 8) & 0xff), (ntohs(async_net) & 0xff));
    fprintf(fp, "%-12s\t\"%s\"\n", ETH_ASYNC_ZONE, prZone((char *)async_zone));
    fclose(fp);
  } else {
    fprintf(stderr, "openetalkdb: cannot open %s for writing\n", fn);
    exit(1);
  }
}

/*
 * identify and set the variable in str (identifier value)
 *
 */

matchvariable(str)
char *str;
{
  char name[64], value[64];
#ifdef ISO_TRANSLATE
  void cISO2Mac();
#endif ISO_TRANSLATE

  linep = str;
  getfield(name, sizeof(name), 0);
  getfield(value, sizeof(value), 0);

  if (strcmp(name, ETH_INTERFACE) == 0)
    strncpy((char *)interface, value, sizeof(interface));
#ifdef PHASE2
  else if (strcmp(name, ETH_NET_START) == 0)
    net_range_start = htons(atnetshort(value));
  else if (strcmp(name, ETH_NET_END) == 0)
    net_range_end = htons(atnetshort(value));
#endif PHASE2
  else if (strcmp(name, ETH_THIS_NET) == 0)
    this_net = htons(atnetshort(value));
  else if (strcmp(name, ETH_THIS_NODE) == 0)
    this_node = atoi(value) & 0xff;
  else if (strcmp(name, ETH_THIS_ZONE) == 0)
#ifdef ISO_TRANSLATE
    {
    cISO2Mac(value);
    strncpy((char *)this_zone, value, sizeof(this_zone));
    }
#else  ISO_TRANSLATE
    strncpy((char *)this_zone, value, sizeof(this_zone));
#endif ISO_TRANSLATE
  else if (strcmp(name, ETH_BRIDGE_NET) == 0)
    bridge_net = htons(atnetshort(value));
  else if (strcmp(name, ETH_BRIDGE_NODE) == 0)
    bridge_node = atoi(value) & 0xff;
  else if (strcmp(name, ETH_BRIDGE_IP) == 0)
    name_toipaddr(value, &bridge_addr);
  else if (strcmp(name, ETH_NIS_NET) == 0)
    nis_net = htons(atnetshort(value));
  else if (strcmp(name, ETH_NIS_NODE) == 0)
    nis_node = atoi(value) & 0xff;
  else if (strcmp(name, ETH_ASYNC_NET) == 0)
    async_net = htons(atnetshort(value));
  else if (strcmp(name, ETH_ASYNC_ZONE) == 0)
    strncpy((char *)async_zone, value, sizeof(async_zone));
  else {
    fprintf(stderr, "bogus line in file: %s\n", str);
    exit(1);
  }
}

/*
 * Get a short number or address.
 */
atnetshort(st)
register char *st;
{
  register char *cp;

  if (*st == '\0')
    return (0);
  if ((cp = (char *)index(st, '.')) == 0)
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
  return(0);
}

/*
 * Get next field from 'line' buffer into 'str'.  'linep' is the 
 * pointer to current position.
 *
 * Fields are white space separated, except within quoted strings.
 * If 'quote' is true the quotes of such a string are retained, otherwise
 * they are stripped.  Quotes are included in strings by doubling them or
 * escaping with '\'.
 */

getfield(str, len, quote)
	char *str;
{
	register char *lp = linep;
	register char *cp = str;

	while (*lp == ' ' || *lp == '\t')
		lp++;	/* skip spaces/tabs */
	if (*lp == 0 || *lp == '#') {
		*cp = 0;
		return;
	}
	len--;	/* save a spot for a null */

	if (*lp == '"' || *lp == '\'') {		/* quoted string */
		register term = *lp;

		if (quote) {
			*cp++ = term;
			len -= 2;	/* one for now, one for later */
		}
		lp++;
		while (*lp) {
			if (*lp == term) {
				if (lp[1] == term)
					lp++;
				else
					break;
			}
			/* check for \', \", \\ only */
			if (*lp == '\\'
			  && (lp[1] == '\'' || lp[1] == '"' || lp[1] == '\\'))
				lp++;
			*cp++ = *lp++;
			if (--len <= 0) {
				fprintf(stderr,
				  "string truncated: %s, linenum %d",
				    str, linecnt);
				if (quote)
					*cp++ = term;
				*cp = 0;
				linep = lp;
				return;
			}
		}
		if (!*lp)
			fprintf(stderr,"unterminated string: %s, linenum %d",
			    str, linecnt);
		else {
			lp++;	/* skip the terminator */

			if (*lp && *lp != ' ' && *lp != '\t' && *lp != '#') {
				fprintf(stderr,
				  "garbage after string: %s, linenum %d",
				    str, linecnt);
				while (*lp && *lp != ' ' &&
				    *lp != '\t' && *lp != '#')
					lp++;
			}
		}
		if (quote)
			*cp++ = term;
	} else {
		while (*lp && *lp != ' ' && *lp != '\t' && *lp != '#') {
			*cp++ = *lp++;
			if (--len <= 0) {
				fprintf(stderr,
				  "string truncated: %s, linenum %d",
				    str, linecnt);
				break;
			}
		}
	}
	*cp = 0;
	linep = lp;
}

/*
 * if the zone name contains a double or single forward quote,
 * escape it with '\' so that it is retained when read back in.
 *
 */

char *
prZone(zone)
char *zone;
{
	int i;
	static char azone[48];

	i = 0;
	while(*zone != '\0' && i < sizeof(azone)) {
		if(*zone == '"' || *zone == '\'' || *zone == '\\')
			azone[i++] = '\\';
		azone[i++] = *zone++;
	}
	azone[i] = '\0';
	return(azone);
}
