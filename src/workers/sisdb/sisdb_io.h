
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_IO_H
#define _SISDB_IO_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"
#include "sis_file.h"

#include "sisdb.h"
#include "sis_obj.h"

//  非结构化数据IO
s_sis_sds sisdb_single_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_);
s_sis_sds sisdb_single_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_);
int sisdb_single_set(s_sisdb_cxt *sisdb_, const char *key_, uint16 format_, s_sis_sds argv_);
int sisdb_single_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_);
int sisdb_single_dels(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_);

// 以下为结构化数据的IO
// 只返回内存中的对应数据
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

int sisdb_single_sub(s_sisdb_cxt *sisdb_, const char *key_, s_sis_net_message *);
int sisdb_single_unsub(s_sisdb_cxt *sisdb_, const char *key_, s_sis_net_message *);

int sisdb_sub(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_net_message *);
int sisdb_unsub(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_net_message *);

int sisdb_subsno(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_net_message *);
int sisdb_unsubsno(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_net_message *);

// // 订阅消息 系统会保留相关信息
// int sis_net_class_subscibe(s_sis_net_class *, s_sis_net_message *);
// // 发布消息 对所有订阅了我的消息的广播
// int sis_net_class_publish(s_sis_net_class *, s_sis_net_message *);
// // 收到客户的订阅请求增加发布的map
// int sis_net_class_pub_add(s_sis_net_class *, s_sis_net_message *);
// // 客户断开连接 删除该客户的订阅map
// int sis_net_class_pub_del(s_sis_net_class *, int sid_);

#endif  /* _SISDB_IO_H */
