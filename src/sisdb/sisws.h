
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISWS_H
#define _SISWS_H

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
#include "sis_net.obj.h"
#include "sisdb.h"

#pragma pack(push, 1)

typedef struct s_sisdb_ws_config {
	int   port;    
    char  protocol[128];
    void *service;   // 启动的通讯服务线程句柄
    s_sis_thread work_thread; 
} s_sisdb_ws_config;

#pragma pack(pop)

s_sis_object *siscs_send_local_obj(const char *command_, const char *key_, 
					     	   const char *val_, size_t ilen_);

bool worker_ws_open(s_sisdb_worker *, s_sis_json_node *);
void worker_ws_close(s_sisdb_worker *);
void worker_ws_working(s_sisdb_worker *);

#endif  /* _DIGGER_H */
