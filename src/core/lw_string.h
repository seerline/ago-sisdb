#ifndef _LW_STRING_H
#define _LW_STRING_H

#include "lw_base.h"
#include "lw_thread.h"
#include "lw_public.h"
#include "zmalloc.h"

//char *out_, size_t outLen 必须在外部以 char [N] 方式定义 outLen 必须小于等于N
#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

int safestrcpy(char *out_, size_t outLen, const char *in_);
int safestrncpy(char *out_, size_t outLen, const char *in_, size_t inLen);

void gbk_to_utf8(const char*  in, char*  out, size_t  outLen);
void utf8_to_gbk(const char*  in, size_t  inLen, char*  out, size_t  outLen);

void getpy_of_utf8(const char *in, char *out, size_t);
void getpy_of_gbk(const char *in, char *out, size_t);

void str_to_upper(char *s);
void str_to_lower(char *s);
void trim(char *s);
void delreturn(char *s);

int getchnum(const char *s, char c);
int getsubnum(const char *s, char c);
void getsubstr(char *sub,size_t subLen, const char *in, char c, int index);
int substrcmp(const char *sub, const char *s, char c);  //-1没有匹配的
int headsubstrcmp(const char *sub, const char *s, char c);  //-1没有匹配的

#ifdef _MSC_VER
char *strsep(char **stringp, const char *delim);
#endif

int  parse_uri(const char *uri_, char *protocol_, size_t  pLen, char * ip_, size_t  iLen, int *port_);
bool parse_auth(const char *auth_, char * username_, size_t  uLen, char * password_, size_t  pLen);

char wait(const char *str);
/////////////////////////////////
int char_to_int(const char ** str);
int int_to_char(char **str, int chr);
char *next_token(char *buf, char *token, const char *stopchars);
void str_to_hex(char *chr_, char *src_, int len_);
unsigned char char_to_hex(char ch);

unsigned long long strcode_to_int64(char* str);
unsigned long long strtoint64(const char* str, int len);
double strtodouble(const char* str_, double default_);
int strtoint(const char* str_, int default_);

#endif //_LW_STRING_H