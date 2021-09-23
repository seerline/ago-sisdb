#ifndef _MEMDB_H
#define _MEMDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "sisdb_sub.h"
#include "worker.h"

typedef struct s_memdb_unit
{
	int8           style;
	s_sis_object  *obj;   // 
} s_memdb_unit;

typedef struct s_memdb_cxt
{
	int                 status;      // 工作状态

	s_sis_sds           work_name;   // 数据库名字 memdb
	s_sis_map_pointer  *work_keys;   // 数据集合的字典表 s_memdb_unit

	s_sisdb_sub_cxt    *work_sub_cxt; // 

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

} s_memdb_cxt;

bool  memdb_init(void *, void *);
void  memdb_uninit(void *);

// 初始化 用来接收外部参数 传递参数 并且初始化自己
int cmd_memdb_init(void *worker_, void *argv_);
// 获得一个set写入的数据
int cmd_memdb_get(void *worker_, void *argv_);
// 写入一个数据 数据会在重启清理
int cmd_memdb_set(void *worker_, void *argv_);
// 订阅默认键值
int cmd_memdb_sub(void *worker_, void *argv_);
// 订阅多个符合条件的键值 头匹配
int cmd_memdb_hsub(void *worker_, void *argv_);
// 发布一个信息
int cmd_memdb_pub(void *worker_, void *argv_);
// 取消订阅
int cmd_memdb_unsub(void *worker_, void *argv_);

#endif