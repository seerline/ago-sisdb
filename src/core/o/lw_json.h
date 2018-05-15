
#ifndef _LW_JSON_H
#define _LW_JSON_H

#include "lw_base.h"

#define DP_JSON_NULL   0
//#define DP_JSON_BOOL   1
#define DP_JSON_INT    2
#define DP_JSON_DOUBLE 3
#define DP_JSON_STRING 4
#define DP_JSON_ARRAY  5
#define DP_JSON_OBJECT 6

typedef struct s_json_node
{
	struct s_json_node *next, *prev; /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct s_json_node *child;		 /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
	int type;						 /* The type of the item, as above. */
	char *key_w;					 /* The item's name,  只读状态时，此处为0，当设置了内容时，同时设置key_r*/
	char *value_w;					 /* The item's value, 只读状态时，此处为0，当设置了内容时，同时设置value_r*/
	char *key_r;					 /* The item's name,  获取数据时，只读这里，默认加载时，这里填充content的map映射地址，当有修改发生时，这里和key_w地址一致*/
	char *value_r;					 /* The item's value, */
} s_json_node;

typedef struct dp_json_read_handle
{
	char *content; //文件内容，用于映射
	int position;
	struct s_json_node *node;
} dp_json_read_handle; //专门提供给读json的快速结构体

//-------- output option function-------- //
#ifdef __cplusplus
extern "C" {
#endif

dp_json_read_handle *dp_json_open(const char *filename_);
void dp_json_close(dp_json_read_handle *handle_);
void dp_json_free(s_json_node *c);

dp_json_read_handle *dp_json_load(const char *content_);  //按内容加载
void dp_json_save(s_json_node *node_, const char *outfilename_); //把json存到文件中

char *dp_json_print(s_json_node *item);
char *dp_json_print_unformatted(s_json_node *item);
/* clone a s_json_node item */
s_json_node *dp_json_clone(s_json_node *src_, int recurse);

//-------------------------------------------------------------//

int dp_json_get_int(s_json_node *root, const char *key_, int defaultvalue);
double dp_json_get_double(s_json_node *root, const char *key_, double defaultvalue);
const char *dp_json_get_str(s_json_node *root, const char *key_);
bool dp_json_get_bool(s_json_node *root, const char *key_, bool defaultvalue);

int dp_json_get_array_int(s_json_node *root, int index_, int defaultvalue);
double dp_json_get_array_double(s_json_node *root, int index_, double defaultvalue);
const char *dp_json_get_array_str(s_json_node *root, int index_);
bool dp_json_get_array_bool(s_json_node *root, int index_, bool defaultvalue);

int dp_json_array_get_size(s_json_node *array_);
s_json_node *dp_json_array_get_item(s_json_node *array_, int index_);
s_json_node *dp_json_object_get_item(s_json_node *object_, const char *key_);
s_json_node *dp_json_next_item(s_json_node *now_);
//根据路径读取数据，
s_json_node *dp_json_find_item(s_json_node *node, const char *path_);

//======== write option =============//
s_json_node *dp_json_create_array();  //创建一个数组
s_json_node *dp_json_create_object(); //创建一个对象
void dp_json_free(s_json_node *c);

//if key_==NULL,表示新建一个json "{}"
void dp_json_object_add_item(s_json_node *object, const char *key_, s_json_node *item);
//if key==NULL,表示新建一个array "[]"
void dp_json_array_add_item(s_json_node *array_, s_json_node *item);

void dp_json_array_add_int(s_json_node *node_, long value_);
void dp_json_array_add_double(s_json_node *node_, double value_);
void dp_json_array_add_string(s_json_node *node_, const char *value_);

void dp_json_set_int(s_json_node *node_, const char *key_, long value_);
void dp_json_set_double(s_json_node *node_, const char *key_, double value_, int demical_);
void dp_json_set_string(s_json_node *node_, const char *key_, const char *value_);

//如果key_为空，就直接修改node的值
void dp_json_array_delete_item(s_json_node *array_, int which);
s_json_node *dp_json_object_detach_item(s_json_node *object, const char *key_);
void dp_json_object_delete_item(s_json_node *object, const char *key_);
void dp_json_array_set_item(s_json_node *array_, int index_, s_json_node *newitem);
void dp_json_object_set_item(s_json_node *src_, const char *key_, s_json_node *newitem);

#ifdef __cplusplus
}
#endif

#endif
