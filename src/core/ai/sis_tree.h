
#ifndef _SIS_TREE_H
#define _SIS_TREE_H

// #include <sis_core.h>

// #define SIS_TREE_NULL    0
// #define SIS_TREE_ROOT    1 // 根节点
// #define SIS_TREE_BOLE    2 // 树干节点
// #define SIS_TREE_LEAF    3 // 叶子节点

// // 多叉树 
// // 样例 x:1 y:2 z:3
// // 从根结点x开始,找到其下级节点 inv = 1 的节点 y
// // 

// typedef struct s_sis_tree_node
// {
// 	struct s_sis_tree_node *next, *prev; 
// 	struct s_sis_tree_node *child, *father;

// 	uint8  type;
// 	int    in;          // 父亲节点的分支标记符 
// 	int    out;	        // 如果是叶子节点表示结果 -1 0 1
// 	void  *info;        // 节点数据指针
// } s_sis_tree_node;

// #ifdef __cplusplus
// extern "C" {
// #endif

// //////////////////////////////////////////////
// //   output main function define
// ///////////////////////////////////////////////
// s_sis_tree_handle *sis_tree_open(const char *fn_);
// void sis_tree_delete_node(s_sis_tree_node *node_);
// void sis_tree_close(s_sis_tree_handle *handle_);

// s_sis_tree_handle *sis_tree_load(const char *content_, size_t len_);  //按内容加载
// void sis_tree_save(s_sis_tree_node *node_, const char *fn_); //把json存到文件中

// s_sis_tree_node *sis_tree_clone(s_sis_tree_node *src_, int child_); // child_==0 表示只当前节点

// //////////////////////////////////////////////
// //======== write option =============//
// //   write function define
// ///////////////////////////////////////////////
// s_sis_tree_node *sis_tree_create_array();  //创建一个数组
// s_sis_tree_node *sis_tree_create_object(); //创建一个对象

// //*********************************//
// //  以下函数都会改变只读属性
// //*********************************//

// // 追加到child的末尾，key不能有'.'
// void sis_tree_array_add_node(s_sis_tree_node *source_, s_sis_tree_node *node_);
// void sis_tree_object_add_node(s_sis_tree_node *source_, const char *key_, s_sis_tree_node *node_);

// // 追加到child的末尾，key不能有'.'
// // 增加到child，在末尾增加一个元素
// // 修改到child，如果没有发现key_,就增加一个
// void sis_tree_object_add_int(s_sis_tree_node *node_, const char *key_, long value_);
// void sis_tree_object_set_int(s_sis_tree_node *node_, const char *key_, long value_);

// void sis_tree_object_add_uint(s_sis_tree_node *node_, const char *key_, unsigned long value_);
// void sis_tree_object_set_uint(s_sis_tree_node *node_, const char *key_, unsigned long value_);

// void sis_tree_object_add_double(s_sis_tree_node *node_, const char *key_, double value_, int dot_);
// void sis_tree_object_set_double(s_sis_tree_node *node_, const char *key_, double value_, int dot_);

// void sis_tree_object_add_string(s_sis_tree_node *node_, const char *key_, const char *value_, size_t len_);
// void sis_tree_object_set_string(s_sis_tree_node *node_, const char *key_, const char *value_, size_t len_);

// //----------------------------------------------//
// // 追加到child的末尾，key不能有'.'
// // 增加到child，在末尾增加一个元素
// // 修改到child，如果没有发现key_,就增加一个 
// void sis_tree_array_add_int(s_sis_tree_node *node_, long value_);
// void sis_tree_array_set_int(s_sis_tree_node *node_, int index_, long value_);

// void sis_tree_array_add_uint(s_sis_tree_node *node_, unsigned long value_);
// void sis_tree_array_set_uint(s_sis_tree_node *node_, int index_, unsigned long value_);

// void sis_tree_array_add_double(s_sis_tree_node *node_, double value_, int dot_);
// void sis_tree_array_set_double(s_sis_tree_node *node_, int index_,  double value_, int dot_);

// void sis_tree_array_add_string(s_sis_tree_node *node_, const char *value_, size_t len_);
// void sis_tree_array_set_string(s_sis_tree_node *node_, int index_,  const char *value_, size_t len_);

// //-------------------------------------------------------------//
// //======== read & print option =============//
// //  提供给其他以s_sis_sis__node为基础的文件read和输出的操作
// //-------------------------------------------------------------//
// s_sis_tree_node *sis_tree_create_node(void);

// void sis_tree_printf(s_sis_tree_node *node_, int *i);
// //======== read option =============//

// int64 sis_tree_get_int(s_sis_tree_node *root_, const char *key_, int64 defaultvalue_);
// double sis_tree_get_double(s_sis_tree_node *root_, const char *key_, double defaultvalue_);
// const char *sis_tree_get_str(s_sis_tree_node *root_, const char *key_);
// bool sis_tree_get_bool(s_sis_tree_node *root_, const char *key_, bool defaultvalue_);

// int64 sis_array_get_int(s_sis_tree_node *root_, int index_, int64 defaultvalue_);
// double sis_array_get_double(s_sis_tree_node *root_, int index_, double defaultvalue_);
// const char *sis_array_get_str(s_sis_tree_node *root_, int index_);
// bool sis_tree_array_bool(s_sis_tree_node *root_, int index_, bool defaultvalue_);

// int sis_tree_get_size(s_sis_tree_node *node_);
// s_sis_tree_node *sis_tree_first_node(s_sis_tree_node *node_);
// s_sis_tree_node *sis_tree_next_node(s_sis_tree_node *node_);
// s_sis_tree_node *sis_tree_last_node(s_sis_tree_node *node_);
// //根据路径读取数据，路径为 aaa.bbb.ccc
// s_sis_tree_node *sis_tree_cmp_child_node(s_sis_tree_node *object_, const char *key_);
// s_sis_tree_node *sis_tree_find_node(s_sis_tree_node *node_, const char *path_);

// void sis_tree_show(s_sis_tree_node *node_, int *i);

// #ifdef __cplusplus
// }
// #endif
#endif
