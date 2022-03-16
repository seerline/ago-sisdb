
#include <sis_net.uv.h>
#include <sis_math.h>

// ***************************************************** //
// 实在没搞懂libuv的运行机制 明明已经 close 了所有handle
// 执行 uv_stop 后仍然不能保证从 uv_run 中优雅的退出
// 只能在timer.c中的 uv__next_timeout 函数中设置结点为空时的超时为20毫秒
// 原始为 -1 (永远等待) 暂时解决这个问题, 等以后 libuv 升级后看看有没有其他解决方案
// 理想状态是 uv_stop 后系统自动清理所有未完成的工作 然后返回 
// ***************************************************** //
// 理想状态下 libuv负责网络通讯 有数据了用回调返回 所有网络时间通过回调可以解决
// 用户需要发送数据时 应该把发送的数据作为事件注册到libuv中就不管了，由libuv自行调度把数据推到对端
// 如果发生错误，回调出来 交给用户处理 是否重连重发等等
// ***************************************************** //

// 下面的函数可以清理所有的未释放的句柄
// typedef void *QUEUE[2];
// #define QUEUE_NEXT(q)       (*(QUEUE **) &((*(q))[0]))
// #define QUEUE_DATA(ptr, type, field)  ((type *) ((char *) (ptr) - offsetof(type, field)))
// #define QUEUE_FOREACH(q, h)  for ((q) = QUEUE_NEXT(h); (q) != (h); (q) = QUEUE_NEXT(q))

// void uv_clear_active_handles(uv_loop_t *loop)
// {
// 	if (loop == NULL || (int)loop->active_handles == -1)
// 	{
// 		return;
// 	}
// 	QUEUE *list;
// 	uv_handle_t *handle;
// 	QUEUE_FOREACH(list, &loop->handle_queue)
// 	{
// 		handle = QUEUE_DATA(list, uv_handle_t, handle_queue);
// 		if (uv_is_active(handle))
// 		{
// 			printf("flags : %x type :%d %p\n",
// 				   handle->flags, handle->type, (void *)handle);
// 			uv_read_stop((uv_stream_t *)handle);
// 			uv_close(handle, NULL);
// 		}
// 	}
// }
// --- SIGPIPE 需要忽略这个信号 ----
// 在多CPU测试 经常会剩余几个包发不出去 这个可能和 wait_queue 有关 避免问题 做一个裸的测试

/////////////////////////////////////////////////
//  s_sis_net_nodes
/////////////////////////////////////////////////
// 速度测试

#if 0

#include <signal.h>

int __isclient = 1;
int __exit = 0;
#define  MAX_TEST_CLIENT 1

s_sis_socket_server *server = NULL;

typedef struct s_test_client {
	int sno;
	int status;
	s_sis_socket_client *client;
} s_test_client;

s_test_client *client[MAX_TEST_CLIENT];

void exithandle(int sig)
{
	printf("--- exit .%d. %d\n", __exit, sig);
	if (__exit == 1)
	{
		exit(0);
	}
	__exit = 1;
	if (__isclient)
	{
		for (int i = 0; i < MAX_TEST_CLIENT; i++)
		{
			if (client[i])
			{
				sis_socket_client_destroy(client[i]->client);
				sis_free(client[i]);
			}
			/* code */
		}	
	}
	else
	{
		sis_socket_server_destroy(server);
	}
	__exit = 2;
	printf("--- exit .ok . \n");
}
#define SPEED_SERVER 1 
// 测试服务端发送数据的速度 client记录收到字节数
// 经过测试比实际带宽慢10倍
size_t speed_send_size = 0;
size_t speed_recv_size = 0;
size_t speed_recv_curr = 0;
msec_t speed_recv_msec = 0;

// #define ACTIVE_CLIENT 1 // client主动发请求

