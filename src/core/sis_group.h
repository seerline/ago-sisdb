#ifndef _sis_group_h
#define _sis_group_h

// 多分组
#include "sis_list.h"
#include "sis_math.h"

#pragma pack(push,1)

///////////////////////
// s_sis_groups 定义
///////////////////////

// 内部都为升序 
typedef struct s_sis_groups_kv {
	int     gindex;      // 哪个分组 -1 未进入分组
	double  groupv;      // 用于分组的值
	double  placev;      // 用于排序的值
	void   *value;       // 唯一对象 在整个分组中唯一
} s_sis_groups_kv;

typedef struct s_sis_groups_info {
	double  minv;        // 最小值 >=
	double  maxv;        // 最大值 < 
} s_sis_groups_info;

typedef struct s_sis_groups_iter {
	int8              inited;
	int8              skip0;    // 是否跳过 0 组
	int8              sortly;   // 遍历的排序方式 0 默认降序 1 升序
	int               gindex;   // 当前分组号
	int               pindex;   // 当前分组记录号
} s_sis_groups_iter;

typedef struct s_sis_groups {
	int                  nums;          // 期望分组数量
	int8                 style;         // 分组方式 0 按正负数量分别均分 1 按 正负数量分别 黄金分割
	s_sis_pointer_list  *samples;       // 实际数据 s_sis_groups_kv

	s_sis_groups_iter   *iter;          // 当前遍历的对象  

	int8                 sorted;        // 是否已经排序
	int                  count;         // 实际分组数量 默认最少 + 0组 有正负最少分组 3 单正负最少 2
	int8                 zero_alone;    // 0 默认0混和 1 0值单独一组 
	int                  zero_index;    // 0 排序后 0 组位于什么位置
	s_sis_groups_info   *sorts;         // 每个分组排序的信息 0 是zero序列的排序方式 
	void  (*vfree)(void *);             // 释放 value 的函数
	s_sis_pointer_list **vindexs;       // 仅保存数据指针 zero_index为 0值的分组 从 1 开始为实际分组 统一由小到大
} s_sis_groups;

///////////////////////
// s_sis_fgroup 定义
///////////////////////
typedef struct s_sis_fgroup {
	int		     maxcount; // 总数
	int		     count;    // 当前个数
	double      *key;      // 索引
	void        *val;      // 数据指针
	void (*vfree)(void *); // == NULL 不释放对应内存
} s_sis_fgroup;


#pragma pack(pop)

s_sis_groups *sis_groups_create(int nums, int style, void *vfree_); 
void sis_groups_destroy(void *);
void sis_groups_clear(s_sis_groups *);

double sis_groups_get_groupv(s_sis_groups *groups, int index);
double sis_groups_get_placev(s_sis_groups *groups, int index);

// 对数据进行分组重排 不存在同时正负的分组 简化问题
void sis_groups_resort(s_sis_groups *);

// 从大到小放入数据 执行 sis_groups_resort 后生效
int  sis_groups_set(s_sis_groups *, double groupv_, double placev_, void *value_);
//
int  sis_groups_write(s_sis_groups *, s_sis_groups_kv *);
// groupv改变后修改位置
void sis_groups_change(s_sis_groups *groups, s_sis_groups_kv *gkv);
// 找对应数据指针
int sis_groups_find(s_sis_groups *, void *value_);
// 删除数据
void sis_groups_delete_v(s_sis_groups *, void *value_);
// 从分组中移除数据 但并不删除
void sis_groups_remove_v(s_sis_groups *, void *value_);

// gindex == -1 从 samples 获取数据
s_sis_groups_kv *sis_groups_get_kv(s_sis_groups *, int index_);

void *sis_groups_get(s_sis_groups *, int index_);

// gindex == -1 从 samples 获取数据
int sis_groups_getsize(s_sis_groups *);
///////
// 获取指定队列的值
s_sis_groups_kv *sis_group_get_kv(s_sis_groups *, int gindex_, int index_);
// 指定队列的个数
int sis_group_getsize(s_sis_groups *, int gindex_);
//////////////////////////////
// 打开一个遍历对象 0 表示降序  1表示升序
s_sis_groups_iter *sis_groups_iter_start(s_sis_groups *, int sortly, int skip0);

// 获取下一条数据 NULL 表示队列结束
s_sis_groups_iter *sis_groups_iter_next(s_sis_groups *);

void *sis_groups_iter_get_v(s_sis_groups *);

s_sis_groups_kv *sis_groups_iter_get_kv(s_sis_groups *);

void sis_groups_iter_stop(s_sis_groups *);


///////////////////////
// s_sis_fgroup 定义
///////////////////////

s_sis_fgroup *sis_fgroup_create(void *vfree_); 
void sis_fgroup_destroy(void *);
void sis_fgroup_clear(s_sis_fgroup *list_);
// 默认从大到小排序
int   sis_fgroup_set(s_sis_fgroup *, double key_, void *in_);
void *sis_fgroup_get(s_sis_fgroup *, int index_);

double sis_fgroup_getkey(s_sis_fgroup *, int index_);

// 找对应数据指针
int sis_fgroup_find(s_sis_fgroup *, void *value_);

void sis_fgroup_reset(s_sis_fgroup *list_, double key_, int index_);

void sis_fgroup_del(s_sis_fgroup *list_, int index_);
int sis_fgroup_getsize(s_sis_fgroup *list_);


#endif