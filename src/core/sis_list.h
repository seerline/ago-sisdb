#ifndef _SIS_LIST_H
#define _SIS_LIST_H

#include "sis_core.h"
#include "sis_str.h"
#include "sis_sds.h"

#include "sis_malloc.h"

// 定义一个结构体链表
// 定义一个指针读写链表
// 定义一个字符串读写链表

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_struct_list ---------------------------------//
//  固定长度的列表
//////////////////////////////////////////////////////////////////////////

typedef struct s_sis_struct_list {
	int		     maxcount; // 总数
	int		     count;    // 当前个数
	int		     start;    // 开始位置 优化limit引起的频繁内存移动
	int          len;      // 每条记录的长度
	void        *buffer;   // 必须是mallco申请的char*类型
} s_sis_struct_list;

#define STRUCT_LIST_STEP_ROW 256  //默认增加的记录数
#ifdef __cplusplus
extern "C" {
#endif
s_sis_struct_list *sis_struct_list_create(int len_); //len_为每条记录长度
void sis_struct_list_destroy(void *list_);
void sis_struct_list_clear(s_sis_struct_list *list_);

int sis_struct_list_pushs(s_sis_struct_list *, void *in_, int count_);
int sis_struct_list_push(s_sis_struct_list *, void *in_);
int sis_struct_list_insert(s_sis_struct_list *, int index_, void *in_);
int sis_struct_list_inserts(s_sis_struct_list *, int index_, void *in_, int count_);
int sis_struct_list_update(s_sis_struct_list *, int index_, void *in_);
void *sis_struct_list_first(s_sis_struct_list *);
void *sis_struct_list_last(s_sis_struct_list *);
void *sis_struct_list_get(s_sis_struct_list *, int index_);
void *sis_struct_list_next(s_sis_struct_list *list_, void *);

void *sis_struct_list_empty(s_sis_struct_list *list_);
void *sis_struct_list_offset(s_sis_struct_list *list_, void *, int offset_);

void sis_struct_list_set_size(s_sis_struct_list *list_, int len_);
void sis_struct_list_set_maxsize(s_sis_struct_list *list_, int maxlen_);
int sis_struct_list_set(s_sis_struct_list *, void *in_, int inlen_);

int sis_struct_list_setone(s_sis_struct_list *, int index_, void *in_);

void sis_struct_list_rect(s_sis_struct_list *list_, int rows_);
void sis_struct_list_limit(s_sis_struct_list *, int limit_);
int sis_struct_list_clone(s_sis_struct_list *src_, s_sis_struct_list *dst_);
int sis_struct_list_append(s_sis_struct_list *src_, s_sis_struct_list *dst_);

void *sis_struct_list_pop(s_sis_struct_list *list_);
int sis_struct_list_delete(s_sis_struct_list *src_, int start_, int count_);
int sis_struct_list_pack(s_sis_struct_list *list_);

#ifdef __cplusplus
}
#endif


///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_sort_list ---------------------------------//
//  固定长度的列表
//////////////////////////////////////////////////////////////////////////


typedef struct s_sis_sort_list {
	int8               isascend; // 是否为升序
	s_sis_struct_list *key;      // int64 类型
	s_sis_struct_list *value;    // 结构类型
} s_sis_sort_list;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_sort_list *sis_sort_list_create(int len_); 
void sis_sort_list_destroy(void *);
void sis_sort_list_clear(s_sis_sort_list *list_);
void sis_sort_list_clone(s_sis_sort_list *src_,s_sis_sort_list *des_);

void *sis_sort_list_set(s_sis_sort_list *, int64 key_, void *in_);
void *sis_sort_list_first(s_sis_sort_list *);
void *sis_sort_list_last(s_sis_sort_list *);
void *sis_sort_list_get(s_sis_sort_list *, int index_);

void *sis_sort_list_find(s_sis_sort_list *, int64 key_);

void *sis_sort_list_next(s_sis_sort_list *list_, void *value_);
void *sis_sort_list_prev(s_sis_sort_list *list_, void *value_);
// 返回下一条记录指针
void *sis_sort_list_del(s_sis_sort_list *list_, void *value_);

void sis_sort_list_deli(s_sis_sort_list *list_, int index_);

int sis_sort_list_getsize(s_sis_sort_list *list_);
#ifdef __cplusplus
}
#endif

