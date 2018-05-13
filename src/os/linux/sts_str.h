
#ifndef _STS_STR_H
#define _STS_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <sts_types.h>
#include <sts_memory.h>

#define sts_sprintf snprintf

int sts_strcpy(char *out_, size_t olen_, const char *in_);
int sts_strncpy(char *out_, size_t olen_, const char *in_, size_t ilen_);

// 以第一个字符串为长度，从头开始进行比较
int sts_strcase_match(const char *son_, const char *source_);

int sts_strcasecmp(const char *s1_, const char *s2_);

void sts_trim(char *s);

//STS_MALLOC
char *sts_strdup(const char *str_, size_t len_); 
//STS_MALLOC
char *sts_str_sprintf(size_t mlen_, const char *fmt_, ...);

const char *sts_str_split(const char *s, size_t *len_, char c);

void sts_str_substr(char *out_, size_t olen_, const char *in_, char c, int idx_);

const char *sts_str_replace(const char *in, char ic_,char oc_); // 把in中的ic替换为oc
void sts_str_to_lower(char *in_);
void sts_str_to_upper(char *in_);

// int sts_strlen_right(const char *str_, const char * right_,const char *ctf_);
// int sts_strlen_left(const char *str_, const char * left_, const char *ctf_);
const char *sts_str_getline(const char *e, int *len, const char *s, size_t size_);


#endif //_STS_STR_H
