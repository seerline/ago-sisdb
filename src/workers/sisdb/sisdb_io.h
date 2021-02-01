
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

#ifndef _SISDB_IO_H
#define _SISDB_IO_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"
#include "sis_file.h"

#include "sisdb.h"
#include "sis_obj.h"

#include "sisdb_collect.h"

// #define SISDB_COM_LAST  "{\"search\":{\"start\":-1,\"count\":1}}"

//  非结构化数据IO
s_sis_sds sisdb_one_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_);
s_sis_sds sisdb_one_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_);
int sisdb_one_set(s_sisdb_cxt *sisdb_, const char *key_, uint8 style_, s_sis_sds argv_);
int sisdb_one_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_);
int sisdb_one_dels(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_);

// 以下为结构化数据的IO
// 没有date字段 就只返回内存中的对应数据 
// (未支持,可能返回数据过多)如果参数中有 workdate:20200101 字段就表示内存有取内存 内存没有取磁盘 全部没有才返回空
s_sis_sds sisdb_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_);

// 只返回内存中每个key的最后一条记录
s_sis_sds sisdb_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_sds argv_);

// 删除某个key的某些数据
int sisdb_del(s_sisdb_cxt *sisdb_, const char *key_,  s_sis_sds argv_);
// 删除多个key的数据
int sisdb_dels(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_sds argv_);

// 以json方式写入数据 自动创建数据结构表
int sisdb_set_chars(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_);
// 以二进制方式写入数据 数据表必须已经存在 否则返回错误
int sisdb_set_bytes(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_);

/////////////////
//  sub function
/////////////////
// 处理最新的数据订阅问题
void sisdb_make_sub_message(s_sisdb_cxt *sisdb_, s_sisdb_collect *collect_, uint8 style_, s_sis_sds in_);

int sisdb_one_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *);
int sisdb_multiple_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *);

int sisdb_unsub_whole(s_sisdb_cxt *sisdb_, int );
int sisdb_one_unsub(s_sisdb_cxt *sisdb_,s_sis_net_message *);
int sisdb_multiple_unsub(s_sisdb_cxt *sisdb_, s_sis_net_message *);

#endif  /* _SISDB_IO_H */