// 排序指针队列
typedef struct s_sis_pint_slist {
	int		     maxcount;  // 总数
	int		     count;     // 当前个数
	int8         isascend;  // 是否为升序
	int          *keys;     // int* 类型
	void         *value;    // 结构类型
} s_sis_pint_slist;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_pint_slist *sis_pint_slist_create(); 
void sis_pint_slist_destroy(void *);
void sis_pint_slist_clear(s_sis_pint_slist *list_);

void sis_pint_slist_set_maxsize(s_sis_pint_slist *list_, int rows_);

int sis_pint_slist_set(s_sis_pint_slist *, int key_, void *in_);

void *sis_pint_slist_get(s_sis_pint_slist *, int index_);

int sis_pint_slist_delk(s_sis_pint_slist *list_, int key_);

void sis_pint_slist_del(s_sis_pint_slist *list_, int index_);

int sis_pint_slist_getsize(s_sis_pint_slist *list_);

#ifdef __cplusplus
}
#endif

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_double_list -----------------------------//
// 一组浮点数的数组，支持取均值，排序后取分割区块值 
//////////////////////////////////////////////////////////////////////////
typedef struct s_sis_split {
	int min;
	int max;
} s_sis_split;

typedef struct s_sis_double_split {
	double minv;
	double maxv;
} s_sis_double_split;

typedef struct s_sis_double_sides {
	double up_minv;
	double up_midv;
	double up_maxv;
	double up_topv;  // 极限大
	double dn_topv;  // 极限小
	double dn_minv;
	double dn_midv;
	double dn_maxv;
} s_sis_double_sides;

typedef struct s_sis_double_list {
	double avgv;
	double minv;
	double maxv;
	s_sis_struct_list *value;   // double
} s_sis_double_list;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_double_list *sis_double_list_create(); 
void sis_double_list_destroy(void *);
// 升序
int sis_sort_double_list(const void *arg1, const void *arg2);
// 升序
int sis_sort_uint32_list(const void *arg1, const void *arg2);

void sis_double_list_calc(s_sis_double_list *src_);
// 排序必须放置到新的数组中
void sis_double_list_sort(s_sis_double_list *src_);
// 限制型插入 不计算最大最小值 均值
int sis_double_list_push_limit(s_sis_double_list *, int maxnums, double in_);

int sis_double_list_push(s_sis_double_list *, double in_);

double sis_double_list_get(s_sis_double_list *, int index_);
double *sis_double_list_gets(s_sis_double_list *, int index_);

int sis_double_list_getsize(s_sis_double_list *list_);

void sis_double_list_clear(s_sis_double_list *list_);

// 根据合计值 按nums来分割数据
int sis_double_list_value_split(s_sis_double_list *list_, int nums_, double split[]);
// 根据合计值 按nums来分割数据
int sis_double_list_split(s_sis_double_list *list_, int nums_, double split[]);
// 以0为中间点
int sis_double_list_count_nozero_split(s_sis_double_list *list_,s_sis_struct_list *splits_, int nums_);
// 以0为中间点 不包括0附近的值
int sis_double_list_count_zero_pair_nosort(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_);
int sis_double_list_count_zero_pair(s_sis_double_list *list_,s_sis_struct_list *splits_, int nums_);
// 获取区间分片，排序后按数量分片
int sis_double_list_count_split(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_);
// 获取区间分片，取左右两边为边界的分片, steps_ 为采样步长
int sis_double_list_count_sides(s_sis_double_list *list_, s_sis_struct_list *sides_, int steps_);

// 获取区间分片，按最大最小值分片,
int sis_double_list_simple_split(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_);
int sis_double_list_simple_sides(s_sis_double_list *list_, s_sis_struct_list *sides_, int steps_);

#ifdef __cplusplus
}
#endif
///////////////////////////////////////////////////////////////////////////
//------------------------s_pointer_list --------------------------------//
//  存储指针的列表,记录长度为sizeof(char *)
///////////////////////////////////////////////////////////////////////////
#define POINTER_LIST_STEP_ROW 1024  //默认增加的记录数

