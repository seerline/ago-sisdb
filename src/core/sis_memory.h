
#ifndef _SIS_MEMORY_H
#define _SIS_MEMORY_H

#include <sis_core.h>
#include <sis_malloc.h>
#include <sis_str.h>

// #define SIS_MEMORY_SIZE  65536  // read buffer size
#define SIS_MEMORY_SIZE  1024*1024*16  // read buffer size
// 传说中32K读取文件速度最快，千万不要改大了
// 64位机似乎64K更快

typedef struct s_sis_memory {
    size_t  size;     // 有数据的长度
	size_t  maxsize;  // 缓存总长度
	size_t  offset;   // 有效数据的起始位置
    char   *buffer; 
} s_sis_memory;

#define SIS_MEMORY_SWAP(a, b) { s_sis_memory *c = a; a = b; b = c; }

#ifdef __cplusplus
extern "C" {
#endif
s_sis_memory *sis_memory_create();  
s_sis_memory *sis_memory_create_size(size_t size_);

void sis_memory_destroy(void *);

void sis_memory_clone(s_sis_memory *src_, s_sis_memory *des_);

void sis_memory_swap(s_sis_memory *m1_, s_sis_memory *m2_);

void sis_memory_pack(s_sis_memory *m_);
void sis_memory_clear(s_sis_memory *m_);

size_t sis_memory_cat(s_sis_memory *, char *, size_t);  // memory tail cat, and repack buffer

size_t sis_memory_cat_int(s_sis_memory *, int);  // memory tail cat, and repack buffer
int sis_memory_get_int(s_sis_memory *);

size_t sis_memory_cat_int64(s_sis_memory *, int64);  // memory tail cat, and repack buffer
int64 sis_memory_get_int64(s_sis_memory *);

size_t sis_memory_cat_double(s_sis_memory *, double);  // memory tail cat, and repack buffer
double sis_memory_get_double(s_sis_memory *);

size_t sis_memory_readfile(s_sis_memory *, s_sis_file_handle, size_t);
size_t sis_memory_read(s_sis_memory *, s_sis_handle, size_t);

size_t sis_memory_get_size(s_sis_memory *);
size_t sis_memory_get_freesize(s_sis_memory *);

void sis_memory_set_size(s_sis_memory *m_, size_t len_);
size_t sis_memory_get_maxsize(s_sis_memory *m_);
size_t sis_memory_set_maxsize(s_sis_memory *, size_t);

char *sis_memory_read_line(s_sis_memory *m_, size_t *len_);

size_t sis_memory_get_line_sign(s_sis_memory *);
void sis_memory_move(s_sis_memory *, size_t);
char *sis_memory(s_sis_memory *);

void sis_memory_jumpto(s_sis_memory *, size_t);
size_t sis_memory_get_address(s_sis_memory *);

// 定义一个动态的长度表示
// 0 - 250 一个字节表示
// 第一个字节为251, 后面跟1个字节X 250 + X为实际数量
// 第一个字节为252, 后面跟2个字节X 250 + X为实际数量
// 第一个字节为253, 后面跟4个字节X X为实际数量
// 254保留 ***第一个字节为254, 后面跟8个字节X X为实际数量 ***
// 255保留
// 返回偏移的位数
// 按字节写 最后一个参数取值范围是 1 2 4 8 四个数字

size_t sis_memory_cat_byte(s_sis_memory *m_, int64 in_, int bytes_);
int64 sis_memory_get_byte(s_sis_memory *m_, int bytes_);

size_t sis_memory_getpos(s_sis_memory *m_);
void   sis_memory_setpos(s_sis_memory *m_,size_t);

size_t sis_memory_cat_ssize(s_sis_memory *s_, size_t in_);
// 返回实际的长度
size_t sis_memory_get_ssize(s_sis_memory *s_);
//  = 0 缓存不够 >1 移动的字节数 size_ 返回实际的数值
bool sis_memory_try_ssize(s_sis_memory *s_);

#ifdef __cplusplus
}
#endif
#endif //_SIS_FILE_H
