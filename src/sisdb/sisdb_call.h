
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_CALL_H
#define _SISDB_CALL_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_map.h"
#include "sis_list.h"

#include "sis_malloc.h"
#include "sis_math.h"
#include "sis_time.h"
#include "sis_json.h"
#include "sis_str.h"
#include "sisdb_sys.h"

#include "sisdb_collect.h" 

#define SIS_QUERY_COM_NORMAL  "{\"format\":\"struct\"}"
#define SIS_QUERY_COM_INFO    "{\"format\":\"struct\",\"fields\":\"dot,pzoom,vunit\"}"
#define SIS_QUERY_COM_SEARCH  "{\"format\":\"struct\",\"search\":{\"min\":%d,\"max\":%d}}"

#pragma pack(push,1)

#pragma pack(pop)

////////////////////////////////
//  下面是功能调用
////////////////////////////////
// 返回值 NULL 
void *sisdb_call_list_sds(void *db_, void *com_);

void *sisdb_call_market_init(void *db_, void *com_);

void *sisdb_call_get_close_sds(void *db_,void *com_);

// 仅仅在info表中获取匹配的代码
void *sisdb_call_get_code_sds(void *db_,void *com_);
// 得到任意数据表中共有多少key
void *sisdb_call_get_collects_sds(void *db_, void *com_); 

// 得到除权后的价格
void * sisdb_call_get_right_sds(void *db_, void *com_);
#endif  /* _SIS_CALL_H */
