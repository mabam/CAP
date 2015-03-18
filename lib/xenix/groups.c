/*
 * $Author: djh $ $Date: 91/02/15 22:50:36 $
 * $Header: groups.c,v 2.1 91/02/15 22:50:36 djh Rel $
 * $Revision: 2.1 $
 */

/*
 * BSD groups functions for non-BSD systems.
 *
 * For now, we're always part of one group, our egid.
 */

int
initgroups(username, usergid)
    char *username;
    int usergid;
{
    /*
     * When this function is called, setgid() has already been called.
     * Therefore, we don't have to do anything.
     */

    return 0;
}

int
getgroups(gv, gmax)
    int *gv;
    int gmax;
{
    if (gmax < 1)
	return 0;

    gv[0] = getegid();
    return 1;
}
