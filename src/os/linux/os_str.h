
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

#define sis_atoll atoll

char *sis_strcat(char *out_, size_t *olen_, const char *in_);

static inline int sis_strcpy(char *out_, size_t olen_, const char *in_)
{
	if (!in_ || !out_)
	{
		if (out_) 
		{
			out_[0] = 0;
		}
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

static inline int sis_strncpy(char *out_, size_t olen_, const char *in_, size_t ilen_)
{
	if (out_) 
	{
		out_[0] = 0;
	}
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

static inline void sis_trim(char *s)
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
// 返回目标字符串的长度
static inline size_t sis_strlen(const char *str_)
{
	return str_ ? strlen(str_) : 0;
}

static inline int sis_lldtoa(long long val_, char *buf_, size_t ilen_, unsigned radix_)
{
	char *p;		 /*   pointer   to   traverse   string   */
	char *firstdig;  /*   pointer   to   first   digit   */
	unsigned digval; /*   value   of   digit   */
	if (val_ < 0)
	{
		buf_[0] = '-';
		p = &buf_[1];
		val_ = -1 * val_;
		ilen_--;
	}
	else
	{
		p = &buf_[0];
	}
	firstdig = p; /*   save   pointer   to   first   digit   */
	do
	{
		digval = (unsigned)(val_ % radix_);
		val_ /= radix_; /*   get   next   digit   */

		/*   convert   to   ascii   and   store   */
		if (digval > 9)
			*p++ = (char)(digval - 10 + 'a'); /*   a   letter   */
		else
			*p++ = (char)(digval + '0'); /*   a   digit   */
		ilen_--;
		if (ilen_ < 1)
		{
			buf_[0] = 0;
			return -1;
		}
	} while (val_ > 0);

	*p-- = '\0'; /*   terminate   string;   p   points   to   last   digit   */

	char reverse;		 /*   reverse   char   */
	do
	{
		reverse = *p;
		*p = *firstdig;
		*firstdig = reverse; /*   swap   *p   and   *firstdig   */
		--p;
		++firstdig;			/*   advance   to   next   two   digits   */
	} while (firstdig < p); /*   repeat   until   halfway   */
	return 0;
}

static inline int sis_llutoa(unsigned long long val_, char *buf_, size_t ilen_, unsigned radix_)
{
	char *p;		 /*   pointer   to   traverse   string   */
	char *firstdig;  /*   pointer   to   first   digit   */
	unsigned digval; /*   value   of   digit   */

	p = buf_;
	firstdig = p; /*   save   pointer   to   first   digit   */
	do
	{
		digval = (unsigned)(val_ % radix_);
		val_ /= radix_; /*   get   next   digit   */

		/*   convert   to   ascii   and   store   */
		if (digval > 9)
			*p++ = (char)(digval - 10 + 'a'); /*   a   letter   */
		else
			*p++ = (char)(digval + '0'); /*   a   digit   */
		ilen_--;
		if (ilen_ < 1)
		{
			buf_[0] = 0;
			return -1;
		}
	} while (val_ > 0);

	*p-- = '\0'; /*   terminate   string;   p   points   to   last   digit   */

	char reverse;		 /*   reverse   char   */
	do
	{
		reverse = *p;
		*p = *firstdig;
		*firstdig = reverse; /*   swap   *p   and   *firstdig   */
		--p;
		++firstdig;			/*   advance   to   next   two   digits   */
	} while (firstdig < p); /*   repeat   until   halfway   */
	return 0;
}
#endif //_OS_STR_H
