
#ifndef _SIS_NET_H
#define _SIS_NET_H

#include <sis_net.uv.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_net.node.h>
#include <sis_method.h>
#include <sis_memory.h>
#include <sis_thread.h>

#define SIS_NET_INIT		  1  // 初始化后
#define SIS_NET_WAITINIT      2  // 等待连接的状态 连接失败或者连接断开 
#define SIS_NET_INITING       3  // 连接前设置状态...
#define SIS_NET_INITED	      4  // 正常工作
#define SIS_NET_EXITING	  5  // 网络服务关闭中...

#define SIS_NET_EXIT          0  // 准备退出，通知线程结束工作,拒绝所有活动请求

// 网络通讯时的角色
#define SIS_NET_FLAG_NONE   0  //
#define SIS_NET_FLAG_REQ    (1<<0)  //A在该状态下为请求端，主动发一个请求到B，
#define SIS_NET_FLAG_ANS    (1<<1)  //A在该状态下为应答端，等待接收一条信息，
#define SIS_NET_FLAG_SUB    (1<<2)  //A在该状态下处于循环接收数据的状态，
#define SIS_NET_FLAG_PUB    (1<<3)  //A在该状态下处于不断发送数据的状态，
// #define SIS_NET_FLAG_REG    (1<<4)  //A在该状态下处于注册服务的状态，接收消息并发送信息
#define SIS_NET_FLAG_BLOCK  (1<<5)  //堵塞，一直等待有一个新的数据收到，再解除堵塞状态，

#define SIS_NET_ROLE_CLIENT    0  // client 发起请求方
#define SIS_NET_ROLE_SERVER    1  // server 等待请求，相应请求

#define SIS_NET_IO_CONNECT     0   // 主动链接对方 connect client - 需要支持响应ping的功能 
#define SIS_NET_IO_LISTEN      1   // 被动接收链接 listen server - 主动发送ping 

#define SIS_NET_SIGN_MAX_LEN  128

typedef struct s_sis_url {
	char io_connect[SIS_NET_SIGN_MAX_LEN];     // 连接方向 connect listen 两种方式
	char io_role[SIS_NET_SIGN_MAX_LEN];        // 角色 client server 两种方式 由客户端发起请求
	char io_ver[SIS_NET_SIGN_MAX_LEN];         // 数据交换协议 子协议 目前仅表示文件格式
    char protocol[SIS_NET_SIGN_MAX_LEN];  // 通讯协议 protocol -- 目前只支持 redis / tcp / file;
    char ip[SIS_NET_SIGN_MAX_LEN];        //ip 对方ip地址 为0.0.0.0表示本机为服务器，等待连接
	//redis://192.168.1.202:3679   从redis上获取数据
	//mongodb://192.168.1.202:3679  从redis上获取数据
	//tcp://127.0.0.1:7328  从tcp对应数据获取数据 io_ver = redis
	//ws://127.0.0.1:7328  从ws对应数据获取数据  io_ver = json 格式
    int  port;  // 端口号
    bool auth;  // 是否需要用户验证
    char username[SIS_NET_SIGN_MAX_LEN];   // username 
    char password[SIS_NET_SIGN_MAX_LEN];   // 密码
	// -------------- //
	int  gap_msec;    // 延迟毫秒数 > 0 表示需要在该延迟后才发送数据 默认为 0 立即发送
	int  gap_size;    // 延迟大小K > 发送缓存大于该值后就立即发送 默认为 0 立即发送
	// -------------- //
	// 下面应该建立一个key-value的对照表，作为指针存在，方便存储各种数据
	char workpath[SIS_PATH_LEN];  // 需要处理的路径名 文件传输时使用 
	char workname[SIS_PATH_LEN];  // 需要处理的文件名 protocol 为file时适用 或表示为数据库名
} s_sis_url;


#define SIS_NET_PROTOCOL_FILE   1
#define SIS_NET_PROTOCOL_TCP    0  // - redis
#define SIS_NET_PROTOCOL_HTTP   2
#define SIS_NET_PROTOCOL_HTTPS  3
#define SIS_NET_PROTOCOL_WS     4
#define SIS_NET_PROTOCOL_WSS    5

