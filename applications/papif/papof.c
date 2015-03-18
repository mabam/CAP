/*
 * Basic output filter for the 4.2 spooling system
 * 
 * Write out the banner (the input) into .banner for the input filter.
 * The input filter can then print it out if it wants.
 *
 * Note: Do a sigstop on self when we see ^Y^A which denotes end of job.
 * exiting is the WRONG thing to do at this point.
 *
 * Copyright (c) 1985, 1987 by The Trustees of Columbia University in the City
 *  of New York
 *
 * Author: Charlie C. Kim
 *
 */

#include <stdio.h>
#include <signal.h>
#include <netat/sysvcompat.h>

#ifdef USESTRINGDOTH
# include <string.h>
#else  USESTRINGDOTH
# include <strings.h>
#endif USESTRINGDOTH

#ifdef BANNERFIRST
# ifndef BANNER
#  define BANNER
# endif
#endif

#ifdef BANNERLAST
# ifndef BANNER
#  define BANNER
# endif
#endif

#ifdef CHARGEBANNER
# ifndef BANNER
#  define BANNER
# endif
#endif

#ifdef BANNERFILE
# ifndef BANNER
#  define BANNER
# endif
#endif
#ifndef BANNERFILE
# define BANNERFILE ".banner"
#endif

#ifdef BANNER
#ifdef PSBANNER
char bannerpro[] = ".banner.pro";
#endif PSBANNER
#endif BANNER

FILE *bannerfile;
char *bannerfname;

char buf[BUFSIZ];

main()
{
  int c, cl, i;
  int dosusp;
#ifdef BANNER
  char *getenv();
#ifdef PSBANNER
  int psstart, dopsbanner;
#endif PSBANNER
#endif BANNER

  while (1) {
#ifdef BANNER
    if ((bannerfname = getenv("BANNER")) == NULL)
      bannerfname = BANNERFILE;
    if ((bannerfile = fopen(bannerfname, "w")) == NULL) {
      perror("Can't open .banner");
      exit(8);
    }
#ifdef PSBANNER
    psstart = dopsbanner = 0;
#else  PSBANNER
    psbannerstart(bannerfile);
#endif PSBANNER
#endif BANNER
    cl = -1, c = -1, dosusp = 0;
    do {
#ifdef BANNER
      for ( i = 0; i < BUFSIZ ; i++) {
#endif
	cl = c;
	c = getchar();
#ifdef BANNER
	buf[i] = c;
#endif
	if (cl == '\031' && c == '\01') {
	  dosusp = 1;
	  break;
	}
#ifdef BANNER
	if (c == EOF)
	  break;
	if (c == '\n')
	  break;
      }
      buf[i] = '\0';
      if (dosusp) 
	break;
#ifdef PSBANNER
      if (!psstart) {
	psstart++;
	if (!(dopsbanner = ps_banner(bannerfile, buf)))
	  psbannerstart(bannerfile);
      }
      if (!dopsbanner && (c != EOF || i != 0))
	psbannerline(bannerfile,buf);
#else  PSBANNER
      if (c != EOF || i != 0)
	psbannerline(bannerfile,buf);
#endif PSBANNER
#endif BANNER
    } while (c != EOF && !dosusp);
#ifdef BANNER
#ifdef PSBANNER
    if (!dopsbanner)
      psbannerend(bannerfile);
#else  PSBANNER
    psbannerend(bannerfile);
#endif PSBANNER
    fclose(bannerfile);	/* close off file here - end of job */
#endif BANNER
    if (c == EOF)
      break;
#ifdef DEBUG
    fprintf(stderr,"Waiting for next job...");
#endif DEBUG
#ifdef SIGSTOP
    kill(getpid(), SIGSTOP);
#else SIGSTOP
    pause();	/* KLUDGE ALERT */
#endif SIGSTOP
  }
}

#ifdef BANNER
psbannerstart(fd)
FILE *fd;
{
  fputs("%!\n", fd);
  fputs("/fs 8 def\n", fd);
  fputs("/Courier findfont fs scalefont setfont\n", fd);
  fputs("/vpos 72 10 mul def\n", fd); /*  at 10 inches .5 inch margin */
  fputs("/LS {36 vpos moveto show /vpos vpos fs sub def} def\n", fd);
}

psbannerline(fd,line)
FILE *fd;
unsigned char *line;
{
  int l = strlen((char *)line);
  static char spaces[] = "        ";
  int i, pos;
  unsigned char c;

  if (line[0] == '\f')
    return;
  putc('(', fd);
  for (i = 0,pos=0; i < l ; pos++, i++) {
    c = *line++;
    if (c != '\t')
      if (c < ' ' || c > '\177')
	c = '\267';
    switch (c) {
    case '(':
      fputs("\\(",fd);
      break;
    case ')':
      fputs("\\)",fd);
      break;
    case '\\':
      fputs("\\\\",fd);
      break;
    case '\t':
      fputs((pos%8) ? spaces+(pos % 8) : spaces, fd);
      pos += (8 - (pos % 8)) - 1;
      break;
    default:
      putc(c, fd);
      break;
    }
  }
  fputs(") LS\n", fd);
}

psbannerend(fd)
FILE *fd;
{
  fputs("showpage\n", fd);  
}

#ifdef PSBANNER
char *
topsstr(str)
register char *str;
{
  register char *cp;
  static char psbuf[BUFSIZ];

  for(cp = psbuf ; *str ; ) {
    if(*str == '(' || *str == ')' || *str == '\\')
      *cp++ = '\\';
    *cp++ = *str++;
  }
  *cp = 0;
  return(psbuf);
}

ps_banner(fd, cp)
FILE *fd;
char *cp;
{
  register char *up, *jp, *dp;
  register FILE *pro;
  register int i, n;
  char buf[BUFSIZ];

  if((pro = fopen(bannerpro, "r")) == NULL)
    return(0);
  jp = cp;
  for( ; ; ) {
    if((jp = index(jp, ' ')) == NULL) {
      fclose(pro);
      return(0);
    }
    if(strncmp(jp, "  Job: ", 7) == 0) {
      *jp = 0;
      jp += 7;
      break;
    }
    jp++;
  }
  if(up = rindex(cp, ':'))
    *up++ = 0;
  else {
    up = cp;
    cp = "";
  }
  dp = jp;
  for( ; ; ) {
    if((dp = index(dp, ' ')) == NULL) {
      fclose(pro);
      return(0);
    }
    if(strncmp(dp, "  Date: ", 8) == 0) {
      *dp = 0;
      dp += 8;
      break;
    }
    dp++;
  }
  while((i = fread(buf, 1, BUFSIZ, pro)) > 0)
    fwrite(buf, 1, i, fd);
  fclose(pro);
  fprintf(fd, "(%s)", topsstr(cp));
  fprintf(fd, "(%s)", topsstr(up));
  fprintf(fd, "(%s)", topsstr(jp));
  fprintf(fd, "(%s) P\n", topsstr(dp));
  return(1);
}
#endif PSBANNER
#endif BANNER
