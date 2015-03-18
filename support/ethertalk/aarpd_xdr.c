/*
 * Created: Charles Hedrick, Rutgers University <hedrick@rutgers.edu>
 *
 */

#include <rpc/rpc.h>
#include "aarpd.h"

bool_t
xdr_etheraddr(xdrs, objp)
	XDR *xdrs;
	etheraddr objp;
{
	if (!xdr_vector(xdrs, (char *)objp, 6, sizeof(u_char), xdr_u_char)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_bridgeaddr(xdrs, objp)
	XDR *xdrs;
	bridgeaddr objp;
{
	if (!xdr_vector(xdrs, (char *)objp, 4, sizeof(u_char), xdr_u_char)) {
		return (FALSE);
	}
	return (TRUE);
}

