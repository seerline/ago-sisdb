#ifndef _SUBDB_H
#define _SUBDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "sisdb_sub.h"
#include "sis_db.h"
#include "worker.h"

typedef struct s_subdb_userinfo
{
	int	            cid;
	int	            status;    // 0 初始化 1 正在订阅 2 停止订阅 
	s_sis_sds       sub_keys;   // 订阅 keys
	s_sis_sds       sub_sdbs;   // 订阅 sdbs
} s_subdb_userinfo;

typedef struct s_subdb_cxt
{
	int                 status;      // 工作状态 0 未工作 1 已经开始 2 初始化完成 正常工作 3 已经结束

	int                 work_date;   // 工作日期
	s_sis_sds			work_path;   // 工作路径
	s_sis_sds			work_name;   // 工作路径

	int                 work_readers; // 现有工作人数
	s_sis_sds           work_keys;   // 工作 keys
	s_sis_sds           work_sdbs;   // 工作 sdbs
	s_sis_map_kint     *map_users;   // 用户列表 s_subdb_userinfo

	s_sisdb_sub_cxt    *work_sub_cxt; // 

	int                 wlog_init;    // 是否初始化
	s_sis_worker       *wlog_worker;  // 当前使用的写log类
	s_sis_method       *wlog_method;

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

} s_subdb_cxt;

bool  subdb_init(void *, void *);
void  subdb_uninit(void *);

// 初始化 用来接收外部参数 传递参数 并且初始化自己
int cmd_subdb_init(void *worker_, void *argv_);
// 写入一个数据 
int cmd_subdb_set(void *worker_, void *argv_);
// 数据流开始写入 
int cmd_subdb_start(void *worker_, void *argv_);
// 数据流写入完成 设置标记 方便发送订阅完成信号
int cmd_subdb_stop(void *worker_, void *argv_);
// 需要传入 key sdb val 收到数据后压缩存储
int cmd_subdb_pub(void *worker_, void *argv_);  // s_sis_db_chars
// 订阅默认键值
int cmd_subdb_sub(void *worker_, void *argv_);
// 取消订阅
int cmd_subdb_unsub(void *worker_, void *argv_);
// 写 log 
int cmd_subdb_wlog(void *worker_, void *argv_);

#endif