
#ifndef _SIS_NET_H
#define _SIS_NET_H

#include <sis_net.uv.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_net.msg.h>
#include <sis_method.h>
#include <sis_memory.h>
#include <sis_thread.h>
#include <sis_map.h>
#include <sis_net.node.h>
#include <sis_json.h>
#include <sis_message.h>
#include <sis_crypt.h>

#include "sis_list.lock.h"
// 网络协议版本号
#define SIS_NET_VERSION        1   

// 连接方式 谁主动去连接
#define SIS_NET_IO_CONNECT     0   // 主动链接对方 connect client - 需要支持响应ping的功能 
#define SIS_NET_IO_WAITCNT     1   // 被动接收链接 listen accept server - 主动发送ping 

// 角色 连接后 谁来发验证请求
#define SIS_NET_ROLE_REQUEST   0  // client 发起请求方 request
#define SIS_NET_ROLE_ANSWER    1  // server 等待请求，相应请求 answer

// 请求 方决定网络数据压缩方式 
#define SIS_NET_ZIP_NONE       0
#define SIS_NET_ZIP_SNAPPY     1

// 请求 方决定网络数据加密编码方式 
#define SIS_NET_CRYPT_NONE     0
#define SIS_NET_CRYPT_SSL      1

// 网络协议 最外层的数据包
// 以 tcp://127.0.0.1:7329
// 以 redis://127.0.0.1:7329 redis协议格式
// 以 ws://127.0.0.1:7329 
// 以 wss://127.0.0.1:7329
// 以 http://127.0.0.1:7329
// 以 https://127.0.0.1:7329
// 以 file://127.0.0.1:7329 和网络文件交互 需要用户名

#define SIS_NET_PROTOCOL_TCP   0  // 默认为TCP打包协议 仍然是WS包协议
#define SIS_NET_PROTOCOL_WS    1  // 可扩展为WS打包协议 如果是字符串就是原文 如果是字节流 最后必然有一个描述结构体 s_sis_net_tail
#define SIS_NET_PROTOCOL_RDS   2  // 可扩展为redis协议 此时 format compress coded 全部失效

#define SIS_NET_NONE			(0)  // 初始状态
#define SIS_NET_DISCONNECT  	(1)  // 连接断开的状态 
#define SIS_NET_CONNECTED    	(2)  // 连接已建立 - 由于底层已经处理了自动重连 这里就只有两个状态 
#define SIS_NET_HANDING  		(3)  // 客户端:已发送握手信息 等待服务端返回 | 服务端:等待客户端发送握手请求
#define SIS_NET_HANDANS  		(4)  // 服务端:收到客户端握手 并返回数据 等待返回数据完成
#define SIS_NET_LOGINFAIL		(5)  // 未通过登录校验 断开连接 不再自动连接
#define SIS_NET_WORKING  		(6)  // 正常工作状态 可以发送数据了 此时已经处理完底层交互
#define SIS_NET_STOPCONNECT  	(7)  // 用户关闭 不再发起连接
#define SIS_NET_EXIT		    (8)  // 准备退出，通知线程结束工作 结束后 SIS_NET_NONE

#pragma pack(push,1)
/**
 * @brief 网络连接配置数据，从文件中获取，包含IP、端口、加密方式、压缩方式、通讯协议、连接方式、角色方式等
 */
typedef struct s_sis_url {
	uint8          io;     // 连接方式 等待连接 主动去连接两种方式
	uint8          role;        // 角色 client server 两种方式 由客户端发起请求 server 响应请求
	uint8          version;     // 数据交换协议版本号 协议格式变更在这里处理 默认为 1 ws的协议 
    uint8          protocol;    // 通讯协议 -- 默认为 0 tcp 协议     
	uint8          compress;    // 压缩方式 默认不压缩
	uint8          crypt;       // 加密方式 默认不加密   
	char           ip4[16];     // ip地址 
	char           name[128];   // 域名地址 
    int            port;        // 端口号
	s_sis_map_sds *dict;  // 其他字段的对应表 用于不常用的字典数据
} s_sis_url;

