/* "$Author: djh $ $Date: 1995/08/28 10:38:35 $" */
/* "$Header: /mac/src/cap60/applications/lwsrv/RCS/packed.h,v 2.1 1995/08/28 10:38:35 djh Rel djh $" */
/* "$Revision: 2.1 $" */

/*
 * packed.h -  create packed printer descriptions
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#ifndef _PACKED_H_
#define	_PACKED_H_

#define	MAXPACKEDSIZE		((unsigned)0xffff)
#define	PACKEDCHARDELTA		256
#define	PACKEDSHORTDELTA	128

typedef unsigned short unshort;

typedef struct PackedChar {
    unsigned n;
    unsigned cmax;
    char *pc;
} PackedChar;
typedef struct PackedShort {
    unsigned n;
    unsigned smax;
    unshort *ps;
} PackedShort;

#define	AddrPackedChar(p)	((p)->pc)
#define	AddrPackedShort(p)	((p)->ps)
#define	NPackedChar(p)		((p)->n)
#define	NPackedShort(p)		((p)->n)

unsigned AddToPackedChar(/* PackedChar *pc, char *str */);
unsigned AddToPackedShort(/* PackedShort *ps, unsigned i */);
unshort *AllocatePackedShort(/* PackedShort *ps, int nshort */);
PackedChar *CreatePackedChar();
PackedShort *CreatePackedShort();
void FreePackedChar(/* PackedChar *pc */);
void FreePackedShort(/* PackedShort *ps */);

#endif /* _PACKED_H_ */
