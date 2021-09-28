#ifndef _SIS_NET_NODE_H
#define _SIS_NET_NODE_H

#include "sis_core.h"
#include "sis_list.h"
#include "sis_malloc.h"
#include "sis_memory.h"
#include "sis_net.msg.h"
#include "sis_obj.h"
#include "sis_json.h"
/////////////////////////////////////////////////
//  s_sis_net_nodes
/////////////////////////////////////////////////

// 队列结点
typedef struct s_sis_net_node {
    s_sis_object          *obj;    // 数据区
    struct s_sis_net_node *next;
} s_sis_net_node;

typedef struct s_sis_net_nodes {
	s_sis_mutex_t      lock;  

    int64              wnums;
    s_sis_net_node    *whead;
    s_sis_net_node    *wtail;

    int64              rnums;
    s_sis_net_node    *rhead;  // 已经被pop出去 保存在这里 除非下一次pop或free
	s_sis_net_node    *rtail;
} s_sis_net_nodes;


s_sis_net_nodes *sis_net_nodes_create();
void sis_net_nodes_destroy(s_sis_net_nodes *nodes_);
void sis_net_nodes_clear(s_sis_net_nodes *nodes_);

int  sis_net_nodes_push(s_sis_net_nodes *nodes_, s_sis_object *obj_);
int  sis_net_nodes_read(s_sis_net_nodes *nodes_, int );
void sis_net_nodes_free_read(s_sis_net_nodes *nodes_);
// 队列是否为空
int  sis_net_nodes_none(s_sis_net_nodes *nodes_);

///////////////////////////////////////////////////////////////////////////
//----------------------s_sis_net_list --------------------------------//
//  以整数为索引 存储指针的列表
///////////////////////////////////////////////////////////////////////////
#define SIS_NET_NOUSE  0  // 可以使用
#define SIS_NET_USEED  1  // 正在使用
#define SIS_NET_CLOSE  2  // 已经关闭

typedef struct s_sis_net_list {
	time_t         wait_sec;     // 是否等待 300 秒 0 就是直接分配
	time_t        *stop_sec;     // stop_time 上次删除时的时间 
	int		       max_count;    // 当前最大个数
	int		       cur_count;    // 当前有效个数
	unsigned char *used;         // 是否有效 初始为 0 
	void          *buffer;       // used 为 0 需调用vfree
	void (*vfree)(void *, int);       // == NULL 不释放对应内存
} s_sis_net_list;

s_sis_net_list *sis_net_list_create(void *); 
void sis_net_list_destroy(s_sis_net_list *list_);
void sis_net_list_clear(s_sis_net_list *list_);

int sis_net_list_new(s_sis_net_list *list_, void *in_);
void *sis_net_list_get(s_sis_net_list *, int index_);

int sis_net_list_first(s_sis_net_list *);
int sis_net_list_next(s_sis_net_list *, int index_);
int sis_net_list_uses(s_sis_net_list *);

int sis_net_list_stop(s_sis_net_list *list_, int index_);

////////////////////////////////////////////////////////
//  标准编码解码函数
////////////////////////////////////////////////////////
// 总是返回真
bool sis_net_encoded_normal(s_sis_net_message *in_, s_sis_memory *out_);
// 返回失败 表示数据出错 断开链接 重新开始
bool sis_net_decoded_normal(s_sis_memory* in_, s_sis_net_message *out_);
// json 转 netmsg
void sis_json_to_netmsg(s_sis_json_node* node_, s_sis_net_message *out_);

#endif
