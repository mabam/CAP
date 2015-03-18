#include "mactypes.h"

#define HQXBUFLEN 512
byte hqxbuf[HQXBUFLEN+1], *buf_ptr, *buf_end, *buf_start=hqxbuf+1;

#define MAXLINE 255
byte line[MAXLINE+1], *line_ptr, *line_end, *line_start=line+1;

int line_count, file_count;
int save_state, total_bytes, save_run_length;
word save_nibble;
char binfname[BINNAMELEN], hqxfname[BINNAMELEN];
FILE *hqxfile, *binfile;

/* This routine reads the header of a hqxed file and appropriately twiddles it,
    determines if it has CRC problems, creates the .bin file, and puts the info
    into the .bin file.
    Output is hqx_datalen, hqx_rsrclen, type, binfname, binfile */

hqx_to_bin_hdr(type, hqx_datalen, hqx_rsrclen)
char *type;
ulong *hqx_datalen, *hqx_rsrclen;
{   register byte *hqx_ptr, *hqx_end;
    register ulong calc_crc;
    hqx_buf *hqx_block;
    hqx_header *hqx;
    info_header info;
    ulong mtim;
    short crc;

    extern word magic[];
    extern FILE *verbose;
    extern char *dir, *ext;
    extern short calc_mb_crc();

    /* read the hqx header, assuming that I won't exhaust hqxbuf in so doing */
    fill_hqxbuf();
    hqx_block = (hqx_buf *) buf_ptr;
    hqx = (hqx_header *) (hqx_block->name + hqx_block->nlen);
    hqx_ptr = buf_ptr;
    hqx_end = (byte *) hqx + sizeof(hqx_header) - 1;
    calc_crc = 0;
    while (hqx_ptr < hqx_end)
        calc_crc = (((calc_crc&0xff) << 8) | *hqx_ptr++) ^ magic[calc_crc >> 8];
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    buf_ptr = hqx_ptr;

    /* stuff the hqx header data into the info header */
    bzero(&info, sizeof(info_header));
    info.nlen = hqx_block->nlen;
    strncpy(info.name, hqx_block->name, info.nlen);     /* name */
    bcopy(hqx->type, info.type, 9);             /* type, author, flag */
    info.flags  &= 0x7e;                        /* reset lock bit, init bit */
    if (hqx->protect & 0x40) info.protect = 1;  /* copy protect bit */
    bcopy(hqx->dlen, info.dlen, 8);             /* dlen, rlen */
    mtim = time2mac(time(0));
    bcopy(&mtim, info.mtim, 4);
    bcopy(&mtim, info.ctim, 4);
    info.uploadvers = '\201';
    info.readvers = '\201';

    /* calculate MacBinary CRC */
    crc = calc_mb_crc(&info, 124, 0);
    info.crc[0] = (char) (crc >> 8);
    info.crc[1] = (char) crc;

    /* Create the .bin file and write the info to it */
    unixify(hqx_block->name);
    sprintf(binfname, "%s/%s%s", dir, hqx_block->name, ext);
    fprintf(verbose,
        "Converting     %-30s type = \"%4.4s\", author = \"%4.4s\"\n",
        hqx_block->name, info.type, info.auth);
    if ((binfile = fopen(binfname, "w")) == NULL)
        error("Cannot open %s", binfname);
    check_hqx_crc(calc_crc, "File header CRC mismatch in %s", binfname);
    fwrite(&info, sizeof(info), 1, binfile);

    /* Get a couple of items we'll need later */
    bcopy(info.dlen, hqx_datalen, 4);
    *hqx_datalen = mac2long(*hqx_datalen);
    bcopy(info.rlen, hqx_rsrclen, 4);
    *hqx_rsrclen = mac2long(*hqx_rsrclen);
    bcopy(info.type, type, 4);
    }

/* This routine reads the header of a bin file and appropriately twiddles it,
    creates the .hqx file, and puts the info into the .hqx file.
    Output is hqx_datalen, hqx_rsrclen, type, hqxfname, hqxfile */

