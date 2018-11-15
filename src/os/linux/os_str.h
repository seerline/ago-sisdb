
#ifndef _OS_STR_H
#define _OS_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <os_types.h>
// #include <sis_malloc.h>

int sis_strcpy(char *out_, size_t olen_, const char *in_);
int sis_strncpy(char *out_, size_t olen_, const char *in_, size_t ilen_);

void sis_trim(char *s);

#endif //_OS_STR_H
