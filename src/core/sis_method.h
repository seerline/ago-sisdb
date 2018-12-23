
#ifndef _SIS_METHOD_H
#define _SIS_METHOD_H

#include <sis_core.h>
#include <sis_json.h>
#include "sis_map.h"
#include "sis_list.h"

// typedef int _sis_method_define(void *, s_sis_json_node *);
// typedef int _sis_method_logic_define(void *, void *);

typedef struct s_sis_method {
    const char *name;   // 方法的名字
    const char *style;  // 方法属于的类别，相当于命名空间 subscribe append zip 等
	void *(*proc)(void *, s_sis_json_node *);
    // _sis_method_define *proc;
	const char *explain;  // 方法的说明
}s_sis_method;

// 参数默认为一串字段
typedef struct s_sis_method_node {
    s_sis_method    *method;
	s_sis_json_node *argv;
	void            *out;   // 出口数据区指针，仅仅在SIS_METHOD_CLASS_FILTER才临时生成，并在结果输出后销毁
	// int              option; // 和上次结果比较是 0 与还是 1或
    struct s_sis_method_node *next, *prev;    // 或的关系
    struct s_sis_method_node *child, *father; // 与的关系
}s_sis_method_node;

#define SIS_METHOD_CLASS_MARKING  0 // 对数据源打标记，选股用, 对传入数据运算用
#define SIS_METHOD_CLASS_FILTER   1 // 输出和in数据同构的结果数据集，数据库查询时用
// 传入方法的结构例子
// in和out必须是同构的数据，out应该是in的同集或子集
// 分以下几种情况
// 1.仅仅做标记，out和in一样，merge和across设置为空就可以了，方法中不做数据减少动作
// 2.取in的子集，

typedef struct s_sis_method_class {
	int      	style;  // 类型
	void       *obj;     // 入口数据区指针，
	s_sis_method_node        *node;   // 方法链表，方法传递value的数据
	void(*merge)(void *, void *);      // 和同一等级的方法求并集
	void(*across)(void *, void *);     // 和上一等级的方法求交集
	void(*free)(void *);          // free的方法,释放in和out
	void*(*malloc)();       // malloc的方法,用于申请out和下一集in时使用
	// _sis_method_logic_define *merge;  // 和同一等级的方法求并集
	// _sis_method_logic_define *across; // 和上一等级的方法求交集
}s_sis_method_class;
//////////////////////////////////////////////
//   method_map 这里定义的是方法列表
///////////////////////////////////////////////
s_sis_map_pointer *sis_method_map_create(s_sis_method *methods_, int count_);
s_sis_method *sis_method_map_find(s_sis_map_pointer *map_, const char *name_, const char *style_);
void sis_method_map_destroy(s_sis_map_pointer *map_);

//////////////////////////////////////////////
//   method_node 这里定义的是实际方法的参数和链表
///////////////////////////////////////////////

// s_sis_method_node* _sis_method_node_load(
// 		s_sis_map_pointer *map_,
// 		s_sis_method_node *first_,
// 		s_sis_method_node *father_,
// 		const char *style_, 
// 		s_sis_json_node *node_);

// // 起点，最外层为一个或者关系的方法列表，
// s_sis_method_node *sis_method_node_create(
// 	s_sis_map_pointer *map_,
// 	const char *style_, 
// 	s_sis_json_node *node_);

// void sis_method_node_destroy(void *node_, void *other_);
// 
s_sis_method_class *sis_method_class_create(
	s_sis_map_pointer *map_,
	const char *style_, 
	s_sis_json_node *node_);

void *sis_method_class_execute(s_sis_method_class *class_);

// 0 -  全部清理  1 - 只清理输出的数据
void sis_method_class_clear(s_sis_method_class *,int);

void sis_method_class_destroy(void *node_, void *other_);

#endif
