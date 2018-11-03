
#include <errno.h>
#include <os_charset.h>
// void sis_gbk_to_utf8(const char *in, char *out, size_t olen_)
// {
// 	iconv_t cd;
// 	char *pin, *pout;
// 	size_t inlen, outlen;

// 	cd = iconv_open("utf-8", "gbk");
// 	if (cd == (iconv_t)-1)
// 	{
// 		sis_out_error(3)("iconv_open false!..[%d]\n", errno);
// 		return;
// 	}
// 	memset(out, 0, olen_);
// 	pin = (char *)in;
// 	pout = out;
// 	inlen = strlen(in) + 1;
// 	outlen = olen_;

// 	if (-1 == (int)iconv(cd, &pin, &inlen, &pout, &outlen))
// 	{
// 		sis_out_error(3)("iconv false!..[%d]\n", errno);
// 	}
// 	iconv_close(cd);
// 	olen_ = outlen;
// }

// void sis_utf8_to_gbk(const char *in, size_t ilen_, char *out, size_t olen_)
// {

// 	if (ilen_ < 1 || olen_ < 1)
// 	{
// 		return;
// 	}

// 	iconv_t cd;
// 	char *pin, *pout;
// 	size_t inlen, outlen;

// 	cd = iconv_open("gbk", "utf-8");

// 	if (cd == (iconv_t)-1)
// 	{
// 		sis_out_error(3)("iconv_open false!..[%d]\n", errno);
// 	}
// 	memset(out, 0, olen_);
// 	char *srcstr;
// 	srcstr = (char *)sis_malloc(ilen_ + 1);
// 	sis_strncpy(srcstr, ilen_ + 1, in, ilen_);
// 	sis_trim(srcstr);
// 	pin = srcstr;
// 	inlen = strlen(srcstr) + 1;

// 	pout = out;
// 	outlen = olen_;

// 	if (-1 == (int)iconv(cd, &pin, &inlen, &pout, &outlen))
// 	{
// 		sis_out_error(3)("iconv false!..[%d]%s\n", errno, in);
// 	}
// 	iconv_close(cd);
// 	olen_ = outlen;
// 	sis_free(srcstr);
// }
