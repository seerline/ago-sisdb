
#ifndef _SIS_FILE_CSV_H
#define _SIS_FILE_CSV_H

#include <sis_core.h>
#include <sis_file.h>
#include <sis_list.h>

typedef struct s_sis_file_csv 
{
	s_sis_file_handle   fp;
	char 			    sign[8];
	s_sis_pointer_list *list;
}s_sis_file_csv;

s_sis_file_csv * sis_file_csv_open(const char *name_, char c_, int mode_, int access_);
void sis_file_csv_close(s_sis_file_csv *csv_);

int sis_file_csv_getsize(s_sis_file_csv *csv_);
int64 sis_file_csv_get_int(s_sis_file_csv *csv_, int idx_, int field, int64 defaultvalue_);
double sis_file_csv_get_double(s_sis_file_csv *csv_, int idx_, int field, double defaultvalue_);
void sis_file_csv_get_str(s_sis_file_csv *csv_, int idx_, int field, char *out_, size_t olen_);

const char *sis_file_csv_get_ptr(s_sis_file_csv *csv_, int idx_, int field);

// size_t sis_file_csv_read(s_sis_file_csv *csv_, char *in_, size_t ilen_);
// size_t sis_file_csv_write(s_sis_file_csv *csv_, char *in_, size_t ilen_);

// s_sis_sds sis_file_csv_get(s_sis_file_csv *csv_, char *key_);
// size_t sis_file_csv_set(s_sis_file_csv *csv_, char *key_, char *in_, size_t ilen_);

// 独立的csv写入函数
s_sis_sds sis_csv_make_str(s_sis_sds in_, char *str_, size_t len_);
s_sis_sds sis_csv_make_int(s_sis_sds in_, int64 val_);
s_sis_sds sis_csv_make_uint(s_sis_sds in_, uint64 val_);
s_sis_sds sis_csv_make_double(s_sis_sds in_, double val_, int dot_);
s_sis_sds sis_csv_make_end(s_sis_sds in_);


#endif