struct s_sis_net_class;

typedef void (*callback_net_recv)(struct s_sis_net_class *,s_sis_net_message *);
typedef s_sis_sds (*callback_net_pack)(struct s_sis_net_class *,s_sis_net_message *);
typedef size_t (*callback_net_unpack)(struct s_sis_net_class *,s_sis_net_message *, char* in_, size_t ilen_);

// 消息交换机
typedef struct s_sis_net_switchboard {
	int    cid;    	       // 哪个客户端的信息 -- 仅在server时有用, client时为0
	s_sis_net_message *request;
	callback_net_recv cb_reply; // 回调函数
}s_sis_net_switchboard;

typedef struct s_sis_net_client {
	int cid;

	s_sis_net_message *message;  // set 时消息的指针 有且仅有一条

	s_sis_map_pointer *sub_msgs;// s_sis_net_message
	// sub 时消息存储在这里，有多条，收到消息后需做匹配后回调
	// 收到的消息中 source 一致，若source一致，后面的回调会覆盖前面的回调
	// s_sis_memory *send_buffer;
	s_sis_memory *recv_buffer;
} s_sis_net_client;

typedef struct s_sis_net_class {
	s_sis_url    url;
	
	int connect_style;  // 连接类型 - tcp ws
	int connect_role;   // server client 主动被动
	int connect_method; // listen connect 主动被动

	s_sis_map_pointer *map_command; // command 列表 

	s_sis_socket_server *server;
	s_sis_socket_client *client; // -- 
	s_sis_plan_task *client_thread_connect; // 线程句柄

	int status;         // 当前的工作状态 SIS_NET_INIT...
	int connect_status; // 当前的网络连接状态 SIS_NET_CONNECTED...

	int send_status;
	
	s_sis_mutex_rw mutex_read;
	s_sis_mutex_rw mutex_write;

	s_sis_map_pointer *sessions; // 子客户端链接 s_sis_net_client
	s_sis_map_pointer *sub_clients; // s_sis_string_list cid 列表 
	// 其他人订阅我的key 找不到就不广播 每个键对应一个字符串列表

	// 系统信息回调
	void(*cb_connected)(struct s_sis_net_class *, int cid_); // 连接成功 client时 cid=0
	// 必须等待握手成功，才能发出链接成功
	void(*cb_disconnect)(struct s_sis_net_class *, int cid_); // 断开连接
	// void(*cb_auth)(struct s_sis_net_class *);  // 用户合法校验
	// 数据序列化和反序列化的回调
	callback_net_pack cb_pack;
	callback_net_unpack cb_unpack;

	// 收到数据后的回调函数 
	callback_net_recv cb_recv;  // 收到请求数据
	callback_net_recv_reply cb_recv_reply;  // 收到应答数据

} s_sis_net_class;

s_sis_net_client *sis_net_client_create(int cid_); 
void sis_net_client_destroy(void *);

///////
s_sis_net_class *sis_net_class_create(s_sis_url *url_); //rale_ 表示身份
void sis_net_class_destroy(s_sis_net_class *);

void sis_net_class_set_recv_cb(s_sis_net_class *sock_, callback_net_recv cb_);

void sis_net_class_open(s_sis_net_class *sock_);
void sis_net_class_close(s_sis_net_class *sock_);

// 发送请求 立即返回 发送的信息保存 如果 cb=NULL 表示收到响应直接丢弃
// 如果请求为sub就进入订阅模式，sub模式必须有cb 否则发送失败
int sis_net_class_send(s_sis_net_class *sock_, s_sis_net_message *mess_, callback_net_recv_reply cb_);
// 发送应答 发送后状态设置为正常
int sis_net_class_reply(s_sis_net_class *sock_, s_sis_net_message *mess_);

// 1、底层协议格式 参考redis协议
// https://blog.csdn.net/u014608280/article/details/84586042

