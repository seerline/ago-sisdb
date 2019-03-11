
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISCS_IO_H
#define _SISCS_IO_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"
#include "sis_file.h"
#include "sis_map.h"
#include "sis_net.h"

#include "siscs.h"
#include "sis_thread.h"

#pragma pack(push,1)

#define DIGGER_DB_IN   0
#define DIGGER_DB_OUT  1

typedef struct s_siscs_server
{
	
	int status; //是否已经初始化 0 没有初始化

	int    log_level;
	size_t log_size;
	char   log_path[SIS_PATH_LEN];   //log路径

	s_sis_struct_list *services; // s_siscs_service;

}s_siscs_server;

#pragma pack(pop)

int siscs_open(const char *conf_);
void siscs_close();

void *siscs_get_server();

#endif  /* _DIGGER_IO_H */
