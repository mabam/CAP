/*
 * $Author: djh $ $Date: 1995/06/26 06:06:02 $
 * $Header: /local/mulga/mac/src/cap60/contrib/RCS/aufsmkusr.c,v 2.1 1995/06/26 06:06:02 djh Rel djh $
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
 * aufsmkusr - modify or create a new .afppass file
 *
 * usage:	aufsmkusr
 *		aufsmkusr user1 ...
 *		aufsmkusr -f batchfile
 *
 * The .afppass file stores the values for minimum password
 * length, maximum login failures, current login failures,
 * password expiry date and the user's AUFS password.
 *
 * It is encrypted with the global key set with aufsmkkey.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>

#include <netat/appletalk.h>
#include <netat/afppass.h>

#ifdef DISTRIB_PASSWDS

char *linep;
char *progname;
char *batchfile = NULL;

main(argc, argv)
int argc;
char *argv[];
{
  int c;
  char *cp;
  char user[10];
  extern char *optarg;
  extern int optind, opterr;
  int aufsmkbatch(), aufsmkuser();

  opterr = 0;
  progname = argv[0];

  if (geteuid() != 0) {
    fprintf(stderr, "%s: Permission Denied.\n", progname);
    exit(1);
  }

  /*
   * get global key parameters
   *
   */
  if (afpdp_init(AFP_DISTPW_FILE) < 0) {
    fprintf(stderr, "%s: can't get key from %s\n", progname, AFP_DISTPW_FILE);
    exit(2);
  }

  /*
   * process command line options
   *
   */
  while ((c = getopt(argc, argv, "f:")) != -1) {
    switch (c) {
      case 'f':
	batchfile = optarg;
        break;
      default:
	fprintf(stderr, "usage: aufsmkusr [-f file] [user ...]\n");
        exit(1);
        break;
    }
  }

  /*
   * process users in batch file
   *
   */
  if (batchfile != NULL) {
    (void)aufsmkbatch(batchfile);
    exit(0);
  }

  /*
   * process users in argument list
   *
   */
  if (optind < argc) {
    for ( ; optind < argc; optind++) {
      fprintf(stderr, "\nSetting AUFS password for %s\n", argv[optind]);
      (void)aufsmkuser(argv[optind]);
    }
    exit(0);
  }

  /*
   * do single users 
   *
   */
  printf("AUFS user: ");
  fgets(user, sizeof(user), stdin);
  if ((cp = (char *)index(user, '\n')) != NULL)
    *cp = '\0';

  if (user[0] == '\0')
    exit(0);

  if (aufsmkuser(user) < 0)
    exit(3);

  exit(0);
}

/*
 * make password file for 'user'
 *
 */

int
aufsmkuser(user)
char *user;
{
  char abuf[80], *cp;
  char pass1[10], pass2[10];
  struct passwd *pw, *getpwnam();
  extern struct afppass *afp_glob;
  time_t now, when, then, afpdp_gdat();
  struct afppass afppass, *afp, *afpdp_read();
  int afpdp_init(), afpdp_gnum(), afpdp_writ();
  void print_date();

  if ((pw = getpwnam(user)) == NULL) {
    fprintf(stderr, "%s: no such user: \"%s\"\n", progname, user);
    return(-1);
  }

  bzero((char *)&afppass, sizeof(struct afppass));

  /*
   * get current values or set defaults
   *
   */
  if ((afp = afpdp_read(user, pw->pw_uid, pw->pw_dir)) != NULL)
    bcopy((char *)afp, (char *)&afppass, sizeof(struct afppass));
  else
    bcopy((char *)afp_glob, (char *)&afppass, 8); /* not password */

  /*
   * minimum password length 0 - 8 (0 to disable)
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

  /*
   * current login attempt failures
   *
   */
  if (afppass.afp_numattempt > 0) {
    printf("User \"%s\" has %d failed login attempt%s.\n", user,
      afppass.afp_numattempt, (afppass.afp_numattempt== 1) ? "" : "s");
    printf("Reset number of failed login attempts: [%d] ? ",
      afppass.afp_numattempt);
    afppass.afp_numattempt = (u_char)afpdp_gnum(afppass.afp_numattempt, 255);
  }

  /*
   * make sure user afppass not period
   * (except if disabled)
   *
   */
  time(&now);
  when = ntohl(afppass.afp_expires);
  if (when <= SECS_10_YRS && when != 0) {
    afppass.afp_expires = htonl(when+now);
    when = ntohl(afppass.afp_expires);
  }

  /*
   * expiry date (0 to disable)
   *
   */
  print_date(when);
  printf("Password Expires (NNd or NNm or YY/MM/DD [HH:MM:SS]): ? ");
  if ((then = afpdp_gdat()) != 0xffffffff) {
    if (then <= SECS_10_YRS && then != 0)
      then += now;
    afppass.afp_expires = htonl(then);
    when = then;
  }
  print_date(when);

  then = ntohl(afp_glob->afp_expires);
  if (then > SECS_10_YRS && when > then)
    printf("WARNING: Global expiry date is %s", ctime(&then));

  /*
   * user password, up to 8 characters
   *
   */
  if (*afppass.afp_password) {
    printf("Change %s's Password (y/n): [n] ? ", user);
    fgets(abuf, sizeof(abuf), stdin);
    if (abuf[0] == 'y' || abuf[0] == 'Y')
      afppass.afp_password[0] = '\0';
  }
  
  while (*afppass.afp_password == '\0') {
    strcpy(pass1, (char *)getpass("User Password: "));
    if (strlen(pass1) < afppass.afp_minmpwlen) {
      printf("Password is shorter than minimum length (%d)\n");
      continue;
    }
    strcpy(pass2, (char *)getpass("Reenter User Password: "));
    if (strcmp(pass1, pass2) != 0) {
      printf("Password mismatch!\n");
      continue;
    }
    strcpy(afppass.afp_password, pass1);
  }

  /*
   * reset defaults and write
   *
   */
  afppass.afp_magic = AFPDP_MAGIC;

  if (afpdp_writ(user, pw->pw_uid, pw->pw_dir, &afppass) < 0) {
    fprintf(stderr, "%s: failed to set AUFS password (same as UNIX ?)\n",
      progname);
    return(-1);
  }

  return(0);
}

