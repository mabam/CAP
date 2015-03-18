
/* sit.h: contains declarations for SIT headers */

struct sitHdr {		/* 22 bytes */
	u_char	sig1[4];		/* = 'SIT!' -- for verification */
	u_char	numFiles[2];	/* number of files in archive */
	u_char	arcLen[4];		/* length of entire archive incl. */
	u_char	sig2[4];		/* = 'rLau' -- for verification */
	u_char	version;		/* version number */
	char reserved[7];
};

struct fileHdr {	/* 112 bytes */
	u_char	compRMethod;		/* rsrc fork compression method */
	u_char	compDMethod;		/* data fork compression method */
	u_char	fName[64];			/* a STR63 */
	char	fType[4];			/* file type */
	char	fCreator[4];		/* creator... */
	char	FndrFlags[2];		/* copy of Finder flags */
	char	cDate[4];			/* creation date */
	char	mDate[4];			/* !restored-compat w/backup prgms */
	u_char	rLen[4];			/* decom rsrc length */
	u_char	dLen[4];			/* decomp data length */
	u_char	cRLen[4];			/* compressed lengths */
	u_char	cDLen[4];
	u_char	rsrcCRC[2];			/* crc of rsrc fork */
	u_char	dataCRC[2];			/* crc of data fork */
	char	reserved[6];
	u_char	hdrCRC[2];			/* crc of file header */
};

/* file format is:
	sitArchiveHdr
		file1Hdr
			file1RsrcFork
			file1DataFork
		file2Hdr
			file2RsrcFork
			file2DataFork
		.
		.
		.
		fileNHdr
			fileNRsrcFork
			fileNDataFork
*/



/* compression methods */
#define noComp	0	/* just read each byte and write it to archive */
#define repComp 1	/* RLE compression */
#define lpzComp 2	/* LZW compression */
#define hufComp 3	/* Huffman compression */

/* all other numbers are reserved */

/*
 * the format of a *.info file made by xbin
 */
struct infohdr {
	char	res0;
	char	name[64];	/*  2 (a str 63) */
	char	type[4];	/* 65 */
	char	creator[4];	/* 69 */
	char	flag[2];	/* 73 */
	char	res1[8];
	char	dlen[4];	/* 83 */
	char	rlen[4];	/* 87 */
	char	ctime[4];	/* 91 */
	char	mtime[4];	/* 95 */
};
