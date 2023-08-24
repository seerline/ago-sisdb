
#ifndef _SIS_JSON_H
#define _SIS_JSON_H

#include <sis_core.h>
#include <sis_file.h>
#include <sis_str.h>
#include <sis_memory.h>

#define SIS_JSON_NULL    0
#define SIS_JSON_ROOT    1 // 根节点
#define SIS_JSON_INT     2
#define SIS_JSON_BOOL    3
#define SIS_JSON_DOUBLE  4
#define SIS_JSON_STRING  5
#define SIS_JSON_ARRAY   6
#define SIS_JSON_OBJECT  7

typedef struct s_sis_json_node
{
	struct s_sis_json_node *next, *prev; 
	struct s_sis_json_node *child, *father;

	uint8  type;
	char  *key;			
	char  *value;
} s_sis_json_node;


typedef struct s_sis_json_handle
{
	const char * error;      // 指向错误的地址
	char  *content;    // json内容
	struct s_sis_json_node *node;
} s_sis_json_handle; //专门提供给读json的快速结构体

typedef int (cb_sis_sub_json)(void *, s_sis_json_node *);

#define MAKER_SET_ARGV(_m_, _j_, _s_, _f_, _n_, _v_) { _s_ = _m_ ? _s_ : _v_; if (_j_) { _s_ = sis_json_cmp_child_node(_j_, _n_) ? _f_(_j_, _n_, _v_) : _s_; }}

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////
//   output main function define
///////////////////////////////////////////////
s_sis_json_handle *sis_json_open(const char *fn_);
void sis_json_delete_node(s_sis_json_node *node_);
void sis_json_close(s_sis_json_handle *handle_);

s_sis_json_handle *sis_json_load(const char *content_, size_t len_);  //按内容加载
void sis_json_save(s_sis_json_node *node_, const char *fn_); //把json存到文件中

s_sis_json_node *sis_json_clone(s_sis_json_node *src_, int child_); 
// child_==0 表示只当前节点

// 把子节点的数据添加到src的节点中去，
// 如果同名节点除了object外都直接覆盖 object 直接进入下级节点遍历 
void sis_json_object_merge(s_sis_json_node *src_, s_sis_json_node *son_);
//////////////////////////////////////////////
//======== write option =============//
//   write function define
///////////////////////////////////////////////
s_sis_json_node *sis_json_create_array();  //创建一个数组
s_sis_json_node *sis_json_create_object(); //创建一个对象

//*********************************//
//  以下函数都会改变只读属性
//*********************************//

// 追加到child的末尾，key不能有'.'
void sis_json_array_add_node(s_sis_json_node *source_, s_sis_json_node *node_);
void sis_json_object_add_node(s_sis_json_node *source_, const char *key_, s_sis_json_node *node_);

// 追加到child的末尾，key不能有'.'
// 增加到child，在末尾增加一个元素
// 修改到child，如果没有发现key_,就增加一个
void sis_json_object_add_int(s_sis_json_node *node_, const char *key_, int64 value_);
void sis_json_object_set_int(s_sis_json_node *node_, const char *key_, int64 value_);

void sis_json_object_add_bool(s_sis_json_node *node_, const char *key_, bool value_);
void sis_json_object_set_bool(s_sis_json_node *node_, const char *key_, bool value_);

void sis_json_object_add_uint(s_sis_json_node *node_, const char *key_, uint64 value_);
void sis_json_object_set_uint(s_sis_json_node *node_, const char *key_, uint64 value_);

void sis_json_object_add_double(s_sis_json_node *node_, const char *key_, double value_, int dot_);
void sis_json_object_set_double(s_sis_json_node *node_, const char *key_, double value_, int dot_);

void sis_json_object_add_string(s_sis_json_node *node_, const char *key_, const char *value_, size_t len_);
void sis_json_object_set_string(s_sis_json_node *node_, const char *key_, const char *value_, size_t len_);

// 初略检测是否json串
bool sis_json_object_valid(const char *value_, size_t len_);
void sis_json_object_add_jstr(s_sis_json_node *node_, const char *key_, const char *value_, size_t len_);
void sis_json_object_set_jstr(s_sis_json_node *node_, const char *key_, const char *value_, size_t len_);

//----------------------------------------------//
// 追加到child的末尾，key不能有'.'
// 增加到child，在末尾增加一个元素
// 修改到child，如果没有发现key_,就增加一个 
void sis_json_array_add_int(s_sis_json_node *node_, int64 value_);
void sis_json_array_set_int(s_sis_json_node *node_, int index_, int64 value_);

void sis_json_array_add_uint(s_sis_json_node *node_, uint64 value_);
void sis_json_array_set_uint(s_sis_json_node *node_, int index_, uint64 value_);

void sis_json_array_add_double(s_sis_json_node *node_, double value_, int dot_);
void sis_json_array_set_double(s_sis_json_node *node_, int index_,  double value_, int dot_);

void sis_json_array_add_string(s_sis_json_node *node_, const char *value_, size_t len_);
void sis_json_array_set_string(s_sis_json_node *node_, int index_,  const char *value_, size_t len_);

//-------------------------------------------------------------//
//======== read & print option =============//
//  提供给其他以s_sis_sis__node为基础的文件read和输出的操作
//-------------------------------------------------------------//
s_sis_json_node *sis_json_create_node(void);

char *sis_json_output(s_sis_json_node *node_, size_t *len_);
char *sis_json_output_zip(s_sis_json_node *node_, size_t *len_);

void sis_json_printf(s_sis_json_node *node_, int *i);
//======== read option =============//
bool sis_json_get_valid(s_sis_json_node *root_, const char *key_);

int64 sis_json_get_int(s_sis_json_node *root_, const char *key_, int64 defaultvalue_);
double sis_json_get_double(s_sis_json_node *root_, const char *key_, double defaultvalue_);
const char *sis_json_get_str(s_sis_json_node *root_, const char *key_);
bool sis_json_get_bool(s_sis_json_node *root_, const char *key_, bool defaultvalue_);

int64 sis_array_get_int(s_sis_json_node *root_, int index_, int64 defaultvalue_);
double sis_array_get_double(s_sis_json_node *root_, int index_, double defaultvalue_);
const char *sis_array_get_str(s_sis_json_node *root_, int index_);
bool sis_json_array_bool(s_sis_json_node *root_, int index_, bool defaultvalue_);

int sis_json_get_size(s_sis_json_node *node_);
s_sis_json_node *sis_json_first_node(s_sis_json_node *node_);
s_sis_json_node *sis_json_next_node(s_sis_json_node *node_);
s_sis_json_node *sis_json_last_node(s_sis_json_node *node_);
//根据路径读取数据，路径为 aaa.bbb.ccc
s_sis_json_node *sis_json_cmp_child_node(s_sis_json_node *object_, const char *key_);
s_sis_json_node *sis_json_find_node(s_sis_json_node *node_, const char *path_);

void sis_json_show(s_sis_json_node *node_, int *i);

#ifdef __cplusplus
}
#endif
#endif