/*
 * handle bulk batch file
 *
 * each line is expected to be of the format:  user "password"
 * passwords containing spaces must be enclosed in double quotes
 *
 * NB: the expiry date is set to now. This requires the user to change
 * their password when they first login.
 *
 */

int
aufsmkbatch(file)
char *file;
{
  time_t now;
  struct stat buf;
  FILE *fp, *fopen();
  struct afppass afppass;
  extern struct afppass *afp_glob;
  struct passwd *pw, *getpwnam();
  char *cp, line[96], user[32];
  int afpdp_writ();
  void getfield();

  if (stat(file, &buf) >= 0) {
    if ((buf.st_mode&0777) != 0600) {
      fprintf(stderr, "WARNING: %s is mode %0o\n", file, buf.st_mode&0777);
      exit(1);
    }
  }

  bzero((char *)&afppass, sizeof(struct afppass));
  bcopy((char *)afp_glob, (char *)&afppass, 8); /* not password */

  if ((fp = fopen(file, "r")) == NULL) {
    perror(file);
    return(-1);
  }

  time(&now);

  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == '#' || line[0] == '\n')
      continue;
    if ((cp = (char *)index(line, '\n')) != NULL)
      *cp = '\0';

    linep = line;
    getfield(user, sizeof(user), 0);
    getfield(afppass.afp_password, sizeof(afppass.afp_password), 0);

    printf("User \"%s\" - ", user);

    /*
     * user exists ?
     *
     */
    if ((pw = getpwnam(user)) == NULL) {
      printf("does not exist - continuing\n");
      continue;
    }

    /*
     * and has a passwod
     *
     */
    if (afppass.afp_password[0] == '\0') {
      printf("has no password set - continuing\n");
      continue;
    }

    /*
     * which expires NOW
     *
     */
    afppass.afp_expires = htonl(now);

    /*
     * then set defaults and write
     *
     */
    afppass.afp_numattempt = 0;
    afppass.afp_magic = AFPDP_MAGIC;
    if (afpdp_writ(user, pw->pw_uid, pw->pw_dir, &afppass) < 0) {
      printf("can't create .afppass file - continuing\n");
      continue;
    }

    printf("OK\n");
  }
  
  (void)fclose(fp);

  return(0);
}

/*
 * output date string
 *
 */

void
print_date(when)
time_t when;
{
  time_t now;

  time(&now);

  if (when < SECS_10_YRS) {
    if (when == 0)
      printf("Password Expiry disabled.\n");
    else
      printf("Password Expiry period %d day%s.\n",
	when/(SECS_IN_DAY), (when/(SECS_IN_DAY) == 1) ? "" : "s");
  } else {
    if (when < now)
      printf("Warning, expiry date has already passed\n");
    printf("Password Expires on %s", ctime(&when));
  }

  return;
}

/*
 * Get next field from 'line' buffer into 'str'.  'linep' is the 
 * pointer to current position.
 *
 * Fields are white space separated, except within quoted strings.
 * If 'quote' is true the quotes of such a string are retained, otherwise
 * they are stripped.  Quotes are included in strings by doubling them or
 * escaping with '\'.
 *
 */

void
getfield(str, len, quote)
char *str;
int len, quote;
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
	      fprintf(stderr, "string truncated: %s\n", str);
	      if (quote)
	        *cp++ = term;
	      *cp = 0;
	      linep = lp;
	      return;
	    }
	  }
	  if (!*lp)
	    fprintf(stderr,"unterminated string: %s", str);
	  else {
	    lp++;	/* skip the terminator */

	    if (*lp && *lp != ' ' && *lp != '\t' && *lp != '#') {
	      fprintf(stderr, "garbage after string: %s", str);
	      while (*lp && *lp != ' ' && *lp != '\t' && *lp != '#')
	        lp++;
	    }
	  }
	  if (quote)
	    *cp++ = term;
	} else {
	  while (*lp && *lp != ' ' && *lp != '\t' && *lp != '#') {
	    *cp++ = *lp++;
	    if (--len <= 0) {
	      fprintf(stderr, "string truncated: %s\n", str);
	      break;
	    }
	  }
	}
	*cp = 0;
	linep = lp;

	return;
}
#else  /* DISTRIB_PASSWDS */
main()
{
  printf("CAP not compiled with DISTRIB_PASSWDS\n");
}
#endif /* DISTRIB_PASSWDS */
