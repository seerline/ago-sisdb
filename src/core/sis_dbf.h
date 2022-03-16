#ifndef _SIS_FILE_DBF_H
#define _SIS_FILE_DBF_H

#include <sis_os.h>
#include <sis_core.h>
#include <sis_file.h>
#include <sis_list.h>
#include <sis_map.h>

#pragma pack(push,1)

typedef struct s_sis_dbf_head
{
	uint8  res0;
	uint8  year;
	uint8  mon;
	uint8  day;
	uint32 count;  // 记录个数
	uint16 head_len;  // 头长度
	uint16 record_len; // 记录长度
	uint8  res1[20];
}s_sis_dbf_head;
//32
typedef struct s_sis_dbf_field
{
	char name[11];
	char type;
	uint16 offset;
	char res1[2];
	char len;
	char dot;
	char res2[14];
}s_sis_dbf_field;
//32
typedef struct s_sis_file_dbf 
{
	s_sis_file_handle    fp;
	s_sis_dbf_head     head;

	s_sis_struct_list *fields;   // 存储所有字段  s_sis_dbf_field 结构体列表
	s_sis_map_pointer *map_fields;   // 存储所有字段  s_sis_dbf_field 结构体列表

	char  *buffer;

}s_sis_file_dbf;

#pragma pack(pop)

// #define SIS_DBF_FIELD_TYPE_INVALID  0
// #define SIS_DBF_FIELD_TYPE_STRING   1
// #define SIS_DBF_FIELD_TYPE_UINT     2
// #define SIS_DBF_FIELD_TYPE_DOUBLE   3


s_sis_file_dbf * sis_file_dbf_open(const char *name_,int mode_, int access_);
void sis_file_dbf_close(s_sis_file_dbf *dbf_);

// size_t sis_file_dbf_read(s_sis_file_dbf *dbf_, char *in_, size_t ilen_);
// size_t sis_file_dbf_write(s_sis_file_dbf *dbf_, char *in_, size_t ilen_);

// s_sis_sds sis_file_dbf_get(s_sis_file_dbf *dbf_, char *key_);
// size_t sis_file_dbf_set(s_sis_file_dbf *dbf_, char *key_, char *in_, size_t ilen_);

int64 sis_file_dbf_get_int(s_sis_file_dbf *dbf_, int index, const char *key_, int64 defaultvalue_);
double sis_file_dbf_get_double(s_sis_file_dbf *dbf_, int index, const char *key_, double defaultvalue_);
void sis_file_dbf_get_str(s_sis_file_dbf *dbf_, int index, const char *key_,char *out_, size_t olen_);

void sis_file_dbf_get_str_of_fid(s_sis_file_dbf *dbf_, int index, int fid, char *out_, size_t olen_);

void sis_file_dbf_set_int(s_sis_file_dbf *dbf_, int index, char *key_, uint64 in_);
void sis_file_dbf_set_double(s_sis_file_dbf *dbf_, int index, char *key_, double in_, int dot_);
void sis_file_dbf_set_str(s_sis_file_dbf *dbf_, int index, char *key_, const char *in_, size_t ilen_);

#endif