typedef struct s_sis_pointer_list {
	int		     maxcount; // 总数
	int		     count;    // 当前个数
	int          len;      // 每条记录的长度
	void        *buffer;   // 
	void (*vfree)(void *);   // == NULL 不释放对应内存
} s_sis_pointer_list;
#ifdef __cplusplus
extern "C" {
#endif
s_sis_pointer_list *sis_pointer_list_create(); 

void sis_pointer_list_destroy(void *list_);
void sis_pointer_list_clear(s_sis_pointer_list *list_);

int sis_pointer_list_clone(s_sis_pointer_list *src_, s_sis_pointer_list *des_);

int sis_pointer_list_push(s_sis_pointer_list *, void *in_);
int sis_pointer_list_set(s_sis_pointer_list *, void *in_);
int sis_pointer_list_update(s_sis_pointer_list *, int index_, void *in_);
int sis_pointer_list_insert(s_sis_pointer_list *, int index_, void *in_);

void *sis_pointer_list_get(s_sis_pointer_list *, int index_);
void *sis_pointer_list_first(s_sis_pointer_list *);
// void *sis_pointer_list_next(s_sis_pointer_list *list_, void *);

int sis_pointer_list_delete(s_sis_pointer_list *src_, int start_, int count_);

int sis_pointer_list_indexof(s_sis_pointer_list *list_, void *in_);
int sis_pointer_list_find_and_update(s_sis_pointer_list *, void *finder_, void *in_);
int sis_pointer_list_find_and_delete(s_sis_pointer_list *list_, void *finder_);
#ifdef __cplusplus
}
#endif

///////////////////////////////////////////////////////////////////////////
//------------------------s_int_list --------------------------------//
//  存储指针的列表,记录长度为sizeof(int64)
///////////////////////////////////////////////////////////////////////////

#define s_sis_int_list s_sis_pointer_list
#define sis_int_list_create sis_pointer_list_create
#define sis_int_list_destroy sis_pointer_list_destroy
#define sis_int_list_clear sis_pointer_list_clear

int sis_int_list_indexof(s_sis_pointer_list *, int64 in_);

int   sis_int_list_push(s_sis_pointer_list *, int64 in_);
int   sis_int_list_set(s_sis_pointer_list *, int64 in_);
int   sis_int_list_update(s_sis_pointer_list *, int index_, int64 in_);
int   sis_int_list_insert(s_sis_pointer_list *, int index_, int64 in_);
int64 sis_int_list_get(s_sis_pointer_list *, int index_);
int   sis_int_list_delete(s_sis_pointer_list *src_, int start_, int count_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_fsort_list ---------------------------------//
//  以浮点数排序的指针类型数据
//////////////////////////////////////////////////////////////////////////

typedef struct s_sis_fsort_list {
	s_sis_struct_list  *key;     // double 类型
	s_sis_pointer_list *value;   // 结构类型
} s_sis_fsort_list;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_fsort_list *sis_fsort_list_create(void *vfree_); 
void sis_fsort_list_destroy(void *);
void sis_fsort_list_clear(s_sis_fsort_list *list_);
// 默认从大到小排序
int  sis_fsort_list_set(s_sis_fsort_list *, double key_, void *in_);
void *sis_fsort_list_get(s_sis_fsort_list *, int index_);

double sis_fsort_list_getkey(s_sis_fsort_list *, int index_);

// 找对应数据指针
int sis_fsort_list_find(s_sis_fsort_list *, void *value_);

void sis_fsort_list_del(s_sis_fsort_list *list_, int index_);
int sis_fsort_list_getsize(s_sis_fsort_list *list_);
#ifdef __cplusplus
}
#endif
///////////////////////////////////////////////////////////////////////////
//----------------------s_sis_index_list --------------------------------//
//  以整数为索引 存储指针的列表
///////////////////////////////////////////////////////////////////////////

typedef struct s_sis_index_list {
	fsec_t         wait_sec; // 是否等待 300 毫秒 0 就是直接分配
	fsec_t        *stop_sec; // stop_time 上次删除时的时间 
	int		       count;    // 当前个数
	unsigned char *used;     // 是否有效 初始为 0 
	void          *buffer;   // used 为 0 需调用vfree
	void (*vfree)(void *);   // == NULL 不释放对应内存
} s_sis_index_list;
#ifdef __cplusplus
extern "C" {
#endif
s_sis_index_list *sis_index_list_create(int count_); 
void sis_index_list_destroy(s_sis_index_list *list_);
void sis_index_list_clear(s_sis_index_list *list_);

int sis_index_list_set(s_sis_index_list *, int index_, void *in_);
// 从list找一个无用的索引 返回索引号 -1 表示列表满
int sis_index_list_new(s_sis_index_list *list_);
void *sis_index_list_get(s_sis_index_list *, int index_);

int sis_index_list_first(s_sis_index_list *);
int sis_index_list_next(s_sis_index_list *, int index_);
int sis_index_list_uses(s_sis_index_list *);

int sis_index_list_del(s_sis_index_list *list_, int index_);

#ifdef __cplusplus
}
#endif

