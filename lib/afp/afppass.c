/*
 * $Author: djh $ $Date: 1995/06/30 11:19:39 $
 * $Header: /mac/src/cap60/lib/afp/RCS/afppass.c,v 2.1 1995/06/30 11:19:39 djh Rel djh $
 * $Revision: 2.1 $
 *
 */

/*
 * AUFS Distributed Passwords
 *
 * Copyright 1995 - The University of Melbourne. All rights reserved.
 * May be used only for CAP/AUFS authentication. Any other use
 * requires prior permission in writing from the copyright owner.
 *
 * djh@munnari.OZ.AU
 * June 1995
 *
 * afppass.c - AUFS Distributed Password library routines.
 *
 * User passwords are normally stored in ~user/.afppass in DES encrypted
 * form. This file also contains values for password expiry date, minimum
 * password length, maximum failed login attempts and number of failed
 * login attempts.
 *
 * For greater security, the file must be owned by the user and be set to
 * mode AFP_DISTPW_MODE (usually 0600 or -rw-------), if this is not the
 * case, the file is deleted.
 *
 * The decryption key is stored in a global afppass (defaults to the
 * file /usr/local/lib/cap/afppass) which also contains default values
 * for expiry date, minimum password length and maximum failed attempts.
 * If this file is not owned by root and mode 0600 it will be removed.
 *
 * Notes:
 * 1. In the case of user home directories mounted via NFS, the files must
 * be set to mode 0644 (since root cannot read mode 0600 files on remote
 * filesystems). You can change the mode using the define
 * -DAFP_DISTPW_MODE=0644
 * 
 * 2. If you prefer to keep the .afppass files centrally, you can define
 * the path using the define -DAFP_DISTPW_PATH=\"/usr/local/lib/cap/upw\"
 *
 * 3. The decryption key for the global afppass is defined by AFP_DIST_PASS
 * Should be localized for each site, using -DAFP_DIST_PASS=\"password\".
 *
 * 4. AFP passwords can only be changed by the user with the AppleShare
 * workstation client or by the UNIX superuser using aufsmkusr.
 *
 * 5. User AFP passwords MUST NOT be identical to UNIX login passwords,
 * this restriction is enforced by the library routines.
 *
 */

#ifdef DISTRIB_PASSWDS

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <time.h>
#include <pwd.h>

#include <netat/appletalk.h>
#include <netat/afppass.h>

#ifdef NEEDFCNTLDOTH
#include <sys/fcntl.h>
#endif NEEDFCNTLDOTH

static struct afppass global;
struct afppass *afp_glob = NULL;

char hex[] = "0123456789ABCDEF";

/*
 * Initialise data structure
 *
 * must be called by root to get global password & settings
 * from the specified file (which must already exist).
 *
 */

int
afpdp_init(path)
char *path;
{
	int fd, len;
	struct stat buf;
	char abuf[AFPPDSIZE];
	void afpdp_decr();

	if (geteuid() != 0)
	  return(-1);

        bzero(&global, sizeof(struct afppass));

	if (stat(path, &buf) < 0)
	  return(-1);

	/*
	 * check mode, size and owner
	 *
	 */
	if ((buf.st_mode&0777) != 0600
	 || buf.st_size != AFPPDSIZE
	 || buf.st_uid != 0) {
	  unlink(path);
	  return(-1);
	}

        if ((fd = open(path, O_RDONLY, 0644)) < 0)
          return(fd);

        len = read(fd, abuf, sizeof(abuf));

        close(fd);

        if (len != sizeof(abuf))
          return(-1);

        /*
         * sanity check on file contents
         *
         */
        if (abuf[16] != '\n' || abuf[33] != '\n')
          return(-1);

        /*
         * decrypt each "line" into the structure
         * using global key
         *
         */
        afpdp_decr(abuf, (u_char *)AFP_DISTPW_PASS, (u_char *)&global);
        afpdp_decr((abuf+17), (u_char *)AFP_DISTPW_PASS, global.afp_password);

	/*
	 * another sanity check
	 *
	 */
        if (global.afp_magic != AFPDP_MAGIC)
          return(-1);
	
	/*
	 * make sure password null terminated
	 *
	 */
	global.afp_password[KEYSIZE] = '\0';

	afp_glob = &global;

	return(0);
}

