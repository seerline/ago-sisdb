#include<stdio.h>
#include<stdlib.h>
#include"uv.h"
 
#define NUM 1
uv_tcp_t server;
#define DEFAULT_BACKLOG 1000
 
uv_loop_t *loop;
struct sockaddr_in dest[NUM];
static int count = -1;
uv_buf_t writebuf[] = {
    {.base = "ABCD",.len = 4},
    {.base = "2",.len = 1}
    };
    
uv_buf_t writebuf1[] = {
    {.base = "3",.len = 1},
    {.base = "4",.len = 1}
    };
 
void creaddr()
{
    int i;
    for(i = 0;i<NUM;i++)
    {
        uv_ip4_addr("127.0.0.1",6000+i,&dest[i]);
    };
}
 
void writecb(uv_write_t *req,int status)
{
   if(status)
   {
       printf("write error");
       //uv_close((uv_handle_t*)req,NULL);
       return;
   }
 
   printf("write succeed!");
}
 
void on_connect(uv_connect_t *req,int status)
{
    if(status) 
    {
        printf("connect error");
        //uv_close((uv_handle_t*)req,NULL);
        return;
    }
    
    struct sockaddr addr;
    struct sockaddr_in addrin;
    int addrlen = sizeof(addr);
    char sockname[17] = {0};
 
    struct sockaddr addrpeer;
    struct sockaddr_in addrinpeer;
    int addrlenpeer = sizeof(addrpeer);
    char socknamepeer[17] = {0};
    
    if(0 == uv_tcp_getsockname((uv_tcp_t*)req->handle,&addr,&addrlen) && 
        0 ==uv_tcp_getpeername((uv_tcp_t*)req->handle,&addrpeer,&addrlenpeer))
    {
        addrin = *((struct sockaddr_in*)&addr);
        addrinpeer = *((struct sockaddr_in*)&addrpeer);
        uv_ip4_name(&addrin,sockname,addrlen);
        uv_ip4_name(&addrinpeer,socknamepeer,addrlenpeer);
        printf("%s:%d sendto %s:%d",sockname, ntohs(addrin.sin_port),socknamepeer, ntohs(addrinpeer.sin_port));  
    }
    else
        printf("get socket fail!\n");
    
    printf(" connect succeed!");  
}
 
void cretcpc(uv_timer_t *timer)
{
    if(++count >= NUM)
    {
        uv_timer_stop(timer);
        printf("\ntimer stop!\n");
        return;
    }
    
    uv_tcp_t *socket = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop,socket);
    
    struct sockaddr_in addrlocal;
    // uv_ip4_addr("127.0.0.1",10000,&addrlocal);
    uv_ip4_addr("127.0.0.1",0,&addrlocal);
    uv_tcp_bind(socket, (const struct sockaddr*)&addrlocal, 0);  
    printf("\nsocket init:%d %d  ",count,ntohs(addrlocal.sin_port));
    
    uv_connect_t *connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
    if(0 == uv_tcp_connect(connect,socket,(const struct sockaddr*)&dest[count],on_connect))
    {
        // uv_write_t writereq;
        // uv_write(&writereq,(uv_stream_t*)socket,writebuf,2/*sizeof(writebuf)/sizeof(uv_buf_t)*/,writecb);
        
        uv_write_t writereq1;
        uv_write(&writereq1,(uv_stream_t*)socket,writebuf1,2/*sizeof(writebuf)/sizeof(uv_buf_t)*/,writecb);
    }
    else
    {
        uv_close((uv_handle_t*)socket,NULL);
        printf("close socket! %d\n",count);
    }
}
 
int main()
{
    loop = uv_default_loop();
 
    creaddr();
    uv_timer_t timer;
    uv_timer_init(loop,&timer);
    
    uv_timer_start(&timer,cretcpc,1000,20);
 
    return uv_run(loop,UV_RUN_DEFAULT);;
}

