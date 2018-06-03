#ifndef _STS_NODE_H
#define _STS_NODE_H

#include "sts_core.h"
#include "sts_malloc.h"

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_list_node --------------------------------//
//  操作 s_sts_list_node 列表的函数
//  value 为 s_sts_sds 类型
///////////////////////////////////////////////////////////////////////////
s_sts_list_node *sts_sdsnode_create(const void *in, size_t inlen);
void sts_sdsnode_destroy(s_sts_list_node *node);

s_sts_list_node *sts_sdsnode_offset_node(s_sts_list_node *node_, int offset);
s_sts_list_node *sts_sdsnode_last_node(s_sts_list_node *node_);
s_sts_list_node *sts_sdsnode_first_node(s_sts_list_node *node_);
s_sts_list_node *sts_sdsnode_push(s_sts_list_node *node_, const void *in, size_t inlen);
s_sts_list_node *sts_sdsnode_update(s_sts_list_node *node_, const void *in, size_t inlen);
s_sts_list_node *sts_sdsnode_clone(s_sts_list_node *node_);
int sts_sdsnode_get_size(s_sts_list_node *node_);
int sts_sdsnode_get_count(s_sts_list_node *node_);

//------------------------sdsnode ------------------------------//
typedef struct s_sts_message_node {
	s_sts_sds	command;   //来源信息专用,当前消息的key  stsdb.get 
	s_sts_sds	key;       //来源信息专用,当前消息的key  sh600600.day 
	s_sts_sds	argv;      //来源信息的参数，为json格式
	s_sts_sds	address;   //来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址

	s_sts_list_node   *links;     //来源信息专用, 数据来源链路，每次多一跳就增加一个节点
	s_sts_list_node   *nodes;     //附带的信息数据链表  node->value 为 s_sts_sds 类型

} s_sts_message_node;


#define STS_MSN_ONE_VAL(a) (a->nodes ? (s_sts_sds)(a->nodes->value) : NULL)
#define STS_MSN_ONE_LEN(a) (a->nodes ? sts_sdslen((s_sts_sds)(a->nodes->value)) : 0)

////////////////////////////////////////////////////////
//  所有的线程和网络端数据交换统统用这个格式的消息结构
////////////////////////////////////////////////////////

s_sts_message_node *sts_message_node_create();
void sts_message_node_destroy(void *in_);

s_sts_message_node *sts_message_node_clone(s_sts_message_node *in_);

#endif
//_STS_NODE_H