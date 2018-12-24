
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_METHOD_H
#define _SISDB_METHOD_H

#include "sis_core.h"
#include "sis_map.h"
#include "sis_method.h"

#include "sisdb_collect.h" 

s_sis_map_pointer *sisdb_method_define_create(s_sis_map_pointer *map_);
void sisdb_method_define_destroy(s_sis_map_pointer *map_);  // sis_method_map_destroy

////////////////////////////////
//  下面是功能调用
////////////////////////////////
// 返回值 NULL 
s_sis_sds sisdb_call_list_sds(s_sis_db *db_, const char *com_);

s_sis_sds sisdb_call_market_init(s_sis_db *db_, const char *com_);

s_sis_sds sisdb_call_get_price_sds(s_sis_db *db_,const char *com_);

// 仅仅在info表中获取匹配的代码
s_sis_sds sisdb_call_get_code_sds(s_sis_db *db_,const char *com_);
// 得到任意数据表中共有多少key
s_sis_sds sisdb_call_get_collects_sds(s_sis_db *db_, const char *com_); 

#endif  /* _SIS_CALL_H */
