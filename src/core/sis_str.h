
#ifndef _SIS_STR_H
#define _SIS_STR_H

#include <os_types.h>
#include <os_str.h>
#include <os_time.h>
#include <sis_malloc.h>

#pragma pack(push,1)
// typedef struct sis {
//     unsigned int len;
//     char val[0];
// } sis;
#pragma pack(pop)
// #define SIS_LEN  (sizeof(unsigned int))

// inline sis_str sis_str_new(const char *str_, unsigned int len_)
// {
//     sis_str str = sis_malloc(len_ + SIS_LEN + 1);
//     memmove(str, &len_, SIS_LEN);
//     str += SIS_LEN;
//     memmove(str,str_,len_);
//     str[len_] = 0;
//     return str;
// }
// inline void sis_str_free(sis_str str_)
// {
//     if(str_)
//     {
//         sis_free(str_ - SIS_LEN);
//     }
// }
// inline unsigned int sis_str_len(sis_str str_)
// {
//     const char *str = str_ - SIS_LEN;
//     unsigned int *o = (unsigned int *)str;
//     return *o;
// }
#ifdef __cplusplus
extern "C" {
#endif
// 以第一个字符串为长度，从头开始进行比较
int sis_strcase_match(const char *son_, const char *source_);

int sis_strcasecmp(const char *s1_, const char *s2_);
int sis_strncasecmp(const char *s1_, const char *s2_, size_t len_);

int sis_strncmp(const char *s1_, const char *s2_, size_t len_);

char *sis_strdup(const char *str_, size_t len_);  SIS_NEW

// char *sis_str_sprintf(size_t mlen_, const char *fmt_, ...); SIS_NEW

const char *sis_str_split(const char *s, size_t *len_, char c);

int sis_str_divide(const char *in_, char ch_, char *one_, char *two_);
// 返回字符所在位置
int sis_strsub(char *big_, char *small_);

int sis_str_pos(const char *in_, size_t ilen_, char c);
int sis_str_substr_nums(const char *s, char c);
void sis_str_substr(char *out_, size_t olen_, const char *in_, char c, int idx_);
// 下面的函数，在比较时，如果s串前导字符和sub匹配就会返回真
int sis_str_subcmp(const char *sub, const char *s, char c);  //-1没有匹配的
// 严格匹配
int sis_str_subcmp_strict(const char *sub, const char *s, char c);  

int sis_str_subcmp_match(const char *sub, const char *s, char c);  

const char *sis_str_replace(const char *in, char ic_,char oc_); // 把in中的ic替换为oc
void sis_str_to_lower(char *in_);
void sis_str_to_upper(char *in_);

const char *sis_str_getline(const char *e, int *len, const char *s, size_t size_);

int sis_str_match(const char* substr_, const char* source_, char c);

// 返回值为sign_开始位置，len为长度
// 例子  http://127.0.0.1:1002  ://
// 返回  127.0.0.1:1002  len = 4
// 
const char *sis_str_parse(const char *src_, const char *sign_, char *out_, size_t olen_);

// char *sis_strsep(char **src_, const char *sign_);
// olen < 8 返回
bool sis_str_get_id(char *out_, size_t olen_);
// olen < 16 返回
// bool sis_str_get_id_long(char *out_, size_t olen_);

bool sis_str_get_time_id(char *out_, size_t olen_);

int64 sis_str_read_long(char *s);

#ifdef __cplusplus
}
#endif

#endif //_SIS_STR_H