typedef struct s_sis_node {
	struct s_sis_node *next, *prev;
	int                index;
	void              *value;        
} s_sis_node;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_node *sis_node_create(); 
void sis_node_destroy(void *node_);

void sis_node_clear(void *node_);

int sis_node_push(s_sis_node *, void *data_);

s_sis_node *sis_node_get(s_sis_node *, int index_);

s_sis_node *sis_node_next(s_sis_node *);

void *sis_node_set(s_sis_node *, int index_, void *data_);

void *sis_node_del(s_sis_node *, int index_);

int sis_node_get_size(s_sis_node *);

#ifdef __cplusplus
}
#endif
typedef struct s_sis_node_list {
	int                 node_size;
	int                 node_count; // 单结点最大数量
	int                 count;      // 实际的数据量
	int                 nouse;      // 被弹出的数量
	s_sis_pointer_list *nodes;      // 数据列表 s_sis_struct_list
} s_sis_node_list;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_node_list *sis_node_list_create(int count, int len_); 
void sis_node_list_destroy(void *list_);
void sis_node_list_clear(s_sis_node_list *list_);

int   sis_node_list_push(s_sis_node_list *, void *in_);
void *sis_node_list_get(s_sis_node_list *, int index_);

void *sis_node_list_empty(s_sis_node_list *);

void *sis_node_list_pop(s_sis_node_list *);

int   sis_node_list_getsize(s_sis_node_list *);
#ifdef __cplusplus
}
#endif
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_string_list --------------------------------//
//  存储不定长字符串的列表，
///////////////////////////////////////////////////////////////////////////
#define STRING_LIST_RD  1
#define STRING_LIST_WR  2

typedef struct s_sis_string_list {
	int    permissions;     //权限
	char*  m_ptr_r;         // 保存的只读字符串
	s_sis_pointer_list *strlist; //存储指针列表 --free为空只读 不为空可写
} s_sis_string_list;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_string_list *sis_string_list_create(); //读写
void sis_string_list_destroy(void *list_);
void sis_string_list_clear(s_sis_string_list *list_);

// read & write
int sis_string_list_load(s_sis_string_list *list_, const char *in_, size_t inlen_, const char *sign);
const char *sis_string_list_get(s_sis_string_list *list_, int index_);
int sis_string_list_getsize(s_sis_string_list *list_);

s_sis_sds sis_string_list_sds(s_sis_string_list *list_);

int sis_string_list_clone(
	s_sis_string_list *src_, 
	s_sis_string_list *des_);
int sis_string_list_merge(
	s_sis_string_list *list_, 
	s_sis_string_list *other_); 
int sis_string_list_across(
	s_sis_string_list *list_, 
	s_sis_string_list *other_);

int sis_string_list_indexof(s_sis_string_list *list_, const char *in_);
int sis_string_list_indexofcase(s_sis_string_list *list_, const char *in_);

int sis_string_list_update(s_sis_string_list *list_, int index_, const char *in_, size_t inlen);
//比较字符串地址并修改，字符串比较应该使用string_list_indexof&sis_string_sis__update
int sis_string_list_find_and_update(s_sis_string_list *list_, char *finder_, const char *in_, size_t inlen);
int sis_string_list_insert(s_sis_string_list *list_, int index_, const char *in_, size_t inlen);
int sis_string_list_push(s_sis_string_list *list_, const char *in_, size_t inlen);
int sis_string_list_push_only(s_sis_string_list *list_, const char *in_, size_t inlen);

void sis_string_list_limit(s_sis_string_list *list_, int limit_);

int sis_string_list_delete(s_sis_string_list *list_, int index_);
//比较字符串地址并删除，字符串比较应该使用string_list_indexof&sis_string_lsis_delete
int sis_string_list_find_and_delete(s_sis_string_list *list_, const char *finder_);

#ifdef __cplusplus
}
#endif

#define SIS_GET_RECORD(_OBJ, _TYPE, _IDX) ((_TYPE*)((char*)(_OBJ) + sizeof(_TYPE)*(_IDX)))
#define SIS_GET_STRUCT_LIST(_OBJ, _TYPE, _IDX) ((_TYPE*)(sis_struct_list_get(_OBJ ,_IDX)))
#define SIS_GET_PONITER_LIST(_OBJ, _TYPE, _IDX) ((_TYPE*)(sis_pointer_list_get(_OBJ ,_IDX)))


#endif //_SIS_LIST_H