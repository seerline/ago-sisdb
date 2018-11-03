
#ifndef _OS_NET_H
#define _OS_NET_H

// #include <sys/time.h> 
// #include <unistd.h>
// #include <strings.h>
#include <signal.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>     // sockaddr_in, "man 7 ip" ,htons
#include <netinet/tcp.h>
#include <arpa/inet.h>   //inet_addr,inet_aton
#include <poll.h>             //poll,pollfd
#include <unistd.h>        //read,write
#include <netdb.h>         //gethostbyname

#define SIS_NET_MAX_SEND_LEN   32*1024
#define SIS_NET_MAX_RECV_LEN   32*1024

void sis_socket_init();
void sis_socket_uninit();

#define sis_net_recv(a,b,c,d) read(a,b,c)
#define sis_net_send(a,b,c,d) write(a,b,c)

#define sis_net_close close


#endif //_OS_NET_H
