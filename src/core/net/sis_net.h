
#ifndef _SIS_NET_H
#define _SIS_NET_H

#include <sis_net.uv.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_net.node.h>
#include <sis_method.h>
#include <sis_memory.h>
#include <sis_thread.h>
#include <sis_map.h>
#include <sis_json.h>
#include <sis_queue.h>
#include <sis_message.h>
#include <sis_compress.h>
#include <sis_crypt.h>
// 网络协议版本号
#define SIS_NET_VERSION        1   

// 连接方式 谁主动去连接
#define SIS_NET_IO_CONNECT     0   // 主动链接对方 connect client - 需要支持响应ping的功能 
#define SIS_NET_IO_WAITCNT     1   // 被动接收链接 listen accept server - 主动发送ping 

// 角色 连接后 谁来发验证请求
#define SIS_NET_ROLE_REQUEST   0  // client 发起请求方 request
#define SIS_NET_ROLE_ANSWER    1  // server 等待请求，相应请求 answer

// request 方决定网络数据压缩方式 
#define SIS_NET_ZIP_NONE       0
#define SIS_NET_ZIP_SNAPPY     1

// request 方决定网络数据加密编码方式 
#define SIS_NET_CRYPT_NONE     0
#define SIS_NET_CRYPT_SSL      1

// 网络协议 最外层的数据包
#define SIS_NET_PROTOCOL_TCP   0  // 默认为TCP打包协议 仍然是WS包协议
#define SIS_NET_PROTOCOL_WS    1  // 可扩展为WS打包协议 如果是字符串就是原文 如果是字节流 最后必然有一个描述结构体 s_sis_memory_info
#define SIS_NET_PROTOCOL_RDS   2  // 可扩展为redis协议 此时 format compress coded 全部失效

#define SIS_NET_NONE			(0)  // 初始状态
#define SIS_NET_DISCONNECT  	(1)  // 连接断开的状态 
#define SIS_NET_CONNECTED    	(2)  // 连接已建立 - 由于底层已经处理了自动重连 这里就只有两个状态 
#define SIS_NET_HANDING  		(3)  // 握手交换数据中 ws 需要握手
#define SIS_NET_LOGINFAIL		(4)  // 未通过登录校验 断开连接 不再自动连接
#define SIS_NET_WORKING  		(5)  // 正常工作状态 可以发送数据了 此时已经处理完底层交互
#define SIS_NET_STOPCONNECT  	(8)  // 用户关闭 不再发起连接
#define SIS_NET_EXIT		    (9)  // 准备退出，通知线程结束工作 结束后 SIS_NET_NONE

#pragma pack(push,1)
// 从配置文件中获取的数据
typedef struct s_sis_url {
	uint8     io;     // 连接方式 等待连接 主动去连接两种方式
	uint8     role;        // 角色 client server 两种方式 由客户端发起请求 server 响应请求
	uint8     version;     // 数据交换协议版本号 协议格式变更在这里处理 默认为 1 ws的协议 
    uint8     protocol;    // 通讯协议 -- 默认为 0 tcp 协议     
	uint8     compress;    // 压缩方式 默认不压缩
	uint8     crypt;       // 加密方式 默认不加密   
	char      ip4[16];      // ip地址 
	char      name[128];   // 域名地址 
    int       port;        // 端口号
    // bool      auth;        // 是否需要用户验证 放在外层实际业务中处理
    // char      username[128];    // username 
    // char      password[128];    // 密码
	s_sis_map_sds *dict;  // 其他字段的对应表 用于不常用的字典数据
} s_sis_url;