bin_to_hqx_hdr(hqx_datalen, hqx_rsrclen)
ulong *hqx_datalen, *hqx_rsrclen;
{   register byte *hqx_ptr, *hqx_end;
    register ulong calc_crc;
    hqx_buf *hqx_block;
    hqx_header *hqx;
    info_header info;
    extern word magic[];
    extern FILE *verbose;
    extern char **hqxnames_left;
    extern char *ext;

    strcpy(binfname, *hqxnames_left++);
    if (!(binfile = fopen(binfname, "r"))) {
        /* Maybe we are supposed to figure out the suffix ourselves? */
        strcat(binfname, ext);
        if (!(binfile = fopen(binfname, "r")))
            error("Cannot open %s", binfname);
        }
    if (!fread(&info, sizeof(info), 1, binfile))
        error("Unexpected EOF in header of %s", binfname);

    /* stuff the info header into the hqx header */
    hqx_block = (hqx_buf *) buf_ptr;
    hqx_block->nlen = info.nlen;
    strncpy(hqx_block->name, info.name, info.nlen);
    hqx = (hqx_header *) (hqx_block->name + hqx_block->nlen);
    hqx->version  = 0;
    bcopy(info.type, hqx->type, 9);             /* type, author, flags */
    if (info.protect = 1) hqx->protect = 0;     /* protect bit: 0x40 */
    else hqx->protect = 0;
    bcopy(info.dlen, hqx->dlen, 8);             /* dlen, rlen */

    /* Create the .hqx file and write the info to it */
    strncpy(hqxfname, info.name, info.nlen);
	hqxfname[info.nlen] = '\0';
    unixify(hqxfname);
    fprintf(verbose,
        "Converting     %-30s type = \"%4.4s\", author = \"%4.4s\"\n",
        hqxfname, info.type, info.auth);

    calc_crc = 0;
    hqx_ptr = (byte *) hqx_block;
    hqx_end = hqx_ptr + hqx_block->nlen + sizeof(hqx_header);
    while (hqx_ptr < hqx_end)
        calc_crc = (((calc_crc&0xff) << 8) | *hqx_ptr++) ^ magic[calc_crc >> 8];
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    buf_ptr = hqx_end;
    write_hqx_crc(calc_crc);

    /* Get a couple of items we'll need later */
    bcopy(info.dlen, hqx_datalen, 4);
    *hqx_datalen = mac2long(*hqx_datalen);
    bcopy(info.rlen, hqx_rsrclen, 4);
    *hqx_rsrclen = mac2long(*hqx_rsrclen);
    }


/* This routine copies bytes from the decoded input stream to the output.  
    It also pads to a multiple of 128 bytes on the output, which is part
    of the .bin format */
word hqx_to_bin_fork(nbytes)
register ulong nbytes;
{   register byte *c;
    register ulong calc_crc;
    register int c_length;
    ulong extra_bytes;
    extern word magic[];

    extra_bytes = 127 - (nbytes+127)%128; /* pad fork to mult of 128 bytes */
    calc_crc = 0;
    for (;;) {
        c = buf_ptr;
        c_length = (c + nbytes > buf_end) ? buf_end - c : nbytes;
        nbytes -= c_length;
        fwrite(c, sizeof(byte), c_length, binfile);
        while (c_length--)
            calc_crc = (((calc_crc&0xff) << 8) | *c++) ^ magic[calc_crc >> 8];
        if (!nbytes) break;
        fill_hqxbuf();
        }
    buf_ptr = c;
    while (extra_bytes--) putc(0, binfile);
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    return (word) calc_crc;
    }

/* This routine copies bytes from the input stream to the encoded output.  
    It also pads to a multiple of 128 bytes on the input, which is part
    of the .bin format */
word bin_to_hqx_fork(nbytes)
register ulong nbytes;
{   register byte *c;
    register ulong calc_crc;
    register int c_length;
    ulong extra_bytes;
    extern word magic[];

    extra_bytes = 127 - (nbytes+127)%128; /* pad fork to mult of 128 bytes */
    calc_crc = 0;
    for (;;) {
        c = buf_ptr;
        c_length = (c + nbytes > buf_end) ? buf_end - c : nbytes;
        nbytes -= c_length;
        fread(c, sizeof(byte), c_length, binfile);
        buf_ptr += c_length;
        while (c_length--)
            calc_crc = (((calc_crc&0xff) << 8) | *c++) ^ magic[calc_crc >> 8];
        if (!nbytes) break;
        empty_hqxbuf();
        }
    buf_ptr = c;

    fseek(binfile, extra_bytes, 1);
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    calc_crc = ((calc_crc&0xff) << 8) ^ magic[calc_crc >> 8];
    return (word) calc_crc;
    }

