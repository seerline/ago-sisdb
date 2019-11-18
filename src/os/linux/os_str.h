
#ifndef _OS_STR_H
#define _OS_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <os_types.h>

#define sis_strsep strsep

#define sis_sprintf snprintf

#define sis_vsnprintf vsnprintf

inline int sis_strcpy(char *out_, size_t olen_, const char *in_)
{
	if (!in_ || !out_)
	{
		return 0;
	}
	size_t len = strlen(in_);
	olen_--; //防止字符串最后一位可能是边界
	len = len > olen_ ? olen_ : len;
	memmove(out_, in_, len);
	// 暂时先注释掉，避免olen长度传输错（过大），导致out越界
	// memset(out_ + len, 0, olen_ - len + 1);
	out_[len] = 0 ;
	return (int)len;
}

inline int sis_strncpy(char *out_, size_t olen_, const char *in_, size_t ilen_)
{
	if (!in_ || !out_ || ilen_ <= 0)
	{
		return 0;
	}
	olen_--;
	size_t len = olen_ > ilen_ ? ilen_ : olen_;
	memmove(out_, in_, len);
	out_[len] = 0;
	return (int)len;
}

inline void sis_trim(char *s)
{
	int i, len;
	len = (int)strlen(s);
	for (i = len - 1; i >= 0; i--)
	{
		// if (s[i] != ' ' && s[i] != 0x0d && s[i] != 0x0a)
		if (s[i] && (unsigned char)s[i] > ' ')
		{
			break;
		}
		else
		{
			s[i] = 0;
		}
	}
	for (i = 0; i < len; i++)
	{
		if (s[i] && (unsigned char)s[i] > ' ')
		// if (s[i] != ' ')
		{
			break;
		}
	}
	if (i != 0)
	{
		memmove(s, s + i, len - i);
		s[len - i] = 0;
	}
}

inline size_t sis_strlen(char *str_)
{
	return str_ ? strlen(str_) : 0;
}
#endif //_OS_STR_H