int sno = 0;
static void cb_server_recv_after(void *handle_, int sid_, char* in_, size_t ilen_)
{
	printf("--- server recv [%d]:%zu \n", sid_, ilen_);
	// printf("server recv [%d]:%zu [%s]\n", sid_, ilen_, in_);	
#ifdef ACTIVE_CLIENT
	if (sno == 0)
	{	sno = 1;
		s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
		s_sis_sds str = sis_sdsempty();
		str = sis_sdscatfmt(str,"server recv cid =  %i", sid_);
		s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
		sis_socket_server_send(srv, sid_, obj);
		sis_object_destroy(obj);
	}
#endif
}
static void cb_client_send_after(void* handle_, int cid, int status)
{
	// s_test_client *cli = (s_test_client *)handle_;
	// if (cli->sno < 10*1000)
	// {
	// 	cli->sno++;
	// 	s_sis_sds str = sis_sdsempty();
	// 	str = sis_sdscatfmt(str,"i am client. [sno = %i]. %i", cli->sno, (int)sis_time_get_now());
	// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
	// 	sis_socket_client_send(cli->client, obj);
	// 	sis_object_destroy(obj);
	// }
}
#ifdef SPEED_SERVER
int sendsize = 10000;

static void cb_client_recv_after(void* handle_, int cid, char* in_, size_t ilen_)
{
	speed_recv_size += ilen_;
	int nums = speed_recv_size/sendsize;
	if (nums % 100000 == 0)
	{
		printf("client recv [%d]:%d %d\n", cid, ilen_, nums);
	}
	if (speed_recv_msec == 0) 
	{
		speed_recv_msec = sis_time_get_now_msec();
	}
	else 
	{
		int offset = sis_time_get_now_msec() - speed_recv_msec;
		if (sis_time_get_now_msec() - speed_recv_msec > 1000)
		{
			speed_recv_msec = sis_time_get_now_msec();
			printf("--- cost : %5d recv : %12zu, %10zu speed(M/s): %zu\n", offset, speed_recv_size, speed_recv_curr, speed_recv_curr/offset/1000);
			speed_recv_curr = 0; 
		}
	}
	speed_recv_curr += ilen_;
}
uv_thread_t server_thread;

// void _thread_write(void* arg)
// {
// 	s_test_client *client = (s_test_client *)arg;
// 	int id = client->sno;
// 	int count = 20*1000*1000; 
// 	// 10*1000*1000 10240000
// 	s_sis_sds str = sis_sdsnewlen(NULL, sendsize);
// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
// 	for (int i = 0; i < count; i++)
// 	{
// 		sis_socket_server_send(server, id, obj);
// 		if (i%1000 == 0)
// 		{
// 			sis_sleep(2);
// 		}
// 	}
// 	sis_object_destroy(obj);
// 	printf("--- send end. %p %d \n", client, id);	
// 	sis_free(client);
// }
void _thread_write(void* arg)
{
	s_test_client *client = (s_test_client *)arg;
	int id = client->sno;
	char buffer[16*1024];
	int count = 20*1000*1000; 
	// 10*1000*1000 10240000
	s_sis_memory *mem = sis_memory_create_size(sendsize);
	sis_memory_cat(mem, buffer, 16*10);
	for (int i = 0; i < count; i++)
	{
		sis_socket_server_send(server, id, mem);
		if (i%1000 == 0)
		{
			sis_sleep(2);
		}
	}
	sis_memory_destroy(mem);
	printf("--- send end. %p %d \n", client, id);	
	sis_free(client);
}
static void cb_new_connect(void *handle_, int sid_)
{
	printf("--- new connect . sid_ = %d \n", sid_);	
	s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
	sis_socket_server_set_rwcb(srv, sid_, cb_server_recv_after, NULL);
	
	s_test_client *client = SIS_MALLOC(s_test_client, client);
	client->sno = sid_;
	printf("--- send start. %p %d \n", client, sid_);	
	uv_thread_create(&server_thread, _thread_write, client);
}

static void cb_connected(void *handle_, int sid_)
{
	printf("--- client connect\n");	
}
#else
static void cb_client_recv_after(void* handle_, int cid, char* in_, size_t ilen_)
{
	printf("client recv: cid : %d [%zu] [%s]\n", cid, ilen_, in_);	
#ifndef ACTIVE_CLIENT
	s_test_client *cli = (s_test_client *)handle_;
	sis_socket_client_send(cli->client, "client recv.",13);
#endif
}

static void cb_new_connect(void *handle_, int sid_)
{
	printf("new connect . sid_ = %d \n", sid_);	
	s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
	sis_socket_server_set_rwcb(srv, sid_, cb_server_recv_after, NULL);
#ifndef ACTIVE_CLIENT
	char str[128];
	sis_sprintf(str, 128, "i am server. [cid=%d].", sid_);
	sis_socket_server_send(srv, sid_, str, strlen(str));
#endif
}

