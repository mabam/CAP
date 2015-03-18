/* mcvert.c - version 1.05 - 10 January, 1990 modified 12 March, 1990 by NP
 * Written by Doug Moore - Rice University - dougm@rice.edu - April '87
 * Sun bug fixes, assorted stuff - Jim Sasaki, March '89
 * Changed default max_line_size from 2000 to unlimited - Doug Moore, April, '89
 * Sun 3/60 doesn't like odd-sized structs.  Bug fixed - Doug Moore, April, '89
 *                                              - aided by Spencer W. Thomas
 * Didn't handle properly many hqx files combined in one file.  Bug fixed -
 *                                           Doug Moore, June, '89
 * Modified to handle MacBinaryII specification. Jim Van Verth, Sept, '89
 *
 * Fixed a bug when there are blank lines in hqx data, as happens when newline
 * get translated to CRLF and then to \n\n, common for some file transfers.
 * The last hqx line would be lost if the previous line was blank or junk.
 *	Glenn Trewitt, Stanford University, 1990	(1.05)
 *
 * Mcvert would hiccup on mail header lines "X-Mailer: ELM [version 2.2 PL14]"
 * as "X-Mailer:" is a vaild hqx line! Added in code to special case this
 * line and keep scanning for the real hqx data.
 *      Nigel Perry, Imperial College, 12 March 1990 [NP]
 *
 * This program may be freely distributed for non-profit purposes.  It may not
 * be sold, by itself or as part of a collection of software.  It may be freely
 * modified as long as no modified version is distributed.  Modifications of
 * interest to all can be incorporated into the program by sending them to me
 * for distribution.  Parts of the code can be used in other programs.  I am not
 * responsible for any damage caused by this program.  I hope you enjoy it.
 */

#include "mactypes.h"

#define HQX 0
#define TEXT 1
#define DATA 2
#define RSRC 3
#define HOST 4
#define FORWARDS 0
#define BACKWARDS 1

FILE *verbose;
char **hqxnames, **hqxnames_left;
char *dir, *ext, *text_author;
char *maxlines_str;
int maxlines;

main(argc, argv)
int argc;
char **argv;
{   char *flags, *getenv();
    int direction, mode, unpit_flag;

    argv++;
    argc--;
    verbose = stderr;
    direction = FORWARDS;
    mode = HQX;
    unpit_flag = 0;

    if ((text_author = getenv("MAC_EDITOR"))    == NULL)    text_author = "MACA";
    if ((ext =         getenv("MAC_EXT"))       == NULL)    ext = ".bin";
    if ((dir =         getenv("MAC_DLOAD_DIR")) == NULL)    dir = ".";
    if ((maxlines_str = getenv("MAC_LINE_LIMIT")) == NULL)  maxlines = 0;
    else										 maxlines = atoi(maxlines_str);
    
    /* Make command line arguments globally accessible */
    hqxnames = (char **) calloc(argc+1, sizeof(char *));
    hqxnames_left = hqxnames;
    while (argc--)  *hqxnames_left++ = *argv++;
    *hqxnames_left = "-";
    hqxnames_left = hqxnames;

    while (strcmp(*hqxnames_left, "-")) {
        if (hqxnames_left[0][0] == '-') {
            flags = *hqxnames_left++;
            while (*++flags)
                switch (*flags) {
                case 'x':
                    mode = HQX;
                    break;
                case 'u':
                    mode = TEXT;
                    break;
                case 'd':
                    mode = DATA;
                    break;
                case 'r':
                    mode = RSRC;
                    break;
		case 'h':
		    mode = HOST;
		    break;
                case 'D':
                    direction = FORWARDS;
                    break;
                case 'U':
                    direction = BACKWARDS;
                    break;
                case 'q':
                    unpit_flag = 0;
                    break;
                case 'p':
                    unpit_flag = 1;
                    break;
                case 's':
                    verbose = fopen("/dev/null", "w");
                    break;
                case 'v':
                    verbose = stderr;
                    break;
                default:
                    error(
                    "Usage: mcvert [ -[r|d|u|x|h] [D|U] [p|q] [s|v] ] filename...",
                    NULL);
                    }
            }

        if (direction == BACKWARDS)
            if (mode == HQX && unpit_flag) re_hqx();/* no re_pit() yet */
            else if (mode == HQX) re_hqx();
            else re_other(mode);
        else
            if (mode == HQX) un_hqx(unpit_flag);
            else un_other(mode);
        }
    }

