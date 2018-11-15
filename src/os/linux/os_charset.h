
#ifndef _OS_CHARSET_H
#define _OS_CHARSET_H

// #include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>
#include <memory.h>
#include <os_str.h>
#include <os_malloc.h>

int sis_gbk_to_utf8(const char*  in_, char*  out_, size_t  olen_);
int sis_utf8_to_gbk(const char*  in_, size_t  ilen_, char*  out_, size_t  olen_);

#endif //_SIS_LOG_H