/*
 * $Author: djh $ $Date: 1995/06/26 06:06:02 $
 * $Header: /local/mulga/mac/src/cap60/contrib/RCS/aufsmkkey.c,v 2.1 1995/06/26 06:06:02 djh Rel djh $
 * $Revision: 2.1 $
 *
 */

/*
 * CAP AFP 2.1 Distributed Passwords
 *
 * Copyright 1995 - The University of Melbourne. All rights reserved.
 * May be used only for CAP/AUFS authentication. Any other use
 * requires prior permission in writing from the copyright owner.
 *
 * djh@munnari.OZ.AU
 * June 1995
 *
 * aufsmkkey.c - modify or create a new global key file.
 *
 * usage:	aufsmkkey
 *
 * The global key file stores default values for minimum password
 * length, maximum login failures, password expiry period or date
 * and the global key used to encrypt ~user/.afppass files.
 *
 * Note: Changing the global key invalidates all of the passwords
 * of the existing user base.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>

#include <netat/appletalk.h>
#include <netat/afppass.h>

#ifdef DISTRIB_PASSWDS

main(argc, argv)
int argc;
char *argv[];
{
  struct afppass afppass;
  char abuf[80], *progname;
  char pass1[10], pass2[10];
  extern struct afppass *afp_glob;
  time_t when, then, afpdp_gdat();
  int afpdp_init(), afpdp_gnum(), afpdp_make();
  void print_date();

  progname = argv[0];

  if (geteuid() != 0) {
    fprintf(stderr, "%s: Permission Denied.\n", progname);
    exit(1);
  }

  /*
   * get global key parameters, if file already exists
   *
   */
  if (afpdp_init(AFP_DISTPW_FILE) < 0)
    bzero((char *)&afppass, sizeof(struct afppass));
  else
    bcopy((char *)afp_glob, (char *)&afppass, sizeof(struct afppass));

  /*
   * minimum password length (0 - 8) (0 to disable)
   *
   */
  printf("Minimum AUFS password length: [%d] ? ", afppass.afp_minmpwlen);
  afppass.afp_minmpwlen = (u_char)afpdp_gnum(afppass.afp_minmpwlen, KEYSIZE);

  /*
   * maximum failed logins (0 - 255) (0 to disable)
   *
   */
  printf("Maximum failed login attempts: [%d] ? ", afppass.afp_maxattempt);
  afppass.afp_maxattempt = (u_char)afpdp_gnum(afppass.afp_maxattempt, 255);

  when = ntohl(afppass.afp_expires);

  /*
   * expiry period (0 - 10 years) or expiry date (0 to disable)
   *
   */
  print_date(when);
  printf("Password Expires (NNd or NNm or YY/MM/DD [HH:MM:SS]): ? ");
  if ((then = afpdp_gdat()) != 0xffffffff) {
    afppass.afp_expires = htonl(then);
    when = then;
  }
  print_date(when);

  /*
   * global key, up to 8 characters
   *
   */
  if (*afppass.afp_password) {
    printf("Change Global Key (y/n): [n] ? ");
    fgets(abuf, sizeof(abuf), stdin);
    if (abuf[0] == 'y' || abuf[0] == 'Y')
      afppass.afp_password[0] = '\0';
  }
  
  while (*afppass.afp_password == '\0') {
    strcpy(pass1, (char *)getpass("Global Key: "));
    if (strlen(pass1) < MINKEYSIZE) {
      printf("Please use at least %d characters!\n", MINKEYSIZE);
      continue;
    }
    strcpy(pass2, (char *)getpass("Reenter Global Key: "));
    if (strcmp(pass1, pass2) != 0) {
      printf("Key Mismatch!\n");
      continue;
    }
    strcpy(afppass.afp_password, pass1);
  }

  /*
   * set defaults and write
   *
   */
  afppass.afp_numattempt = 0;
  afppass.afp_magic = AFPDP_MAGIC;

  if (afpdp_make(AFP_DISTPW_FILE, &afppass) < 0) {
    fprintf(stderr, "%s: failed to set global key\n", progname);
    exit(1);
  }

  exit(0);
}

void
print_date(when)
time_t when;
{
  time_t now;

  time(&now);

  if (when < SECS_10_YRS) {
    printf("Password Expiry period %d day%s%s.\n", when/(SECS_IN_DAY),
      (when/(SECS_IN_DAY) == 1) ? "" : "s", (when == 0) ? " (Disabled)" : "");
  } else {
    if (when < now)
      printf("Warning, expiry date has already passed\n");
    printf("Password Expires on %s", ctime(&when));
  }

  return;
}

#else  /* DISTRIB_PASSWDS */
main()
{
  printf("CAP not compiled with DISTRIB_PASSWDS\n");
}
#endif /* DISTRIB_PASSWDS */
