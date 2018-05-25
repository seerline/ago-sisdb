
#ifndef _STS_FILE_H
#define _STS_FILE_H

#include <sts_core.h>
#include <sts_malloc.h>
#include <sts_str.h>

#define STS_FILE_BUFFER_LEN  1024*1024

typedef struct s_sts_file {
    size_t  maxsize;
    sts_str buffer;

	bool (*open)(char *, int ,int);
	void (*close)();

	size_t (*read)(char *in_, size_t ilen_);
	size_t (*write)(char *in_, size_t ilen_);

	sts_str (*get)(char *key_);
	size_t (*set)(char *key_, char *in_, size_t ilen_);

} s_sts_file;

sts_str sts_file_open_and_read(const char *fn_, size_t *len_);
void  sts_file_getpath(const char *fn, char *out_, int olen_);

#endif //_STS_FILE_H
