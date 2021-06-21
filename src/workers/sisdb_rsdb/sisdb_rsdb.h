#ifndef _SISDB_RSDB_H
#define _SISDB_RSDB_H

#include "sis_method.h"
#include "sis_disk_v1.h"

typedef struct s_sisdb_rsdb_cxt
{
	int                 status;
	s_sis_sds           work_path;
}s_sisdb_rsdb_cxt;

bool  sisdb_rsdb_init(void *, void *);
void  sisdb_rsdb_uninit(void *);
void  sisdb_rsdb_method_init(void *);
void  sisdb_rsdb_method_uninit(void *);

int cmd_sisdb_rsdb_load(void *worker_, void *argv_);
int cmd_sisdb_rsdb_save(void *worker_, void *argv_);
int cmd_sisdb_rsdb_pack(void *worker_, void *argv_);

#endif