/* An array useful for CRC calculations that use 0x1021 as the "seed" */
word magic[] = {
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
    0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
    0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
    0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
    };


/*
 * calc_crc() --
 *   Compute the MacBinary II-style CRC for the data pointed to by p, with the
 *   crc seeded to seed.
 *
 *   Modified by Jim Van Verth to use the magic array for efficiency.
 */
short calc_mb_crc(p, len, seed)
unsigned char *p;
long len;
short seed;
{
  short hold;      /* crc computed so far */
  long  i;         /* index into data */

  extern unsigned short magic[];   /* the magic array */

  hold = seed;     /* start with seed */
  for (i = 0; i < len; i++, p++) {
    hold ^= (*p << 8);
    hold = (hold << 8) ^ magic[(unsigned char)(hold >> 8)];
  }

  return (hold);
} /* calc_crc() */


/* Report a fatal error */
error(msg, name)
char msg[], name[];
{   fprintf(stderr, msg, name);
    putc('\n', stderr);
    exit(1);
    }

/* replace illegal Unix characters in file name */
/* make sure host file name doesn't get truncated beyond recognition */
unixify(np)
register byte *np;
{   register ulong c;
    c = strlen(np);
    if (c > SYSNAMELEN - 4) c = SYSNAMELEN - 4;
    np[c] = '\0';
    np--;
    while (c = *++np)
        if (c <= ' ' || c == '/' || c > '~') *np = '_';
    }

/* Convert Unix time (GMT since 1-1-1970) to Mac
                                    time (local since 1-1-1904) */
#define MACTIMEDIFF 0x7c25b080 /* Mac time of 00:00:00 GMT, Jan 1, 1970 */

ulong time2mac(time)
ulong time;
{   struct timeb tp;
    ftime(&tp);
    return long2mac(time + MACTIMEDIFF
                    - 60 * (tp.timezone - 60 * tp.dstflag));
    }


/* This procedure copies the input file to the output file, basically, although
    in TEXT mode it changes LF's to CR's and in any mode it forges a Mac info 
    header.  Author type for TEXT mode can come from the MAC_EDITOR environ-
    ment variable if it is defined. */

