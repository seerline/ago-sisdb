
#ifndef _STS_FILE_H
#define _STS_FILE_H

#include <sts_core.h>
#include <sts_malloc.h>
#include <sts_str.h>

#define STS_FILE_BUFFER_LEN  1024*1024

#define WRITE_FILE_INT(f__,l__)  { int len__=(int)l__ ;sts_file_write(f__, &len__, sizeof(int), 1);}
#define WRITE_FILE_STRING(f__,s__,l__)  { int len__=(int)l__ ;sts_file_write(f__, &len__, sizeof(int), 1); \
                                        sts_file_write(f__, s__, 1, len__); }

typedef struct s_sts_file {
    size_t  maxsize;
    s_sts_sds buffer;

	bool (*open)(char *, int ,int);
	void (*close)();

	size_t (*read)(char *in_, size_t ilen_);
	size_t (*write)(char *in_, size_t ilen_);

	s_sts_sds (*get)(char *key_);
	size_t (*set)(char *key_, char *in_, size_t ilen_);

} s_sts_file;

s_sts_sds sts_file_read_to_sds(const char *fn_);
void sts_get_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_);

#endif //_STS_FILE_H
