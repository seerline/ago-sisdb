
#ifndef _SIS_CONF_H
#define _SIS_CONF_H

// 包含了这个头，就基本包括了所有的os目录的文件头了
#include <sis_core.h>
#include <sis_file.h>
#include <sis_json.h>
#include <sis_memory.h>
// 要求可以从json字符串转化为conf格式，方便js客户端配置好参数后直接发回给服务器进行处理
// 要求支持 # 注释
// 要求支持 include xxxx.conf 
// 只需要支持解析和读取，输出，不要求修改

#define SIS_CONF_NOTE_SIGN '#'  
#define SIS_CONF_INCLUDE "include"  

// conf配置文件结构体
typedef struct s_sis_conf_handle
{
	int  err_no;           // 0 无错 
	int  err_lines;        // 出错的行数
	char err_msg[255];     // 打印错误附近的信息
	char path[SIS_PATH_LEN];  //保存主conf的路径
	struct s_sis_json_node *node;
} s_sis_conf_handle;         //专门提供给读json的快速结构体

//-------- output option function-------- //
#ifdef __cplusplus
extern "C" {
#endif

s_sis_conf_handle *sis_conf_open(const char *fn_); // 从文件打开 不读取注释
void sis_conf_close(s_sis_conf_handle *handle_); // 关闭并释放

s_sis_conf_handle *sis_conf_load(const char *content_, size_t len_);

// 输出json的格式数据，不含conf的注释
#define sis_conf_to_json sis_json_output
#define sis_conf_to_json_zip sis_json_output_zip

#define sis_conf_get_int sis_json_get_int
#define sis_conf_get_double sis_json_get_double
#define sis_conf_get_str sis_json_get_str
#define sis_conf_get_bool sis_json_get_bool

#define sis_conf_get_size sis_json_get_size
#define sis_conf_first_node sis_json_first_node
#define sis_conf_next_node sis_json_next_node
#define sis_conf_last_node sis_json_last_node

#define sis_conf_cmp_child_node sis_json_cmp_child_node
#define sis_conf_find_node  sis_json_find_node

s_sis_sds sis_conf_file_to_json_sds(const char *fn_);


int sis_conf_sub(const char *fn_, void *source_, cb_sis_sub_json *cb_);

#ifdef __cplusplus
}
#endif
#endif