/*
 * return pointer to structure representing ~user/.afppass
 *
 */

struct afppass *
afpdp_read(user, uid, home)
char *user;
int uid;
char *home;
{
	int fd, len;
	struct stat buf;
	static struct afppass afppass;
	char key[KEYSIZE], abuf[AFPPDSIZE], path[MAXPATHLEN];
	void afpdp_decr();

	if (afp_glob == (struct afppass *)NULL)
	  return((struct afppass *)NULL);

	bzero(&afppass, sizeof(struct afppass));

#ifdef AFP_DISTPW_PATH
	sprintf(path, "%s/%s%s", AFP_DISTPW_PATH, user, AFP_DISTPW_USER);
#else  /* AFP_DISTPW_PATH */
	sprintf(path, "%s/%s", home, AFP_DISTPW_USER);
#endif /* AFP_DISTPW_PATH */

	if (stat(path, &buf) < 0)
	  return((struct afppass *)NULL);

	/*
	 * check mode, size and owner
	 *
	 */
	if ((buf.st_mode&0777) != AFP_DISTPW_MODE
	 || buf.st_size != AFPPDSIZE) {
	  unlink(path); /* delete file */
	  return((struct afppass *)NULL);
	}
	if (buf.st_uid != uid)
	  return((struct afppass *)NULL);

	if ((fd = open(path, O_RDONLY, 0644)) < 0)
	  return((struct afppass *)NULL);

	len = read(fd, abuf, sizeof(abuf));

	close(fd);

	if (len != sizeof(abuf))
	  return((struct afppass *)NULL);

	/*
	 * sanity check on file contents
	 *
	 */
	if (abuf[16] != '\n' || abuf[33] != '\n')
	  return((struct afppass *)NULL);

	/*
	 * copy global key, xor with 'user' to
	 * prevent interchange of .afppass files
	 *
	 */
	bcopy((char *)afp_glob->afp_password, key, KEYSIZE);
	if ((len = strlen(user)) > KEYSIZE)
	  len = KEYSIZE;
	while (--len >= 0)
	  key[len] ^= user[len];

	/*
	 * decrypt each "line" into the structure using key
	 *
	 */
	afpdp_decr(abuf, key, (u_char *)&afppass);
	afpdp_decr((abuf+17), key, afppass.afp_password);

	if (afppass.afp_magic != AFPDP_MAGIC)
	  return((struct afppass *)NULL);

	/*
	 * make sure password null terminated
	 *
	 */
	afppass.afp_password[KEYSIZE] = '\0';

	return(&afppass);
}

/*
 * write a (possibly new) ~user/.afppass
 *
 * fail if UNIX password is used.
 *
 */

int
afpdp_writ(user, uid, home, afppass)
char *user;
int uid;
char *home;
struct afppass *afppass;
{
	int fd, i, j;
	char key[KEYSIZE], abuf[AFPPDSIZE], path[MAXPATHLEN];
	void afpdp_encr();

	if (afp_glob == (struct afppass *)NULL)
	  return(-1);

	if (afppass == (struct afppass *)NULL)
	  return(-1);

	if (afppass->afp_magic != AFPDP_MAGIC)
	  return(-1);

	/*
	 * ensure password null padded
	 *
	 */
	if ((i = strlen(afppass->afp_password)) > KEYSIZE)
	  i = KEYSIZE;
	
	for (j = i; j < KEYSIZE; j++)
	  afppass->afp_password[j] = '\0';

