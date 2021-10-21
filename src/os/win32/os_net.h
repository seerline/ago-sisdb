
#ifndef _OS_NET_H
#define _OS_NET_H

#include <sis_os.h>
#include <os_str.h>

#define SIS_NET_MAX_SEND_LEN   32*1024
#define SIS_NET_MAX_RECV_LEN   32*1024

#ifndef socklen_t 
typedef int socklen_t;
#endif
#ifndef int64_addr
typedef unsigned __int64  int64_addr;
#endif

#ifdef __cplusplus
extern "C" {
#endif
void sis_socket_init();
void sis_socket_uninit();

int sis_socket_getip4(const char *name_, char *ip_, size_t ilen_);

#define sis_net_recv(a,b,c,d) read(a,b,c)
#define sis_net_send(a,b,c,d) write(a,b,c)

#define sis_net_close close

#ifdef __cplusplus
}
#endif
#endif //_OS_NET_H
