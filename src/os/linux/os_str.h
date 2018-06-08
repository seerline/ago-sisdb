
#ifndef _OS_STR_H
#define _OS_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <os_types.h>
#include <sts_malloc.h>

int sts_strcpy(char *out_, size_t olen_, const char *in_);
int sts_strncpy(char *out_, size_t olen_, const char *in_, size_t ilen_);

#define sts_snprintf snprintf

#endif //_OS_STR_H
