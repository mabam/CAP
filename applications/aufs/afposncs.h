/*
 * $Author: djh $ $Date: 91/02/15 21:08:58 $
 * $Header: afposncs.h,v 2.1 91/02/15 21:08:58 djh Rel $
 * $Revision: 2.1 $
*/


/*
 *  afposncs.h - Appletalk Filing Protocol OS Normalizing for Character Sets
 *
 * structures used to hold normalizing character set routines and declarations
 *
 *
 *  Feb 1988 Charlie C. Kim 
 *
 */

typedef struct {		/* trans tab */
  char *ncs_trans_name;		/* Translation name */
  char *ncs_suffix;		/* Suffix to translate on */
  int ncs_suffix_len;
  int ncs_conj;			/* conjunction (suffix <conj> creator/type */
  byte *ncs_file_creator;	/* File creator to xlate on */
  int ncs_fc_len;
  byte *ncs_file_type;		/* File type to xlate on */
  int ncs_ft_len;
  byte *ncs_totable;
  byte *ncs_fromtable;
} NCS_TransTable;

/* extern TransTab transtables[]; */

char *ncs_tt_name();
int ncs_istrans();

#define NCS_TRANS_UNIXTOMAC 1
#define NCS_TRANS_MACTOUNIX 2

#define NCS_AND 0
#define NCS_OR 1
