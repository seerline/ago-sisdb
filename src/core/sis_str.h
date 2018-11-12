
#ifndef _SIS_STR_H
#define _SIS_STR_H

#include <os_types.h>
#include <os_str.h>
#include <sis_malloc.h>

// 以第一个字符串为长度，从头开始进行比较
int sis_strcase_match(const char *son_, const char *source_);

int sis_strcasecmp(const char *s1_, const char *s2_);
int sis_strncasecmp(const char *s1_, const char *s2_, size_t len_);

void sis_trim(char *s);

//SIS_MALLOC
char *sis_strdup(const char *str_, size_t len_); 
//SIS_MALLOC
char *sis_str_sprintf(size_t mlen_, const char *fmt_, ...);

const char *sis_str_split(const char *s, size_t *len_, char c);

int sis_str_substr_nums(const char *s, char c);
void sis_str_substr(char *out_, size_t olen_, const char *in_, char c, int idx_);
int sis_str_subcmp(const char *sub, const char *s, char c);  //-1没有匹配的
int sis_str_subcmp_head(const char *sub, const char *s, char c);  //-1没有匹配的,比较头部几个字符是否相同

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

#endif //_SIS_STR_H
