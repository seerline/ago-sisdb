#ifndef _sisdb_rseg_h
#define _sisdb_rseg_h

#include "sis_method.h"
#include "sisdb_incr.h"
#include "sisdb_wseg.h"

// 需要支持读取的数据直接压缩好返回 根据回调函数

#define  SIS_RSEG_NONE     0 // 订阅未开始
#define  SIS_RSEG_WORK     2 // 订阅未开始
#define  SIS_RSEG_EXIT     3 // 准备退出退出成功后设置为NONE
// #define  SIS_RSEG_BREAK    4 // 中断订阅模式

// 目前仅支持无时序的get 和单时序的订阅 订阅时要注意
// 合并多序列放到外层调用来处理 
typedef struct s_sisdb_rseg_cxt
{
    int                status;          // 工作状态

    s_sis_sds_save    *work_path;       // 数据文件路径，可配置 也可传入
    s_sis_sds_save    *work_name;       // 工作者名称，可配置 也可传入
    int                work_date;       // 需要发送行情的日期，例如20210923
    s_sis_sds          work_keys;       // 需要发送行情的股票列表，以逗号分隔的字符串表示
    s_sis_sds          work_sdbs;       // 需要发送行情的格式数据，用JSON字符串表示

    s_sis_disk_reader *work_reader;     // 读盘类

    int                wget_date;       // 上次get的日期
    s_sis_sds          curr_keys;       // 实际合并的键值
    s_sis_sds          curr_sdbs;       // 实际的合并数据结构，用JSON字符串表示
    s_sis_map_list    *maps_sdbs;       // 合并后的表对应的map s_sis_dynamic_db

    s_sis_thread       work_thread;     // 读文件时间长 需要启动一个线程处理

    void              *cb_source;       // 要将行情发往的目的地，一般是目标工作者的对象指针
    sis_method_define *cb_sub_start;    // 一日行情订阅开始时执行的回调函数，日期必须是字符格式的日期
    sis_method_define *cb_sub_stop;     // 一日行情订阅结束时执行的回调函数，日期必须是字符格式的日期
    sis_method_define *cb_dict_sdbs;    // 表结构 json字符串 
    sis_method_define *cb_dict_keys;    // 需要发送行情的股票列表，代码串 字符串
    sis_method_define *cb_sub_chars;    // s_sis_db_chars

}s_sisdb_rseg_cxt;

bool sisdb_rseg_init(void *, void *);
void sisdb_rseg_uninit(void *);

int cmd_sisdb_rseg_get(void *worker_, void *argv_);
int cmd_sisdb_rseg_sub(void *worker_, void *argv_);
int cmd_sisdb_rseg_unsub(void *worker_, void *argv_);

int sisdb_rseg_init_sdbs(s_sisdb_rseg_cxt *context, int idate);
void sisdb_rseg_clear(s_sisdb_rseg_cxt *context);
void sisdb_rseg_sub_start(s_sisdb_rseg_cxt *context);
void sisdb_rseg_sub_stop(s_sisdb_rseg_cxt *context);

#endif