#ifndef _SICDB_H
#define _SICDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "sisdb_sub.h"
#include "worker.h"

typedef struct s_sicdb_unit
{
	int8           style;
	s_sis_object  *obj;   // 
} s_sicdb_unit;

typedef struct s_sicdb_cxt
{
	int                 status;      // 工作状态

	s_sis_sds           work_name;   // 数据库名字 sicdb
	s_sis_map_pointer  *work_keys;   // 数据集合的字典表 s_sicdb_unit

	s_sisdb_sub_cxt    *work_sub_cxt; // 

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

	// 如果订阅不是当天日期 或当天已经处理了收盘 那么就直接从磁盘读取数据 rsno  
} s_sicdb_cxt;

bool  sicdb_init(void *, void *);
void  sicdb_uninit(void *);

// 打开
int cmd_sicdb_open(void *worker_, void *argv_);
// 关闭
int cmd_sicdb_close(void *worker_, void *argv_);
// 获得一个set写入的数据
int cmd_sicdb_get(void *worker_, void *argv_);
// 写入一个数据 数据会在重启清理
int cmd_sicdb_set(void *worker_, void *argv_);
// 订阅默认键值
int cmd_sicdb_sub(void *worker_, void *argv_);
// 订阅多个符合条件的键值 头匹配
int cmd_sicdb_hsub(void *worker_, void *argv_);
// 发布一个信息
int cmd_sicdb_pub(void *worker_, void *argv_);
// 取消订阅
int cmd_sicdb_unsub(void *worker_, void *argv_);

#endif