	/*
	 * check that the proposed new password
	 * is NOT identical to the UNIX password
	 * (and that the user exists ...)
	 *
	 */
	if (afpdp_upas(uid, afppass->afp_password) <= 0)
	  return(-1);

#ifdef AFP_DISTPW_PATH
	sprintf(path, "%s/%s%s", AFP_DISTPW_PATH, user, AFP_DISTPW_USER);
#else  /* AFP_DISTPW_PATH */
	sprintf(path, "%s/%s", home, AFP_DISTPW_USER);
#endif /* AFP_DISTPW_PATH */

	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, AFP_DISTPW_MODE)) < 0)
	  return(-1);

	/*
	 * copy global key, xor with 'user' to
	 * prevent interchange of .afppass files
	 *
	 */
	bcopy((char *)afp_glob->afp_password, key, KEYSIZE);
	if ((i = strlen(user)) > KEYSIZE)
	  i = KEYSIZE;
	while (--i >= 0)
	  key[i] ^= user[i];

	/*
	 * encrypt each half of structure into buffer
	 *
	 */
	afpdp_encr((u_char *)afppass, key, abuf);
	afpdp_encr(afppass->afp_password, key, abuf+17);

	abuf[16] = '\n';
	abuf[33] = '\n';

	if (write(fd, abuf, sizeof(abuf)) != sizeof(abuf))
	  return(-1);

	fchmod(fd, AFP_DISTPW_MODE);
	fchown(fd, uid, -1);

	close(fd);

	return(0);
}

/*
 * write a (possibly new) /usr/local/lib/cap/afppass
 *
 */

int
afpdp_make(path, afppass)
char *path;
struct afppass *afppass;
{
	int fd, i, j;
	char abuf[AFPPDSIZE];
	void afpdp_encr();

	if (geteuid() != 0)
	  return(-1);

	if (afppass == (struct afppass *)NULL)
	  return(-1);

	if (afppass->afp_magic != AFPDP_MAGIC)
	  return(-1);

	/*
	 * ensure password null padded
	 *
	 */
	if ((i = strlen(afppass->afp_password)) > KEYSIZE)
	  i = KEYSIZE;
	
	for (j = i; j < KEYSIZE; j++)
	  afppass->afp_password[j] = '\0';

	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0)
	  return(-1);

	/*
	 * encrypt each half of structure into buffer
	 *
	 */
	afpdp_encr((u_char *)afppass, (u_char *)AFP_DISTPW_PASS, abuf);
	afpdp_encr(afppass->afp_password, (u_char *)AFP_DISTPW_PASS, abuf+17);

	abuf[16] = '\n';
	abuf[33] = '\n';

	if (write(fd, abuf, sizeof(abuf)) != sizeof(abuf))
	  return(-1);

	fchmod(fd, 0600);
	fchown(fd, 0, -1);

	close(fd);

	return(0);
}

/*
 * decrypt 'str' using 'key', into 'buf'
 *
 * 'str' assumed to be 16 hex chars or null
 * 'buf' assumed to be 8 bytes long
 *
 */

void
afpdp_decr(str, key, buf)
char *str;
u_char *key, *buf;
{
	int i, j, k;
	u_char mykey[KEYSIZE];

	if ((i = strlen((char *)key)) > KEYSIZE)
	  i = KEYSIZE;

	/*
	 * copy key. DES ignores bottom bit,
	 * so shift one up, add null padding
	 *
	 */
	for (j = 0; j < i; j++)
	  mykey[j] = *(key+j) << 1;
	for (j = i; j < KEYSIZE; j++)
	  mykey[j] = '\0';

	/*
	 * copy str. convert hex string to data
	 *
	 */
	if (str != NULL) {
	  for (i = 0, j = 0; i < KEYSIZE; i++) {
	    buf[i] = 0;
	    for (k = 0; k < 2; j++, k++) {
	      if (str[j] >= '0' && str[j] <= '9')
	        buf[i] += (str[j] - '0');
	      if (str[j] >= 'A' && str[j] <= 'F')
	        buf[i] += (str[j] - 'A' + 10);
	      if (str[j] >= 'a' && str[j] <= 'f')
	        buf[i] += (str[j] - 'a' + 10);
	      if (k == 0)
	        buf[i] *= 16;
	    }
	  }
	}

	/*
	 * initialise and run DES
	 *
	 */
#ifndef DES_AVAIL
	desinit(0);
	dessetkey(mykey);
	dedes(buf);
	desdone();
#else	/* DES_AVAIL */
	{
	  char pass[64], pkey[64];

	  for (i = 0; i < 8; i++)
	    for (j = 0; j < 8; j++)
	      pass[(i*8)+j] = (buf[i] >> (7-j)) & 0x01;
	  for (i = 0; i < 8; i++)
	    for (j = 0; j < 8; j++)
	      pkey[(i*8)+j] = (mykey[i] >> (7-j)) & 0x01;
	  setkey(pkey);
	  encrypt(pass, 1);
	  for (i = 0; i < 8; i++) {
	    buf[i] = 0;
	    for (j = 0; j < 8; j++)
	      buf[i] |= ((pass[(i*8)+j] & 0x01) << (7-j));
	  }
	}
#endif	/* DES_AVAIL */

	return;
}

