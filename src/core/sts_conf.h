
#ifndef _STS_CONF_H
#define _STS_CONF_H

// 包含了这个头，就基本包括了所有的os目录的文件头了
#include <sts_core.h>
#include <sts_file.h>
#include <sts_json.h>

// 要求可以从json字符串转化为conf格式，方便js客户端配置好参数后直接发回给服务器进行处理
// 要求支持 # 注释
// 要求支持 include xxxx.conf 
// 只需要支持解析和读取，输出，不要求修改

#define STS_CONF_NOTE_SIGN '#'  
#define STS_CONF_INCLUDE "include"  

typedef struct s_sts_conf_handle
{
	const char * error;     // 指向错误的地址 错误地址为空表示正常处理完毕
	char   path[STS_PATH_LEN];  //保存主conf的路径
	struct s_sts_json_node *node;
} s_sts_conf_handle;         //专门提供给读json的快速结构体

//-------- output option function-------- //

s_sts_conf_handle *sts_conf_open(const char *fn_); // 从文件打开 不读取注释
void sts_conf_close(s_sts_conf_handle *handle_); // 关闭并释放

s_sts_conf_handle *sts_conf_load(const char *content_, size_t len_);
// 输出json的格式数据，不含conf的注释
#define sts_conf_to_json sts_json_output
#define sts_conf_to_json_zip sts_json_output_zip

#define sts_conf_get_int sts_json_get_int
#define sts_conf_get_double sts_json_get_double
#define sts_conf_get_str sts_json_get_str
#define sts_conf_get_bool sts_json_get_bool

#define sts_conf_get_size sts_json_get_size
#define sts_conf_first_node sts_json_first_node
#define sts_conf_next_node sts_json_next_node
#define sts_conf_last_node sts_json_last_node

#define sts_conf_cmp_child_node sts_json_cmp_child_node
#define sts_conf_find_node  sts_json_find_node


#endif
