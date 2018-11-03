
#ifndef _SIS_FILE_H
#define _SIS_FILE_H

#include <sis_core.h>
#include <sis_malloc.h>
#include <sis_str.h>

#define SIS_FILE_BUFFER_LEN  1024*1024

#define WRITE_FILE_INT(f__,l__)  { int len__=(int)l__ ;sis_file_write(f__, &len__, sizeof(int), 1);}
#define WRITE_FILE_STRING(f__,s__,l__)  { int len__=(int)l__ ;sis_file_write(f__, &len__, sizeof(int), 1); \
                                        sis_file_write(f__, s__, 1, len__); }

typedef struct s_sis_file {
    size_t  maxsize;
    s_sis_sds buffer;

	bool (*open)(char *, int ,int);
	void (*close)();

	size_t (*read)(char *in_, size_t ilen_);
	size_t (*write)(char *in_, size_t ilen_);

	s_sis_sds (*get)(char *key_);
	size_t (*set)(char *key_, char *in_, size_t ilen_);

} s_sis_file;

s_sis_sds sis_file_read_to_sds(const char *fn_);
void sis_get_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_);

#endif //_SIS_FILE_H