/* Essentials for Binhex 8to6 run length encoding */
#define RUNCHAR 0x90
#define MAXRUN 255
#define IS_LEGAL <0x40
#define ISNT_LEGAL >0x3f
#define DONE 0x7F /* tr68[':'] = DONE, since Binhex terminator is ':' */
#define SKIP 0x7E /* tr68['\n'|'\r'] = SKIP, i. e. end of line char.  */
#define FAIL 0x7D /* character illegal in binhex file */

byte tr86[] =
        "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr"; 
byte tr68[] = {
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, SKIP, FAIL, FAIL, SKIP, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, FAIL, FAIL,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, FAIL,
    0x14, 0x15, DONE, FAIL, FAIL, FAIL, FAIL, FAIL,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, FAIL,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, FAIL,
    0x2C, 0x2D, 0x2E, 0x2F, FAIL, FAIL, FAIL, FAIL,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, FAIL,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, FAIL, FAIL,
    0x3D, 0x3E, 0x3F, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL,
    };

/*
 *  This procedure transparently reads and decodes the hqx input.  It does run 
 *  length and 6 to 8 decoding.
 */
#define READING 0
#define SKIPPING 1
#define FIND_START_COLON 2

/* NP 12/3/90: A dirty hack to handle "X-Mailer: ELM [version 2.2 PL14]" */
#define X_MAIL_STR "\054\014\043\061\070\073\065\077\177"
#define X_MAIL_LEN strlen(X_MAIL_STR)

fill_hqxbuf()
{   register ulong c, nibble;
    register int not_in_a_run = TRUE, state68;
    register byte *fast_buf, *fast_line;
    static int status = FIND_START_COLON;

    buf_ptr = fast_buf = buf_start;
    fast_line = line_ptr;
    state68 = save_state;
    nibble = save_nibble;
    if (save_run_length > 0) {
        c = save_run_length;
        save_run_length = 0;
        goto continue_run;
        }
    while (fast_buf < buf_end) {
        next_char:
        if ((c = *fast_line++) ISNT_LEGAL) {
            if (c == DONE) break;
            next_line:
            if (!fgets(line_start, MAXLINE, hqxfile) && !new_in_hqx_file())
                if (status == FIND_START_COLON) exit(0);
		else error("Premature EOF in %s\n", hqxfname);
            line_ptr = line_start;
            scan_line:
            fast_line = line_ptr;
	    while ((*fast_line = tr68[*fast_line]) IS_LEGAL) fast_line++;
	    c = *fast_line;
            switch (status) {
            case READING:
                if (c == SKIP && fast_line == line_end) break;
                if (c == DONE) {
                    status = FIND_START_COLON;
                    break;
                    }
                status = SKIPPING;
                goto next_line;
            case SKIPPING:
                if (c == SKIP && fast_line == line_end) {
                    status = READING;
                    break;
                    }
		/*  GMT, 1/9/90: Added this clause to avoid losing the last
		 *  line if it was preceeded by a skipped line.  */
                if (c == DONE) { 
		  /* NP 12/3/90: A dirty hack to handle "X-Mailer: ELM [version 2.2 PL14]" */
		  if( (fast_line - line_ptr == X_MAIL_LEN - 1)
		     && (strncmp(line_ptr, X_MAIL_STR, X_MAIL_LEN) == 0)) goto next_line;
                    status = FIND_START_COLON;
                    break;
                    }
                goto next_line;
            case FIND_START_COLON:
                if (*line_start == DONE) {
                    status = READING;
                    line_ptr++;
                    goto scan_line;
                    }
                goto next_line;
                }
            fast_line = line_ptr;
            c = *fast_line++;
	  }

        /* Finally, we have the next 6 bits worth of data */
        switch (state68++) {
        case 0:
            nibble = c;
            goto next_char;
        case 1:
            nibble = (nibble << 6) | c;
            c = nibble >> 4;
            break;
        case 2:
            nibble = (nibble << 6) | c;
            c = (nibble >> 2) & 0xff;
            break;
        case 3:
            c = (nibble << 6) & 0xff | c;
            state68 = 0;
            break;
            }
        if (not_in_a_run)
            if (c != RUNCHAR) *fast_buf++ = c;
            else {not_in_a_run = FALSE; goto next_char;}
        else {
            if (c--) {
                not_in_a_run = buf_end - fast_buf;
                if (c > not_in_a_run) {
                    save_run_length = c - not_in_a_run;
                    c = not_in_a_run;
                    }
                continue_run:
                not_in_a_run = fast_buf[-1];
                while (c--) *fast_buf++ = not_in_a_run;
                }
            else *fast_buf++ = RUNCHAR;
            not_in_a_run = TRUE;
            }
        }
    total_bytes += fast_buf - buf_ptr;
    buf_start[-1] = fast_buf[-1];
    line_ptr = fast_line;
    save_state = state68;
    save_nibble = nibble;
    }


