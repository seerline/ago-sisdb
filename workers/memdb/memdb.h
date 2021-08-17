#ifndef _MEMDB_H
#define _MEMDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "sisdb_worker.h"
#include "worker.h"


typedef struct s_memdb_cxt
{
	int                 status;      // 工作状态

	s_sis_sds           dbname;      // 数据库名字 memdb
	s_sis_map_kobj     *work_keys;   // 数据集合的字典表 sis_object -> 

	s_sis_map_kobj     *msub_keys;   // 订阅对应的字典表 sis_object -> s_sis_sub_cxt

	// 多个 client 订阅的列表 需要一一对应发送
	// s_sisdb_sub_cxt      *sub_readers; // 除非序列号 客户号 订阅信息都一样 才直接覆盖 否则都当成新的订阅处理

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

} s_memdb_cxt;

bool  memdb_init(void *, void *);
void  memdb_uninit(void *);
void  memdb_working(void *);  

// 获得一个set写入的数据
int memdb_get(void *worker_, void *argv_);
// 写入一个数据 数据会在重启清理
int memdb_set(void *worker_, void *argv_);
// 订阅一个键值
int memdb_sub(void *worker_, void *argv_);
// 发布一个信息
int memdb_pub(void *worker_, void *argv_);
// 订阅多个符合条件的键值 头匹配
int memdb_msub(void *worker_, void *argv_);
// 取消订阅
int memdb_unsub(void *worker_, void *argv_);

#endif