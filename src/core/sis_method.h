
#ifndef _SIS_METHOD_H
#define _SIS_METHOD_H

#include <sis_core.h>
#include <sis_json.h>
#include "sis_map.h"
#include "sis_list.h"

typedef int (sis_method_define)(void *, void *);

#define SIS_METHOD_ARGV        "argv"
#define SIS_METHOD_VOID_TRUE   ((void *)1)
#define SIS_METHOD_VOID_FALSE  ((void *)0)

#define SIS_METHOD_OK      1 
#define SIS_METHOD_TRANS   2 
#define SIS_METHOD_ERROR   0

#define SIS_METHOD_NIL     -3
#define SIS_METHOD_NOCMD   -1
#define SIS_METHOD_NOWORK  -2
#define SIS_METHOD_NULL    -3
#define SIS_METHOD_REPEAT  -5
#define SIS_METHOD_NOANS   -6  // 对端1秒未响应

#define SIS_INT_TO_VOID(n)  ((void *)(uint64)n)
#define SIS_VOID_TO_INT(v)  ((int)(uint64)v)

#define SIS_METHOD_ACCESS_NONE      0  //      不限制
#define SIS_METHOD_ACCESS_READ      1  // 1    可读
#define SIS_METHOD_ACCESS_WRITE     2  // 1    可写
#define SIS_METHOD_ACCESS_DEL       4  // 1    可删
#define SIS_METHOD_ACCESS_NONET   0x100  // 禁止网络数据包 仅限于进程间调用
#define SIS_METHOD_ACCESS_NOLOG   0x200  // 禁止写LOG 通常用于屏蔽和LOG冲突的操作

#define SIS_METHOD_ACCESS_RDWR    3  // 11   可写
#define SIS_METHOD_ACCESS_ADMIN   7  // 111  可删除

typedef struct s_sis_method {
    const char *name;     // 方法的名字
	int (*proc)(void *, void *);
    int access;   // 方法属于的权限，相当于命名空间 read write admin 等
	const char *explain;  // 方法的说明
}s_sis_method;

// 方法的映射表, 通过映射表快速调用对应函数 s_sis_method
#define s_sis_methods s_sis_map_pointer

#ifdef __cplusplus
extern "C"
{
#endif
    s_sis_methods *sis_methods_create(s_sis_method *methods_, int count_);
    void sis_methods_destroy(s_sis_methods *);
	// 注入新的办法
    // int sis_methods_register(s_sis_methods *map_, s_sis_method *methods_, const char *style_);

    // s_sis_method *sis_methods_get(s_sis_methods *map_, const char *name_, const char *style_);

    int sis_methods_register(s_sis_methods *map_, s_sis_method *methods_);

    s_sis_method *sis_methods_get(s_sis_methods *map_, const char *name_);

#ifdef __cplusplus
}
#endif

// 参数默认为一串字段
typedef struct s_sis_method_node {
    s_sis_method    *method;
	s_sis_json_node *argv;
	bool             ok;    // 为 SIS_METHOD_CLASS_JUDGE 类型准备
	void            *out;   // 出口数据区指针，仅仅在SIS_METHOD_CLASS_FILTER才临时生成，并在结果输出后销毁
	// int              option; // 和上次结果比较是 0 与还是 1或
    struct s_sis_method_node *next, *prev;    // 或的关系
    struct s_sis_method_node *child, *father; // 与的关系
}s_sis_method_node;

#define SIS_METHOD_CLASS_MARKING  0 // 对数据源打标记，选股用, 对传入数据运算用
#define SIS_METHOD_CLASS_JUDGE    1 // 求一个结果，判断条件是否成立
#define SIS_METHOD_CLASS_FILTER   2 // 输出和in数据同构的结果数据集，数据库查询时用
// 应该支持一种与或关系的选项

// 传入方法的结构例子
// in和out必须是同构的数据，out应该是in的同集或子集
// 分以下几种情况
// 1.仅仅做标记，out和in一样，merge和across设置为空就可以了，方法中不做数据减少动作
// 2.取in的子集，

typedef struct s_sis_method_class {
	int      	style;      // 类型
	// bool        ok;      // 为 SIS_METHOD_CLASS_JUDGE 类型准备
	void       *obj;        // 入口数据区指针，
	s_sis_method_node *node;   // 方法链表，方法传递value的数据
	void(*merge)(void *, void *);      // 和同一等级的方法求并集
	void(*across)(void *, void *);     // 和上一等级的方法求交集
	void(*free)(void *);          // free的方法,释放in和out
	void*(*malloc)();       // malloc的方法,用于申请out和下一集in时使用
}s_sis_method_class;
//////////////////////////////////////////////
//   method_map 这里定义的是方法列表
///////////////////////////////////////////////
s_sis_map_pointer *sis_method_map_create(s_sis_method *methods_, int count_);
s_sis_method *sis_method_map_find(s_sis_map_pointer *map_, const char *name_);
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
//////////////////////////////////////////////
//   method_class 管理多个方法的类，嵌套或并列
///////////////////////////////////////////////

s_sis_method_class *sis_method_class_create(
	s_sis_map_pointer *map_,
	s_sis_json_node *node_);

// 下面的算法可能有破坏内存的嫌疑
void *sis_method_class_execute(s_sis_method_class *class_);

// 顺序执行第一层所有函数，参数层层传递即可
void *sis_method_class_loop(s_sis_method_class *class_);

// 0 -  全部清理  1 - 只清理输出的数据 针对 SIS_METHOD_CLASS_FILTER 类型
void sis_method_class_clear(s_sis_method_class *,int);

void sis_method_class_destroy(s_sis_method_class *class_);

#endif
