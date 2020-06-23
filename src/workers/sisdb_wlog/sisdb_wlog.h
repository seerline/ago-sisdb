#ifndef _SISDB_WLOG_H
#define _SISDB_WLOG_H

#include "sis_method.h"
#include "sis_map.h"
#include "sis_disk.h"

typedef struct s_sisdb_wlog_unit
{
	char               dsnames[128];     // 数据集名字 
	s_sis_disk_class  *wlog;
}s_sisdb_wlog_unit;


typedef struct s_sisdb_wlog_cxt
{
	int                 status;

	s_sis_sds           work_path;

	s_sis_map_pointer  *datasets;  // 每个数据集一个文件	
}s_sisdb_wlog_cxt;

s_sisdb_wlog_unit *sisdb_wlog_unit_create(s_sisdb_wlog_cxt *cxt_, const char *);
void sisdb_wlog_unit_destroy(void *);

bool  sisdb_wlog_init(void *, void *);
void  sisdb_wlog_uninit(void *);
void  sisdb_wlog_method_init(void *);
void  sisdb_wlog_method_uninit(void *);

// 从log中获取数据并回写
int cmd_sisdb_wlog_read(void *worker_, void *argv_);
// 接收数据并顺序写入log
int cmd_sisdb_wlog_write(void *worker_, void *argv_);
// 数据全部处理完后清理内存
int cmd_sisdb_wlog_clear(void *worker_, void *argv_);


#endif