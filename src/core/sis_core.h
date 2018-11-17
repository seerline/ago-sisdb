
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

#define SIS_NAME_LEN  32


#endif //_SIS_CORE_H
