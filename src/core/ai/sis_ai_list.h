#ifndef _SIS_AI_LIST_H
#define _SIS_AI_LIST_H

#include <sis_core.h>
#include <sis_list.h>

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_ai_list ---------------------------------//
//  时序动态结构的列表
//////////////////////////////////////////////////////////////////////////
typedef struct s_sis_ai_list_field {
	char name[64];
    int  agoidx;    // 大于该值的都填一样的值，初始为 -1 
} s_sis_ai_list_field;

typedef struct s_sis_ai_list_key {
	int    time;   // 以此为顺序排列
	double newp;   // 价格
    int    output; // 输出类型
} s_sis_ai_list_key;

typedef struct s_sis_ai_list {
    int isinit;
	s_sis_struct_list  *fields;   // 特征字符串，只能追加不能插入和删除 s_sis_ai_list_field
    s_sis_struct_list  *klists;   // 结构类型 s_sis_ai_list_key
	s_sis_pointer_list *vlists;   // 动态数据结构类型指针 double *value 单条数据个数为fields的个数
} s_sis_ai_list;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_ai_list *sis_ai_list_create(); 
void sis_ai_list_destroy(s_sis_ai_list *list_);
void sis_ai_list_clear(s_sis_ai_list *list_);

int sis_ai_list_init_field(s_sis_ai_list *, const char *fn_);
int sis_ai_list_init_finished(s_sis_ai_list *);
// 根据时间插入对应的数据
int sis_ai_list_key_push(s_sis_ai_list *,  int time, double newp, double output);
int sis_ai_list_value_push(s_sis_ai_list *, int time, const char *fn_, double value_);

const char *sis_ai_list_get_field(s_sis_ai_list *, int index_);
s_sis_ai_list_key *sis_ai_list_get_key(s_sis_ai_list *, int index_);

double sis_ai_list_get(s_sis_ai_list *, int fidx_, int kidx_);

int sis_ai_list_get_field_size(s_sis_ai_list *list_);
int sis_ai_list_getsize(s_sis_ai_list *list_);

#ifdef __cplusplus
}
#endif

#endif /* _SIS_AI_LIST_H */