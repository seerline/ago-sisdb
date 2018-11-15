

#include <os_charset.h>
#include </usr/include/iconv.h>


int sis_gbk_to_utf8(const char *in, char *out_, size_t olen_)
{
	iconv_t cd;
	char *pin, *pout;
	size_t inlen, outlen;

	cd = iconv_open("utf-8", "gbk");
	if (cd == (iconv_t)-1)
	{
		// sis_out_log(3)("iconv_open false!..[%d]\n", errno);
		return errno;
	}
	memset(out_, 0, olen_);
	pin = (char *)in;
	pout = out_;
	inlen = strlen(in) + 1;
	outlen = olen_;

	if (-1 == (int)iconv(cd, &pin, &inlen, &pout, &outlen))
	{
		return errno;
		// sis_out_log(3)("iconv false!..[%d]\n", errno);
	}
	iconv_close(cd);
	olen_ = outlen;
	return 0;
}

int sis_utf8_to_gbk(const char *in, size_t ilen_, char *out_, size_t olen_)
{
	if (ilen_ < 1 || olen_ < 1)
	{
		return -2;
	}

	iconv_t cd;
	char *pin, *pout;
	size_t inlen, outlen;

	cd = iconv_open("gbk", "utf-8");

	if (cd == (iconv_t)-1)
	{
		// sis_out_log(3)("iconv_open false!..[%d]\n", errno);
		return errno;
	}
	memset(out_, 0, olen_);
	char *srcstr;
	srcstr = (char *)sis_malloc(ilen_ + 1);
	sis_strncpy(srcstr, ilen_ + 1, in, ilen_);
	sis_trim(srcstr);
	pin = srcstr;
	inlen = strlen(srcstr) + 1;

	pout = out_;
	outlen = olen_;

	if (-1 == (int)iconv(cd, &pin, &inlen, &pout, &outlen))
	{
		// sis_out_log(3)("iconv false!..[%d]%s\n", errno, in);
		return errno;
	}
	iconv_close(cd);
	olen_ = outlen;
	sis_free(srcstr);
	return 0;
}


// void gb2312_to_utf8(const char*  in, char*  out, size_t  outLen){
// 	WCHAR *strSrc;
// 	LPSTR szRes;

// 	//获得临时变量的大小
// 	int i = MultiByteToWideChar(CP_ACP, 0, in, -1, NULL, 0);
// 	strSrc = (WCHAR *)zmalloc((i + 1)*sizeof(WCHAR));
// 	MultiByteToWideChar(CP_ACP, 0, in, -1, strSrc, i);

// 	//获得临时变量的大小
// 	i = WideCharToMultiByte(CP_UTF8, 0, strSrc, -1, NULL, 0, NULL, NULL);
// 	szRes = (CHAR *)zmalloc((i + 1)*sizeof(CHAR));
// 	int j = WideCharToMultiByte(CP_UTF8, 0, strSrc, -1, szRes, i, NULL, NULL);

// 	zfree(strSrc);
// 	memset(out, 0, outLen);
// 	if (i > outLen)
// 		memmove(out, szRes, outLen);
// 	else
// 		memmove(out, szRes, i);
// 	zfree(szRes);
// }
// void utf8_to_gb2312(const char*  in, size_t  inLen, char*  out, size_t  outLen)
// {
// 	int i = MultiByteToWideChar(CP_UTF8, 0, in, (int)inLen, NULL, 0);
// 	wchar_t* strSrc = (wchar_t *)zmalloc((i + 1)*sizeof(wchar_t));   
// 	memset(strSrc, 0, i + 1);
// 	MultiByteToWideChar(CP_UTF8, 0, in, (int)inLen, strSrc, i);
// 	//i = WideCharToMultiByte(CP_ACP, 0, strSrc, -1, NULL, 0, NULL, NULL);
// 	int ii = WideCharToMultiByte(CP_ACP, 0, strSrc, i, NULL, 0, NULL, NULL);
// 	char* szRes = (char *)zmalloc(ii + 1);   
// 	memset(szRes, 0, ii + 1);
// 	//int j = WideCharToMultiByte(CP_ACP, 0, strSrc, -1, szRes, i, NULL, NULL);
// 	int j = WideCharToMultiByte(CP_ACP, 0, strSrc, i, szRes, ii, NULL, NULL);
// 	if (strSrc) zfree(strSrc);
// 	memset(out, 0, outLen);
// 	if (j > outLen)//ii
// 		memmove(out, szRes, outLen);
// 	else
// 		memmove(out, szRes, j);
// 	zfree(szRes);
// }

