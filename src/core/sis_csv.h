
#ifndef _SIS_FILE_CSV_H
#define _SIS_FILE_CSV_H

#include <sis_core.h>
#include <sis_file.h>
#include <sis_list.h>

typedef struct s_sis_file_csv 
{
	sis_file_handle    fp;
	char 			   sign[8];
	s_sis_struct_list *list;
}s_sis_file_csv;

s_sis_file_csv * sis_file_csv_open(const char *name_,int mode_, int access_);
void sis_file_csv_close(s_sis_file_csv *csv_);

size_t sis_file_csv_read(s_sis_file_csv *csv_, char *in_, size_t ilen_);
size_t sis_file_csv_write(s_sis_file_csv *csv_, char *in_, size_t ilen_);

s_sis_sds sis_file_csv_get(s_sis_file_csv *csv_, char *key_);
size_t sis_file_csv_set(s_sis_file_csv *csv_, char *key_, char *in_, size_t ilen_);

#endif

