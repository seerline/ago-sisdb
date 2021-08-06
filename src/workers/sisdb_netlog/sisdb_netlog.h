#ifndef _SISDB_NETLOG_H
#define _SISDB_NETLOG_H

#include "sis_method.h"
#include "sis_map.h"
#include "sis_disk.h"

#define  SIS_NETLOG_NONE     0 // 订阅未开始
#define  SIS_NETLOG_READ     1 // 工作模式
#define  SIS_NETLOG_WRITE    2 // 工作模式

typedef struct s_sisdb_netlog_cxt
{
	int                 status;

	s_sis_sds           work_path;
	s_sis_sds           work_name;
	int                 work_date;

	s_sis_disk_reader  *reader;
	s_sis_disk_writer  *writer;

	void               *cb_source;
	sis_method_define  *cb_netmsg;
	sis_method_define  *cb_sub_start;
	sis_method_define  *cb_sub_stop;
}s_sisdb_netlog_cxt;

bool  sisdb_netlog_init(void *, void *);
void  sisdb_netlog_uninit(void *);

// 从log中获取数据
int cmd_sisdb_netlog_sub(void *worker_, void *argv_);

int cmd_sisdb_netlog_unsub(void *worker_, void *argv_);

// 写文件结束 关闭文件 
int cmd_sisdb_netlog_open(void *worker_, void *argv_);
// 接收数据并顺序写入log
int cmd_sisdb_netlog_write(void *worker_, void *argv_);
// 写文件结束 关闭文件 
int cmd_sisdb_netlog_close(void *worker_, void *argv_);

// 删除wlog文件 方法调用统一用 message
int cmd_sisdb_netlog_move(void *worker_, void *argv_);

#endif