/*
 * encrypt 'buf' using 'key', into 'str'
 *
 * 'buf' assumed to be 8 bytes (null padded)
 * 'str' assumed have space for 16 characters or null
 *
 */

void
afpdp_encr(buf, key, str)
u_char *buf, *key;
char *str;
{
	int i, j, k;
	u_char mykey[KEYSIZE];

	if ((i = strlen(key)) > KEYSIZE)
	  i = KEYSIZE;

	/*
	 * copy key. DES ignores bottom bit,
	 * so shift one up, add null padding
	 *
	 */
	for (j = 0; j < i; j++)
	  mykey[j] = *(key+j) << 1;
	for (j = i; j < KEYSIZE; j++)
	  mykey[j] = '\0';

	/*
	 * initialise and run DES
	 *
	 */
#ifndef DES_AVAIL
	desinit(0);
	dessetkey(mykey);
	endes(buf);
	desdone();
#else	/* DES_AVAIL */
	{
	  char pass[64], pkey[64];

	  for (i = 0; i < 8; i++)
	    for (j = 0; j < 8; j++)
	      pass[(i*8)+j] = (buf[i] >> (7-j)) & 0x01;
	  for (i = 0; i < 8; i++)
	    for (j = 0; j < 8; j++)
	      pkey[(i*8)+j] = (mykey[i] >> (7-j)) & 0x01;
	  setkey(pkey);
	  encrypt(pass, 0);
	  for (i = 0; i < 8; i++) {
	    buf[i] = 0;
	    for (j = 0; j < 8; j++)
	      buf[i] |= ((pass[(i*8)+j] & 0x01) << (7-j));
	  }
	}
#endif	/* DES_AVAIL */

	if (str == NULL)
	  return;

	/*
	 * convert to Hex digits
	 *
	 */
	for (i = 0, j = 0; i < KEYSIZE; i++) {
	  str[j++] = hex[(buf[i] >> 4) & 0x0f];
	  str[j++] = hex[(buf[i] & 0x0f)];
	}

	return;
}

/*
 * compare password against UNIX account password
 *
 * returns: -1 if nonexistent, 0 if same, 1 if no match
 *
 */

int
afpdp_upas(uid, passwd)
int uid;
char *passwd;
{
	struct passwd *pw, *getpwuid();

	if ((pw = getpwuid(uid)) == NULL)
	  return(-1);

	if (strcmp((char *)crypt(passwd, pw->pw_passwd), pw->pw_passwd) == 0)
	  return(0);

	return(1);
}

/*
 * check password expiry date (global and user)
 *
 * return
 *	1  if password expired and user allowed to update
 *	-1 if password expired and user can't update
 *	0  if password hasn't expired
 *
 */

int
afpdp_pwex(afp)
struct afppass *afp;
{
	time_t now, then;

	if (afp_glob == (struct afppass *)NULL
	 || afp == (struct afppass *)NULL)
	  return(-1);

	time(&now);
	then = ntohl(afp_glob->afp_expires);

	/*
	 * enforce global expiry date
	 *
	 */
	if (then > SECS_10_YRS && now > then)
	  return(-1);

	/*
	 * otherwise check user expiry date
	 *
	 */
	if ((then = ntohl(afp->afp_expires)) == 0)
	  return(0);

	if (now > then)
	  return(1);

	return(0);
}

/*
 * update user expiry date
 *
 */

void
afpdp_upex(afp)
struct afppass *afp;
{
	time_t now, then;

	if (afp_glob == (struct afppass *)NULL
	 || afp == (struct afppass *)NULL)
	  return;

