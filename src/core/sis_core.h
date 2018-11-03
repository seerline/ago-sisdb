
#ifndef _SIS_CORE_H
#define _SIS_CORE_H

#include <sis_os.h>
#include <sis_log.h>

#include <sis_malloc.h>

// 服务器返回类型
#define SIS_SERVER_REPLY_OK        0
#define SIS_SERVER_REPLY_ERR       1

// 服务器运行状态
#define SIS_SERVER_STATUS_NOINIT    0
#define SIS_SERVER_STATUS_INITED    1
#define SIS_SERVER_STATUS_LOADED    2
#define SIS_SERVER_STATUS_CLOSE     3

// 任务运行模式 间隔秒数运行，按分钟时间点运行
#define SIS_WORK_MODE_NONE     0
#define SIS_WORK_MODE_GAPS     1  // 间隔秒数运行，需要配合开始和结束时间
#define SIS_WORK_MODE_PLANS    2  // 按时间列表运行，时间精确到分钟
#define SIS_WORK_MODE_ONCE     3  // 只运行一次

#define SIS_NAME_LEN  32

#endif //_SIS_CORE_H
