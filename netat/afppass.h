/*
 * $Author: djh $ $Date: 1995/06/20 03:27:33 $
 * $Header: /mac/src/cap60/netat/RCS/afppass.h,v 2.1 1995/06/20 03:27:33 djh Rel djh $
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
 * afppass.h - AUFS Distributed Password defines.
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

#define KEYSIZE		8
#define MINKEYSIZE	6
#define AFPPDSIZE	34

struct afppass {
  u_char afp_magic;		/* magic for sanity checking */
#define AFPDP_MAGIC	79
  u_char afp_minmpwlen;		/* mininum password length */
  u_char afp_maxattempt;	/* maximum failed login attempts */
  u_char afp_numattempt;	/* count of attempts to date */
  time_t afp_expires;		/* date password expires */
  u_char afp_password[10];	/* current user password */
};

/*
 * Gobal key. Should be localized by each site.
 *
 */

#ifndef AFP_DISTPW_PASS
#define AFP_DISTPW_PASS		"%-[&'*!~"
#endif  AFP_DISTPW_PASS

/*
 * Misc files & permission.
 *
 */

#ifndef AFP_DISTPW_FILE
#define AFP_DISTPW_FILE		"/usr/local/lib/cap/afppass"
#endif  AFP_DISTPW_FILE

#ifndef AFP_DISTPW_USER
#define AFP_DISTPW_USER		".afppass"
#endif  AFP_DISTPW_USER

#ifndef AFP_DISTPW_MODE
#define AFP_DISTPW_MODE		0600
#endif  AFP_DISTPW_MODE

/*
 * Misc numbers.
 *
 */

#define SECS_IN_DAY		24*60*60
#define SECS_IN_MON		30*24*60*60
#define SECS_10_YRS		10*365*24*60*60
