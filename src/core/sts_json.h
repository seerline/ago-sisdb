
#ifndef _STS_JSON_H
#define _STS_JSON_H

#include <sts_core.h>
#include <stdarg.h>

#define STS_JSON_NULL    0
#define STS_JSON_ROOT    1 // 根节点
#define STS_JSON_INT     2
#define STS_JSON_DOUBLE  3
#define STS_JSON_STRING  4
#define STS_JSON_ARRAY   5
#define STS_JSON_OBJECT  6

typedef struct s_sts_json_node
{
	struct s_sts_json_node *next, *prev; 
	struct s_sts_json_node *child;

	uint8  type;    // 有include时立即加载文件，并把内容插入content中继续解析
	char  *key;			
	char  *value;
} s_sts_json_node;


typedef struct s_sts_json_handle
{
	const char * error;      // 指向错误的地址
	bool   readonly;   // 读写模式，只读就从content
	char  *content;    // 文件内容，只读模式下使用的映射
	size_t position;   // 当前位置，临时变量，只读解析时用 
	struct s_sts_json_node *node;
} s_sts_json_handle; //专门提供给读json的快速结构体

//////////////////////////////////////////////
//   output main function define
///////////////////////////////////////////////
s_sts_json_handle *sts_json_open(const char *fn_);
void sts_json_delete_node(s_sts_json_handle *handle_, s_sts_json_node *node_);
void sts_json_close(s_sts_json_handle *handle_);

s_sts_json_handle *sts_json_load(const char *content_, size_t len_);  //按内容加载
void sts_json_save(s_sts_json_node *node_, const char *fn_); //把json存到文件中

s_sts_json_node *sts_json_clone(s_sts_json_node *src_, int child_); // child_==0 表示只当前节点

//////////////////////////////////////////////
//======== write option =============//
//   write function define
///////////////////////////////////////////////
s_sts_json_node *sts_json_create_array();  //创建一个数组
s_sts_json_node *sts_json_create_object(); //创建一个对象

//*********************************//
//  以下函数都会改变只读属性
//*********************************//

// 追加到child的末尾，key不能有'.'
void sts_json_add_node(s_sts_json_handle *h_, s_sts_json_node *source_, s_sts_json_node *node_);

// 追加到child的末尾，key不能有'.'
#define sts_json_object_add_node sts_json_add_node
// 增加到child，在末尾增加一个元素
// 修改到child，如果没有发现key_,就增加一个
void sts_json_object_add_int(s_sts_json_handle *h_, s_sts_json_node *node_, const char *key_, long value_);
void sts_json_object_set_int(s_sts_json_handle *h_, s_sts_json_node *node_, const char *key_, long value_);

void sts_json_object_add_double(s_sts_json_handle *h_, s_sts_json_node *node_, const char *key_, double value_, int demical_);
void sts_json_object_set_double(s_sts_json_handle *h_, s_sts_json_node *node_, const char *key_, double value_, int demical_);

void sts_json_object_add_string(s_sts_json_handle *h_, s_sts_json_node *node_, const char *key_, const char *value_);
void sts_json_object_set_string(s_sts_json_handle *h_, s_sts_json_node *node_, const char *key_, const char *value_);

//----------------------------------------------//
// 追加到child的末尾，key不能有'.'
#define sts_json_array_add_node sts_json_add_node
// 增加到child，在末尾增加一个元素
// 修改到child，如果没有发现key_,就增加一个 
void sts_json_array_add_int(s_sts_json_handle *h_, s_sts_json_node *node_, long value_);
void sts_json_array_set_int(s_sts_json_handle *h_, s_sts_json_node *node_, int index_, long value_);

void sts_json_array_add_double(s_sts_json_handle *h_, s_sts_json_node *node_, double value_, int demical_);
void sts_json_array_set_double(s_sts_json_handle *h_, s_sts_json_node *node_, int index_,  double value_, int demical_);

void sts_json_array_add_string(s_sts_json_handle *h_, s_sts_json_node *node_, const char *value_);
void sts_json_array_set_string(s_sts_json_handle *h_, s_sts_json_node *node_, int index_,  const char *value_);

//-------------------------------------------------------------//
//======== read & print option =============//
//  提供给其他以s_sts_json_node为基础的文件read和输出的操作
//-------------------------------------------------------------//
s_sts_json_node *sts_json_create_node(void);

char *sts_json_output(s_sts_json_node *node_, size_t *len_);
char *sts_json_output_zip(s_sts_json_node *node_, size_t *len_);

//======== read option =============//

int sts_json_get_int(s_sts_json_node *root_, const char *key_, int defaultvalue_);
double sts_json_get_double(s_sts_json_node *root_, const char *key_, double defaultvalue_);
const char *sts_json_get_str(s_sts_json_node *root_, const char *key_);
bool sts_json_get_bool(s_sts_json_node *root_, const char *key_, bool defaultvalue_);

int sts_json_get_size(s_sts_json_node *node_);
s_sts_json_node *sts_json_first_node(s_sts_json_node *node_);
s_sts_json_node *sts_json_next_node(s_sts_json_node *node_);
s_sts_json_node *sts_json_last_node(s_sts_json_node *node_);
//根据路径读取数据，路径为 aaa.bbb.ccc
s_sts_json_node *sts_json_cmp_child_node(s_sts_json_node *object_, const char *key_);
s_sts_json_node *sts_json_find_node(s_sts_json_node *node_, const char *path_);


#endif
