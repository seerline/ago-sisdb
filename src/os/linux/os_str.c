#include <os_str.h>

int sis_strcpy(char *out_, size_t olen_, const char *in_)
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

int sis_strncpy(char *out_, size_t olen_, const char *in_, size_t ilen_)
{
	if (!in_ || !out_)
	{
		return 0;
	}

	size_t len = strlen(in_);
	len = len > ilen_ ? ilen_ : len;
	olen_--; //防止字符串最后一位可能是边界
	len = len > olen_ ? olen_ : len;
	memmove(out_, in_, len);
	// 暂时先注释掉，避免olen长度传输错（过大），导致out越界
	// memset(out_ + len, 0, olen_ - len + 1);
	out_[len] = 0;
	return (int)len;
}

