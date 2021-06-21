#ifndef _SISDB_WSNO_H
#define _SISDB_WSNO_H

#include "sis_method.h"

#include "snodb.h"
#include <sis_disk_v1.h>

typedef struct s_sisdb_wsno_cxt
{
 
    s_sis_disk_v1_class  *write_class;   // 写盘类
    s_sis_sds          work_path;     // 可配置 也可传入
    int                page_size;    

    int                iswhead;       // 是否写了头
 
	s_sis_sds          wsno_date;    // 文件名      
	s_sis_sds          wsno_keys;    
	s_sis_sds          wsno_sdbs; 

	s_snodb_worker    *wsno_unzip;     // 解压 s_snodb_compress 中来的数据

}s_sisdb_wsno_cxt;

bool sisdb_wsno_init(void *, void *);
void sisdb_wsno_uninit(void *);

void market_file_method_init(void *);

int cmd_sisdb_wsno_init(void *worker_, void *argv_);
int cmd_sisdb_wsno_getcb(void *worker_, void *argv_);

#endif