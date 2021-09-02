
#ifndef _SIS_UTILS_H
#define _SIS_UTILS_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sis_os.h"
#include "sis_dynamic.h"
#include "sis_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////
// 对 s_sis_dynamic_db 信息的提取和转换
/////////////////////////////////////////////////
// 数据转换为array
s_sis_sds sis_dynamic_db_to_array_sds(s_sis_dynamic_db *db_, const char *key_, void *in_, size_t ilen_); 
// 数据转换为csv
s_sis_sds sis_dynamic_db_to_csv_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_);
// 表字段转 conf
s_sis_sds sis_dynamic_dbinfo_to_conf(s_sis_dynamic_db *db_, s_sis_sds in_);
// 表字段转 json 
s_sis_json_node *sis_dynamic_dbinfo_to_json(s_sis_dynamic_db *db_);
// 直接通过配置转数据格式
s_sis_sds sis_dynamic_conf_to_array_sds(const char *confstr_, void *in_, size_t ilen_); 

// json 转字符串
s_sis_sds sis_json_to_sds(s_sis_json_node *node_, bool iszip_);

// match_keys : * --> whole_keys
// match_keys : k,m1 | whole_keys : k1,k2,m1,m2 --> k1,k2,m1
s_sis_sds sis_match_key(s_sis_sds match_keys, s_sis_sds whole_keys);
// 得到匹配的sdbs
// match_sdbs : * | whole_sdbs : {s1:{},s2:{},k1:{}} --> s1,s2,k1
// match_sdbs : s1,s2,s4 | whole_sdbs : {s1:{},s2:{},k1:{}} --> s1,s2
s_sis_sds sis_match_sdb(s_sis_sds match_keys, s_sis_sds whole_keys);
// 得到匹配的sdbs
// match_sdbs : * --> whole_sdbs
// match_sdbs : s1,s2 | whole_sdbs : {s1:{},s2:{},k1:{}} --> {s1:{},s2:{}}
s_sis_sds sis_match_sdb_of_sds(s_sis_sds match_sdbs, s_sis_sds  whole_sdbs);
// 得到匹配的sdbs
// whole_sdbs 是s_sis_dynamic_db结构的map表
// match_sdbs : * --> whole_sdbs of sis_sds
// match_sdbs : s1,s2 | whole_sdbs : {s1:{},s2:{},k1:{}} --> {s1:{},s2:{}}
s_sis_sds sis_match_sdb_of_map(s_sis_sds match_sdbs, s_sis_map_list *whole_sdbs);

#ifdef __cplusplus
}
#endif

#endif /* _SIS_DICT_H */
