
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISCS_H
#define _SISCS_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_map.h"
#include "sis_str.h"
#include "sis_list.h"
#include "sis_time.h"
#include "sis_json.h"

#include "sis_malloc.h"
#include "sis_math.h"
#include "sis_thread.h"
#include "sis_method.h"

#pragma pack(push, 1)

typedef struct s_siscs_service {

	int  port;    
    char protocol[128];
    void *service;   // 启动的通讯服务线程句柄
    s_sis_thread_id_t thread_id; 
}s_siscs_service;

#pragma pack(pop)

int siscs_service_start(s_siscs_service *);
void siscs_service_stop(s_siscs_service *);

#endif  /* _DIGGER_H */
