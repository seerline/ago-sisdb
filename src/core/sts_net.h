
#ifndef _STS_NET_H
#define _STS_NET_H

#include <os_net.h>
#include <os_thread.h>
#include <sts_malloc.h>
#include <sts_str.h>
#include <sts_node.h>

#define STS_NET_INIT			1  //初始化后的状态
#define STS_NET_ERROR			2  //初始化没有的协议
#define STS_NET_EXIT		    0  //准备退出，通知线程结束工作

#define STS_NET_CONNECTED    3  //正常工作
#define STS_NET_CONNECTING   4  //连接前设置状态...
#define STS_NET_WAITCONNECT  5  //等待连接的状态 连接失败或者连接断开
#define STS_NET_CONNECTSTOP  6  //不再发起连接

// 网络通讯时的角色
#define STS_NET_ROLE_NONE   0  //
#define STS_NET_ROLE_REQ    (1<<0)  //A在该状态下为请求端，主动发一个请求到B，
#define STS_NET_ROLE_ANS    (1<<1)  //A在该状态下为应答端，等待接收一条信息，
#define STS_NET_ROLE_SUB    (1<<2)  //A在该状态下处于循环接收数据的状态，
#define STS_NET_ROLE_PUB    (1<<3)  //A在该状态下处于不断发送数据的状态，
//#define STS_NET_ROLE_REG    (1<<4)  //A在该状态下处于注册服务的状态，接收消息并发送信息

#define STS_NET_SIGN_MAX_LEN  128

#define STS_NET_PROTOCOL_TYPE  "redis,tcp,http,https,ws,wss"
#define STS_NET_PROTOCOL_REDIS  0
#define STS_NET_PROTOCOL_TCP    1
#define STS_NET_PROTOCOL_HTTP   2
#define STS_NET_PROTOCOL_HTTPS  3
#define STS_NET_PROTOCOL_WS     4
#define STS_NET_PROTOCOL_WSS    5

typedef struct s_sts_url {
    char protocol[STS_NET_SIGN_MAX_LEN];  //protocol -- 目前只支持 redis / tcp / file;
    char ip[STS_NET_SIGN_MAX_LEN];  
                        //ip 对方ip地址 为*表示本机为服务器，等待连接
						//redis://192.168.1.202:3679   从redis上获取数据
						//mongodb://192.168.1.202:3679  从redis上获取数据
						//owner://SH:0  从market列表选取code为SH的数据接口，0备用
    int  port;  // 端口号
    bool auth;  // 是否需要用户验证
    char username[STS_NET_SIGN_MAX_LEN];   // username 
    char password[STS_NET_SIGN_MAX_LEN];   // 密码
	// -------------- //
	char dbname[STS_NET_SIGN_MAX_LEN];  // 数据库名或文件名  
} s_sts_url;

typedef struct s_sts_socket {
	s_sts_url    url;
	int          rale;
	int          flags;    //当前状态  req 等待发请求, ans 发完请求等待应答, 应答接收后状态修改为req， 
						   //订阅状态，sub 只接受数据; pub只发送数据, ans可以同时存在；
						   //  - client    STS_NET_ROLE_REQ | STS_NET_ROLE_SUB 发完请求后处于订阅状态
						   //  - service   STS_NET_ROLE_REQ | STS_NET_ROLE_SUB | STS_NET_ROLE_PUB 
	int    status;         //当前的网络连接状态 STS_NET_CONNECTED...

	s_sts_mutex_rw  mutex_message_send;    //发送队列锁
	s_sts_list     *message_send;   /* 需要处理的发送信息列表 */

	s_sts_mutex_rw  mutex_message_recv;    //接收信息锁
	s_sts_list     *message_recv;  /* 需要处理的接收到的信息列表 */

	void          *handle_read;    //通过该句柄发出的消息不等待回应，由线程接受信息，线程循环接受redis发布过来的信息，然后写入message_recv，
	int            status_read;    //当前的状态；
	s_sts_mutex_rw mutex_read;

	void          *handle_write; //一应一答的通用操作模式
	int            status_write; //当前的状态；
	s_sts_mutex_rw mutex_write;

	s_sts_list *pub_keys;  /* 其他人订阅我的key */
	s_sts_list *sub_keys;  /* 我要订阅别人的key */

	bool fastwork_mode; //快速通道模式
	int  fastwork_count;//记录数
	int  fastwork_bytes;//字节数

	//仅仅向服务器发送信息，不需要返回响应信息
	bool(*socket_send_message)(struct s_sts_socket *, s_sts_message_node *);
	//发信息并等待服务器响应
	s_sts_message_node * (*socket_query_message)(struct s_sts_socket *, s_sts_message_node *); 

	void(*open)(struct s_sts_socket *);
	void(*close)(struct s_sts_socket *);
	
} s_sts_socket;


s_sts_socket *sts_socket_create(s_sts_url *url_,int rale_); //rale_ 表示身份
void sts_socket_destroy(s_sts_socket *sock_);

void sts_socket_open(s_sts_socket *sock_);
void sts_socket_close(s_sts_socket *sock_);

bool sts_socket_send_message(s_sts_socket *sock_, s_sts_message_node *mess_);
s_sts_message_node *sts_socket_query_message(s_sts_socket *sock_, s_sts_message_node *mess_);

#endif //_STS_NET_H
