/* There are dummy routines that describe what the des subroutines */
/* should do */
/* If you get the "real" routines, the names should match - in other words */
/* you will have to rename "setkey" to "dessetkey" */

#define bit8 char

/*
 * desinit - should intialize things for DES encryption.  Return code of
 * 0 means all okay.  Return code of -1 should indicate that des encryption
 * is not available.
 *
 * The desinit routine, if it allows mutiple modes of operations, should
 * take (mode == 0) to mean "standard DES".
 *
 * Multiple calls to desinit should not result in multiple initializations
 * (e.g. desinit is in effect until desdone is issued)
 *
 */
desinit(mode)
int mode;
{
#ifdef sun
  return (0);
#else
  return(-1);			/* nothing here. */
#endif
}

/*
 * setkey will set the encryption/decryption key to the specified
 * 64 bit block (organized as an array of 8 8-bit values)
 *
 * ***WARNING*** THIS IS CALLED setkey IN THE ROUTINES AS DISTRIBUTED.
*/
dessetkey(key)
bit8 key[8];
{
}

/*
 * endes should encrypt the specifed block (8 element array of
 * 8-bit values).  The result is returned in-place.
 *
*/
endes(block)
bit8 block[8];
{
}


/*
 * desdone should undo what desinit does (e.g. free memory, etc)
 *
*/
desdone()
{
}

