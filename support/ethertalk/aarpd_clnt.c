/*
 * RPC client calls for CAP with Native EtherTalk
 *
 * Created: Charles Hedrick, Rutgers University <hedrick@rutgers.edu>
 * Modified: David Hornsby, Melbourne University <djh@munnari.OZ.AU>
 *	16/02/91: add rtmp_[sg]etbaddr_clnt()
 *	28/04/91: add range_set_clnt()
 */

#ifdef SOLARIS
#define PORTMAP
#endif SOLARIS
#include <rpc/rpc.h>
#include <sys/time.h>
#include <netat/sysvcompat.h>
#include "aarpd.h"

bool_t xdr_etheraddr();
bool_t xdr_bridgeaddr();

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

/*
 * AARP resolver
 */

u_char *
aarp_resolve_clnt(argp, clnt)
	int *argp;
	CLIENT *clnt;
{
	static etheraddr res;
	bzero((char *)res, sizeof(res));
	if (clnt_call(clnt, AARP_RESOLVE, xdr_int, (caddr_t)argp,
	    xdr_etheraddr, (char *)res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (res);
}

/*
 * Get the bridge address
 */

u_char *
rtmp_getbaddr_clnt(argp, clnt)
	int *argp;
	CLIENT *clnt;
{
	static bridgeaddr res;
	bzero((char *)res, sizeof(res));
	if (clnt_call(clnt, RTMP_GETBADDR, xdr_int, (caddr_t)argp,
	    xdr_bridgeaddr, (char *)res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (res);
}

/*
 * Set the bridge address
 */

u_char *
rtmp_setbaddr_clnt(argp, clnt)
	int *argp;
	CLIENT *clnt;
{
	static bridgeaddr res;
	bzero((char *)res, sizeof(res));
	if (clnt_call(clnt, RTMP_SETBADDR, xdr_int, (caddr_t)argp,
	    xdr_bridgeaddr, (char *)res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (res);
}

#ifdef PHASE2
/*
 * Set the network range
 */

u_char *
range_set_clnt(argp, clnt)
	int *argp;
	CLIENT *clnt;
{
	static bridgeaddr res;	/* convenient size */
	bzero((char *)res, sizeof(res));
	if (clnt_call(clnt, NET_RANGE_SET, xdr_int, (caddr_t)argp,
	    xdr_bridgeaddr, (char *)res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (res);
}
#endif PHASE2
