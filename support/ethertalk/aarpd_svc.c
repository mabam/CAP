/*
 * RPC dispatcher
 *
 * Created: Charles Hedrick, Rutgers University <hedrick@rutgers.edu>
 * Modified: David Hornsby, Melbourne University <djh@munnari.OZ.AU>
 *	16/02/91: add RTMP_[SG]ETBADDR
 *	28/04/91: add NET_RANGE_SET
 *
 */

#include <stdio.h>
#ifdef SOLARIS
#define PORTMAP
#endif SOLARIS
#include <rpc/rpc.h>
#include <netat/sysvcompat.h>
#include "aarpd.h"

bool_t xdr_etheraddr();
bool_t xdr_bridgeaddr();

extern char *aarp_resolve_svc();
extern char *rtmp_getbaddr_svc();
extern char *rtmp_setbaddr_svc();
#ifdef PHASE2
extern char *range_set_svc();
#endif PHASE2

void
aarpdprog(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		int aarp_resolve_svc_arg;
	} argument;

	char *result, *(*local)();
	bool_t (*xdr_argument)(), (*xdr_result)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case AARP_RESOLVE:
		xdr_argument = xdr_int;
		xdr_result = xdr_etheraddr;
		local = (char *(*)()) aarp_resolve_svc;
		break;

	case RTMP_GETBADDR:
		xdr_argument = xdr_int;
		xdr_result = xdr_bridgeaddr;
		local = (char *(*)()) rtmp_getbaddr_svc;
		break;

	case RTMP_SETBADDR:
		xdr_argument = xdr_int;
		xdr_result = xdr_bridgeaddr;
		local = (char *(*)()) rtmp_setbaddr_svc;
		break;

#ifdef PHASE2
	case NET_RANGE_SET:
		xdr_argument = xdr_int;
		xdr_result = xdr_bridgeaddr;
		local = (char *(*)()) range_set_svc;
		break;
#endif PHASE2
		
	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL
	 && !svc_sendreply(transp, xdr_result, (caddr_t)result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
}
