#ifdef SIZESERVER

#include <stdio.h>
#include <fstab.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <ufs/fs.h>
#include <ufs/inode.h>
/*
 * files could be in
 * /usr/include/sys
 *
#include <sys/fs.h>
#include <sys/inode.h>
 *
 */
#include "sizeserver.h"

main()
{
	register int uid;
	struct volsize vs;
	char path[BUFSIZ];

	uid = getuid();
	for( ; ; ) {
		if(read(0, path, BUFSIZ) <= 0)
			exit(0);
		volumesize(path, uid, &vs.total, &vs.free);
		write(0, (char *)&vs, sizeof(vs));
	}
}

volumesize(path, uid, ntot, nfree)
char *path;
int uid;
long *ntot, *nfree;
{
	register int fd;
	register long total, avail, used;
	register int i;
	struct stat statbuf, statbuf2;
	struct fstab *fsp;
	struct fs super;

	if(stat(path, &statbuf) < 0) {
unknown:
		*ntot = 0x1000000;
		*nfree = 0x1000000;
		return;
	}
	setfsent();
	while(fsp = getfsent()) {
		if(stat(fsp->fs_spec, &statbuf2) == 0 &&
		 statbuf2.st_rdev == statbuf.st_dev) {
			path = fsp->fs_spec;
			break;
		}
	}
	endfsent();
	if(fsp == NULL)
		goto unknown;
	if((fd = open(path, O_RDONLY, 0)) < 0)
		goto unknown;
	(void)lseek(fd, (long)(SBLOCK * DEV_BSIZE), 0);
	i = read(fd, (char *)&super, sizeof(super));
	(void)close(fd);
	if(i != sizeof(super))
		goto unknown;
	total = super.fs_dsize;
	used = total - (super.fs_cstotal.cs_nbfree * super.fs_frag +
	    super.fs_cstotal.cs_nffree);
	avail = (avail = total * (100 - super.fs_minfree) / 100) > used ?
	 (avail - used) : 0;
	*nfree = (uid == 0 ? (total - used) : avail) * super.fs_fsize;
	*ntot = total * super.fs_fsize;
}

#else  SIZESERVER
#include <stdio.h>
main()
{
 	printf("sizeserver: not compiled with -DSIZESERVER\n");
}
#endif SIZESERVER
