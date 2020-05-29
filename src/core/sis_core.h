
#ifndef _SIS_CORE_H
#define _SIS_CORE_H

#include <sis_os.h>
#include <os_file.h>
#include <os_str.h>
#include <os_net.h>
#include <os_time.h>
#include <os_thread.h>

#include <sis_log.h>

#include <sis_malloc.h>

// server reply style define
#define SIS_SERVER_REPLY_OK        0
#define SIS_SERVER_REPLY_ERR       1

// server run status define
#define SIS_SERVER_STATUS_NOINIT      0
#define SIS_SERVER_STATUS_MAIN_INIT   1  // 基本初始化完成
#define SIS_SERVER_STATUS_WORK_INIT   2  // 功能初始化完成
#define SIS_SERVER_STATUS_INITED      3  // 全部初始化完成
#define SIS_SERVER_STATUS_CLOSE       4

#define SIS_NAME_LEN  32

#endif //_SIS_CORE_H
