
#ifndef _SIS_FILE_INI_H
#define _SIS_FILE_INI_H

#include <sis_core.h>
#include <sis_file.h>
#include <sis_map.h>

// []
// xx=1

typedef struct s_sis_file_ini
{
	s_sis_file_handle   fp;
	s_sis_map_list     *groups;  //  s_sis_map_list *的集合 
}s_sis_file_ini;

s_sis_file_ini * sis_file_ini_open(const char *name_, int mode_, int access_);
void sis_file_ini_close(s_sis_file_ini *);

int sis_file_ini_get_group_size(s_sis_file_ini *);
int sis_file_ini_get_item_size(s_sis_file_ini *, int gidx_);
// const char *sis_file_ini_get_key(s_sis_file_ini *, int gidx_, int idx_);
const char *sis_file_ini_get_val(s_sis_file_ini *, int gidx_, int idx_);



#endif

