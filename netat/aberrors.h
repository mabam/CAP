/*
 * $Author: djh $ $Date: 91/02/15 22:58:28 $
 * $Header: aberrors.h,v 2.1 91/02/15 22:58:28 djh Rel $
 * $Revision: 2.1 $
 *
*/

/*
 * aberrors.h - AppleTalk Errors
 *
 * AppleTalk package for UNIX (4.2 BSD).
 *
 * Copyright (c) 1986, 1987, 1988 by The Trustees of Columbia University 
 *   in the City of New York.
 *
 *
 * Edit History:
 *
 *  June 30, 1986    Schilit	Created.
 *
 */

/* Error codes */

#define noErr 		0

#define	ddpSktErr	-91	/* error in socket number */
#define	ddpLenErr	-92	/* data length too big */
#define	noBridgeErr	-93	/* no net bridge for non-local send */
#define	lapProtErr	-94	/* error in attach/detach */
#define	excessCollsns	-95	/* excessive collisions on write */
#define	portInUse	-97	/* driver Open error */
#define	portNotCf	-98	/* driver Open error */

/* ATP error codes */

#define	reqFailed	-1096	/* SendRequest failed: retrys exceeded */
#define tooManyReqs 	-1097	/* Too many concurrent requests */
#define tooManySkts 	-1098	/* Socket table full */
#define	badATPSkt	-1099	/* bad ATP responding socket */
#define badBuffNum  	-1100	/* Bad sequence number */
#define	noRelErr	-1101	/* no release received */
#define cbNotFound  	-1102	/* Control block not found */
#define noSendResp  	-1103	/* atpaddrsp issued before sndresp */
#define	noDataArea	-1104	/* no data area for MPP request */
#define	reqAborted	-1105	/* SendRequest aborted by RelTCB */

/* NBP error codes */

#define	nbpBuffOvr	-1024	/* buffer overflow in LookupName */
#define	nbpNoConfirm	-1025	/* name not confirmed on ConfirmName */
#define	nbpConfDiff	-1026	/* name confirmed at different socket */
#define	nbpDuplicate	-1027	/* duplicate name already exists */
#define	nbpNotFound	-1028	/* name not found on remove */
#define	nbpNISErr	-1029	/* error trying to open the NIS */

#define buf2SmallErr 	-3101
#define noMPPErr     	-3102
#define ckSumErr     	-3103
#define extractErr   	-3104
#define readQErr     	-3105
#define atpLenErr    	-3106
#define atpBadRsp    	-3107
#define recNotFnd    	-3108
#define sktClosed 	-3109

/* ASP Errors */
/* errors, don't really belong here, but... */
/* -1060 thru -1065 belong to US!!! yeah! */
#define BadReqRcvd -1060	/* improper request recv'ed  */
#define noATPResource -1061	/* problems with atp resource */
#define aspFault -1062		/* internal error */
#define BadVersNum -1066
#define BufTooSmall -1067
#define NoMoreSessions -1068
#define NoServers -1069
#define ParamErr -1070
#define ServerBusy -1071
#define SessClosed -1072
#define SizeErr -1073
#define TooManyClients -1074
#define NoAck -1075

