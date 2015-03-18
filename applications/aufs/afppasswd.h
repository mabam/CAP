/*
 * $Author: djh $ $Date: 91/02/15 21:09:06 $
 * $Header: afppasswd.h,v 2.1 91/02/15 21:09:06 djh Rel $
 * $Revision: 2.1 $
*/

/*
 * afppasswd.h - Appletalk Filing Protocol - support for internal password file
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University in the
 * City of New York.
 *
 * Edit History:
 *
 *  June 1987     CCKim	Created.
 *
*/

boolean init_aufs_passwd();
boolean is_aufs_user();
char *user_aufs_passwd();
struct passwd *aufs_unix_user();