un_other(mode)
int mode;
{   register ulong b;
    register ulong nchars;
    char txtfname[BINNAMELEN], binfname[BINNAMELEN];
    FILE *txtfile, *binfile; 
    char *suffix;
    struct stat stbuf;
    info_header info;
    int extra_chars;
    ulong dlen, rlen, mtim, ctim;
    short crc, calc_mb_crc();

    if (mode == DATA) suffix = ".data";
    else if (mode == RSRC) suffix = ".rsrc";
    else suffix = ".text";

    while (hqxnames_left[0][0] != '-') {

        strcpy(txtfname, *hqxnames_left++);
        if (!(txtfile = fopen(txtfname, "r"))) {
            /* Maybe we are supposed to figure out the suffix ourselves? */
            strcat(txtfname, suffix);
            if (!(txtfile = fopen(txtfname, "r")))
                error("Cannot open %s", txtfname);
            }

        if (stat(txtfname, &stbuf))
            error("Cannot read %s", txtfname);

        /* stuff header data into the info header */
        bzero(&info, sizeof(info_header));
        info.nlen = strlen(txtfname);
        info.nlen = (info.nlen > NAMELEN) ? NAMELEN : info.nlen;
	info.name[info.nlen] = '\0';
        strcpy(info.name, txtfname);           /* name */
        mtim = time2mac(stbuf.st_mtime);
        ctim = time2mac(stbuf.st_ctime);
        bcopy(&mtim, info.mtim, 4);
        bcopy(&ctim, info.ctim, 4);
	info.uploadvers = '\201';
	info.readvers = '\201';

        if (mode == RSRC) {
            /* dlen is already zero */
            rlen = long2mac(stbuf.st_size);
            bcopy(&rlen, info.rlen, 4);
            bcopy("APPL", info.type, 4);
            bcopy("CCOM", info.auth, 4);
            }
        else {
            dlen = long2mac(stbuf.st_size);
            bcopy(&dlen, info.dlen, 4);
            /* rlen is already zero */
            bcopy("TEXT", info.type, 4);
            if (mode == DATA) bcopy("????", info.auth, 4);
            else bcopy(text_author, info.auth, 4);
            }

	/* calculate CRC */
	crc = calc_mb_crc(&info, 124, 0);
	info.crc[0] = (char) (crc >> 8);
	info.crc[1] = (char) crc;

        /* Create the .bin file and write the info to it */
        sprintf(binfname, "%s/%s%s", dir, txtfname, ext);
        if ((binfile = fopen(binfname, "w")) == NULL)
            error("Cannot open %s", binfname);
        fprintf(verbose,
                "Converting     %-30s type = \"%4.4s\", author = \"%4.4s\"\n",
                txtfname, info.type, info.auth);
        fwrite(&info, sizeof(info), 1, binfile);

        nchars = stbuf.st_size;
        extra_chars = 127 - (nchars+127) % 128;
        if (mode == TEXT) while (nchars--) {
            b = getc(txtfile);
            if (b == LF) b = CR;
            putc(b, binfile);
            }
        else while (nchars--) putc(getc(txtfile), binfile);

        while (extra_chars--) putc(0, binfile);
        fclose(binfile);
        fclose(txtfile);
        }
    }

/* This procedure copies the input file to the output file, basically, although
    in TEXT mode it changes CR's to LF's and in any mode it skips over the Mac
    info header. */

re_other(mode)
int mode;
{   register ulong b;
    register ulong nchars;
    char txtfname[BINNAMELEN], binfname[BINNAMELEN];
    FILE *txtfile, *binfile; 
    char *suffix;
    info_header info;

    if (mode == DATA) suffix = ".data";
    else if (mode == RSRC) suffix = ".rsrc";
    else suffix = ".text";

    while (hqxnames_left[0][0] != '-') {

        strcpy(binfname, *hqxnames_left++);
        if ((binfile = fopen(binfname, "r")) == NULL) {
            /* Maybe we are supposed to figure out the suffix ourselves? */
            strcat(binfname, ext);
            if (!(binfile = fopen(binfname, "r")))
                error("Cannot open %s", binfname);
            }

        /* Read the info from the .bin file, create the output file */
        fread(&info, sizeof(info), 1, binfile);
        strncpy(txtfname, info.name, info.nlen);
	txtfname[info.nlen] = '\0';
        fprintf(verbose,
                "Converting     %-30s type = \"%4.4s\", author = \"%4.4s\"\n",
                txtfname, info.type, info.auth);
        if ((txtfile = fopen(txtfname, "r")) == NULL) {
            if ((txtfile = fopen(txtfname, "w")) == NULL)
                error("Cannot open %s", txtfname);
            }
        else {
            fclose(txtfile);
            strcat(txtfname, suffix);
            if ((txtfile = fopen(txtfname, "w")) == NULL)
                error("Cannot open %s", txtfname);
            }

        nchars = mac2long(* (ulong *) info.dlen);
        if (mode == TEXT) while (nchars--) {
            b = getc(binfile);
            if (b == CR) b = LF;
            putc(b, txtfile);
            }
        else if (mode == DATA) while (nchars--)
            putc(getc(binfile), txtfile);
        else {
            while (nchars--) getc(binfile);
            nchars = mac2long(* (ulong *) info.rlen);
            while (nchars--) putc(getc(binfile), txtfile);
            }

        fclose(binfile);
        fclose(txtfile);
        }
    }
