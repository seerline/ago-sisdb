#ifndef _SISDB_INCR_H
#define _SISDB_INCR_H

#include "sis_method.h"
#include "sis_db.h"
#include "sis_disk.h"
#include "sis_incrzip.h"

// 结构化数据压缩和解压

#define  SISDB_ZIP_PART_SIZE  SIS_DISK_MAXLEN_SICPART
#define  SISDB_ZIP_PAGE_SIZE  SIS_DISK_MAXLEN_SICPAGE

// 对获取的数据压缩或解压
// 对待sdb多记录压缩 需要一次性把单类数据压缩进去
// 对待sno多记录压缩 需要修改sdbs的结构传入 worker中
typedef struct s_sisdb_incr
{
	s_sis_incrzip_class  *incrzip;      // 

	void                 *cb_source;    // 返回对象
	cb_incrzip_encode    *cb_encode;    // 数据以回调方式返回 
	cb_incrzip_decode    *cb_decode;    // 数据以回调方式返回 
	int      	          page_size;    // 超过多大数据重新初始化 字节
	int      	          part_size;    // 单个数据块的大小
	int      	          summ_size;    // 累计尺寸

	s_sis_map_list       *work_keys;    // key 的结构字典表 s_sis_sds
	s_sis_map_list       *work_sdbs;    // sdb 的结构字典表 s_sis_dynamic_db 包括
} s_sisdb_incr;

//////////////////////////////////////////////////////////////////
//------------------------s_sisdb_incr -----------------------//
//////////////////////////////////////////////////////////////////
s_sisdb_incr *sisdb_incr_create();
void sisdb_incr_destroy(s_sisdb_incr *);

void sisdb_incr_clear(s_sisdb_incr *);

int sisdb_incr_getsize(s_sisdb_incr *);

void sisdb_incr_set_keys(s_sisdb_incr *, s_sis_sds );
void sisdb_incr_set_sdbs(s_sisdb_incr *, s_sis_sds );

int sisdb_incr_get_kidx(s_sisdb_incr *, const char *);
int sisdb_incr_get_sidx(s_sisdb_incr *, const char *);

const char *sisdb_incr_get_kname(s_sisdb_incr *, int);
const char *sisdb_incr_get_sname(s_sisdb_incr *, int);

// 写入需要解压的数据
// sisdb_incr_set_keys(reader_->sub_ziper, work_keys);
// sisdb_incr_set_sdbs(reader_->sub_ziper, work_sdbs);
// sisdb_incr_unzip_start(reader_->sub_ziper, reader_, cb_encode);
void sisdb_incr_unzip_start(s_sisdb_incr *, void *cb_source, cb_incrzip_decode *);
void sisdb_incr_unzip_set(s_sisdb_incr *, s_sis_db_incrzip *);
void sisdb_incr_unzip_stop(s_sisdb_incr *worker);
// 写入需要压缩的数据
// sisdb_incr_set_keys(reader_->sub_ziper, work_keys);
// sisdb_incr_set_sdbs(reader_->sub_ziper, work_sdbs);
// sisdb_incr_zip_start(reader_->sub_ziper, reader_, cb_encode);
void sisdb_incr_zip_start(s_sisdb_incr *, void *cb_source, cb_incrzip_encode *);
void sisdb_incr_zip_set(s_sisdb_incr *, int ,int, char *, size_t);
void sisdb_incr_zip_restart(s_sisdb_incr *);
void sisdb_incr_zip_stop(s_sisdb_incr *worker);

#endif