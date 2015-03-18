/* "$Author: djh $ $Date: 1995/08/30 00:53:28 $" */
/* "$Header: /local/mulga/mac/src/cap60/applications/lwsrv/RCS/parse.h,v 2.1 1995/08/30 00:53:28 djh Rel djh $" */
/* "$Revision: 2.1 $" */

/*
 * parse.h -  read a configuration file and parse the information
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#ifndef _PARSE_H_
#define	_PARSE_H_

typedef struct Location {
  int magic;
  int offset;
  int size;
} Location;

#define	KEYWORDSKEYSIZE		14
#define	OPTIONCHAR		'-'

#define	MagicNumber		0x4c77436f	/* 'LwCo' */
#define	StringVal(a)		(isOption(a) && !isSpecialOpt(a))
#define	ListVal(a)		(!isOption(a) || isSpecialOpt(a))
#define isOption(a)		(*(char *)(a) == OPTIONCHAR)
#define	isSpecialOpt(a)		(Option(a) && index(specialOpts, Option(a)))
#define	Option(a)		(((char *)(a))[1])

extern char datasuffix[];
extern char includename[];
extern char keywords_key[];
extern char *libraryfile;
extern List *optionlist;
extern List *printerlist;
extern KVTree **_printers;
extern char specialOpts[];
extern KVTree **thequery;

void configargs(/* char *dbname */);
void initkeyword(/* FILE *fp */);

#endif /* _PARSE_H_ */
