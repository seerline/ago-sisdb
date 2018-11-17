
#ifndef _SIS_MEMORY_H
#define _SIS_MEMORY_H

#include <sis_core.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <os_file.h>

#define SIS_DB_MEMORY_SIZE  128*1024  // 数据区大小，

typedef struct s_sis_memory {
    // bool    eof;     // 是否读到文件尾
    size_t  size;
	size_t  maxsize;
	size_t  offset;  //偏移距离
    char   *buffer;  // 实际缓存区
} s_sis_memory;

s_sis_memory *sis_memory_create();   
void sis_memory_destroy(s_sis_memory *);

void sis_memory_pack(s_sis_memory *m_);
void sis_memory_clear(s_sis_memory *m_);

size_t sis_memory_cat(s_sis_memory *, char *, size_t);  // 尾部添加，重新清理内存
size_t sis_memory_readfile(s_sis_memory *, sis_file_handle, size_t);
size_t sis_memory_get_size(s_sis_memory *);

size_t sis_memory_get_line_sign(s_sis_memory *);
void sis_memory_move(s_sis_memory *, size_t);
char *sis_memory(s_sis_memory *);

#endif //_SIS_FILE_H
