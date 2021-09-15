﻿#ifndef _SISDB_RSNO_H
#define _SISDB_RSNO_H

#include "sis_method.h"
#include "sisdb_worker.h"

// 需要支持读取的数据直接压缩好返回 根据回调函数

#define  SIS_RSNO_NONE     0 // 订阅未开始
#define  SIS_RSNO_WORK     1 // 自动运行模式
#define  SIS_RSNO_CALL     2 // 订阅运行模式
#define  SIS_RSNO_EXIT     3 // 退出

typedef struct s_sisdb_rsno_cxt
{
    int                status;         // 工作状态
    s_sis_sds          work_path;      // 可配置 也可传入
    s_sis_sds          work_name;      // 可配置 也可传入
    int                work_date;
    s_sis_sds          work_keys;
    s_sis_sds          work_sdbs;
    s_sis_disk_reader *work_reader;

	s_sisdb_worker    *work_ziper;     // cb_sub_inctzip 有值时 需要压缩读出的数据
    s_sis_thread       work_thread;    // 读文件时间长 需要启动一个线程处理

    void              *cb_source;      // 
    sis_method_define *cb_sub_start;    // 必须是字符的日期
    sis_method_define *cb_sub_stop;     // 必须是字符的日期
    sis_method_define *cb_dict_sdbs;    // 表结构 json字符串 
    sis_method_define *cb_dict_keys;    // 代码串 字符串
    sis_method_define *cb_sub_inctzip;  // 增量压缩格式
    sis_method_define *cb_sub_chars;    // s_sis_db_chars

}s_sisdb_rsno_cxt;

bool sisdb_rsno_init(void *, void *);
void sisdb_rsno_uninit(void *);
void sisdb_rsno_working(void *);

int cmd_sisdb_rsno_sub(void *worker_, void *argv_);
int cmd_sisdb_rsno_unsub(void *worker_, void *argv_);
int cmd_sisdb_rsno_setcb(void *worker_, void *argv_);

void sisdb_rsno_sub_start(s_sisdb_rsno_cxt *context);
void sisdb_rsno_sub_stop(s_sisdb_rsno_cxt *context);

#endif