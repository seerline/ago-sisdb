
#ifndef _SIS_NET_IO_H
#define _SIS_NET_IO_H

#include <sis_core.h>
#include <sis_memory.h>
#include <sis_net.h>

void sis_net_ask_with_chars(s_sis_net_message *netmsg_, 
    char *cmd_, char *key_, char *val_, size_t vlen_);

void sis_net_ask_with_bytes(s_sis_net_message *netmsg_, 
    char *cmd_, char *key_, char *val_, size_t vlen_);

void sis_net_ask_with_argvs(s_sis_net_message *netmsg_, const char *in_, size_t ilen_);

// in_被吸入
void sis_net_ans_with_chars(s_sis_net_message *, const char *in_, size_t ilen_);
// *** 这个函数需要检查
void sis_net_ans_set_key(s_sis_net_message *netmsg_, const char *kname_, const char *sname_);

void sis_net_ans_with_bytes(s_sis_net_message *, const char *in_, size_t ilen_);
void sis_net_ans_with_argvs(s_sis_net_message *, const char *in_, size_t ilen_);
// 获取数据流
s_sis_sds sis_net_get_argvs(s_sis_net_message *netmsg_, int index);

void sis_net_ans_with_noreply(s_sis_net_message *);

void sis_net_ans_with_int(s_sis_net_message *, int in_);
void sis_net_ans_with_ok(s_sis_net_message *);
void sis_net_ans_with_error(s_sis_net_message *, char *rval_, size_t vlen_);
void sis_net_ans_with_null(s_sis_net_message *);
void sis_net_ans_with_sub_start(s_sis_net_message *, const char *info_);
void sis_net_ans_with_sub_wait(s_sis_net_message *,  const char *info_);
void sis_net_ans_with_sub_stop(s_sis_net_message *,  const char *info_);

#endif //_SIS_CRYPT_H