new_in_hqx_file()
{   char *hqx_ext;
    extern char **hqxnames_left;
    if (*hqxnames_left[0] == '\0' || *hqxnames_left[0] == '-') return FALSE;
    strcpy(hqxfname, *hqxnames_left++);
    hqx_ext = hqxfname + strlen(hqxfname) - 4;
    if (!strcmp(hqx_ext, ".hqx"))
        if (!freopen(hqxfname, "r", hqxfile))
            error("Cannot open %s\n", hqxfname);
            else;
    else {
        if (!freopen(hqxfname, "r", hqxfile)) {
            hqx_ext += 4;
            strcpy(hqx_ext, ".hqx");
            if (!freopen(hqxfname, "r", hqxfile)) {
                error("Cannot find %s\n", hqxfname);
            }
        }
      }
    fgets(line_start, MAXLINE, hqxfile);
    return TRUE;
    }

/*
 *  This procedure transparently encodes and writes the hqx output.  
 *  It does run length and 8 to 6 encoding.
 */
empty_hqxbuf()
{   register ulong c, nibble, last_c;
    register byte *fast_buf, *fast_line;
    register int state86, dont_look_for_runs = FALSE, run_length;
    extern int maxlines;

    run_length = save_run_length;
    last_c = buf_start[-1];
    fast_buf = buf_start;
    fast_line = line_ptr;
    state86 = save_state;
    nibble = save_nibble;
    while (fast_buf < buf_ptr) {
        c = *fast_buf++;
        if (dont_look_for_runs) dont_look_for_runs = FALSE;
        else if (last_c == c &&  run_length < MAXRUN) {run_length++; continue;}
        else {
            if (run_length >1) {
                --fast_buf;
                if (run_length == 2 && last_c != RUNCHAR) c = last_c;
                else {
                    c = RUNCHAR;
                    *--fast_buf = run_length;
                    dont_look_for_runs = TRUE;
                    }
                run_length = 1;
                }
            else last_c = c;
            if (c == RUNCHAR && !dont_look_for_runs) {
                *--fast_buf = 0;
                dont_look_for_runs = TRUE;
                }
            }

        if (fast_line == line_end) {
            if (line_count++ == maxlines) new_out_hqx_file();
            fputs(line_start, hqxfile);
            fast_line = line_start;
            }

        switch (state86++) {
        case 0:
            *fast_line++ = tr86[ c >> 2 ];
            nibble = (c << 4) & 0x3f;
            break;
        case 1:
            *fast_line++ = tr86[ (c >> 4) | nibble ];
            nibble = (c << 2) & 0x3f;
            break;
        case 2:
            *fast_line++ = tr86[ (c >> 6) | nibble ];
            if (fast_line == line_end) {
                if (line_count++ == maxlines) new_out_hqx_file();
                fputs(line_start, hqxfile);
                fast_line = line_start;
                }
            *fast_line++ = tr86[ c & 0x3f ];
            state86 = 0;
            break;
            }
        }
    save_run_length = run_length;
    buf_start[-1] = last_c;
    buf_ptr = buf_start;
    line_ptr = fast_line;
    save_state = state86;
    save_nibble = nibble;
    }

