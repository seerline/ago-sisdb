﻿#ifndef _SISDB_WSDB_H
#define _SISDB_WSDB_H

#include "sis_method.h"
#include "sis_disk.h"
#include "sisdb_worker.h"

#define  SIS_WSDB_NONE     0 // 
#define  SIS_WSDB_OPEN     1 // 文件是否打开
#define  SIS_WSDB_EXIT     3 // 退出

typedef struct s_sisdb_wsdb_cxt
{
	int                status;
	s_sis_sds          work_path;
	s_sis_sds          work_name;
    s_sis_sds          safe_path; // pack 时需要
	s_sis_disk_writer *writer;        // 写盘类

	int                wheaded;
	s_sis_sds          work_keys;     // 筛选后的 
	s_sis_sds          work_sdbs;     // 筛选后的

	s_sisdb_worker    *work_unzip;    // 解压 s_snodb_compress 中来的数据

} s_sisdb_wsdb_cxt;

bool  sisdb_wsdb_init(void *, void *);
void  sisdb_wsdb_uninit(void *);

int cmd_sisdb_wsdb_write(void *worker_, void *argv_);

int cmd_sisdb_wsdb_start(void *worker_, void *argv_);
int cmd_sisdb_wsdb_stop(void *worker_, void *argv_);

void sisdb_wsdb_start(s_sisdb_wsdb_cxt *context);
void sisdb_wsdb_stop(s_sisdb_wsdb_cxt *context);

#endif