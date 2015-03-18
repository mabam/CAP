static char rcsid[] = "$Author: djh $ $Date: 1995/08/28 10:38:35 $";
static char rcsident[] = "$Header: /mac/src/cap60/applications/lwsrv/RCS/packed.c,v 2.1 1995/08/28 10:38:35 djh Rel djh $";
static char revision[] = "$Revision: 2.1 $";

/*
 * packed.c -  create packed printer descriptions
 *
 * UNIX AppleTalk spooling program: act as a laserwriter
 * AppleTalk package for UNIX (4.2 BSD).
 *
 */

#include <stdio.h>
#include <sys/param.h>
#include <netat/appletalk.h>
#include <netat/sysvcompat.h>
#include <netat/compat.h>
#ifdef USESTRINGDOTH
#include <string.h>
#else  /* USESTRINGDOTH */
#include <strings.h>
#endif /* USESTRINGDOTH */
#include "packed.h"

char *malloc();
char *realloc();

unsigned
AddToPackedChar(pc, str)
register PackedChar *pc;
char *str;
{
  register int l, n;

  l = strlen(str) + 1;
  if ((n = pc->n) + l > pc->cmax && (pc->pc = realloc(pc->pc,
   (pc->cmax += l + PACKEDCHARDELTA) * sizeof(char))) == NULL)
    errorexit(1, "AddToPackedChar: Out of memory\n");
  strcpy(pc->pc + n, str);
  /*
   * This doesn't work on machines with 16 bit ints!
   */
  if ((pc->n += l) > MAXPACKEDSIZE)
    errorexit(2, "AddToPackedChar: PackedChar overflow\n");
  return(n);
}

unsigned
AddToPackedShort(ps, i)
register PackedShort *ps;
unsigned i;
{
  if (ps->n >= ps->smax && (ps->ps = (unshort *)realloc(ps->ps,
   (ps->smax += PACKEDSHORTDELTA) * sizeof(unshort))) == NULL)
    errorexit(1, "AddToPackedShort: Out of memory\n");
  ps->ps[ps->n] = i;
  return(ps->n++);
}

unshort *
AllocatePackedShort(ps, nshort)
register PackedShort *ps;
int nshort;
{
  register int n;

  if ((n = ps->n) + nshort > ps->smax && (ps->ps = (unshort *)realloc(ps->ps,
   (ps->smax += nshort + PACKEDSHORTDELTA) * sizeof(unshort))) == NULL)
    errorexit(1, "AllocatePackedShort: Out of memory\n");
  ps->n += nshort;
  return(ps->ps + n);
}

PackedChar *
CreatePackedChar()
{
  register PackedChar *pc;
  static char errstr[] = "CreatePackedChar: Out of memory\n";

  if ((pc = (PackedChar *)malloc(sizeof(PackedChar))) == NULL)
    errorexit(1, errstr);
  if ((pc->pc = malloc((pc->cmax = PACKEDCHARDELTA) * sizeof(char)))
   == NULL)
    errorexit(2, errstr);
  pc->n = 0;
  return(pc);
}

PackedShort *
CreatePackedShort()
{
  register PackedShort *ps;
  static char errstr[] = "CreatePackedChar: Out of memory\n";

  if ((ps = (PackedShort *)malloc(sizeof(PackedShort))) == NULL)
    errorexit(1, errstr);
  if ((ps->ps = (unshort *)malloc((ps->smax = PACKEDCHARDELTA) * sizeof(unshort)))
   == NULL)
    errorexit(2, errstr);
  ps->n = 0;
  return(ps);
}

void
FreePackedChar(pc)
register PackedChar *pc;
{
  free(pc->pc);
  free((char *)pc);
}

void
FreePackedShort(ps)
register PackedShort *ps;
{
  free((char *)ps->ps);
  free((char *)ps);
}
