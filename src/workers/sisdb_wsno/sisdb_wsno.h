#ifndef _SISDB_WSNO_H
#define _SISDB_WSNO_H

#include "sis_method.h"

#include "sdcdb.h"
#include "sisdb.h"
#include <sis_disk.h>

typedef struct s_sisdb_wsno_cxt
{
 
    s_sis_disk_class  *write_class;   // 写盘类
    s_sis_sds          work_path;     // 可配置 也可传入
    int                page_size;    
    
    int                iswhead;       // 是否写了头
 
	s_sis_sds          wsno_date;    // 文件名      
	s_sis_sds          wsno_keys;    
	s_sis_sds          wsno_sdbs; 

	s_sdcdb_worker    *wsno_unzip;     // 解压 s_sdcdb_compress 中来的数据

    // sis_method_define *cb_sub_start;    // 必须是字符的日期
    // sis_method_define *cb_sub_stop;     // 必须是字符的日期

    // sis_method_define *cb_dict_sdbs;    // 表结构 json字符串 
    // sis_method_define *cb_dict_keys;    // 代码串 字符串

    // sis_method_define *cb_sdcdb_compress;  // s_sdcdb_compress
    // sis_method_define *cb_sisdb_bytes;     // s_sisdb_chars

}s_sisdb_wsno_cxt;

bool sisdb_wsno_init(void *, void *);
void sisdb_wsno_uninit(void *);

void market_file_method_init(void *);

int cmd_sisdb_wsno_getcb(void *worker_, void *argv_);

#endif