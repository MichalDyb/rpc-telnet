#include "telnet.h"

bool_t
xdr_MyData (XDR *xdrs, MyData *objp)
{
	register int32_t *buf;

	 if (!xdr_bytes (xdrs, (char **)&objp->message.message_val, (u_int *) &objp->message.message_len, ~0))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->state))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->client_id))
		 return FALSE;
	return TRUE;
}
