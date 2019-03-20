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
s_sis_list_node *sis_sdsnode_update(s_sis_list_node *node_, const void *in, size_t inlen);
s_sis_list_node *sis_sdsnode_clone(s_sis_list_node *node_);
int sis_sdsnode_get_size(s_sis_list_node *node_);
int sis_sdsnode_get_count(s_sis_list_node *node_);

//------------------------sdsnode ------------------------------//
// 非常好的数据结构，基本可以满足所有网络通许所有的信息了                //
//--------------------------------------------------------------//
typedef struct s_sis_message_node {
	s_sis_sds	command;   //来源信息专用,当前消息的key  sisdb.get 
	s_sis_sds	key;       //来源信息专用,当前消息的key  sh600600.day 
	s_sis_sds	argv;      //来源信息的参数，为json格式
	s_sis_sds	source;   //来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址

	s_sis_list_node   *links;     //来源信息专用, 数据来源链路，每次多一跳就增加一个节点
	s_sis_list_node   *nodes;     //附带的信息数据链表  node->value 为 s_sis_sds sis_

} s_sis_message_node;


#define SIS_MSN_ONE_VAL(a) (a->nodes ? (s_sis_sds)(a->nodes->value) : NULL)
#define SIS_MSN_ONE_LEN(a) (a->nodes ? sis_sdslen((s_sis_sds)(a->nodes->value)) : 0)

////////////////////////////////////////////////////////
//  所有的线程和网络端数据交换统统用这个格式的消息结构
////////////////////////////////////////////////////////

s_sis_message_node *sis_message_node_create();
void sis_message_node_destroy(void *in_);
s_sis_list_node *sis_message_node_set_nodes(s_sis_message_node *node_,const char *in_,size_t ilen_);

s_sis_message_node *sis_message_node_clone(s_sis_message_node *in_);

#endif
//_SIS_NODE_H