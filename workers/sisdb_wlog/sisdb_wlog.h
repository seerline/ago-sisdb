#ifndef _SISDB_WLOG_H
#define _SISDB_WLOG_H

#include "sis_method.h"
#include "sis_map.h"
#include "sis_disk_v1.h"

typedef struct s_sisdb_wlog_cxt
{
	int                 status;

	s_sis_sds           work_path;

	s_sis_disk_v1_class   *wlog;

	void               *cb_source;
	sis_method_define  *cb_recv;
	sis_method_define  *cb_sub_start;
	sis_method_define  *cb_sub_stop;
}s_sisdb_wlog_cxt;

bool  sisdb_wlog_init(void *, void *);
void  sisdb_wlog_uninit(void *);
void  sisdb_wlog_method_init(void *);
void  sisdb_wlog_method_uninit(void *);

// 从log中获取数据并回写
int cmd_sisdb_wlog_read(void *worker_, void *argv_);

// 写文件结束 关闭文件 
int cmd_sisdb_wlog_open(void *worker_, void *argv_);
// 接收数据并顺序写入log
int cmd_sisdb_wlog_write(void *worker_, void *argv_);
// 写文件结束 关闭文件 
int cmd_sisdb_wlog_close(void *worker_, void *argv_);

// 删除wlog文件 argv_ 为 表名
int cmd_sisdb_wlog_move(void *worker_, void *argv_);
// 检查wlog文件是否存在 argv_ 为 表名
int cmd_sisdb_wlog_exist(void *worker_, void *argv_);

#endif