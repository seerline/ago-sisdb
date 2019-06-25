
#ifndef _SIS_MEMORY_H
#define _SIS_MEMORY_H

#include <sis_core.h>
#include <sis_malloc.h>
#include <sis_str.h>

#define SIS_DB_MEMORY_SIZE  32*1024  // read buffer size
// 传说中32K读取文件速度最快，千万不要改大了

typedef struct s_sis_memory {
    size_t  size;
	size_t  maxsize;
	size_t  offset;  
    char   *buffer; 
} s_sis_memory;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_memory *sis_memory_create();   
void sis_memory_destroy(s_sis_memory *);

void sis_memory_pack(s_sis_memory *m_);
void sis_memory_clear(s_sis_memory *m_);

size_t sis_memory_cat(s_sis_memory *, char *, size_t);  // memory tail cat, and repack buffer
size_t sis_memory_readfile(s_sis_memory *, sis_file_handle, size_t);
size_t sis_memory_get_size(s_sis_memory *);

void sis_memory_set_size(s_sis_memory *m_, size_t len_);
size_t sis_memory_get_maxsize(s_sis_memory *m_);
size_t sis_memory_set_maxsize(s_sis_memory *, size_t);

char *sis_memory_read_line(s_sis_memory *m_, size_t *len_);

size_t sis_memory_get_line_sign(s_sis_memory *);
void sis_memory_move(s_sis_memory *, size_t);
char *sis_memory(s_sis_memory *);

void sis_memory_jumpto(s_sis_memory *, size_t);
size_t sis_memory_get_address(s_sis_memory *);

#ifdef __cplusplus
}
#endif
#endif //_SIS_FILE_H
