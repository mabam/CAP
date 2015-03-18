/*
 * do run length compression for unxbin
 *
 * David Gentzel, Lexeme Corporation
 */

#include <stdio.h>

#define RUNCHAR	0x90
#define MAXREP	255

extern void putchar_6();

/*
 * Output a character to the current output file generating run length
 * compression on the fly.  All output goes through putchar_6 to do conversion
 * from 8 bit to 6 bit format.  When c == EOF, call putchar_6 with EOF to flush
 * pending output.
 */
void putchar_run(c)
register int c;
{
    static unsigned int rep = 1;	/* # of repititions of lastc seen */
    static int lastc = EOF;		/* last character passed to us */

    if (c == lastc)	/* increment rep */
    {
	/* repetition count limited to MAXREP */
	if (++rep == MAXREP)
	{
	    putchar_6(RUNCHAR);
	    putchar_6(MAXREP);
	    rep = 1;
	    lastc = EOF;
	}
    }
    else
    {
	switch (rep)
	{
	    case 2:	/* not worth running for only 2 reps... */
		putchar_6(lastc);
		if (lastc == RUNCHAR)
		    putchar_6(0);
		break;
	    case 1:
		break;
	    default:
		putchar_6(RUNCHAR);
		putchar_6(rep);
		break;
	}
	putchar_6(c);	/* output character (EOF flushes) */
	rep = 1;
	if (c == RUNCHAR)
	    putchar_6(0);
	lastc = c;
    }
}
