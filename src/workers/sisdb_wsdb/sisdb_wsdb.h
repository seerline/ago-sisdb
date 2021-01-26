#ifndef _SISDB_WSDB_H
#define _SISDB_WSDB_H

#include "sis_method.h"
#include "sis_disk.h"

typedef struct s_sisdb_wsdb_cxt
{
	int                 status;
	s_sis_sds           work_path;
    s_sis_sds           safe_path; // pack 时需要
    size_t              page_size; // sno 的 page 大小 

}s_sisdb_wsdb_cxt;

bool  sisdb_wsdb_init(void *, void *);
void  sisdb_wsdb_uninit(void *);
void  sisdb_wsdb_method_init(void *);
void  sisdb_wsdb_method_uninit(void *);

int cmd_sisdb_wsdb_load(void *worker_, void *argv_);
int cmd_sisdb_wsdb_save(void *worker_, void *argv_);
int cmd_sisdb_wsdb_pack(void *worker_, void *argv_);

#endif