#ifndef _SIS_NET_NODE_H
#define _SIS_NET_NODE_H

#include "sis_core.h"
#include "sis_list.h"
#include "sis_malloc.h"
#include "sis_memory.h"

//------------------------sdsnode ------------------------------//
// 非常好的数据结构，基本可以满足所有网络通许所有的信息了                //
//--------------------------------------------------------------//

#define SIS_NET_SOURCE   0x01    // 有 source
#define SIS_NET_CMD      0x02    // 有 cmd
#define SIS_NET_KEY      0x04    // 有 key
#define SIS_NET_VAL      0x08    // 有 val
#define SIS_NET_ARGVS    0x10    // 有 argvs
#define SIS_NET_INSIDE   0x20    // 内部通信
#define SIS_NET_ASK      0x40    // 请求
// #define SIS_NET_ANS      0x80    // 应答
#define SIS_NET_RCMD     0x80    // 应答标志

#define SIS_NET_ANS_NIL         -1  // 数据为空
#define SIS_NET_ANS_OK           0  // 数据正确
#define SIS_NET_ANS_ERROR        1  // 数据错误
#define SIS_NET_ANS_NOAUTH       2  // 未登录验证
#define SIS_NET_ANS_SUB_START    5  // 订阅开始
#define SIS_NET_ANS_SUB_WAIT     6  // 订阅缓存数据结束 等待新的数据
#define SIS_NET_ANS_SUB_STOP     7  // 订阅结束


// #define SIS_NET_ANS_KEY      0x10    // 返回字符串数据 source + key
// #define SIS_NET_ANS_VAL      0x20    // 有数据 返回一个缓存区  source + rval
// #define SIS_NET_ANS_SIGN     0x40    // 有数据 返回一个信号   source + rint 
// #define SIS_NET_ANS_CHARS    0x70    // 返回字符串数据
// #define SIS_NET_ANS_ARGVS    0x80    // 有数据 返回多个缓存区  只有 argvs 数据 二进制专用
// #define SIS_NET_ANS_BYTES    0x80    // 有数据 返回多个缓存区  只有 argvs 数据 二进制专用

// #define SIS_NET_ANS_MSG      0xF0  // 应答数据 

// #define SIS_NET_ANS_INT      0x0200  // 有数据 返回一个整数   source + rint 
// #define SIS_NET_ANS_VAL      0x0400  // 有数据 返回一个缓存区  source + rval
// #define SIS_NET_ANS_ARGVS    0x0800  // 有数据 返回多个缓存区  source + argvs
// #define SIS_NET_ANS_SIGN     0x1000  // 有数据 返回一个信号    source + rint 
// #define SIS_NET_ANS_KEY      0x2000  // 有数据 且有key  source + key


// #define SIS_NET_ANS_SIGN_NIL         -1  // 数据为空
// #define SIS_NET_ANS_SIGN_OK           0  // 数据正确
// #define SIS_NET_ANS_SIGN_ERROR        1  // 数据错误
// #define SIS_NET_ANS_SIGN_NOAUTH       2  // 未登录验证
// #define SIS_NET_ANS_SIGN_SUB_START    5  // 订阅开始
// #define SIS_NET_ANS_SIGN_SUB_WAIT     6  // 订阅缓存数据结束 等待新的数据
// #define SIS_NET_ANS_SIGN_SUB_STOP     7  // 订阅结束

// request 方决定网络数据传输方式 是按字节流还是 JSON 字符串
#define SIS_NET_FORMAT_CHARS   0 
#define SIS_NET_FORMAT_BYTES   1 

// 把请求和应答统一结合到 s_sis_net_message 中
// 应答目前约定只支持一级数组，
typedef struct s_sis_net_message {
	int                 cid;       // 哪个客户端的信息 -1 表示向所有用户发送
	// 通常server等待消息然后再回送消息 但不排除在某些网络限制情况下 server向client请求数据
	uint32              refs;      // 引用次数

	s_sis_sds	        source;    // 来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址 s_sis_object   

	uint8               format;    // 数据是字符串还是字节流 根据此标记进行打包和拆包
	//
	uint8               style;     // 是应答还是请求 提供给二进制数据使用
   
	s_sis_sds	        cmd;       // 请求信息专用,当前消息的cmd  sisdb.get 
	s_sis_sds	        key;       // 请求信息专用,当前消息的key  sh600600,sh600601.day,info  
	s_sis_sds	        val;       // 请求信息的参数，为json格式 

	int64               rcmd;      // 应答的类型 由这个整数确定应答为什么类型
	s_sis_sds	        rval;      // 应答缓存
	uint16              rfmt;      // 返回数据格式 临时变量

	// 存放字节流数据
	s_sis_pointer_list *argvs;     //  按顺序获取 s_sis_sds

} s_sis_net_message;


////////////////////////////////////////////////////////
//  所有的线程和网络端数据交换统统用这个格式的消息结构
////////////////////////////////////////////////////////

s_sis_net_message *sis_net_message_create();

void sis_net_message_destroy(void *in_);

void sis_net_message_incr(s_sis_net_message *);
void sis_net_message_decr(void *);

void sis_net_message_clear(s_sis_net_message *);
size_t sis_net_message_get_size(s_sis_net_message *);

s_sis_net_message *sis_net_message_clone(s_sis_net_message *in_);

////////////////////////////////////////////////////////
//  解码函数
////////////////////////////////////////////////////////

bool sis_net_encoded_normal(s_sis_net_message *in_, s_sis_memory *out_);
bool sis_net_decoded_normal(s_sis_memory* in_, s_sis_net_message *out_);

#endif
//_SIS_NODE_H