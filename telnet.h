#ifndef _TELNET_H_RPCGEN
#define _TELNET_H_RPCGEN

#include <rpc/rpc.h>


#ifdef __cplusplus
extern "C" {
#endif


struct MyData {
	struct {
		u_int message_len;
		char *message_val;
	} message;
	int state;
	int client_id;
};
typedef struct MyData MyData;

#define MY_PROGRAM 0x31230000
#define MY_VERSION 1

#if defined(__STDC__) || defined(__cplusplus)
#define EXECUTE 1
extern  MyData * execute_1(MyData *, CLIENT *);
extern  MyData * execute_1_svc(MyData *, struct svc_req *);
extern int my_program_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define EXECUTE 1
extern  MyData * execute_1();
extern  MyData * execute_1_svc();
extern int my_program_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_MyData (XDR *, MyData*);

#else /* K&R C */
extern bool_t xdr_MyData ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_TELNET_H_RPCGEN */
