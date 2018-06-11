
#ifndef _STS_MEMORY_H
#define _STS_MEMORY_H

#include <sts_core.h>
#include <sts_malloc.h>
#include <sts_str.h>
#include <os_file.h>

#define STS_DB_MEMORY_SIZE  128*1024  // 数据区大小，

typedef struct s_sts_memory {
    size_t  size;
	size_t  maxsize;
	size_t  offset;  //偏移距离
    char *buffer;  // 实际缓存区
} s_sts_memory;

s_sts_memory *sts_memory_create();   
void sts_memory_destroy(s_sts_memory *);

void sts_memory_pack(s_sts_memory *m_);

size_t sts_memory_cat(s_sts_memory *, char *, size_t);  // 尾部添加，重新清理内存
size_t sts_memory_readfile(s_sts_memory *, sts_file_handle, size_t);
size_t sts_memory_get_size(s_sts_memory *);
void sts_memory_move(s_sts_memory *, size_t);
char *sts_memory(s_sts_memory *);

#endif //_STS_FILE_H