static void cb_connected(void *handle_, int sid_)
{
	printf("client connect\n");	
#ifdef ACTIVE_CLIENT
	s_test_client *cli = (s_test_client *)handle_;
	cli->status = 1;
	cli->sno = 0;
	s_sis_sds str = sis_sdsempty();
	str = sis_sdscatfmt(str,"i am client. [sno = %i].", cli->sno);
	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
	// printf("send : %s\n", str);	
	// for (int i = 0; i < 10000; i++)
	{
		sis_socket_client_send(cli->client, obj);
	}
	// sis_socket_client_send(cli->client, obj);
	sis_object_destroy(obj);
#endif
}
#endif

static void cb_disconnected(void *handle_, int sid_)
{
	printf("--- client disconnect\n");	
	s_test_client *cli = (s_test_client *)handle_;
	cli->status = 0;
}

uv_thread_t connect_thread;
int main(int argc, char **argv)
{
	safe_memory_start();

	// sis_signal(SIGINT, exithandle);
	// sis_signal(SIGKILL, exithandle);
	// // sis_sigignore(SIGPIPE);
	// for (int i = 0; i < 1000000; i++)
	// {
	// 	sis_signal(i, exithandle);
	// }
	// sis_sigignore(SIGPIPE);

	if (argc < 2)
	{
		// it is server
		__isclient = 0;

		server = sis_socket_server_create();

		server->port = 7777;
		sis_strcpy(server->ip, 128, "0.0.0.0");

		sis_socket_server_set_cb(server, cb_new_connect, NULL);

		sis_socket_server_open(server);

		printf("--- server working ... \n");

	}
	else
	{
		for (int i = 0; i < MAX_TEST_CLIENT; i++)
		{
			client[i] = (s_test_client *)sis_malloc(sizeof(s_test_client));
			client[i]->client = sis_socket_client_create();
			client[i]->client->cb_source = client[i];

			client[i]->client->port = 7777;
			sis_strcpy(client[i]->client->ip, 128, "127.0.0.1");

			sis_socket_client_set_cb(client[i]->client, cb_connected, cb_disconnected);
			sis_socket_client_set_rwcb(client[i]->client, cb_client_recv_after, cb_client_send_after);

			sis_socket_client_open(client[i]->client);

			printf("--- client working ... %d \n", client[i]->status);
			/* code */
		}
		

	}

	while (__exit != 2)
	{
		// printf("--- check memory --- \n");
		// safe_memory_stop();
		sis_sleep(1000);
		// sis_sleep(50);
	}
	safe_memory_stop();
	return 0;	
}
#endif

#if 0

uv_thread_t server_thread;

void _thread_write(void* arg)
{
	int count = 20*1000*1000; 
	s_sis_sds *data = sis_malloc(sizeof(s_sis_sds) * count);
	int start = 0;
	for (int i = 0; i < count; i++)
	{
		data[i] = sis_sdsnewlen(NULL, 10000);
		if (i % 100000 == 0)
		{
			for (int k = start; k < i; k++)
			{
				sis_sdsfree(data[k]);
			}
			sis_sleep(1000);
			printf("---free %d -> %d 100M\n", start, i);
			start = i;
		}
	}
	sis_free(data);
}
// 内存释放测试
int main(int argc, char **argv)
{
	safe_memory_start();

	uv_thread_create(&server_thread, _thread_write, NULL);

	while (1)
	{
		sis_sleep(1000);
		safe_memory_stop();
	}
}
#endif

#if 0

#define SPEED_SERVER 1 
// 测试服务端发送数据的速度 client记录收到字节数
// 经过测试比实际带宽慢10倍
size_t speed_send_size = 0;
size_t speed_recv_size = 0;
size_t speed_recv_curr = 0;
msec_t speed_recv_msec = 0;

int __exit = 0;
uv_loop_t *server_loop = NULL;
uv_loop_t *client_loop = NULL;

uv_tcp_t    server_handle;	
uv_thread_t server_thread;

uv_tcp_t    client_handle;	
uv_thread_t client_thread;

uv_buf_t read_buffer;
uv_buf_t write_buffer;


