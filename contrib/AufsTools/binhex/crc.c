/*
 * compute the crc used in .hqx files for unxbin
 *
 * David Gentzel, Lexeme Corporation
 *
 * Based on crc code in xbin.c by Darin Adler of TMQ Software.
 */

#include <stdio.h>

#define BYTEMASK	0xFF
#define WORDMASK	0xFFFF
#define WORDBIT		0x10000

#define CRCCONSTANT	0x1021

static unsigned int crc;

extern void putchar_run();

/*
 * Update the crc.
 */
static void docrc(c)
register unsigned int c;
{
    register int i;
    register unsigned long temp = crc;

    for (i = 0; i < 8; i++)
    {
	c <<= 1;
	if ((temp <<= 1) & WORDBIT)
	    temp = (temp & WORDMASK) ^ CRCCONSTANT;
	temp ^= (c >> 8);
	c &= BYTEMASK;
    }
    crc = temp;
}

/*
 * Copy all characters from file in to the current output file computing a crc
 * as we go.  Append 2 byte crc to the output.  in may be NULL (empty file).
 */
void make_file_crc(in)
register FILE *in;
{
    register int c;

    crc = 0;
    if (in != NULL)
	while ((c = getc(in)) != EOF)
	{
	    putchar_run(c);
	    docrc(c);
	}
    docrc(0);
    docrc(0);
    putchar_run((crc >> 8) & BYTEMASK);
    putchar_run(crc & BYTEMASK);
}

/*
 * Copy count characters from buffer in to the current output file computing a
 * crc as we go.  Append 2 byte crc to the output.
 */
void make_buffer_crc(in, count)
register unsigned char *in;
register int count;
{
    crc = 0;
    while (count--)
    {
	putchar_run(*in);
	docrc(*in++);
    }
    docrc(0);
    docrc(0);
    putchar_run((crc >> 8) & BYTEMASK);
    putchar_run(crc & BYTEMASK);
}
