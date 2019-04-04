#include <stdio.h>  
#include <stdlib.h>  
#include "uv.h"  
 
#define DEFAULT_BACKLOG 1000
#define NUM 1/*1013*/
 
uv_loop_t *loop;  
struct sockaddr_in addr[NUM];  
struct sockaddr_in addrCli;  
uv_tcp_t server[NUM];  
 
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {  
    buf->base = (char*) malloc(suggested_size);  
    buf->len = suggested_size;  
}  
  
void echo_write(uv_write_t *req, int status) {  
    if (status) {  
        fprintf(stderr, "Write error %s\n", uv_strerror(status));  
    }  
    free(req);  
}  
  
void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{    
    if (nread < 0) 
    {  
        if (nread != UV_EOF)  
            printf("Read error %d\n",(int)nread);  
        uv_close((uv_handle_t*) client, NULL);  
        return;
    } 
    else if (nread > 0) 
    {  
        printf("read msg : %s\n",buf->base);  
        uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t)); 
        uv_buf_t wrbuf = uv_buf_init(buf->base, nread); 
        uv_write(req, client, &wrbuf, 1, echo_write);
    }  
 
    struct sockaddr addrlo1;
    struct sockaddr_in addrin1;  
    int addrlen1 = sizeof(addrlo1);
    char sockname1[17] = {0};
 
    struct sockaddr addrpeer1;
    struct sockaddr_in addrinpeer1;    
    int addrlenpeer1 = sizeof(addrpeer1);
    char socknamepeer1[17] = {0};
 
    int ret1 = uv_tcp_getsockname((const uv_tcp_t *)client,&addrlo1,&addrlen1);
    int ret2 = uv_tcp_getpeername((const uv_tcp_t *)client,&addrpeer1,&addrlenpeer1);
    if(0 ==  ret1 && 0 == ret2)
    {
        addrin1 = *((struct sockaddr_in*)&addrlo1);
        addrinpeer1 = *((struct sockaddr_in*)&addrpeer1);
        uv_ip4_name(&addrin1,sockname1,addrlen1);
        uv_ip4_name(&addrinpeer1,socknamepeer1,addrlenpeer1);
        printf("re %s:%d From %s:%d \n",sockname1, ntohs(addrin1.sin_port)/*,buf->base*/,socknamepeer1, ntohs(addrinpeer1.sin_port));  
    }
    else 
        printf("get socket fail %d %d\n",ret1,ret2);
    
    /*buf->base 需要处理，会影响打印*/
    if (buf->base)  
        free(buf->base);  
    
    uv_close((uv_handle_t*) client, NULL); 
}  
  
void on_new_connection(uv_stream_t *server, int status) {  
    if (status < 0) 
    {  
        printf("New connection error %s\n", uv_strerror(status));  
        return;  
    }  
      
    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));  
    uv_tcp_init(loop, client);  
 
    //client->data = server;暂不需要
    
    if (uv_accept(server, (uv_stream_t*) client) == 0) {  
        uv_read_start((uv_stream_t*) client, alloc_buffer, echo_read);  
       // uv_close((uv_handle_t*) client, NULL); 关闭后读不到数据
    }  
    else {  
        uv_close((uv_handle_t*) client, NULL);  
    }  
}  
 
void creaddr()
{
    int i;
    for(i = 0;i<NUM;i++)
    {
        uv_ip4_addr("127.0.0.1",6000+i,&addr[i]);
        uv_tcp_init(loop, &server[i]);  
        uv_tcp_bind(&server[i], (const struct sockaddr*)&addr[i], 0);  
 
        if (uv_listen((uv_stream_t*)&server[i], DEFAULT_BACKLOG, on_new_connection)) 
        {  
            printf("Listen error %d\n",6000+i);  //最大支持1014
            return;  
        }  
        else
        {
            printf("Listen ok %d\n",6000+i);
        }
        
    };
}
 
int main() 
{  
    loop = uv_default_loop();  
 
    creaddr();
 
    return uv_run(loop, UV_RUN_DEFAULT);
}  

