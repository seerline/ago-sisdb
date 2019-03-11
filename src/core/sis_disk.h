
//******************************************************
// Copyright (C) 2018, Coollyer <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SIS_DISK_H
#define _SIS_DISK_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"

#include "os_file.h"
#include "sis_memory.h"

#define SIS_DISK_VERSION   1    // 数据结构版本
////////////////////////////////////////////////////
// 这是一个高效写入的模块，需要满足以下特性
// 无论何时，保证新传入尽快落盘 aof，落盘结束，返回真
// 当达到设定条件，就对 aof 文件进行转移，原线程继续写新数据，
// 默认启动一个存盘线程，定时检查aof尺寸大小，超过尺寸就开始处理，（超过500M、超过300秒未更新也存一次盘）
// 存盘线程，禁止所有读操作，然后开始逐条处理转移的aof文件 到磁盘文件，
// 磁盘主文件只能顺序写入，后面的会覆盖前面的数据，文件合并时会处理合并和删除问题
// 磁盘文件处理时，并不改变索引文件的有效块数，除非全部数据处理成功，最后才更新索引的有效块数，
// 因此，定位写入文件位置，首先要读取出索引文件的有效位置，再开始操作，即便处理中退出，也
// 如果索引文件破坏，就重新建立索引，需时较长，需遍历所有的磁盘文件；
// 文件以不同数据结构为不同文件群组为保存规则，方便针对性拷贝迁移；
// 
// 文件按大块16M,头部保存 头长度 +（标志位0:字段1:压缩，64名长）结构名+字段描述+压缩说明，后面跟每个64K的个股数据，超过64K后，开始下一个块；
// 数据结构不同就必须切到新的数据块，在定位写入前需要检查本地结构和文件结构是否相同，不同就开新块，相同就在老块上继续写入
// 小块为 （标志位0:字段1:压缩，64名长）key + 开始时间 + 数据 ，并和idx文件中的key一一对应，只有在索引中有的才能被检索到，
// 重建索引规则，同一个key，一旦后面的key时间和前面的数据时间有交叉（不包括粘连），前面的块就直接放弃
// 索引格式为 有效块数 + 第一块：[2byte标志位] key（16） + 开始时间（8） + 文件编号（2）+ offset(4 文件偏移) 
// 全部处理完后，更新索引文件的有效块号，然后删除aof，放开读取操作，等待下一轮落盘操作，
// ------------ //
// 安全起见，对生成的序列化数据，按接收顺序每5分钟保存一个块，方便数据回放

// #define SIS_DB_FILE_CONF "%s/%s.json"    // 数据结构定义
// #define SIS_DB_FILE_MAIN "%s/%s.sdb"   // 主数据库
// #define SIS_DB_FILE_ZERO "%s/%s.sdb.0" // 不完备的数据块
// #define SIS_DB_FILE_AOF  "%s/%s.aof"     // 顺序写入

// // #define SIS_DB_FILE_OUT_CSV  	"%s/%s/%s/%s.csv"    // 主数据库 db/sisdb/sh600600/day.csv
// // #define SIS_DB_FILE_OUT_JSON  	"%s/%s/%s/%s.json"   // 主数据库 db/sisdb/sh600600/day.json
// // #define SIS_DB_FILE_OUT_ARRAY   "%s/%s/%s/%s.arr"   // 主数据库 db/sisdb/sh600600/day.json
// // #define SIS_DB_FILE_OUT_STRUCT  "%s/%s/%s/%s.bin"    // 主数据库 db/sisdb/sh600600/day.bin
// // #define SIS_DB_FILE_OUT_ZIP  	"%s/%s/%s/%s.zip"    // 主数据库 db/sisdb/sh600600/day.zip

// #define SIS_DB_FILE_OUT_CSV  	"%s/%s/%s.%s.csv"    // 主数据库 db/sisdb/sh600600.day.csv
// #define SIS_DB_FILE_OUT_JSON  	"%s/%s/%s.%s.json"   // 主数据库 db/sisdb/sh600600.day.json
// #define SIS_DB_FILE_OUT_ARRAY   "%s/%s/%s.%s.arr"   // 主数据库 db/sisdb/sh600600.day.json
// #define SIS_DB_FILE_OUT_STRUCT  "%s/%s/%s.%s.bin"    // 主数据库 db/sisdb/sh600600.day.bin
// #define SIS_DB_FILE_OUT_ZIP  	"%s/%s/%s.%s.zip"    // 主数据库 db/sisdb/sh600600.day.zip
// #pragma pack(push,1)
// typedef struct s_sis_sdb_head{
// 	uint32	size;    // 数据大小
// 	char    key[SIS_MAXLEN_KEY];
// 	uint8	format;  // 数据格式 json array struct zip 
//                      // 还有一个数据结构是 refer 存储 catch 和 zip 三条记录
// }s_sis_sdb_head;

// #define SIS_AOF_TYPE_DEL   1   // 删除
// #define SIS_AOF_TYPE_JSET  2   // json写
// #define SIS_AOF_TYPE_SSET  3   // 结构二进制写
// #define SIS_AOF_TYPE_ASET  4   // 数组写

// #define SIS_AOF_TYPE_CREATE   10   // 创建新表（默认为结构化数据），需要一起发送字段定义
// // 仅仅在jset中，提供一种直接创建法，例如：
// // sisdb.set mycode.mytable {"time":20110101,"value":1000}
// // mytable并不存在，会自动创建一个，在这个函数中，后面字段只有两种类型，一种是32位整形，一种是32位float
// // #define SIS_AOF_TYPE_FIELD    11   // 字段重构 ，后面跟所有字段定义，需要把相同字段的数据转到新数据字段中
// // 直接封闭所有数据访问，全部重构后，存盘再启动服务，

// typedef struct s_sis_aof_head{
// 	uint32	size;    // 数据大小
// 	char    key[SIS_MAXLEN_KEY];
// 	uint8   type;    // 操作类型
// 	// uint8	format;  // 数据格式 json array struct zip 
// 	//                  // 还有一个数据结构是 refer 存储 catch 和 zip 三条记录
// }s_sis_aof_head;

// #pragma pack(pop)

// //------v1.0采用全部存盘的策略 ---------//
// // 到时间保存
// bool sisdb_file_save(s_sisdb_server *server_);
// // 对用户的get请求同时输出到磁盘中，方便检查数据
// bool sisdb_file_get_outdisk(const char * key_,  int fmt_, s_sis_sds in_);

// bool sisdb_file_save_aof(s_sisdb_server *server_, 
//             int fmt_, const char *key_, 
//             const char *val_, size_t len_);

// bool sisdb_file_save_conf(s_sisdb_server *server_);
// // 开机时加载数据
// bool sisdb_file_load(s_sisdb_server *server_);

// // 检查本地配置和数据库存盘的结构是否一致，如果不一致返回错误
// // bool sisdb_file_check(const char *dbpath_, s_sis_db *db_);

// // bool sisdb_file_save_json(const char *service_, const char *db_);
// // bool sisdb_file_load_json(const char *service_, const char *db_);

#endif  /* _SISDB_FILE_H */