static void cb_client_recv_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (nread < 0) 
	{
		exit(100);
	}
	speed_recv_size += nread;
	if (speed_recv_msec == 0) 
	{
		speed_recv_msec = sis_time_get_now_msec();
	}
	else 
	{
		int offset = sis_time_get_now_msec() - speed_recv_msec;
		if (sis_time_get_now_msec() - speed_recv_msec > 1000)
		{
			speed_recv_msec = sis_time_get_now_msec();
			printf("cost : %5d recv : %12zu, %10zu speed(M/s): %lld\n", offset, speed_recv_size, speed_recv_curr, (long long)speed_recv_curr/offset/1000);
			speed_recv_curr = 0; 
		}
	}
	speed_recv_curr += nread;
}
static void cb_client_recv_before(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (suggested_size > read_buffer.len)
	{
		sis_free(read_buffer.base);
		read_buffer.base = sis_malloc(suggested_size + 1024);
		read_buffer.len = suggested_size + 1024;
	}
	*buffer = read_buffer;
}
static void _cb_client_connect(uv_connect_t *handle, int status)
{
	if(uv_read_start(handle->handle, cb_client_recv_before, cb_client_recv_after)) 
		exit(2);

}
int count = 0;
int maxcount = 1000*1000*10;
int sendsize = 16384;
uv_write_t write_req;

#define THREAD_WRITE 1
#ifndef THREAD_WRITE
// 直接写可以跑满带宽
static void cb_server_write_ok(uv_write_t *requst, int status)
{
	if (count > maxcount)
	{
		printf("send ok\n");
		return ;
	}
	if(uv_write(&write_req, (uv_stream_t*)&client_handle, &write_buffer, 1, cb_server_write_ok))
	{
		exit(4);
	}
	count++;
}

static void cb_new_connect(uv_stream_t *handle, int status)
{
	printf("new connect . sid_ = 1 \n");	
	if(uv_tcp_init(server_loop, &client_handle)) exit(2);

	if (uv_accept((uv_stream_t*)&server_handle, (uv_stream_t*)&client_handle)) exit(3);
	
	count = 0;
	// 直接写数据基本能跑满 10G - 7G
	cb_server_write_ok(NULL, 0);
}
#else
// 这样写也是可以的 做好同步就行了 写入就发送 发送完再读数据有数据就处理 没数据就等待
// 无论如何libuv上次数据传输完毕后 必须一次性把临时缓存中数据全部发出去 否则速度慢
// 但奇怪的是如果在发送成功回调中发送数据 就很快 怀疑是如果不在在回调中发送数据 就会进入消息队列
// 而我再次写入数据时，消息队列可能有未知的耗时操作导致速度慢了
// 无论如何 要求只要发送就必须把需要发送的一次性推出去，否则速度会很慢
// 这样又要求每个客户端自己保存自己的数据 有点痛苦 等空的时候换掉他 

s_sis_struct_list  *write_list;
s_sis_mutex_t       mmmm;
s_sis_wait_thread  *work_thread;
volatile int may_write = 0;

static void cb_server_write_ok(uv_write_t *requst, int status);
static void _write_one()
{
	may_write = 0;
	printf("=1=%d\n", may_write);
	// if(uv_write(&write_req, (uv_stream_t*)&client_handle, &write_buffer, 1, cb_server_write_ok))
	if(uv_write(&write_req, (uv_stream_t*)&client_handle, 
		sis_struct_list_first(write_list), write_list->count, cb_server_write_ok))
	{
		printf("write fail\n");
	}
}

static void cb_server_write_ok(uv_write_t *requst, int status)
{
	may_write = 1;
	printf("=2=%d\n", may_write);
	// _write_one();
	// sis_wait_thread_notice(work_thread);
}

