%{
/*
 * parsey - UNIX AppleTalk spooling program: act as a laserwriter
 *   yacc configuration file parser
 *
 */
#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include "list.h"
#include "parse.h"

typedef struct NameVal {
  char *name;
  void *val;
} NameVal;

/*
 * Globals
 */

char *libraryfile = 0;
List *optionlist = 0;
List *printerlist = 0;

/*
 * Function typecasts
 */

static char *addnl();
static NameVal *CreateNameVal();

int strcmp();
char *strdup();

%}
%union {
  char *string;
  List *list;
  NameVal *nameval;
}
%token ATTYPE AUFSSECURITY BANNER CHECKSUM COMMA DEBUG DENIEDMESSAGE DIRDICT
%token DSC EEXEC ENCODING EOS EQUAL FEATUREQUERY FILERES FONT FONTFILE FORM
%token INCLUDE KEEPFILE LIBRARY LOGFILE LPAREN LPRARGUMENT LPRCMD NEXT
%token NOCOLLECT OPTIONS PASSTHRU PATTERN PRINTERQUERY PRINTERQUEUE QUERY
%token QUERYFILE RPAREN SINGLEFORK STRING TRACEFILE TRANSCRIPT VERBOSE VMSTATUS

%type <string> ATTYPE AUFSSECURITY BANNER CHECKSUM DEBUG DENIEDMESSAGE DIRDICT
%type <string> DSC EEXEC ENCODING FEATUREQUERY FILERES FONT FONTFILE FORM
%type <string> INCLUDE KEEPFILE LOGFILE LPRARGUMENT LPRCMD NEXT NOCOLLECT
%type <string> PASSTHRU PATTERN PRINTERQUERY PRINTERQUEUE QUERY QUERYFILE
%type <string> SINGLEFORK STRING TRACEFILE TRANSCRIPT VERBOSE VMSTATUS
%type <string> lwsrvnoarg lwsrvwitharg optnoarg optwitharg queryargtype
%type <string> querynoargtype querytype shortresource special
%type <list> block clause list optionblock optionclause optlist stringcomma
%type <list> stringlist stringnlcomma stringnllist stringnlset stringset
%type <nameval> aprinter include lwsrvoption optstatement printeroption
%type <nameval> printerqueue query resource specialstrings statement

%start	configuration
%%
configuration	:	library options printers
		;

library		:	/* empty */
		|	LIBRARY EQUAL STRING EOS
			{
			  libraryfile = $3;
			}

options		:	/* empty */
		|	OPTIONS EQUAL optionblock
			{
			  optionlist = $3;
			}
		;

optionblock	:	optstatement
			{
			  register List *lp;

			  lp = CreateList();
			  AddToList(lp, (void *)$1->name);
			  AddToList(lp, (void *)$1->val);
			  free((char *)$1);
			  $$ = lp;
			}
		|	optionclause EOS
		;

optionclause	:	LPAREN optlist RPAREN
			{
			  $$ = $2;
			}
		;

optlist		:	optstatement
			{
			  register List *lp;

			  lp = CreateList();
			  AddToList(lp, (void *)$1->name);
			  AddToList(lp, (void *)$1->val);
			  free((char *)$1);
			  $$ = lp;
			}
		|	optlist optstatement
			{
			  AddToList($1, (void *)$2->name);
			  AddToList($1, (void *)$2->val);
			  free((char *)$2);
			  $$ = $1;
			}
		;

optstatement	:	lwsrvoption EOS
		|	printeroption EOS
		|	query EOS
		|	resource EOS
		|	specialstrings EOS
		;

lwsrvoption	:	lwsrvwitharg STRING
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		|	lwsrvnoarg
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = NULL;
			  $$ = np;
			}
		;

lwsrvwitharg	:	LOGFILE
		|	DEBUG
		|	AUFSSECURITY
		|	LPRCMD
		|	QUERYFILE
		;

lwsrvnoarg	:	VERBOSE
		|	SINGLEFORK
		;

printers	:	aprinter
			{
			  printerlist = CreateList();
			  AddToList(printerlist, (void *)$1->name);
			  AddToList(printerlist, (void *)$1->val);
			  free((char *)$1);
			}
		|	printers aprinter
			{
			  AddToList(printerlist, (void *)$2->name);
			  AddToList(printerlist, (void *)$2->val);
			  free((char *)$2);
			}
		;

aprinter	:	STRING EQUAL block
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$3;
			  $$ = np;
			}
		;

block		:	statement
			{
			  register List *lp;

			  lp = CreateList();
			  AddToList(lp, (void *)$1->name);
			  AddToList(lp, (void *)$1->val);
			  free((char *)$1);
			  $$ = lp;
			}
		|	clause EOS
		;

clause		:	LPAREN list RPAREN
			{
			  $$ = $2;
			}
		;