	time(&now);
	then = ntohl(afp_glob->afp_expires);

	if (then > SECS_10_YRS || then == 0)
	  afp->afp_expires = afp_glob->afp_expires;
	else
	  afp->afp_expires = htonl(now+then);

	return;
}

/*
 * read a positive integer up to 'maxm'
 * use 'def' if no input provided
 *
 */

int
afpdp_gnum(def, maxm)
int def, maxm;
{
	int num = 0;
	char abuf[80];

	do {
	  fgets(abuf, sizeof(abuf), stdin);
	  if (abuf[0] == '\n')
	    return(def);
	  num = atoi(abuf);
	  if (num > maxm)
	    printf("Maximum value is %d, try again: [%d] ? ", maxm, def);
	  if (num < 0)
	    printf("Number must be positive, try again: [%d] ? ", def);
	} while (num > maxm || num < 0);

	return(num);
}

/*
 * read a date or time from standard input
 *
 * format can be a period in the form
 *	NNNNd	(days)
 *	NNNNm	(months)
 *
 * or an absolute time
 *	YY/MM/DD [HH:MM:SS]
 *
 * return 0xffffffff if null response
 *
 */

time_t
afpdp_gdat()
{
	struct tm tm;
	time_t mult = 0;
	char abuf[80], *cp;

	bzero(abuf, sizeof(abuf));
	fgets(abuf, sizeof(abuf), stdin);

	if (abuf[0] == '\n')
	  return(0xffffffff);

	/*
	 * explicit days ?
	 *
	 */
	if ((cp = (char *)index(abuf, 'd')) != NULL) {
	  *cp = '\0';
	  mult = SECS_IN_DAY;
	  return(mult * atoi(abuf));
	}

	/*
	 * or months ?
	 *
	 */
	if ((cp = (char *)index(abuf, 'm')) != NULL) {
	  *cp = '\0';
	  mult = SECS_IN_MON;
	  return(mult * atoi(abuf));
	}

	/*
	 * check for YY/MM/DD
	 *
	 */
	cp = abuf;
	bzero((char *)&tm, sizeof(struct tm));
	if ((char *)index(cp, '/') != NULL) {
	  if (cp[2] == '/' && cp[5] == '/') {
	    cp[2] = cp[5] = cp[8] = '\0';
	    if ((tm.tm_year = atoi(cp)) < 95)
	      tm.tm_year += 100;		/* year - 1900 */
	    tm.tm_mon = atoi(cp+3) - 1;		/* 0 - 11 */
	    tm.tm_mday = atoi(cp+6);		/* 1 - 31 */
	    tm.tm_isdst = 1;
	    cp += 8;
	  } else {
	    printf("Sorry I don't understand that format, use YY/MM/DD\n");
	    return(0);
	  }
	}

	/*
	 * and optional HH:MM:SS
	 *
	 */
	if (cp != abuf && *cp++ == '\0') {
	  if ((char *)index(cp, ':') != NULL) {
	    if (cp[2] == ':' && cp[5] == ':') {
	      cp[2] = cp[5] = cp[8] = '\0';
	      tm.tm_hour = atoi(cp);		/* 0 - 23 */
	      tm.tm_min = atoi(cp+3);		/* 0 - 59 */
	      tm.tm_sec = atoi(cp+6);		/* 0 - 59 */
	      tm.tm_isdst = 1;
	    } else {
	      printf("Sorry I don't understand that format, use HH:MM:SS\n");
	      return(0);
	    }
	  }
	}

	/*
	 * tm set ?
	 *
	 */
	if (tm.tm_isdst) {
	  tm.tm_isdst = 0;
#if defined(sun) && !defined(SOLARIS)
	  return(timelocal(&tm));
#else  /* sun && !SOLARIS */
	  return(mktime(&tm));
#endif /* sun && !SOLARIS */
	}

	/*
	 * default days
	 *
	 */
	return(atoi(abuf)*SECS_IN_DAY);
}
#else  /* DISTRIB_PASSWDS */
int pass_dummy_for_ld;
#endif /* DISTRIB_PASSWDS */