void _thread_read1(void* arg)
{
	// return ;
	while (1)
    {
		if (may_write == 1)
		{
			printf("=3=%d %d\n", may_write, write_list->count);
			if (write_list->count > 0)
			{
				// sis_mutex_lock(&mmmm);
				// sis_struct_list_pop(write_list);
				_write_one();
				sis_struct_list_clear(write_list);
				// sis_mutex_unlock(&mmmm);
			}
		}
		else
		{
			// printf("%d\n", may_write);
		}
	}
}
// void _thread_read(void* arg)
// {
// 	sis_wait_thread_start(work_thread);
// 	while (sis_wait_thread_noexit(work_thread))
//     {
// 		if (may_write == 1)
// 		{
// 			if (write_list->count > 0)
// 			{
// 				// sis_mutex_lock(&mmmm);
// 				// sis_struct_list_pop(write_list);
// 				_write_one();
// 				// sis_mutex_unlock(&mmmm);
// 			}
// 		}
// 		sis_sleep(1);
// 		// printf("timeout exit. %d \n", 1);
// 		// if (sis_wait_thread_wait(work_thread, 50) == SIS_WAIT_TIMEOUT)
// 		// {
// 		// 	printf("timeout exit. %d \n", 2);
// 		// }  
// 	}
// 	sis_wait_thread_stop(work_thread);
// }
void _thread_write(void* arg)
{
	uv_buf_t buffer;
	buffer.base = write_buffer.base;
	buffer.len = write_buffer.len;
	for (int i = 0; i < maxcount; i++)
	{
		// sis_mutex_lock(&mmmm);
		sis_struct_list_push(write_list, &buffer);
		// sis_mutex_unlock(&mmmm);
	}
	may_write = 1;
	// cb_server_write_ok(NULL, 0);
	// sis_wait_thread_notice(work_thread);
	// sis_mutex_unlock(&mmmm);
	printf("write count = %d %d\n", may_write, write_list->count);
}

static void cb_new_connect(uv_stream_t *handle, int status)
{
	printf("new connect . sid_ = 1 \n");	
	if(uv_tcp_init(server_loop, &client_handle)) exit(2);

	if (uv_accept((uv_stream_t*)&server_handle, (uv_stream_t*)&client_handle)) exit(3);
	
	// may_write = 1;
	count = 0;
	// 启动一个线程 写看看速度
	write_list = sis_struct_list_create(sizeof(uv_buf_t));
	sis_struct_list_set_size(write_list, maxcount + 1);
	
	uv_thread_create(&server_thread, _thread_write, NULL);
	uv_thread_create(&client_thread, _thread_read1, NULL);
}
#endif
int main(int argc, char **argv)
{
	if (argc < 2)
	{
#ifdef THREAD_WRITE
		sis_mutex_create(&mmmm);
		// work_thread = sis_wait_thread_create(50);
		// if (!sis_wait_thread_open(work_thread, _thread_read, NULL))
		// {
		// 	LOG(1)("can't start reader thread.\n");
		// 	return NULL;
		// }
#endif
		// it is server
		write_buffer = uv_buf_init((char*)sis_malloc(sendsize), sendsize);
		server_loop = sis_malloc(sizeof(uv_loop_t)); 
		uv_loop_init(server_loop);


		if(uv_tcp_init(server_loop, &server_handle)) exit(0);


		struct sockaddr_in bind_addr;
		if(uv_ip4_addr("127.0.0.1", 7777, &bind_addr)) exit(0);

		if(uv_tcp_bind(&server_handle, (const struct sockaddr*)&bind_addr, 0)) exit(0);
		
		if(uv_listen((uv_stream_t*)&server_handle, SOMAXCONN, cb_new_connect)) exit(0);
		printf("server ok\n");
		uv_run(server_loop, UV_RUN_DEFAULT);

		uv_loop_close(server_loop); 		
	}
	else
	{
		read_buffer = uv_buf_init((char*)sis_malloc(sendsize), sendsize);

		client_loop = sis_malloc(sizeof(uv_loop_t)); 
		uv_loop_init(client_loop);

		if (uv_tcp_init(client_loop, &client_handle)) 
		{
			printf("==0==\n");
			return 0;
		}
		struct sockaddr_in bind_addr;
		if (uv_ip4_addr("127.0.0.1", 7777, &bind_addr)) 
		{
			printf("==1==\n");
			return 0;
		}

		uv_connect_t connect_req;
		if(uv_tcp_connect(&connect_req, &client_handle, (const struct sockaddr*)&bind_addr, _cb_client_connect)) 
		{
			printf("==2==\n");
			return 0;
		}

		printf("client ok\n");
		uv_run(client_loop, UV_RUN_DEFAULT); 
		printf("client ok\n");
		uv_loop_close(client_loop); 
	}

	return 0;
}
#endif