// // 二进制协议会在每个数据包最后放入此结构 解包时先从最后取出该结构 
// // 该结构可以增加crc数据校验等功能
// // 因为数据是否被压缩只有压缩后才能直到是否成功 不成功用原文 所以这个结构必须放到数据末尾
// // 也可用此数据判断二进制数据是否合法
// // 字符串数据没有该结构 统一不压缩 不加密
// // 如果需要压缩加密把字符串当字节流处理就可以了
// // 根据ws协议头就知道后面的数据是什么格式 二进制就跟这个结构 字符出就没有这个结构
// typedef struct s_sis_net_tail {
// 	unsigned char is_bytes : 1;     // 数据以什么格式传播
// 	unsigned char is_compress : 1;  // 数据是否被压缩
// 	unsigned char is_crypt : 1;     // 数据是否被加密 
// 	unsigned char is_crc16 : 1;     // 是否有crc16校验 如果有去前面取16个字节用于校验
// 	unsigned char other : 4;        // 备用
// } s_sis_net_tail;

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
typedef int (*sis_net_pack_define)(s_sis_memory *in_, s_sis_net_tail *, s_sis_memory *out_);
typedef int (*sis_net_unpack_define)(s_sis_memory *in_, s_sis_net_tail *, s_sis_memory *out_);

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
/** 网络连接对象存储的与每个客户端对应的上下文数据*/
typedef struct s_sis_net_context {
	uint8              status;       // 当前的工作状态 SIS_NET_WORKING...HANDING DISCONNECT
	int                rid;          // 对端的 socket ID 
	s_sis_url          rurl;         // 客户端的相关信息
	// 网络收到的内容 脱壳 解压 解密 后放入recv_buffer 解析后把完整的数据包 放入主队列 剩余数据保留等待下次收到数据
	s_sis_memory      *recv_memory;  // 接收数据的残余缓存

	s_sis_net_mems    *recv_nodes;   // s_sis_memory 链表

	void              *father;       // s_sis_net_class *的指针
	s_sis_net_slot    *slots;        // 根据协议对接不同功能函数	
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

/** 网络连接处理类，包含网络配置结构体，客户端服务器对象，各种回调函数 */
typedef struct s_sis_net_class {
	s_sis_url            *url;      // 本机需要监听的ip地址

	uint8                 work_status;  // 当前的工作状态 SIS_NET_WORKING...NONE EXIT 3 种状态
	
	s_sis_socket_client  *client;       // 二选一 cxts 只有一条记录
	s_sis_socket_server  *server;    
	// 对应的连接客户的集合 s_sis_net_context
	s_sis_map_kint       *cxts;         // 连接服务器的 s_sis_net_context

	s_sis_wait_thread    *read_thread;  // 为保证每次只处理一个请求 用该线程统一处理收到的消息

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

void sis_url_clone(s_sis_url *scr_, s_sis_url *des_);
void sis_url_set(s_sis_url *, const char *key, char *val);
char *sis_url_get(s_sis_url *, const char *key);

void sis_url_set_ip4(s_sis_url *, const char *ip_);

bool sis_url_load(s_sis_json_node *node_, s_sis_url *url_);

////////////////////////////////////////////////////////
//  为防止频繁拷贝 对 sis_net_message 消息体进行如下包装
////////////////////////////////////////////////////////

s_sis_object *sis_net_send_message(s_sis_net_context *cxt_, s_sis_net_message *mess_);

s_sis_memory *sis_net_make_message(s_sis_net_context *cxt_, s_sis_net_message *mess_);
int sis_net_recv_message(s_sis_net_context *cxt_, s_sis_memory *in_, s_sis_net_message *mess_);

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

void sis_net_class_close_cxt(s_sis_net_class *cls_, int sid_);

int sis_net_class_set_cb(s_sis_net_class *, int sid_, void *source_, cb_net_reply cb_);
// 放到外部交互时使用
// 连接后由客户端决定是否发送login信息 如果不发送就以默认的方式通讯 如果发送服务端收到信息,根据自身支持的协议返回一个确认包
// 服务端就直到客户端传过来是什么格式 客户端也知道了服务端是什么格式
// 此功能实现由外部控制 
int sis_net_class_set_slot(s_sis_net_class *, int sid_, char *compress, char * crypt, char * protocol);

// 不阻塞 不要释放传进来的 s_sis_net_message 系统自己会处理
int sis_net_class_send(s_sis_net_class *, s_sis_net_message *);



#endif //_SIS_NET_H
