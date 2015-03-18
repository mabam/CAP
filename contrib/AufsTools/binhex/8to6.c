/*
 * convert 8 bit data stream into 6 bit data stream
 *
 * David Gentzel, Lexeme Corporation
 */

#include <stdio.h>

#define MAXLINELEN	62

static
char tr[]="!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";

/*
 * Output a character to the current output file converting from 8 bit to 6 bit
 * representation.  When called with EOF, flush all pending output.
 */
void putchar_6(c)
int c;
{
    static unsigned char buf[3];
    static unsigned int num_bytes = 0;
    static count = 1;	/* # of characters on current line */
			    /* start at 1 to include colon */

    if (c == EOF)	/* flush buffer on EOF */
    {
	while (num_bytes != 0)
	    putchar_6(0);
	count = 1; /* for next file */
	return;
    }

    buf[num_bytes++] = c;
    if (num_bytes == 3)
    {

	num_bytes = 0;
	putchar(tr[buf[0] >> 2]);
	if (count++ > MAXLINELEN)
	{
	    count = 0;
	    putchar('\n');
	}
	putchar(tr[((buf[0] & 0x03) << 4) | (buf[1] >> 4)]);
	if (count++ > MAXLINELEN)
	{
	    count = 0;
	    putchar('\n');
	}
	putchar(tr[((buf[1] & 0x0F) << 2) | (buf[2] >> 6)]);
	if (count++ > MAXLINELEN)
	{
	    count = 0;
	    putchar('\n');
	}
	putchar(tr[buf[2] & 0x3F]);
	if (count++ > MAXLINELEN)
	{
	    count = 0;
	    putchar('\n');
	}
    }
}
