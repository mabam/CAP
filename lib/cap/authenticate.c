/*
 * Copyright (c) 1990, The Regents of the University of California.
 * Edward Moy, Workstation Software Support Group, Workstation Support Services,
 * Information Systems and Technology.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software so long as it is not sold for profit,
 * provided that this notice and the original copyright notices are
 * retained.  The University of California makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#ifdef AUTHENTICATE
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netat/appletalk.h>		/* include appletalk definitions */

#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#define	ANYNBPNAME	(1 << 1)
#define	ANYPROGNAME	(1 << 0)
#ifndef AUTHCONFIG
#define	AUTHCONFIG	"/etc/cap.auth"
#endif AUTHCONFIG
#define	CLEARSYMBOL	'*'
#define	DOINNET		(1 << 0)
#define	DONETWORK	(1 << 2)
#define	DOOUTNET	(1 << 1)
#define	MAXAUTHNBP	64
#define	NNETS		16

#define	issep(x)	(isspace(x) || (x) == ',')

typedef unsigned short Net;

typedef struct {
	Net *list;
	Net *ptr;
	int count;
	int size;
} NetList;

typedef struct {
	char *progname;
	char *nbpname;
	char *type;
	unsigned short flags;
} Ident;

static void innet();
/* static void kerberos(); */
static void network();
static void outnet();

static char anystr[] = "*";
static char *authconfig = AUTHCONFIG;
static char authtype[32];
static int doauthenticate = 0;
static int donetwork = FALSE;
static NetList inlist;
static Ident myident;
static unsigned short mynode;
static int ntypetab;
static NetList outlist;
static struct {
	char *type;
	void (*func)();
} typetab[] = {
	{"innet", innet},
	/* {"kerberos", kerberos}, */
	{"network", network},
	{"outnet", outnet},
	{0, 0},
};

char *malloc();
char *realloc();

static
lowercase(str)
register char *str;
{
	while(*str) {
		if(isupper(*str))
			*str = tolower(*str);
		str++;
	}
}

static
comparenet(n1, n2)
register Net *n1, *n2;
{
	return((*n1 > *n2) ? 1 : ((*n1 < *n2) ? -1 : 0));
}

static
compareident(n1, n2)
register Ident *n1, *n2;
{
	register int comp;

	if(!(n1->flags & ANYPROGNAME) && !(n1->flags & ANYPROGNAME) &&
	 (comp = strcmp(n1->progname, n2->progname)) != 0)
		return(comp);
	if(!(n1->flags & ANYNBPNAME) && !(n1->flags & ANYNBPNAME) &&
	 (comp = strcmp(n1->nbpname, n2->nbpname)) != 0)
		return(comp);
	return(0);
}

static
findtype(ident)
register Ident *ident;
{
	register int i, low, high, com;

	low = 0;
	high = ntypetab - 1;
	while(low <= high) {
		i = (low + high) / 2;
		if ((com = strcmp(ident->type, typetab[i].type)) == 0)
			return(i);
		if(com > 0)
			low = i + 1;
		else
			high = i - 1;
	}
	return(-1);
}

static
addnet(net, list)
register Net net;
register NetList *list;
{
	register int i;

	if(list->list == NULL) {
		if((list->list = (Net *)malloc(NNETS * sizeof(Net))) == NULL) {
			fprintf(stderr, "%s: malloc failed\n",
			 myident.progname);
			exit(1);
		}
		list->ptr = list->list;
		list->count = 0;
		list->size = NNETS;
	}
	if(list->count >= list->size) {
		if((list->list = (Net *)realloc(list->list, (list->size +=
		  NNETS) * sizeof(Net))) == NULL) {
			fprintf(stderr, "%s: realloc failed\n",
			 myident.progname);
			exit(1);
		}
		list->ptr = list->list + list->count;
	}
	*list->ptr++ = net;
	list->count++;
}

static
findnet(net, list)
register Net net;
register NetList *list;
{
	register int i, low, high;

	low = 0;
	high = list->count - 1;
	while(low <= high) {
		i = (low + high) / 2;
		if (net == list->list[i])
			return(TRUE);
		if(net > list->list[i])
			low = i + 1;
		else
			high = i - 1;
	}
	return(FALSE);
}

static char *
parse(cp, ident)
register char *cp;
register Ident *ident;
{
	register char *p, *q;

	while(isspace(*cp))
		cp++;
	ident->progname = cp;
	if(!(cp = index(cp, '.')))
		return(NULL);
	*cp++ = 0;
	ident->nbpname = cp;
	if(!(cp = index(cp, '.')))
		return(NULL);
	*cp++ = 0;
	ident->type = cp;
	while(*cp && !isspace(*cp))
		cp++;
	if(*cp)
		*cp++ = 0;
	ident->flags = 0;
	if(*ident->progname == 0 || strcmp(ident->progname, anystr) == 0)
		ident->flags |= ANYPROGNAME;
	else
		lowercase(ident->progname);
	if(*ident->nbpname == 0 || strcmp(ident->nbpname, anystr) == 0)
		ident->flags |= ANYNBPNAME;
	else
		lowercase(ident->nbpname);
	return(cp);
}

