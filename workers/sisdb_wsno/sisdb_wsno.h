#ifndef _SISDB_WSNO_H
#define _SISDB_WSNO_H

#include "sis_method.h"
#include "sis_disk.h"
#include "sisdb_incr.h"

#define  SIS_WSNO_NONE     0 // 
#define  SIS_WSNO_INIT     1 // 是否初始化
#define  SIS_WSNO_OPEN     2 // 是否打开
#define  SIS_WSNO_HEAD     3 // 是否写了头
#define  SIS_WSNO_EXIT     4 // 退出
#define  SIS_WSNO_FAIL     5 // 打开失败

// 支持chars 非压缩写入
// 支持增量压缩格式写入 解压后再写入文件 开头会跳过非起始包 解压函数会自动忽略开头的非起始包

typedef struct s_sisdb_wsno_cxt
{
    int                status;
    s_sis_sds_save    *work_path;     // 可配置 也可传入
    s_sis_sds_save    *work_name;     // 可配置 也可传入
    s_sis_disk_writer *writer;        // 写盘类
	
	int                wmode;         // 0 append 1 rewrite
	int                stop_time;     // 停止时间
	int                work_date;     // 工作日期  
	s_sis_sds          work_keys;     // 筛选后的 
	s_sis_sds          work_sdbs;     // 筛选后的

	s_sis_sds          wsno_keys;     // 外部写入的keys
	s_sis_sds          wsno_sdbs;     // 外部写入的sdbs

	s_sisdb_incr       *work_unzip;    // 解压 s_snodb_compress 中来的数据

}s_sisdb_wsno_cxt;

bool sisdb_wsno_init(void *, void *);
void sisdb_wsno_uninit(void *);

int cmd_sisdb_wsno_getcb(void *worker_, void *argv_);

bool sisdb_wsno_start(s_sisdb_wsno_cxt *context);
void sisdb_wsno_stop(s_sisdb_wsno_cxt *context);

#endif