
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_COMMAND_H
#define _STS_COMMAND_H

#include "sts_core.h"
#include "sts_conf.h"
#include "sts_map.h"
#include "sts_list.h"

#include "sts_malloc.h"
#include "sts_stock.h"
#include "sts_math.h"

#include "digger_io.h"

typedef s_sts_sds _sts_command_proc(s_digger_server *s_,const char *argv_);

#define STS_JSON_KEY_ARRAY ("value")   // 获取一个股票一条数据
#define STS_JSON_KEY_ARRAYS ("values") //获取一个股票多个数据
#define STS_JSON_KEY_GROUPS ("groups")  //获取多个股票数据
#define STS_JSON_KEY_FIELDS ("fields")  // 字段定义
#define STS_JSON_KEY_COLLECTS ("collects") // 获取一个db的所有股票代码


#define STS_QUERY_COM_NORMAL  "{\"format\":\"struct\"}"
#define STS_QUERY_COM_LAST    "{\"format\":\"struct\",\"range\":{\"start\":-1}}"
#define STS_QUERY_COM_SEARCH  "{\"format\":\"struct\",\"search\":{\"min\":%d,\"max\":%d}}"

#pragma pack(push,1)

typedef struct s_sts_command {
    char *name;
    _sts_command_proc *proc;
}s_sts_command;

#pragma pack(pop)

s_sts_map_pointer *sts_command_create();
void sts_command_destroy(s_sts_map_pointer *map_);
s_sts_command *sts_command_get(s_sts_map_pointer *map_, const char *name_);
////////////////////////////////
//  下面是功能调用
////////////////////////////////
s_sts_sds sts_command_get_price_sds(s_digger_server *s_,const char *com_);
s_sts_sds sts_command_get_right_sds(s_digger_server *s_,const char *com_);

s_sts_sds sts_command_find_code_sds(s_digger_server *s_,const char *com_);

#endif  /* _STS_COMMAND_H */
