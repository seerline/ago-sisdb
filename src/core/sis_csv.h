
#ifndef _SIS_FILE_CSV_H
#define _SIS_FILE_CSV_H

#include <sis_core.h>
#include <sis_file.h>
#include <sis_list.h>
#include <sis_method.h>
#include <sis_map.h>
// 记录信息
typedef struct s_sis_file_csv_unit 
{
	int   index;
	int   cols;   // 第一条记录一般作为标题
	const char **argv;
	int   *argsize;
}s_sis_file_csv_unit;

typedef struct s_sis_file_csv 
{
	s_sis_file_handle   fp;
	char 			    sign[8];
	s_sis_map_int      *head;      // 头信息 对应 cols
	s_sis_pointer_list *list;      // s_sis_file_csv_unit 
}s_sis_file_csv;
#ifdef __cplusplus
extern "C" {
#endif
s_sis_file_csv * sis_file_csv_open(const char *name_, char c_, int mode_, int access_);
void sis_file_csv_close(s_sis_file_csv *csv_);

int sis_file_csv_getsize(s_sis_file_csv *csv_);

// 所有的索引都从0开始 -1 表示获取的是标题 只有 str
int64 sis_file_csv_fget_int(s_sis_file_csv *csv_, int idx_, int field, int64 defaultvalue_);
double sis_file_csv_fget_double(s_sis_file_csv *csv_, int idx_, int field, double defaultvalue_);
const char *sis_file_csv_fget_str(s_sis_file_csv *csv_, int idx_, int field);

int64 sis_file_csv_get_int(s_sis_file_csv *csv_, int idx_, const char *field, int64 defaultvalue_);
double sis_file_csv_get_double(s_sis_file_csv *csv_, int idx_, const char *field, double defaultvalue_);
const char *sis_file_csv_get_str(s_sis_file_csv *csv_, int idx_, const char *field);

const char *sis_file_csv_get_head(s_sis_file_csv *csv_, int hidx_);

// 独立的csv写入函数
s_sis_file_handle sis_csv_write_open(const char *name_, int isnew_);
size_t sis_csv_write(s_sis_file_handle, s_sis_sds);
void sis_csv_write_close(s_sis_file_handle);

s_sis_sds sis_csv_make_str(s_sis_sds in_, const char *str_, size_t len_);
s_sis_sds sis_csv_make_int(s_sis_sds in_, int64 val_);
s_sis_sds sis_csv_make_uint(s_sis_sds in_, uint64 val_);
s_sis_sds sis_csv_make_double(s_sis_sds in_, double val_, int dot_);
s_sis_sds sis_csv_make_end(s_sis_sds in_);

#ifdef __cplusplus
}
#endif
#define  SIS_CSV_GET_INT(a,z)   ({   \
    char s[64]; memmove(s, a->argv[z], a->argsize[z]); s[a->argsize[z]] = 0; \
    sis_trim(s); sis_atoll(s); \
})

#define  SIS_CSV_GET_FLOAT(a,z)  ({   \
    char s[64]; memmove(s, a->argv[z], a->argsize[z]); s[a->argsize[z]] = 0; \
    sis_trim(s); atof(s); \
})

#define  SIS_CSV_GET_STRING(ia,ii,oa,oz)  {   \
	int z = ia->argsize[ii] > (oz - 1) ? oz - 1 : ia->argsize[ii]; \
    memmove(oa, ia->argv[ii], z); oa[z] = 0;  \
}
// 对于大文件csv 采用回调方式一条一条信息返回 进行读取 
// s_sis_file_csv_unit 返回参数结构体
int sis_file_csv_read_sub(const char *name_, char c_, void * cb_source, sis_method_define *cb_);

#endif