// 二进制协议会在每个数据包最后放入此结构 解包时先从最后取出该结构 
// 该结构可以增加crc数据校验等功能
// 因为数据是否被压缩只有压缩后才能直到是否成功 不成功用原文 所以这个结构必须放到数据末尾
// 也可用次数据判断二进制数据是否合法
// 字符串数据没有该结构 统一不压缩 不加密
// 如果需要压缩加密把字符串当字节流处理就可以了
// 根据ws协议头就知道后面的数据是什么格式 二进制就跟这个结构 字符出就没有这个结构
typedef struct s_sis_memory_info {
	unsigned char is_bytes : 1;     // 数据以什么格式传播
	unsigned char is_compress : 1;  // 数据是否被压缩
	unsigned char is_crypt : 1;     // 数据是否被加密 
	unsigned char is_crc16 : 1;     // 是否有crc16校验 如果有去前面取16个字节用于校验
	unsigned char other : 4;     // 备用
} s_sis_memory_info;

// 序列化和反序列化 程序内部的数据和网络通讯协议互转
typedef bool (*sis_net_encoded_define)(s_sis_net_message *in_, s_sis_memory *out_);
typedef bool (*sis_net_decoded_define)(s_sis_memory* in_, s_sis_net_message *out_);
// 压缩和解压 第二道工序 
typedef bool (*sis_net_compress_define)(s_sis_memory *in_, s_sis_memory *out_);
#define sis_net_uncompress_define sis_net_compress_define
// 加密和解密 第三道工序
#define sis_net_encrypt_define sis_net_compress_define
#define sis_net_decrypt_define sis_net_compress_define
// 把数据根据协议打包写入队列中 第四道工序 
// 把来源乱序数据 解包后放入队列中 需要传送的数据可能会拆包也这样处理
typedef int (*sis_net_pack_define)(s_sis_memory *in_, s_sis_memory_info *, s_sis_memory *out_);
typedef int (*sis_net_unpack_define)(s_sis_memory *in_, s_sis_memory_info *, s_sis_memory *out_);

typedef struct s_sis_net_slot {
// 先编码 再压缩 最后再加密
	sis_net_encoded_define  slot_net_encoded; 
	sis_net_decoded_define  slot_net_decoded;
	sis_net_compress_define slot_net_compress;
	sis_net_compress_define slot_net_uncompress;
	sis_net_encrypt_define  slot_net_encrypt;
	sis_net_decrypt_define  slot_net_decrypt;
	sis_net_pack_define     slot_net_pack; 
	sis_net_unpack_define   slot_net_unpack; // 如果有脏数据 需要移动到正确的位置 并返回最新的数据 
} s_sis_net_slot;

void sis_net_slot_set(s_sis_net_slot *slots, uint8 compress, uint8 crypt, uint8 protocol);

// 收到消息后的回调 s_sis_net_message - 
typedef void (*cb_net_reply)(void *, s_sis_net_message *);

