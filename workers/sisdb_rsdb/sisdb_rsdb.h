#ifndef _SISDB_RSDB_H
#define _SISDB_RSDB_H

#include "sis_method.h"
#include "sisdb_incr.h"

#define  SIS_RSDB_NONE     0 // 订阅未开始
#define  SIS_RSDB_WORK     1 // 自动运行模式
#define  SIS_RSDB_CALL     2 // 订阅运行模式
#define  SIS_RSDB_EXIT     3 // 退出

typedef struct s_sisdb_rsdb_cxt
{
	int                status;
	s_sis_sds_save    *work_path;
	s_sis_sds_save    *work_name;

    s_sis_date_pair    work_date;      // 按天检索数据
    s_sis_sds          work_keys;
    s_sis_sds          work_sdbs;
    s_sis_disk_reader *work_reader;

    s_sis_sds          ziper_keys;
    s_sis_sds          ziper_sdbs;
	s_sisdb_incr      *work_ziper;     // cb_sub_incrzip 有值时 需要压缩读出的数据
    s_sis_thread       work_thread;    // 读文件时间长 需要启动一个线程处理

    void              *cb_source;      // 
    sis_method_define *cb_sub_start;    // 必须是字符的日期
    sis_method_define *cb_sub_stop;     // 必须是字符的日期
    sis_method_define *cb_dict_sdbs;    // 表结构 json字符串 
    sis_method_define *cb_dict_keys;    // 代码串 字符串
    sis_method_define *cb_sub_incrzip;  // 增量压缩格式
    sis_method_define *cb_sub_chars;    // s_sis_db_chars

}s_sisdb_rsdb_cxt;

bool  sisdb_rsdb_init(void *, void *);
void  sisdb_rsdb_uninit(void *);
void sisdb_rsdb_working(void *);

int cmd_sisdb_rsdb_get(void *worker_, void *argv_);

int cmd_sisdb_rsdb_sub(void *worker_, void *argv_);
int cmd_sisdb_rsdb_unsub(void *worker_, void *argv_);
int cmd_sisdb_rsdb_setcb(void *worker_, void *argv_);

void sisdb_rsdb_sub_start(s_sisdb_rsdb_cxt *context);
void sisdb_rsdb_sub_stop(s_sisdb_rsdb_cxt *context);

#endif