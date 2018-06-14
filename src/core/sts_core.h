
#ifndef _STS_CORE_H
#define _STS_CORE_H

#include <sts_os.h>
#include <sts_log.h>

#include <sts_malloc.h>

// 服务器返回类型
#define STS_SERVER_REPLY_OK        0
#define STS_SERVER_REPLY_ERR       1

// 服务器运行状态
#define STS_SERVER_STATUS_NOINIT    0
#define STS_SERVER_STATUS_INITED    1
#define STS_SERVER_STATUS_LOADED    2
#define STS_SERVER_STATUS_CLOSE     3

// 任务运行模式 间隔秒数运行，按分钟时间点运行
#define STS_WORK_MODE_NONE     0
#define STS_WORK_MODE_GAPS     1  // 间隔秒数运行，需要配合开始和结束时间
#define STS_WORK_MODE_PLANS    2  // 按时间列表运行，时间精确到分钟

#define STS_NAME_LEN  32

#endif //_STS_CORE_H