typedef struct s_sis_net_context {
	uint8              status;  // 当前的工作状态 SIS_NET_WORKING...HANDING DISCONNECT

	int                rid;     // 对端的 socket ID 
	s_sis_url          rurl;    // 客户端的相关信息

	// 网络收到的内容 脱壳 解压 解密 后放入recv_buffer 解析后把完整的数据包 放入主队列 剩余数据保留等待下次收到数据
	s_sis_memory      *recv_buffer; // 接收数据的残余缓存

	void                 *father;   // s_sis_net_class *的指针
	s_sis_share_list     *ready_send_cxts; // 准备发送的数据 s_sis_net_message - s_sis_object
	s_sis_share_reader   *reader_send;  // 读取发送队列 等待上一个读取结束的读者

	s_sis_net_slot    *slots;     // 根据协议对接不同功能函数	

	void              *cb_source;    // 回调句柄
	cb_net_reply       cb_reply;     // 应答回调

} s_sis_net_context;


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// 对用户来说 连接已建立 配置信息已经存在url中 连接和验证的工作交给 net_class 用户只管通知可以正常工作的时候开始工作
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// 处理流程 建立连接后 class 收到网络信息 就转存到 cxts 并解包 写入 ready_recv_cxts 就返回
//			reader_recv 收到最新包信息 回调处理函数 处理函数如果有数据回写就放入 ready_send_cxts 队列
//			reader_send 收到发送数据 打包完成后 直接写入网络
//          外层只用跟 s_sis_net_message 进行交互 其他全部由 class 处理
/////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct s_sis_net_class {
	s_sis_url            *url;      // 本机需要监听的ip地址

	uint64                ask_sno;
	uint8                 work_status;    // 当前的工作状态 SIS_NET_WORKING...NONE EXIT 3 种状态
	// s_sis_net_slot        slots;     // 请求方使用 	
	
	s_sis_socket_client  *client;    // 二选一 cxts 只有一条记录
	s_sis_socket_server  *server;    
	// 对应的连接客户的集合 s_sis_net_context
	s_sis_map_pointer    *cxts;      // 连接服务器的 s_sis_net_context
	// s_sis_net_context    *client_cxt;       // 客户端的上下文信息
	// 发出的请求,以时间顺序保存 回来的数据一一对应调用回调
	s_sis_pointer_list   *ask_lists;  // s_sis_net_message - s_sis_object

	// recv放的是解包后的数据 send 放的是打包后的数据
	// 刚出队列的数据 等待处理成功后释放 
	// s_sis_object         *after_recv_cxt;  // 当前接收到的数据 等待处理成功后删除 s_sis_net_message 
	// 可能在一个数据包中有多个请求 一次解析逐个放入队列 
	s_sis_share_list     *ready_recv_cxts; // 接收到的数据  s_sis_net_message - s_sis_object
	s_sis_share_reader   *reader_recv;  // 读取接收队列

	// 当前正在发送的信息 刚出队列的 发送成功后释放
	// s_sis_object         *after_send_cxt;  // 已经发送的数据 等待发送成功后删除 s_sis_memory
	// 发送队列自行释放内存
	// s_sis_share_list     *ready_send_cxts; // 准备发送的数据 s_sis_net_message - s_sis_object
	// s_sis_share_reader   *reader_send;  // 读取发送队列

	void                 *cb_source;     // 回调句柄
	cb_socket_connect     cb_connected;  // 链接成功
	cb_socket_connect     cb_disconnect; // 链接断开

} s_sis_net_class;

#pragma pack(pop)

bool sis_net_is_ip4(const char *ip_);

/////////////////////////////////////////////////////////////
// s_sis_url define 
/////////////////////////////////////////////////////////////

s_sis_url *sis_url_create(); 
void sis_url_destroy(void *);

void sis_url_set_ip4(s_sis_url *, const char *ip_);

void sis_url_clone(s_sis_url *scr_, s_sis_url *des_);
void sis_url_set(s_sis_url *, const char *key, char *val);
char *sis_url_get(s_sis_url *, const char *key);

bool sis_url_load(s_sis_json_node *node_, s_sis_url *url_);

////////////////////////////////////////////////////////
//  为防止频繁拷贝 对 sis_net_message 消息体进行如下包装
////////////////////////////////////////////////////////

s_sis_object *sis_net_send_message(s_sis_net_context *cxt_, s_sis_net_message *mess_);

/////////////////////////////////////////////////////////////
// s_sis_net_context define 
/////////////////////////////////////////////////////////////

s_sis_net_context *sis_net_context_create(s_sis_net_class *, int); 
void sis_net_context_destroy(void *);

/////////////////////////////////////////////////////////////
// s_sis_net_class define 
/////////////////////////////////////////////////////////////

s_sis_net_class *sis_net_class_create(s_sis_url *url_); //rale_ 表示身份
void sis_net_class_destroy(s_sis_net_class *);

bool sis_net_class_open(s_sis_net_class *);
void sis_net_class_close(s_sis_net_class *);

void sis_net_class_delete(s_sis_net_class *cls_, int sid_);

int sis_net_class_set_cb(s_sis_net_class *, int sid_, void *source_, cb_net_reply cb_);
// 放到外部交互时使用
// 连接后由客户端决定是否发送login信息 如果不发送就以默认的方式通讯 如果发送服务端收到信息,根据自身支持的协议返回一个确认包
// 服务端就直到客户端传过来是什么格式 客户端也知道了服务端是什么格式
// 此功能实现由外部控制 
int sis_net_class_set_slot(s_sis_net_class *, int sid_, char *compress, char * crypt, char * protocol);