// 格式说明：
			// $  --  二进制流 长度+缓存区 -- 
						// $10\r\n1234567890\r\n  表示10个字节长的字符串
						// $-1\r\n  表示空字符串
			// +  --  成功字符串 以\r\n为结束
			// -  --  串错误字符 以\r\n为结束
			// :  --  整数
			// #  --  订阅的键
			// *  --  +数量+数组 可以为任何类型

// 请求格式例子:
			// *3\r\n
			// $3\r\n
			// set\r\n
			// $5\r\n
			// key11\r\n
			// $7\r\n
			// value11\r\n

// 应答格式例子:
			// *3\r\n
			// *2\r\n
			// :1\r\n
			// :2\r\n
			// -no\r\n
			// *2\r\n
			// +ok\r\n
			// $5\r\n
			// 12345\r\n		
			// 表示一个由3个元素组成的数组，第一个元素包含2个整数的数组
			//    第二个元素为一个错误字符串,
			//    第三个元素为一个成功字符串和一个5字节长度的数据缓存的数组

			// *2\r\n
			// +now\r\n
			// $5\r\n
			// 12345\r\n 
			// 表示收到一个标记为now的数据列，通常用于sub时的返回数据

// 2、高级协议格式 主要应用于 ws 协议下的web应用 
// 基于json数据格式，通过驱动来转换数据格式，从协议层来说相当于底层协议的解释层，

// 格式说明：
// id: 以冒号为ID结束符，以确定信息的一一对应关系 不能有{}():符号
// {} json 数据 -- 字符串
// [] array 数据 -- 字符串
// () 其中放数据长度 + 数据 -- 二进制流
// {}()可以拼接
	
	// id  -- 谁干的事结果给谁 针对异步问题数据的对应关系问题
	// cmd -- 干什么
	// key -- 对谁干
	// argv -- 怎么干...

// 请求格式例子:
// id:{"cmd":"sisdb.get","key":"sh600601.info","argv":[11,"sssss",{"min":16}]}
	// argv参数只能为数值、字符串，[],{}，必须能够被js解析，
// id:{"cmd":"sisdb.set","key":"sh6006001.info"}(10)1234567890(5)12345
	// 传递二进制数据流方式 后面跟的数据类比到argv中

// 应答格式例子:
// id:[[],[]] 
// id:{"fields":{},"values":[[]]}
// id:{"fields":{},"groups":{"sh600600":[]}}
// id:(10)1234567890(5)12345 二进制数据流


// id的意义如下：
// 	所有的消息都以字符串或数字为开始，以冒号为结束；
// 	system:为系统类消息，比如链接、登录、PING等信息；
// 	不放在{}中的原因是解析简单，快速定位转发
// 	1.作为转发中间件时，aaa.bbb.ccc的方式一级一级下去，返回数据后再一级一级上来，最后返回给发出请求的端口，
// 	  方便做分布式应用的链表；因为该值会有变化，和请求数据分开放置；只有内存拷贝没有json解析
// 	2.作为订阅和发布，当客户订阅了某一类数据，异步返回数据时可以准确的定位数据源，
// 	3.对于一应一答的阻塞请求，基本可以忽略不用；
//  4.对于客户端发出多个请求，服务端返回数据时以stock来定位请求；


// #define SIS_NET_PROTOCOL_TYPE  "redis,tcp,http,https,ws,wss"


// 	int style = sis_str_subcmp(url_->protocol,SIS_NET_PROTOCOL_TYPE,',');
// 	if (style < 0) {
// 		return NULL;
// 	}
//     	switch (style)
// 	{
// 		case SIS_NET_PROTOCOL_REDIS:
// 			sock->open = sis_redis_open;
// 			sock->close = sis_redis_close;

// 			sock->socket_send_message = sis_redis_send_message;
// 			sock->socket_query_message = sis_redis_query_message;
// 			break;	
// 		default:
// 			sock->status = SIS_NET_ERROR;
// 			break;
// 	}

#endif //_SIS_NET_H
