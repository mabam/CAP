/*
 * Miscellaneous DeskTop routines
 *
 * Copyright (c) 1993, The University of Melbourne.
 * All Rights Reserved. Permission to publicly redistribute this
 * package (other than as a component of CAP) or to use any part
 * of this software for any purpose, other than that intended by
 * the original distribution, *must* be obtained in writing from
 * the copyright owner.
 *
 * djh@munnari.OZ.AU
 * 15 February 1993
 *
 * Refer: "Inside Macintosh", Volume 1, page I-128 "Format of a Resource File"
 *
 * $Author: djh $
 * $Revision: 2.1 $
 *
 */

#include "dt.h"

/*
 * print 4 byte signatures in hex and ascii representations
 *
 */

int
printsig(sig)
u_char *sig;
{
	int i;

	printf(" :");
	for (i = 0; i < 4; i++)
	  printf("%02x", sig[i]);
	printf(":(");
	for (i = 0; i < 4; i++)
	  printf("%c", isprint(sig[i]) ? sig[i] : '.');
	printf(") ");
}

/*
 * print out a representation of the ICN# and it's mask
 *
 */

int
printicn(icn, typ, len)
u_char *icn;
int typ;
short len;
{
	u_char *p;
	int i, j, k;
	u_long data1, data2, mask;

	if (typ != 1)
	  return;

	for (i = 0, p = icn; i < 32; i += 2, p += 8) {
	  for (j = 0; j < 2; j++) {
	    if (j == 1) printf("    ");
	    bcopy(p+(j*128), &data1, 4);
	    bcopy(p+4+(j*128), &data2, 4);
	    for (k = 0; k < 32; k++) {
	      mask = ((u_long)0x80000000 >> k);
	      if (data1 & mask && data2 & mask)
	        printf("8");
	      else
	        if (data1 & mask)
		  printf("\"");
	        else
		  if (data2 & mask)
		    printf("o");
		  else
		    printf(" ");
	    }
	  }
	  printf("\n");
	}
}
