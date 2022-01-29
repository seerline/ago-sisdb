
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
#include "sisdb_fmap.h"

// 读取数据表结构信息
s_sis_sds sisdb_io_show_sds(s_sisdb_cxt *sisdb_);
// 读取数据表结构信息
int sisdb_io_create(s_sisdb_cxt *sisdb_, const char *sname_, s_sis_json_node *node_);

// 默认单键和多键都是字符型的数据 结构化数据都是二进制数据 
s_sis_sds sisdb_io_get_sds(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_);
// 如果获得的数据是二进制的 就转成json格式数据返回
s_sis_sds sisdb_io_get_chars_sds(s_sisdb_cxt *sisdb_, const char *key_, int rfmt_, s_sis_json_node *node_);
// 修改数据
int sisdb_io_update(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds imem_);

// 以json方式写入数据 自动创建数据结构表
int sisdb_io_set_chars(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds vmem_);
int sisdb_io_set_one_chars(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_);
// 以二进制方式写入数据 数据表必须已经存在 否则返回错误
int sisdb_io_set_bytes(s_sisdb_cxt *sisdb_, const char *key_, s_sis_pointer_list *vlist_);
int sisdb_io_set_one_bytes(s_sisdb_cxt *sisdb_, const char *key_, s_sis_pointer_list *vlist_);
// 删除某个key的某些数据 必须带参数 否则不执行删除操作
int sisdb_io_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_);
int sisdb_io_del_one(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_);
int sisdb_io_drop(s_sisdb_cxt *sisdb_, const char *sname_);
// 多单键取值只处理字符串 非字符串不处理
s_sis_sds sisdb_io_gets_one_sds(s_sisdb_cxt *sisdb_, const char *keys_);
// 多键取值只返回字符串 并且只返回最后一条记录
s_sis_sds sisdb_io_gets_sds(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_);
// 只获取单键数值
s_sis_sds sisdb_io_keys_one_sds(s_sisdb_cxt *sisdb_, const char *keys_);
s_sis_sds sisdb_io_keys_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_json_node *node_);
int sisdb_io_dels_one(s_sisdb_cxt *sisdb_, const char *key_);
// 删除某个key 所有结构 或者删除 某个结构所有 key 的数据 
int sisdb_io_dels(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_);


#endif  /* _SISDB_IO_H */
