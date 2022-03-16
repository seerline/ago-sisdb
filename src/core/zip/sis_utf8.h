#ifndef _SIS_UTF8_H
#define _SIS_UTF8_H

#include "sis_bits.h"
////////////////////////
// UTF-8 格式  转 unicode 说明
// 0000 – 007F
// 0xxxxxxx
// 0080 – 07FF
// 110xxxxx 10xxxxxx
// 0800 – FFFF
// 1110xxxx 10xxxxxx 10xxxxxx
////////////////////////
// unicode 和 gbk 通过编码对照表来进行转换
////////////////////////
#define SIS_BASE64_ENCODE_OUT_SIZE(s) ((unsigned int)((((s) + 2) / 3) * 4 + 1))
#define SIS_BASE64_DECODE_OUT_SIZE(s) ((unsigned int)(((s) / 4) * 3))

// olen 申请空间大于等于 ilen_
size_t sis_utf8_to_gbk(const char *in_, size_t ilen_, char *out_, size_t olen_);
// olen 申请空间大于等于 ilen_/2 * 3
size_t sis_gbk_to_utf8(const char *in_, size_t ilen_, char *out_, size_t olen_);

// olen 申请空间大于等于 SIS_BASE64_ENCODE_OUT_SIZE
size_t sis_base64_encode(const char *in_, size_t ilen_, char *out_, size_t olen_);
// olen 申请空间大于等于 SIS_BASE64_DECODE_OUT_SIZE
size_t sis_base64_decode(const char *in_, size_t ilen_, char *out_, size_t olen_);

#endif