#if 0

#include <signal.h>
uv_loop_t *__loop;
int __isclient = 1;
int __exit = 0;
#define  MAX_TEST_CLIENT = 10;
uv_thread_t thandle;

void exithandle(int sig)
{
	printf("exit .1. \n");
	
	uv_stop(__loop);
	printf("exit .ok . \n");

	uv_thread_join(&thandle);  

	__exit = 1;
}

int64_t counter = 0;

void wait_for_a_while(uv_idle_t* handle) {
    counter++;

    if (counter >= 1000000)
    {
		uv_idle_stop(handle);
	}
	if (counter%1000 == 0)
	{
		printf("Idling... %d\n", counter);
		sis_sleep(10);
	}
}

void _thread_run(void* arg)
{
	uv_run(__loop, UV_RUN_DEFAULT);
	
	printf("thread end . \n");

	uv_loop_close(__loop);
	sis_free(__loop);
}

int main() {
	signal(SIGINT, exithandle);

	__loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(__loop));

    uv_idle_t idler;

    uv_idle_init(__loop, &idler);
    uv_idle_start(&idler, wait_for_a_while);

    printf("Idling...\n");
    // uv_run(__loop, UV_RUN_DEFAULT);
	uv_thread_create(&thandle, _thread_run, NULL);

	while (!__exit)
	{
		sis_sleep(300);
	}

	printf("close . \n");
    return 0;
}
#endif
#if 0
// sendfile //

#include <stdio.h>
#include <uv.h>
#include <stdlib.h>
 
uv_loop_t *loop;
#define DEFAULT_PORT 7000
 
uv_tcp_t mysocket;
 
char *path = NULL;
uv_buf_t iov;
char buffer[128];
 
uv_fs_t read_req;
uv_fs_t open_req;
void on_read(uv_fs_t *req);
void on_write(uv_write_t* req, int status)
{
    if (status < 0) 
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
        uv_fs_t close_req;
        uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
        uv_close((uv_handle_t *)&mysocket,NULL);
        exit(-1);
    }
    else 
    {
        uv_fs_read(uv_default_loop(), &read_req, open_req.result, &iov, 1, -1, on_read);
    }
}
 
void on_read(uv_fs_t *req)
{
    if (req->result < 0) 
    {
        fprintf(stderr, "Read error: %s\n", uv_strerror(req->result));
    }
    else if (req->result == 0) 
    {
        uv_fs_t close_req;
        // synchronous
        uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
        uv_close((uv_handle_t *)&mysocket,NULL);
    }
    else
    {
        iov.len = req->result;
        uv_write((uv_write_t *)req,(uv_stream_t *)&mysocket,&iov,1,on_write);
    }
}
 
void on_open(uv_fs_t *req)
{
    if (req->result >= 0) 
    {
        iov = uv_buf_init(buffer, sizeof(buffer));
        uv_fs_read(uv_default_loop(), &read_req, req->result,&iov, 1, -1, on_read);
    }
    else 
    {
        fprintf(stderr, "error opening file: %s\n", uv_strerror((int)req->result));
        uv_close((uv_handle_t *)&mysocket,NULL);
        exit(-1);
    }
}
 
void on_connect(uv_connect_t* req, int status)
{
    if(status < 0)
    {
        fprintf(stderr,"Connection error %s\n",uv_strerror(status));
 
        return;
    }
 
    fprintf(stdout,"Connect ok\n");
 
    uv_fs_open(loop,&open_req,path,O_RDONLY,-1,on_open);
 
 
}
 
int main(int argc,char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr,"Invaild argument!\n");
 
        exit(1);
    }
    loop = uv_default_loop();
 
    path = argv[1];
 
    uv_tcp_init(loop,&mysocket);
 
    struct sockaddr_in addr;
 
    uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
 
 
    uv_ip4_addr("127.0.0.1",DEFAULT_PORT,&addr);
 
    int r = uv_tcp_connect(connect,&mysocket,(const struct sockaddr *)&addr,on_connect);
 
    if(r)
    {
        fprintf(stderr, "connect error %s\n", uv_strerror(r));
        return 1;
    }
 
    return uv_run(loop,UV_RUN_DEFAULT);
}
#endif
