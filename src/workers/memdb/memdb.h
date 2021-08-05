#ifndef _MEMDB_H
#define _MEMDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "worker.h"

typedef struct s_memdb_reader
{
	s_sis_sds             sub_mkey; // 匹配字符
	s_sis_pointer_list   *netmsgs;    // s_sis_net_message
} s_memdb_reader;

typedef struct s_memdb_cxt
{
	int                 status;      // 工作状态

	s_sis_sds           dbname;      // 数据库名字 memdb
	s_sis_map_pointer  *work_keys;   // 数据集合的字典表 char -> sis_object

	// 多个 client 订阅的列表 需要一一对应发送
	// 同一 mkey 可能有多个用户订阅 如果一个用户有单键值订阅并且和模糊订阅重叠 从单键值订阅表中删除
	s_sis_pointer_list  *msub_readers; // s_memdb_reader 
	// 以 key 为索引的 s_memdb_reader 同一key可能有多个用户订阅
	s_sis_map_pointer   *sub_readers;   // s_memdb_reader

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

}s_memdb_cxt;

s_memdb_reader *memdb_reader_create(s_sis_net_message *netmsg_);
void memdb_reader_destroy(void *);

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