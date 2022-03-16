#ifndef _SISDB_FLOG_H
#define _SISDB_FLOG_H

#include "sis_method.h"
#include "sis_map.h"
#include "sis_disk.h"

#define  SIS_FLOG_NONE     0 // 订阅未开始
#define  SIS_FLOG_READ     1 // 工作模式
#define  SIS_FLOG_WRITE    2 // 工作模式

// 快速LOG的类 不保证掉电情况下写入成功
// 对于安全性要求高的可以使用 slog 类 safe log 类
typedef struct s_sisdb_flog_cxt
{
	int                 status;

	s_sis_sds_save     *work_path;   // 数据库路径 sisdb
	s_sis_sds_save     *work_name;   // 数据库名字 sisdb
	int                 work_date;
	s_sis_memory       *write_memory;

	s_sis_disk_reader  *reader;
	s_sis_disk_writer  *writer;

	void               *cb_source;
	sis_method_define  *cb_netmsg;
	sis_method_define  *cb_sub_start;
	sis_method_define  *cb_sub_stop;
}s_sisdb_flog_cxt;

bool  sisdb_flog_init(void *, void *);
void  sisdb_flog_uninit(void *);

// 从log中获取数据
int cmd_sisdb_flog_sub(void *worker_, void *argv_);

int cmd_sisdb_flog_unsub(void *worker_, void *argv_);

// 写文件结束 关闭文件 
int cmd_sisdb_flog_open(void *worker_, void *argv_);
// 接收数据并顺序写入log
int cmd_sisdb_flog_write(void *worker_, void *argv_);
// 写文件结束 关闭文件 
int cmd_sisdb_flog_close(void *worker_, void *argv_);

// 删除wlog文件 方法调用统一用 message
int cmd_sisdb_flog_remove(void *worker_, void *argv_);

#endif