// 不阻塞 不要释放传进来的 s_sis_net_message 系统自己会处理
int sis_net_class_send(s_sis_net_class *, s_sis_net_message *);

// 阻塞直到有返回 超过 wait_msec 没有数据返回就超时退出
// int sis_net_class_ask_wait(s_sis_net_class *, s_sis_object *send_, s_sis_object *recv_);

/////////////////////////////////////////////////////////////
// 协议说明
/////////////////////////////////////////////////////////////
// 1、底层协议格式 以ws为基础协议 实际数据协议 二进制和JSON混杂格式两种
// 客户端发起连接 连接认证通过后, 
// 客户端 发送 1:{ cmd:login, argv:{version : 1, compress : snappy, format: json, username : xxx, password : xxx}}
// 服务端 成功返回 1: { login : ok }
//       失败返回 1: { logout : "format 不支持."}
// 对回调没有要求的 不填来源 则包以:开头
// 所有标志符不能带 :

// 2、二进制协议格式 主要应用于快速数据交互 
// 二进制协议交互 握手后 必须交互一次结构化数据的字典表 
// 客户端凡事需要发送的二进制结构体都需要首先发送给服务端 并唯一命名 服务器亦然 以便对方能认识自己
// 客户端 发送 2: { cmd:dict, argv:{info:{fields:[[],[]]},market:{fields:[[],[]]},trade:{fields:[[],[]]}} }
// 服务端 发送 2: { cmd:dict, argv:{info:{fields:[[],[]]},market:{fields:[[],[]]},trade:{fields:[[],[]]}} }
// 之后客户端发送格式为 3:set:key:sdb:+..... 对应json的几个参数 key sdb 

// 这些交互底层处理掉 上层收到的就是字符串数据 

// 3、JSON协议格式 主要应用于 ws 协议下的web应用 
// 基于json数据格式，通过驱动来转换数据格式，从协议层来说相当于底层协议的解释层，

// 格式说明：
// id: 以冒号为ID结束符，以确定信息的一一对应关系 不能有{}():符号
// {} json 数据 -- 字符串
// [] array 数据 -- 字符串
	
	// id  -- 谁干的事结果给谁 针对异步问题数据的对应关系问题 [必选]
	// cmd -- 干什么 [必选]
	// key -- 对谁干 [可选]
	// sdb -- 对什么属性干 [可选] 
	// argv -- 怎么干... [可选]

// 请求格式例子:
// id:{"cmd":"sisdb.get","key":"sh600601","sdb":"info","argv":[11,"sssss",{"min":16}]}
	// argv参数只能为数值、字符串，[],{}，必须能够被js解析，
// id:{"cmd":"sisdb.set","key":"sh600601","sdb":"info","argv":[{"time":100,"newp":100.01}]}

// 应答格式例子:
// id:{} 通常返回信息类查询
// id:[[],[]] 单key单sdb
// id:{"000001.info":[[],[]],"000002.info":[[],[]]} 多key或多sdb 
// id:{"info.fields":[[],[]]} 返回字段信息 也可以混合在以上的返回结构


// id的意义如下：
// 	所有的消息都以字符串或数字为开始，以冒号为结束；
// 	system:为系统类消息，比如链接、登录、PING等信息；
// 	不放在{}中的原因是解析简单，快速定位转发
// 	1.作为转发中间件时，aaa.bbb.ccc的方式一级一级下去，返回数据后再一级一级上来，最后返回给发出请求的端口，
// 	  方便做分布式应用的链表；因为该值会有变化，和请求数据分开放置；只有内存拷贝没有json解析
// 	2.作为订阅和发布，当客户订阅了某一类数据，异步返回数据时可以准确的定位数据源，
// 	3.对于一应一答的阻塞请求，基本可以忽略不用；
//  4.对于客户端发出多个请求，服务端返回数据时以stock来定位请求；


#endif //_SIS_NET_H