new_out_hqx_file()
{   char filename[NAMELEN + 7];
    extern int maxlines;
    fprintf(hqxfile, "<<< End of Part %2d >>>\n", file_count);
    fclose(hqxfile);
    file_count++;
    if (maxlines) sprintf(filename, "%s%02d.hqx", hqxfname, file_count);
    else sprintf(filename, "%s.hqx", hqxfname);
    if ((hqxfile = fopen(filename, "w")) == NULL)
        error("Can't create %s", filename);
    if (file_count > 1)
        fprintf(hqxfile, "<<< Start of Part %2d >>>\n", file_count);
    else fprintf(hqxfile, "(This file must be converted with BinHex 4.0)\n\n");
    line_count = 3;
    }

check_hqx_crc(calc_crc, msg, name)
word calc_crc;
char msg[], name[];
{   word read_crc;
    if (buf_ptr >= buf_end) fill_hqxbuf();
    read_crc = *buf_ptr++ << 8;
    if (buf_ptr >= buf_end) fill_hqxbuf();
    read_crc |= *buf_ptr++;
    if (read_crc != calc_crc) error(msg, name);
    }

write_hqx_crc(calc_crc)
word calc_crc;
{   if (buf_ptr == buf_end) empty_hqxbuf();
    *buf_ptr++ = calc_crc >> 8;
    if (buf_ptr == buf_end) empty_hqxbuf();
    *buf_ptr++ = calc_crc;
    }

un_hqx(unpit_flag)
int unpit_flag;
{   char type[4];
    ulong hqx_datalen, hqx_rsrclen;
    word un_pit();
    int unpitting, bytes_read;
    word calc_crc;
    extern char **hqxnames_left;

    hqxfile = fopen("/dev/null", "r");
    line_end = line_start + HQXLINELEN;
    buf_end = buf_start + HQXBUFLEN;
	for (;;) {
        total_bytes = 0;
        line_ptr = line_start;
        line_ptr[0] = SKIP;
        save_state = 0;
        save_run_length = 0;

        hqx_to_bin_hdr(type, &hqx_datalen, &hqx_rsrclen); /* binfname */

        unpitting = unpit_flag && !strcmp(type, "PIT ");
        if (unpitting) {
            fclose(binfile);
            unlink(binfname);
            bytes_read = total_bytes - (buf_end - buf_ptr);
            calc_crc = un_pit();
            bytes_read = total_bytes - (buf_end - buf_ptr) - bytes_read;
            if (bytes_read != hqx_datalen)
                fprintf(stderr,
                  "Warning - Extraneous characters ignored in %s\n", binfname);
            }
        else calc_crc = hqx_to_bin_fork(hqx_datalen);
        check_hqx_crc(calc_crc, "File data CRC mismatch in %s", binfname);

        calc_crc = hqx_to_bin_fork(hqx_rsrclen);
        check_hqx_crc(calc_crc, "File rsrc CRC mismatch in %s", binfname);

        if (!unpitting) fclose(binfile);
        }
    }

re_hqx()
{   word calc_crc;
    ulong hqx_datalen, hqx_rsrclen;
    extern char **hqxnames_left;
    extern int maxlines;
    line_end = line_start + HQXLINELEN;
    buf_end = buf_start + HQXBUFLEN;
    while (*hqxnames_left[0] != '-') {
        hqxfile = fopen("/dev/null", "w");
        line_count = maxlines;
        file_count = 0;
        line_ptr = line_start;
        *line_ptr++ = ':';
        strcpy(line_end, "\n");
        buf_ptr = buf_start;
        save_state = 0;
        save_run_length = 1;

        bin_to_hqx_hdr(&hqx_datalen, &hqx_rsrclen);   /* calculates hqxfname */

        calc_crc = bin_to_hqx_fork(hqx_datalen);
        write_hqx_crc(calc_crc);

        calc_crc = bin_to_hqx_fork(hqx_rsrclen);
        write_hqx_crc(calc_crc);
		*buf_ptr = !buf_ptr[-1];      /* To end a run and to get the last */
		buf_ptr++;
        empty_hqxbuf();                 /* stray bits, temporarily add a char */
        if (save_state != 2) --line_ptr;
        if (line_ptr == line_end) {
            fputs(line_start, hqxfile);
            line_ptr = line_start;
            }
        strcpy(line_ptr, ":\n");
        fputs(line_start, hqxfile);
        fclose(hqxfile);
        }
    }
