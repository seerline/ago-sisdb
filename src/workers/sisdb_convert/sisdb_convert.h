#ifndef _SISDB_CONVERT_H
#define _SISDB_CONVERT_H

#include "sis_method.h"
	
typedef struct s_sisdb_convert_unit
{
	char                   dateset;
	s_sis_dynamic_convert *convert;
} s_sisdb_convert_unit;

typedef struct s_sisdb_convert_cxt
{
	void                *father;      //
	s_sis_map_pointer   *converts;    // 需要转换的表 (dataset+table) s_sis_pointer_list * --> s_sisdb_convert_unit
	
} s_sisdb_convert_cxt;


bool  sisdb_convert_init(void *, void *);
void  sisdb_convert_uninit(void *);
void  sisdb_convert_method_init(void *);
void  sisdb_convert_method_uninit(void *);

int cmd_sisdb_convert_init(void *worker_, void *argv_);
// 接收数据并顺序写入log
int cmd_sisdb_convert_write(void *worker_, void *argv_);


#endif