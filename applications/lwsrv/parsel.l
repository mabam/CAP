%%
\"[^"]*\"	{
		  register int c;

#ifdef __cplusplus
		  unput(c = yyinput());
#else
		  unput(c = input());
#endif
		  yytext[--yyleng] = 0;
		  if (c != '"') {
		    yylval.string = strdup(yytext + 1);
		    return(STRING);
		  }
		  yymore();
		}
'[^']*'		{
		  register int c;

#ifdef __cplusplus
		  unput(c = yyinput());
#else
		  unput(c = input());
#endif
		  yytext[--yyleng] = 0;
		  if (c != '\'') {
		    yylval.string = strdup(yytext + 1);
		    return(STRING);
		  }
		  yymore();
		}
","		{
		  return(COMMA);
		}
";"		{
		  return(EOS);
		}
"="		{
		  return(EQUAL);
		}
[{(]		{
		  return(LPAREN);
		}
[})]		{
		  return(RPAREN);
		}
[ \t\n]+	{
		  /* just ignore whitespace */
		}
"#"[^\n]*\n	{
		  /* just ignore comments */
		}
[^ \t\n,;=(){}"'#]+	{
		  static int searchkeywords();
		  return(searchkeywords(yytext, &yylval.string));
		}
%%
#undef input
#undef unput

static unsigned char backbuf[256];
static unsigned char *backp = backbuf;
static FILE *infp;
static List *keylist;
static struct keywordbuf {
  char *key;
  char *sval;
  int ival;
} keywordbuf[] = {
  {"-A", "-A", DSC},
  {"-C", "-C", LPRCMD},
#ifdef LPRARGS
  {"-L", "-L", LPRARGUMENT},
#endif LPRARGS
  {"-N", "-N", NOCOLLECT},
#ifdef PASS_THRU
  {"-P", "-P", PASSTHRU},
#endif PASS_THRU
#ifdef NeXT
  {"-R", "-R", NEXT},
#endif NeXT
  {"-S", "-S", SINGLEFORK},
  {"-T", "-T", TRANSCRIPT},
#ifdef LWSRV_AUFS_SECURITY
  {"-X", "-X", AUFSSECURITY},
#endif LWSRV_AUFS_SECURITY
#ifdef LW_TYPE
  {"-Y", "-Y", ATTYPE},
#endif LW_TYPE
  {"-a", "-a", DIRDICT},
  {"-d", "-d", DEBUG},
  {"-e", "-e", EEXEC},
  {"-f", "-f", FONTFILE},
  {"-h", "-h", BANNER},
  {"-k", "-k", CHECKSUM},
  {"-l", "-l", LOGFILE},
  {"-p", "-p", PRINTERQUEUE},
  {"-q", "-q", QUERYFILE},
  {"-r", "-r", KEEPFILE},
  {"-t", "-t", TRACEFILE},
  {"-v", "-v", VERBOSE},
  {"alloweexec", "-e", EEXEC},
#ifdef LW_TYPE
  {"appletalktype", "-Y", ATTYPE},
#endif LW_TYPE
#ifdef LWSRV_AUFS_SECURITY
  {"aufssecurity", "-X", AUFSSECURITY},
#endif LWSRV_AUFS_SECURITY
  {"debug", "-d", DEBUG},
  {"deniedmessage", "DeniedMessage", DENIEDMESSAGE},
  {"dontcollect", "-N", NOCOLLECT},
  {"dsc", "-A", DSC},
  {"encoding", "encoding", ENCODING},
  {"featurequery", "FeatureQuery", FEATUREQUERY},
  {"file", "file", FILERES},
  {"font", "font", FONT},
  {"fontfile", "-f", FONTFILE},
  {"form", "form", FORM},
  {"include", "-", INCLUDE},
  {"keepspoolfile", "-r", KEEPFILE},
  {"library", NULL, LIBRARY},
  {"logfile", "-l", LOGFILE},
#ifdef LPRARGS
  {"lprargument", "-L", LPRARGUMENT},
#endif LPRARGS
  {"lprcommand", "-C", LPRCMD},
#ifdef NeXT
  {"nextresolution", "-R", NEXT},
#endif NeXT
  {"nochecksum", "-k", CHECKSUM},
  {"options", NULL, OPTIONS},
#ifdef PASS_THRU
  {"passthru", "-P", PASSTHRU},
#endif PASS_THRU
  {"pattern", "pattern", PATTERN},
  {"printerquery", "PrinterQuery", PRINTERQUERY},
  {"printerqueue", "-p", PRINTERQUEUE},
  {"procsetdir", "-a", DIRDICT},
  {"query", "Query", QUERY},
  {"queryfile", "-q", QUERYFILE},
  {"singlefork", "-S", SINGLEFORK},
  {"suppressbanner", "-h", BANNER},
  {"tracefile", "-t", TRACEFILE},
  {"transcriptoption", "-T", TRANSCRIPT},
  {"verbose", "-v", VERBOSE},
  {"vmstatus", "VMStatus", VMSTATUS},
  {NULL, NULL, 0},
};

void
initkeyword(fp)
FILE *fp;
{
  register List *lp;
  register struct keywordbuf *kp;

  lp = CreateList();
  for (kp = keywordbuf; kp->key; kp++)
    AddToList(lp, (void *)kp);
  keylist = lp;
  infp = fp;
}

static unsigned char input_linebuf[BUFSIZ] = "";
static unsigned char *input_lineptr = input_linebuf;

#ifndef YY_INPUT
static int
input()
{
  register int c;

  if (backp > backbuf)
    return(*--backp);
  if (*input_lineptr) {
    /* putc(*input_lineptr, stderr); /* DEBUG */
    return(*input_lineptr++);
  }
  if (fgets((char *)input_linebuf, BUFSIZ, infp) == NULL)
    return(0);
  linenum++;
  input_lineptr = input_linebuf;
  /* putc(*input_lineptr, stderr); /* DEBUG */
  return(*input_lineptr++);
}
#endif

static int
keycmp(kp, cp)
struct keywordbuf *kp;
char *cp;
{
  return(strcmp(kp->key, cp));
}

static int
searchkeywords(str, outstr)
char *str, **outstr;
{
  register struct keywordbuf *kp;
  register char *cp;
  char buf[BUFSIZ];
  static int keycmp();

  if (*str != '-') {
    strcpy(buf, str);
    for (cp = buf; *cp; cp++)
      if (isupper(*cp))
	*cp = tolower(*cp);
    cp = buf;
  } else
    cp = str;
  if (kp = (struct keywordbuf *)SearchList(keylist, cp, keycmp)) {
    *outstr = kp->sval;
    return(kp->ival);
  }
  *outstr = strdup(str);
  return(STRING);
}

static int
unput(c)
int c;
{
  *backp++ = c;
}
