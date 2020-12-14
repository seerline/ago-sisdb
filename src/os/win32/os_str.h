
#ifndef _OS_STR_H
#define _OS_STR_H

#include <sis_os.h>

#define snprintf _snprintf
//#define vsnprintf(a,b,c,d) vsnprintf_s(a,b,c,NULL,d) 
#define vsnprintf _vsnprintf

#define sis_atoll atoll

char *sis_strcat(char *out_, size_t *olen_, const char *in_);

inline char *sis_strsep(char **string_, const char *sub)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;
    if ((s = *string_)== NULL)
        return (NULL);
    for (tok = s;;) {
        c = *s++;
        spanp = sub;
        do {
            if ((sc =*spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *string_ = s;
                return (tok);
            }
        } while (sc != 0);
    }
    /* NOTREACHED */
}

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

inline char *sis_replace_string(char *str_, char *oldstr_, char *newstr_)
{
	if (strcmp(oldstr_, newstr_) == 0) return str_;
	char *str = str_;
	char *sub;
	int change = strlen(newstr_) - strlen(oldstr_);
	while ((sub = strstr(str, oldstr_)) && sub > str)
	{
		if (change > 0)
		{
			char *o = (char *)sis_malloc(strlen(str) + change + 1);
			int set = 0;
			size_t size = sub - str;
			memmove(o, str, size);
			set += size;  size = strlen(newstr_);
			memmove(o + set, newstr_, size);
			set += size;  size = strlen(str) - (sub - str + strlen(oldstr_));
			memmove(o + set, sub + strlen(oldstr_), size);
			set += size;
			o[set] = 0;
			sis_free(str);
			str = o;
		}
		else
		{
			int set = sub - str;
			size_t size = strlen(newstr_);
			memmove(str + set, newstr_, size);
			set += size;  size = strlen(str) - (sub - str + strlen(oldstr_));
			memmove(str + set, sub + strlen(oldstr_), size);
			set += size;
			str[set] = 0;
		}
	}
	return str;
}
//#define  "%I64d"  "%lld"
//#define  "%I64u"  "%llu"
inline int sis_sprintf(char *str, size_t mlen_, const char *fmt_, ...)
{
	size_t size = strlen(fmt_);
    char *fmt = (char *)sis_malloc(size + 1);
	memmove(fmt, fmt_, size);  fmt[size] = 0;
	fmt = sis_replace_string(fmt, "%ll", "%I64");
	//替换lld

  	va_list args;
    va_start(args, fmt_);
	vsnprintf(str, mlen_ - 1, fmt, args);
    va_end(args);
	str[mlen_-1] = 0;

	sis_free(fmt);
    return strlen(str);
}
inline size_t sis_strlen(const char *str_)
{
	return str_ ? strlen(str_) : 0;
}
// 以下两个函数会异常 放弃使用
// #define sis_lldtoa _i64toa_s
// #define sis_llutoa _ui64toa_s

inline int sis_lldtoa(long long val_, char *buf_, size_t ilen_, unsigned radix_)
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

inline int sis_llutoa(unsigned long long val_, char *buf_, size_t ilen_, unsigned radix_)
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
// inline int strerror_r(int err, char* buf, size_t buflen)
// {
// 	int size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
// 		FORMAT_MESSAGE_IGNORE_INSERTS,
// 		NULL,
// 		err,
// 		0,
// 		buf,
// 		(unsigned long long)buflen,
// 		NULL);
// 	if (size == 0) {
// 		char* strerr = strerror(err);
// 		if (strlen(strerr) >= buflen) {
// 			errno = ERANGE;
// 			return -1;
// 		}
// 		sis_strcpy(buf, buflen, strerr);
// 	}
// 	if (size > 2 && buf[size - 2] == '\r') {
// 		/* remove extra CRLF */
// 		buf[size - 2] = '\0';
// 	}
// 	return 0;
// }
#endif //_OS_STR_H
