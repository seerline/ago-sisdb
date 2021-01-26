#ifndef _SIS_DB_H
#define _SIS_DB_H

#include <sis_core.h>
#include <sis_map.h>
#include <sis_dynamic.h>
#include <sis_conf.h>
#include <sis_csv.h>

/////////////////////////////////////////////////////////
//  数据格是定义 
/////////////////////////////////////////////////////////
// 下面以字节流方式传递
// #define SISDB_FORMAT_SDB     0x0001   // 后面开始为数据 keylen+key + sdblen+sdb + ssize + 二进制数据
// #define SISDB_FORMAT_SDBS    0x0002   // 后面开始为数据 count + keylen+key + sdblen+sdb + ssize + 二进制数据
// #define SISDB_FORMAT_KEY     0x0004   // 后面开始为数据 keylen+key + ssize + 二进制数据
// #define SISDB_FORMAT_KEYS    0x0008   // 后面开始为数据 count + keylen+key + ssize + 二进制数据
#define SISDB_FORMAT_STRUCT  0x0001   // 结构化原始数据
#define SISDB_FORMAT_BITZIP  0x0002   // 相关压缩数据  
#define SISDB_FORMAT_BUFFER  0x0004   // 二进制数据流  

#define SISDB_FORMAT_BYTES   0x00FF   // 二进制数据流  
// 下面必须都是可见字符
#define SISDB_FORMAT_STRING  0x0100   // 直接传数据 json文档 {k1:{},k2:{}}
#define SISDB_FORMAT_JSON    0x0200   // 直接传数据 json文档 {k1:{},k2:{}}
#define SISDB_FORMAT_ARRAY   0x0400   // 数组格式 {k1:[]}
#define SISDB_FORMAT_CSV     0x0800   // csv格式，需要处理字符串的信息
#define SISDB_FORMAT_CHARS   (SISDB_FORMAT_STRING|SISDB_FORMAT_JSON|SISDB_FORMAT_ARRAY|SISDB_FORMAT_CSV) 

typedef struct s_sis_db_chars
{
	const char   *kname;
	const char   *sname;
	uint32        size;
	void         *data;     
} s_sis_db_chars;

// 得到参数中是字符串还是字节流
int sis_db_get_format(s_sis_sds argv_);
// 得到格式
int sis_db_get_format_from_node(s_sis_json_node *node_, int default_);

// 从二进制转字符串
s_sis_sds sis_db_format_sds(s_sis_dynamic_db *db_, const char *key_, int iformat_, 
	const char *in_, size_t ilen_, int isfields_);


#endif //_SIS_DB_H

