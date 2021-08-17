#ifndef _SIS_NOTICE_H
#define _SIS_NOTICE_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "worker.h"

// 这是一个广播组件 所有的订阅 分布式数据同步都通过该组件来处理数据

#define SIS_SUB_ONEKEY_ONE      0   // 订阅一个单键值 指定值 强匹配
#define SIS_SUB_ONEKEY_MUL      1   // 订阅指定的多个KS值 强匹配 sub
#define SIS_SUB_ONEKEY_CMP      2   // 订阅多个单键值 头匹配 
#define SIS_SUB_ONEKEY_ALL      3   // 订阅所有的单键 *

#define SIS_SUB_MULKEY_ONE      5   // 订阅指定的一个KS值 强匹配 sub
#define SIS_SUB_MULKEY_MUL      6   // 订阅指定的多个KS值 强匹配 sub
#define SIS_SUB_MULKEY_CMP      7   // 订阅多个KS值 头匹配 key
#define SIS_SUB_MULKEY_ALL      8   // 订阅所有的KS值 *.* msub

// 统一用sub表示订阅 需要头匹配时 第一个key必须是 *,k1,k2 这样可以统一订阅的参数
// key中只要有 , 就表示为头匹配 sdb 始终是强匹配
// sub_keys 和 sub_sdbs 可能为 * 此时需要特殊处理
typedef struct s_sisdb_sub_unit
{
	uint8                 sub_type;   // 订阅类型
	s_sis_sds             sub_keys;   // SIS_SUB_ONEKEY_CMP SIS_SUB_MULKEY_CMP 时有值
	s_sis_sds             sub_sdbs;   // SIS_SUB_MULKEY_CMP 时有值
	s_sis_pointer_list   *netmsgs;    // s_sis_net_message
} s_sisdb_sub_unit;	

typedef struct s_sis_notice_cxt
{
	// 多个 client 订阅的列表 需要一一对应发送
	// 以 subkey 为索引的 s_sisdb_sub_unit 同一key会有多个用户订阅
	s_sis_map_pointer  *sub_onekeys;     // s_sisdb_sub_unit 
	s_sis_map_pointer  *sub_mulkeys;     // s_sisdb_sub_unit 

	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

} s_sis_notice_cxt;

bool  sis_notice_init(void *, void *);
void  sis_notice_uninit(void *);

// 订阅 头匹配时 需要在key开头增加 "*," 
int sis_notice_sub(void *worker_, void *argv_);
// 取消订阅
int sis_notice_unsub(void *worker_, void *argv_);
// 发布一个信息
int sis_notice_pub(void *worker_, void *argv_);

#endif