list		:	statement
			{
			  register List *lp;

			  lp = CreateList();
			  AddToList(lp, (void *)$1->name);
			  AddToList(lp, (void *)$1->val);
			  free((char *)$1);
			  $$ = lp;
			}
		|	list statement
			{
			  AddToList($1, (void *)$2->name);
			  AddToList($1, (void *)$2->val);
			  free((char *)$2);
			  $$ = $1;
			}
		;

statement	:	printerqueue EOS
		|	printeroption EOS
		|	include EOS
		|	query EOS
		|	resource EOS
		|	specialstrings EOS
		;

printerqueue	:	PRINTERQUEUE STRING
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		;

printeroption	:	optwitharg STRING
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		|	optnoarg
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = NULL;
			  $$ = np;
			}
		;

optwitharg	:	DIRDICT
		|	FONTFILE
		|	DSC
		|	TRANSCRIPT
		|	NEXT
		|	ATTYPE
		|	LPRARGUMENT
		|	TRACEFILE
		;

optnoarg	:	EEXEC
		|	BANNER
		|	CHECKSUM
		|	NOCOLLECT
		|	PASSTHRU
		|	KEEPFILE
		;

include		:	INCLUDE STRING
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		;

query		:	querytype stringnlset
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		;

querytype	:	queryargtype STRING
			{
			  register char *cp;

			  if((cp = (char *)malloc(strlen($1) + strlen($2) + 2))
			   == NULL)
			    errorexit(1, "parse: Out of memory\n");
			  sprintf(cp, "%s\t%s", $1, $2);
			  free($2);
			  $$ = cp;
			}
		|	querynoargtype
		;

queryargtype	:	FEATUREQUERY
		|	QUERY
		;

querynoargtype	:	PRINTERQUERY
		|	VMSTATUS
		;

resource	:	shortresource stringset
			{
			  register NameVal *np;

			  SortList($2, StringSortItemCmp);
			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		;

shortresource	:	FONT
		|	FILERES
		|	PATTERN
		|	FORM
		|	ENCODING
		;

stringnlset	:	STRING
			{
			  register List *list;

			  list = CreateList();
			  AddToList(list, (void *)addnl($1));
			  $$ = list;
			}
		|	LPAREN stringnlcomma RPAREN
			{
			  $$ = $2;
			}
		;

stringnlcomma	:	stringnllist
		|	stringnllist COMMA
		;

stringnllist	:	STRING
			{
			  register List *list;

			  list = CreateList();
			  AddToList(list, (void *)addnl($1));
			  $$ = list;
			}
		|	stringnllist COMMA STRING
			{
			  AddToList($1, (void *)addnl($3));
			  $$ = $1;
			}
		;

stringset	:	STRING
			{
			  register List *list;

			  list = CreateList();
			  AddToList(list, (void *)$1);
			  $$ = list;
			}
		|	LPAREN stringcomma RPAREN
			{
			  $$ = $2;
			}
		;

stringcomma	:	stringlist
		|	stringlist COMMA
		;

stringlist	:	STRING
			{
			  register List *list;

			  list = CreateList();
			  AddToList(list, (void *)$1);
			  $$ = list;
			}
		|	stringlist COMMA STRING
			{
			  AddToList($1, (void *)$3);
			  $$ = $1;
			}
		;

specialstrings	:	special stringnlset
			{
			  register NameVal *np;

			  np = CreateNameVal();
			  np->name = $1;
			  np->val = (void *)$2;
			  $$ = np;
			}
		;

special		:	DENIEDMESSAGE
		;

%%
static char strbuf[256];
int linenum = 0;

static void _Include(/* KVTree **kp, KVTree *ip */);

#include "lex.yy.c"

static NameVal *
CreateNameVal()
{
  register NameVal *np;

  if((np = (NameVal *)malloc(sizeof(NameVal))) == NULL)
    errorexit(1, "CreateNameVal: Out of memory\n");
  return(np);
}

static char *
addnl(str)
char *str;
{
  register char *cp;

  if((cp = (char *)malloc(strlen(str) + 2)) == NULL)
    errorexit(1, "addnl:Out of memory\n");
  strcpy(cp, str);
  strcat(cp, "\n");
  free(str);
  return(cp);
}

yyerror(s)
char *s;
{
  register int i, j;
  register unsigned char *tp, *fp, *cp;
  unsigned char buf[BUFSIZ];

  i = 0;
  for(tp = buf, fp = input_linebuf; fp < input_lineptr; fp++) {
    if(*fp == '\t') {
      i += (j = 8 - (i % 8));
      while(j-- > 0)
	*tp++ = ' ';
      continue;
    }
    *tp++ = *fp;
    i++;
  }
  cp = tp;
  while(*cp++ = *fp++);
  fprintf(stderr, "*** %s in configuration file on or before line %d\n", s,
   linenum);
  fputs((char *)buf, stderr);
  if((i = (tp - buf) - 2) < 0)
    i = 0;
  while(i-- > 0)
    putc('-', stderr);
  fputs("^\n", stderr);
}
