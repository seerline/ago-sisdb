
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

#include "sisdb_collect.h"

// #define SISDB_COM_LAST  "{\"search\":{\"start\":-1,\"count\":1}}"

int sis_str_divide_sds(const char *in_, char ch_, s_sis_sds *one_,  s_sis_sds *two_);

//  非结构化数据IO
s_sis_sds sisdb_one_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_);
s_sis_sds sisdb_one_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_);
int sisdb_one_set(s_sisdb_cxt *sisdb_, const char *key_, uint8 style_, s_sis_sds argv_);
int sisdb_one_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_);
int sisdb_one_dels(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_);

// 得到参数中是字符串还是字节流
int sisdb_get_format(s_sis_sds argv_);
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
// 处理最新的数据订阅问题
void sisdb_make_sub_message(s_sisdb_cxt *sisdb_, s_sisdb_collect *collect_, uint8 style_, s_sis_sds in_, size_t ilen_);

int sisdb_one_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *, bool issno_);
int sisdb_multiple_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *, bool issno_);

int sisdb_unsub_whole(s_sisdb_cxt *sisdb_, int , bool issno_);
int sisdb_one_unsub(s_sisdb_cxt *sisdb_,s_sis_net_message *, bool issno_);
int sisdb_multiple_unsub(s_sisdb_cxt *sisdb_, s_sis_net_message *, bool issno_);

// sno 的订阅需要确定日期 如果没有参数 表示订阅当日且最新的 需要开启线程 传送历史数据 传输完毕后 
// 如果订阅日期是当日的 把订阅的键值注册到 single 和 multiple 列表中
int sisdb_one_subsno(s_sisdb_cxt *sisdb_, s_sis_net_message *);
int sisdb_multiple_subsno(s_sisdb_cxt *sisdb_, s_sis_net_message *);

int sisdb_unsubsno_whole(s_sisdb_cxt *sisdb_, int );
// 如果有线程在工作 就中断线程 否则就调用 sisdb_one_unsub
int sisdb_one_unsubsno(s_sisdb_cxt *sisdb_,s_sis_net_message *);
// 如果有线程在工作 就中断线程 否则就调用 sisdb_multiple_unsub
int sisdb_multiple_unsubsno(s_sisdb_cxt *sisdb_, s_sis_net_message *);

// // 订阅消息 系统会保留相关信息
// int sis_net_class_subscibe(s_sis_net_class *, s_sis_net_message *);
// // 发布消息 对所有订阅了我的消息的广播
// int sis_net_class_publish(s_sis_net_class *, s_sis_net_message *);
// // 收到客户的订阅请求增加发布的map
// int sis_net_class_pub_add(s_sis_net_class *, s_sis_net_message *);
// // 客户断开连接 删除该客户的订阅map
// int sis_net_class_pub_del(s_sis_net_class *, int sid_);

#endif  /* _SISDB_IO_H */