initauthenticate(progname, nbpname)
char *progname, *nbpname;
{
	register FILE *fp;
	register int i, l;
	register char *cp;
	char buf[BUFSIZ];
	Ident ident;

	if(cp = rindex(progname, '/'))
		cp++;
	else
		cp = progname;
	i = strlen(cp);
	if((myident.progname = malloc(i + strlen(nbpname) + 2)) == NULL) {
		fprintf(stderr, "%s: malloc failed\n", cp);
		exit(1);
	}
	strcpy(myident.progname, cp);
	lowercase(myident.progname);
	myident.nbpname = myident.progname + i + 1;
	strcpy(myident.nbpname, nbpname);
	lowercase(myident.nbpname);
	myident.flags = 0;
	if((fp = fopen(authconfig, "r")) == NULL)
		return;
	ntypetab = 0;
	while(typetab[ntypetab].type)
		ntypetab++;
	l = 0;
	while(fgets(buf, BUFSIZ, fp)) {
		l++;
		if(*buf == '#')
			continue;
		if((cp = parse(buf, &ident)) == NULL) {
			fprintf(stderr, "%s: %s: Syntax error, line %d\n",
			 myident.progname, authconfig, l);
			continue;
		}
		if(compareident(&ident, &myident) != 0)
			continue;
		if((i = findtype(&ident)) < 0) {
			fprintf(stderr, "%s: %s: Unknown type, line %d\n",
			 myident.progname, authconfig, l);
			continue;
		}
		(*typetab[i].func)(cp, myident.progname, authconfig, l);
	}
	fclose(fp);
	if(inlist.list)
		qsort((char *)inlist.list, inlist.count, sizeof(Net),
		 comparenet);
	if(outlist.list)
		qsort((char *)outlist.list, outlist.count, sizeof(Net),
		 comparenet);
}

static void
innet(cp, p, a, l)
register char *cp;
char *p, *a;
int l;
{
	register char *ep;
	register int save;
	unsigned n1, n2;

	while(isspace(*cp))
		cp++;
	if(*cp == CLEARSYMBOL) {
		cp++;
		if(inlist.list) {
			free(inlist.list);
			inlist.list = NULL;
			doauthenticate &= ~DOINNET;
		}
	}
	while(isspace(*cp))
		cp++;
	if(*cp == 0)
		return;
	while(*cp) {
		ep = cp;
		while(isdigit(*ep) || *ep == '.')
			ep++;
		if(*ep && !issep(*ep)) {
errinzone:
			fprintf(stderr,
			 "%s: %s: innet: Syntax error, line %d\n",
			 p, a, l);
			exit(1);
		}
		save = *ep;
		*ep = 0;
		if(sscanf(cp, "%u.%u", &n1, &n2) != 2 || n1 > 255 || n2 > 255)
			goto errinzone;
		addnet((n1 << 8) + n2, &inlist);
		cp = ep;
		if(save)
			cp++;
		while(issep(*cp))
			cp++;
	}
	doauthenticate |= DOINNET;
}

static void
outnet(cp, p, a, l)
register char *cp;
char *p, *a;
int l;
{
	register char *ep;
	register int save;
	unsigned n1, n2;

	while(isspace(*cp))
		cp++;
	if(*cp == CLEARSYMBOL) {
		cp++;
		if(outlist.list) {
			free(outlist.list);
			outlist.list = NULL;
			doauthenticate &= ~DOOUTNET;
		}
	}
	while(isspace(*cp))
		cp++;
	if(*cp == 0)
		return;
	while(*cp) {
		ep = cp;
		while(isdigit(*ep) || *ep == '.')
			ep++;
		if(*ep && !issep(*ep)) {
erroutzone:
			fprintf(stderr,
			 "%s: %s: outnet: Syntax error, line %d\n",
			 p, a, l);
			exit(1);
		}
		save = *ep;
		*ep = 0;
		if(sscanf(cp, "%u.%u", &n1, &n2) != 2 || n1 > 255 || n2 > 255)
			goto erroutzone;
		addnet((n1 << 8) + n2, &outlist);
		cp = ep;
		if(save)
			cp++;
		while(issep(*cp))
			cp++;
	}
	doauthenticate |= DOOUTNET;
}

static void
network(cp, p, a, l)
register char *cp;
char *p, *a;
int l;
{
	register char *ep;

	while(isspace(*cp))
		cp++;
	ep = cp + strlen(cp);
	while(--ep >= cp && isspace(*ep))
		*ep = 0;
	if(ep < cp) {
		fprintf(stderr,
		 "%s: %s: network: Not type specified, line %d\n", p, a, l);
		exit(1);
	}
	strcpy(authtype, cp);
	donetwork = TRUE;
	doauthenticate |= DONETWORK;
}

static
netauth(net, node, authtype)
unsigned net, node;
char *authtype;
{
	register int i, nbpactive;
	register OSErr err;
	nbpProto abr;
	EntityName ent;
	AddrBlock addr;
	NBPTEntry buf;

	if(!(nbpactive = isNBPInited()))
		nbpInit();
	bzero((char *)&ent, sizeof(ent));
	sprintf((char *)ent.objStr.s, "%u.%u.%u", net >> 8, net & 0xff, node);
	strcpy((char *)ent.typeStr.s, authtype);
	strcpy((char *)ent.zoneStr.s, "*");
	abr.nbpEntityPtr = &ent;
	abr.nbpAddress.net = htons(net);
	abr.nbpAddress.node = node;
	abr.nbpAddress.skt = 0;
	abr.nbpRetransmitInfo.retransInterval = sectotick(1);
	abr.nbpRetransmitInfo.retransCount = 2;
	err = NBPConfirm(&abr, FALSE);
	if(!nbpactive)
		nbpShutdown();
	return(err == noErr || err == nbpConfDiff);
}

authenticate(net, node)
unsigned net, node;
{
	if(!doauthenticate)
		return(TRUE);
	if(inlist.list && !findnet(net, &inlist))
		return(FALSE);
	if(outlist.list && findnet(net, &outlist))
		return(FALSE);
	if(donetwork)
		return(netauth(net, node, authtype));
	return(TRUE);
}
#else  AUTHENTICATE
int auth_dummy_for_ld;	/* keep the loader and ranlib happy */
#endif AUTHENTICATE
