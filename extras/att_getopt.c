/*
 * Relay-Version: version B 2.10.3 4.3bsd-beta 6/6/85; site seismo.CSS.GOV
 * Posting-Version: version B 2.10.2 9/3/84; site panda.UUCP
 * Path: seismo!harvard!talcott!panda!sources-request
 * From: sources-request@panda.UUCP
 * Newsgroups: mod.sources
 * Subject: public domain AT&T getopt(3)
 * Message-ID: <1159@panda.UUCP>
 * Date: 3 Dec 85 15:22:25 GMT
 * Sender: jpn@panda.UUCP
 * Organization: IEEE/P1003 Portable Operating System Environment Committee
 * Lines: 138
 * Approved: jpn@panda.UUCP
 * 
 * Mod.sources:  Volume 3, Issue 58
 * Submitted by: seismo!ut-sally!jsq (John Quarterman, Moderator mod.std.unix)
 * 
 * [
 * There are two articles here, forwarded from mod.std.unix.  Also, the
 * getopt source code is NOT in shar format - you will have to hand
 * edit this file.       -   John P. Nelson,  moderator, mod.sources
 * ]
 * 
 * ************************
 * 
 * Newsgroups: mod.std.unix
 * Subject: public domain AT&T getopt source
 * Date: 3 Nov 85 19:34:15 GMT
 * 
 * Here's something you've all been waiting for:  the AT&T public domain
 * source for getopt(3).  It is the code which was given out at the 1985
 * UNIFORUM conference in Dallas.  I obtained it by electronic mail
 * directly from AT&T.  The people there assure me that it is indeed
 * in the public domain.
 * 
 * There is no manual page.  That is because the one they gave out at
 * UNIFORUM was slightly different from the current System V Release 2
 * manual page.  The difference apparently involved a note about the
 * famous rules 5 and 6, recommending using white space between an option
 * and its first argument, and not grouping options that have arguments.
 * Getopt itself is currently lenient about both of these things White
 * space is allowed, but not mandatory, and the last option in a group can
 * have an argument.  That particular version of the man page evidently
 * has no official existence, and my source at AT&T did not send a copy.
 * The current SVR2 man page reflects the actual behavor of this getopt.
 * However, I am not about to post a copy of anything licensed by AT&T.
 * 
 * I will submit this source to Berkeley as a bug fix.
 * 
 * I, personally, make no claims or guarantees of any kind about the
 * following source.  I did compile it to get some confidence that
 * it arrived whole, but beyond that you're on your own.
*/

/*LINTLIBRARY*/
#define NULL	0
#define EOF	(-1)
#define ERR(s, c)	if(opterr){\
	extern int strlen(), write();\
	char errbuf[2];\
	errbuf[0] = c; errbuf[1] = '\n';\
	(void) write(2, argv[0], (unsigned)strlen(argv[0]));\
	(void) write(2, s, (unsigned)strlen(s));\
	(void) write(2, errbuf, 2);}

extern int strcmp();
/* bsd based system will require index */
#define strchr index
extern char *strchr();

int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg;

int
getopt(argc, argv, opts)
int	argc;
char	**argv, *opts;
{
	static int sp = 1;
	register int c;
	register char *cp;

	if(sp == 1)
		if(optind >= argc ||
		   argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(EOF);
		else if(strcmp(argv[optind], "--") == NULL) {
			optind++;
			return(EOF);
		}
	optopt = c = argv[optind][sp];
	if(c == ':' || (cp=strchr(opts, c)) == NULL) {
		ERR(": illegal option -- ", c);
		if(argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		return('?');
	}
	if(*++cp == ':') {
		if(argv[optind][sp+1] != '\0')
			optarg = &argv[optind++][sp+1];
		else if(++optind >= argc) {
			ERR(": option requires an argument -- ", c);
			sp = 1;
			return('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if(argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return(c);
}
