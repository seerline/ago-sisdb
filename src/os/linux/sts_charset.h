
#ifndef _STS_CHARSET_H
#define _STS_CHARSET_H

// #include <stdio.h>
#include <stdlib.h>

void sts_gbk_to_utf8(const char*  in, char*  out, size_t  olen_);
void sts_utf8_to_gbk(const char*  in, size_t  ilen_, char*  out, size_t  olen_);

// void sts_get_spell_utf8(const char *in, char *out, size_t);
// void sts_get_spell_gbk(const char *in, char *out, size_t);

#endif //_STS_LOG_H