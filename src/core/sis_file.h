
#ifndef _SIS_FILE_H
#define _SIS_FILE_H

#include <sis_core.h>
#include <sis_malloc.h>
#include <sis_str.h>

#define SIS_FILE_BUFFER_LEN  1024*1024

#define WRITE_FILE_INT(f__,l__)  { int len__=(int)l__ ;sis_file_write(f__, &len__, sizeof(int));}
#define WRITE_FILE_STRING(f__,s__,l__)  { int len__=(int)l__ ;sis_file_write(f__, &len__, sizeof(int)); \
                                        sis_file_write(f__, s__, len__); }

typedef struct s_sis_file {
	s_sis_file_handle fp;
    size_t    maxsize;
    s_sis_sds buffer;
	// s_sis_struct_list *list;

	s_sis_file_handle (*open)(const char *, int mode_, int access_);  // 返回值需要赋值给fp
	void (*close)(s_sis_file_handle fp_);  // 这里关闭的是 fp

	size_t (*read)(s_sis_file_handle, char *in_, size_t ilen_);  // 读出的内容放到in中 ，返回实际读取的字节
	size_t (*write)(s_sis_file_handle, char *in_, size_t ilen_); //

	s_sis_sds (*get)(s_sis_file_handle, char *key_);  
	// 按key_标记读取内容，仅用于kv结构
	// key==NULL 表示直接读文件
	size_t (*set)(s_sis_file_handle, char *key_, char *in_, size_t ilen_); 
	// 按key_标记读取内容，仅用于kv结构
	// key==NULL 表示直接读文件

} s_sis_file;

s_sis_file *sis_file_create();
void sis_file_destroy(s_sis_file *);

s_sis_sds sis_file_read_to_sds(const char *fn_);

bool sis_file_sds_write(const char *fn_, s_sis_sds buffer_);

void sis_cat_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_);

void sis_check_path(const char *fn_);

#endif //_SIS_FILE_H
