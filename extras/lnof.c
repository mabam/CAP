/*
 * Basic output filter for the 4.2 spooling system
 * 
 * Write out the banner (the input) into .banner for the input filter.
 *
 * Note: Do a sigstop on self when we see ^Y^A which denotes end of job.
 * exiting is the WRONG thing to do at this point.
 *
 * Copyright (c) 1985 by The Trustees of Columbia University in the City
 *  of New York
 *
 * Author: Charlie C. Kim
*/

#include <stdio.h>
#include <signal.h>

FILE *bannerfile;

main()
{
  char c;
  while (1) {
    if ((bannerfile = fopen(".banner", "w")) < 0) {
      perror("Can't open .banner");
      exit(8);
    }
    while ((c=getchar()) != '\031' && c != EOF)
      putc(c,bannerfile);
    if (c==EOF) break;		/* should never happen(?) */
    if ((c=getchar()) == '\01') {
#ifdef DEBUG
      fprintf(stderr,"Waiting for next job...");
#endif DEBUG
      fclose(bannerfile);	/* close off file here - end of job */
      kill(getpid(), SIGSTOP);
    }
  }
}
