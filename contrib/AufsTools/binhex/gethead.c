/*
 * Change a .info file into a proper header for a .hqx file
 *
 * David Gentzel, Lexeme Corporation
 *
 * Based on code written by ????.
 */

#include <stdio.h>
#ifdef VMS
# include <types.h>
# include <stat.h>
#else
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "aufs.h"

#define NAMEBYTES 63
#define H_NLENOFF 1
#define H_NAMEOFF 2

/* 65 <-> 80 is the FInfo structure */
#define H_TYPEOFF 65
#define H_AUTHOFF 69
#define H_FLAGOFF 73

#define H_LOCKOFF 81
#define H_DLENOFF 83
#define H_RLENOFF 87
#define H_CTIMOFF 91
#define H_MTIMOFF 95

/* Append cnt bytes to the output buffer starting at head[offset]. */
#define put(cnt, offset) \
{ \
    register char *a = &head[(int) offset]; \
    register int b = (int) (cnt); \
 \
    while (b--) \
	*out++ = *a++; \
}

/* Append cnt bytes to the output buffer starting at string. */
#define put2(cnt, string) \
{ \
    register int b = (int) (cnt); \
    register char *a = (char *) (string); \
 \
    while (b--) \
	*out++ = *a++; \
}

/* Append cnt bytes to the output buffer starting at string + (cnt - 1) and
   working backwards. */
#define put2rev(cnt, string) \
{ \
    register int b = (int) (cnt); \
    register char *a = (char *) (string) + b; \
 \
    while (b--) \
	*out++ = *--a; \
}

/* Build a usable header out of the .info information.  head is the text from
   the .info file, out is an output buffer. */
void gethead(head, out)
register char *head, *out;
{
    put(1, H_NLENOFF);		/* Name length */
    put(head[1], H_NAMEOFF);	/* Name */
    put(1, 0);			/* NULL */
    put(4, H_TYPEOFF);		/* Type */
    put(4, H_AUTHOFF);		/* Author */
    put(2, H_FLAGOFF);		/* Flags */
    put(4, H_DLENOFF);		/* Data length */
    put(4, H_RLENOFF);		/* Resource length */
}

/* Build a usable header out of the .finderinfo information.
   out is an output buffer. */
void aufs_gethead(info, data, rsrc, out)
register char *out;
register FinderInfo *info;
FILE *data, *rsrc;
{   register int len;
    long rlen, dlen;
    struct stat st;

    if(info->fi_bitmap & FI_BM_MACINTOSHFILENAME)
    {   len = strlen(info->fi_macfilename);
	*out++ = (char)len;
	put2(len+1, info->fi_macfilename);
    }
    else
    {   len = strlen(info->fi_shortfilename);
	*out++ = (char)len;
	put2(len+1, info->fi_shortfilename);
    }
    put2(4, info->fndr_type);		/* Type */
    put2(4, info->fndr_creator);	/* Author */
    put2(2, &info->fndr_flags);		/* Flags */

    if (rsrc != NULL)
    {
	(void) fstat(fileno(rsrc), &st);
	rlen = (long) st.st_size;
    }
    else
	rlen = 0L;
    if (data != NULL)
    {
	(void) fstat(fileno(data), &st);
	dlen = (long) st.st_size;
    }
    else
	dlen = 0L;
    put2(4, &dlen);		/* Data length */
    put2(4, &rlen);		/* Resource length */
}

/* Fake a usable header (there was no .info file). */
/* VMS NOTE:
	It is possible that the use of fstat to figure the sizes of the
	.data and .rsrc files will not work correctly if they are not
	Stream_LF files.  Not easy to get around, but not very common either
	(will only cause problem if .info file is missing and either .data
	or .rsrc is not Stream_LF, and xbin creates Stream_LF files).
*/
void fakehead(file, rsrc, data, out)
char *file;
FILE *rsrc, *data;
register char *out;
{
    unsigned char flen;
    long rlen, dlen;
    char flags[2];
    struct stat st;

    flen = (unsigned char) strlen(file);
    if (rsrc != NULL)
    {
	(void) fstat(fileno(rsrc), &st);
	rlen = (long) st.st_size;
    }
    else
	rlen = 0L;
    if (data != NULL)
    {
	(void) fstat(fileno(data), &st);
	dlen = (long) st.st_size;
    }
    else
	dlen = 0L;
    flags[0] = '\0';
    flags[1] = '\0';

    put2(1, &flen);		/* Name length */
    put2(flen, file);		/* Name */
    put2(1, "");		/* NULL */
    put2(4, "TEXT");		/* Type */
    put2(4, "????");		/* Author */
    put2(2, flags);		/* Flags */
#ifdef DONTSWAPINT
    put2(4, dlen);		/* Data length */
    put2(4, rlen);		/* Resource length */
#else
    put2rev(4, dlen);		/* Data length */
    put2rev(4, rlen);		/* Resource length */
#endif
}
