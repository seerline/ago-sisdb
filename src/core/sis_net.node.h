#ifndef _SIS_NET_NODE_H
#define _SIS_NET_NODE_H

#include "sis_core.h"
#include "sis_malloc.h"

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_list_node --------------------------------//
//  操作 s_sis_list_node 列表的函数
//  value 为 s_sis_sds 类型
///////////////////////////////////////////////////////////////////////////
s_sis_list_node *sis_sdsnode_create(const void *in, size_t inlen);
void sis_sdsnode_destroy(s_sis_list_node *node);

s_sis_list_node *sis_sdsnode_offset_node(s_sis_list_node *node_, int offset);
s_sis_list_node *sis_sdsnode_last_node(s_sis_list_node *node_);
s_sis_list_node *sis_sdsnode_first_node(s_sis_list_node *node_);
s_sis_list_node *sis_sdsnode_next_node(s_sis_list_node *node_);
s_sis_list_node *sis_sdsnode_push_node(s_sis_list_node *node_, const void *in, size_t inlen);
s_sis_list_node *sis_sdsnode_push_node_sds(s_sis_list_node *node_, s_sis_sds in_);
s_sis_list_node *sis_sdsnode_update(s_sis_list_node *node_, const void *in, size_t inlen);
s_sis_list_node *sis_sdsnode_clone(s_sis_list_node *node_);
int sis_sdsnode_get_size(s_sis_list_node *node_);
int sis_sdsnode_get_count(s_sis_list_node *node_);

//------------------------sdsnode ------------------------------//
// 非常好的数据结构，基本可以满足所有网络通许所有的信息了                //
//--------------------------------------------------------------//

#define SIS_NET_ASK_ARRAY      0    // 正常请求 数组
#define SIS_NET_ASK_INFO       1    // +PING 等
#define SIS_NET_ASK_STRING     2    // 发送一个缓存区或字符串 

#define SIS_NET_ASK_MESSAGE    10  // 有数据 返回一个缓存区  rval
#define SIS_NET_REPLY_MESSAGE  (SIS_NET_ASK_MESSAGE + 1)  // 有数据 返回一个缓存区  rval

#define SIS_NET_REPLY_STRING   (SIS_NET_REPLY_MESSAGE + 1)  // 有数据 返回一个缓存区  rval
#define SIS_NET_REPLY_ARRAY    (SIS_NET_REPLY_MESSAGE + 2)  // 有数据 返回一个列表数据 rlist
#define SIS_NET_REPLY_INTEGER  (SIS_NET_REPLY_MESSAGE + 3)  // 有数据 返回一个整数    rint 
#define SIS_NET_REPLY_INFO     (SIS_NET_REPLY_MESSAGE + 5)  // 没有数据 rval 为空
#define SIS_NET_REPLY_ERROR    (SIS_NET_REPLY_MESSAGE + 6)  // 没有数据 NIL BUSY ... rval 为错误字符串

// 把请求和应答统一结合到 s_sis_net_message 中
// 应答目前约定只支持一级数组，
typedef struct s_sis_net_message {
	int         cid;       // 哪个客户端的信息 -- 仅在server时有用, client时为0
	uint8       style;     // 消息类型
	uint8       subpub;    // 是否订阅等

	s_sis_sds	command;   // 来源信息专用,当前消息的cmd  sisdb.get 
	s_sis_sds	key;       // 来源信息专用,当前消息的key  sh600600.day 
	s_sis_sds	argv;      // 来源信息的参数，为json格式 
	s_sis_sds	source;    // 来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址
	s_sis_list_node   *argvs;     //附带的信息数据链表  node->value 为 s_sis_sds

	int64       rint;          // 应答为整数
	s_sis_sds	rval;          // 应答缓存
	s_sis_list_node   *rlist;  // 应答的信息数据链表  node->value 为 s_sis_sds

	void        *cb_reply;     // 应答回调

	s_sis_list_node   *links;     //来源信息专用, 数据来源链路，每次多一跳就增加一个节点
} s_sis_net_message;


#define SIS_MSN_ONE_VAL(a) (a ? (s_sis_sds)(a->value) : NULL)
#define SIS_MSN_ONE_LEN(a) (a ? sis_sdslen((s_sis_sds)(a->value)) : 0)

////////////////////////////////////////////////////////
//  所有的线程和网络端数据交换统统用这个格式的消息结构
////////////////////////////////////////////////////////

s_sis_net_message *sis_net_message_create();
void sis_net_message_destroy(void *in_);

s_sis_net_message *sis_net_message_clone(s_sis_net_message *in_);

#endif
//_SIS_NODE_H