
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
#define SIS_SERVER_STATUS_NOINIT    0
#define SIS_SERVER_STATUS_INITED    1
#define SIS_SERVER_STATUS_LOADED    2
#define SIS_SERVER_STATUS_CLOSE     3

#define SIS_NAME_LEN  32


#endif //_SIS_CORE_H
