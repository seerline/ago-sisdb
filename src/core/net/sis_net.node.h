#ifndef _SIS_NET_NODE_H
#define _SIS_NET_NODE_H

#include "sis_core.h"
#include "sis_list.h"
#include "sis_malloc.h"
#include "sis_memory.h"
//------------------------sdsnode ------------------------------//
// 非常好的数据结构，基本可以满足所有网络通许所有的信息了                //
//--------------------------------------------------------------//

#define SIS_NET_ASK_PING       0x01    // PING 包
#define SIS_NET_ASK_CMD        0x02    // 正常请求 有 source + cmd
#define SIS_NET_ASK_KEY        0x04    // 正常请求 有 source + key
#define SIS_NET_ASK_VAL        0x08    // 正常请求 有 source + val
#define SIS_NET_ASK_ARGVS      0x10    // 正常请求 只有 argvs 数据
#define SIS_NET_ASK_NORMAL     0x1E    // 正常请求 全部都有

#define SIS_NET_ASK_MSG        0x00FF  // 有数据 返回一个缓存区  rval

#define SIS_NET_REPLY_MSG      0xFF00  // 有数据 返回一个缓存区  rval

#define SIS_NET_REPLY_PONG     0x0100  // PONG 包
#define SIS_NET_REPLY_NORMAL   0x0200  // 有数据 返回一个缓存区  source + rval
#define SIS_NET_REPLY_INT      0x0400  // 有数据 返回一个整数   source + rint 
#define SIS_NET_REPLY_OK       0x0800  // 没有数据 source + rval 为错误字符串
#define SIS_NET_REPLY_ERROR    0x1000  // 没有数据 source + rval 为错误字符串

// 把请求和应答统一结合到 s_sis_net_message 中
// 应答目前约定只支持一级数组，
typedef struct s_sis_net_message {
	int                 cid;       // 哪个客户端的信息 -1 表示向所有用户发送
	// 通常server等待消息然后再回送消息 但不排除在某些网络限制情况下 server向client请求数据
	uint32              refs;      // 引用次数

	s_sis_sds	        source;    // 来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址 s_sis_object   

	uint16              style;     // 是应答还是请求 SIS_NET_ASK
   
	s_sis_sds	        cmd;       // 请求信息专用,当前消息的cmd  sisdb.get 
	s_sis_sds	        key;       // 请求信息专用,当前消息的key  sh600600,sh600601.day,info  
	s_sis_sds	        val;       // 请求信息的参数，为json格式 

	int64               rint;      // 应答为整数
	s_sis_sds	        rval;      // 应答缓存

	s_sis_pointer_list *argvs;     // 附带的信息数据链表 多余的参数放在这里 按顺序获取 s_sis_sds

	s_sis_pointer_list *links;      //信息传递专用, 数据来源链路，每次多一跳就增加一个节点 收到一个就销毁最后一个结点
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

bool sis_net_encoded_json(s_sis_net_message *in_, s_sis_memory *out_);
bool sis_net_decoded_json(s_sis_memory* in_, s_sis_net_message *out_);

bool sis_net_encoded_bytes(s_sis_net_message *in_, s_sis_memory *out_);
bool sis_net_decoded_bytes(s_sis_memory* in_, s_sis_net_message *out_);


#endif
//_SIS_NODE_H