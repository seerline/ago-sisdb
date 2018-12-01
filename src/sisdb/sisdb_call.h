
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_ASK_H
#define _SISDB_ASK_H

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

typedef s_sis_sds _sisdb_call_proc(s_sis_db *s_,const char *com_);


#define SIS_QUERY_COM_NORMAL  "{\"format\":\"struct\"}"
#define SIS_QUERY_COM_INFO    "{\"format\":\"struct\",\"fields\":\"dot,prc-unit,vol-unit\"}"
#define SIS_QUERY_COM_SEARCH  "{\"format\":\"struct\",\"search\":{\"min\":%d,\"max\":%d}}"

#pragma pack(push,1)

typedef struct s_sisdb_call {
    const char *name;
    _sisdb_call_proc *proc;
    const char *explain;
}s_sisdb_call;

#pragma pack(pop)

void sisdb_init_call_define(s_sis_map_pointer *calls_);
s_sisdb_call *sisdb_call_find_define(s_sis_map_pointer *map_, const char *name_);

////////////////////////////////
//  下面是功能调用
////////////////////////////////
// 返回值 NULL 
s_sis_sds sisdb_call_list_command(s_sis_db *db_, const char *com_);
s_sis_sds sisdb_call_market_init(s_sis_db *db_, const char *com_);

s_sis_sds sisdb_call_get_price_sds(s_sis_db *db_,const char *com_);

// 仅仅在info表中获取匹配的代码
s_sis_sds sisdb_call_get_code_sds(s_sis_db *db_,const char *com_);
// 得到任意数据表中共有多少key
s_sis_sds sisdb_call_get_collects_sds(s_sis_db *db_, const char *com_); 

#endif  /* _SIS_CALL_H */
