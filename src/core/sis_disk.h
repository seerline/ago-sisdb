
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

// #define SIS_DISK_VERSION   1    // 数据结构版本
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




//  最后决定采用 key+value的方式发送数据
// # 1、流式数据文件 -- 按时间顺序存储 
// # 特性 :  毫秒，维度最高，仅仅实盘演练回测使用

// # 按日期一天一个文件，各种数据结构混合存入
// # 20190301.aof -- 流式数据文件
// #     所有的块大小都是64K，文件的第一块为索引头
// #     头部为 start(4) stop(4) 没有日期的毫秒数 
// #     start=stop=0 表示为文件描述头的块，可多个；
// #     S表示后面为结构体定义和编号，0为结束符, 
// #     C表示后面为代码表和编号，0为分隔符
// #     start||stop>0 表示后面为数据块，大小固定为64K，
// #     数据块 以 2B(代码)+1B(数据表)+1条记录的压缩数据 +...+下一条代码
// #     ... 如果大于64K，一个结构没写完，就直接生成新的数据包，方便传输读取时

// #     读取数据时根据头的最大最小值决定每个块大致的时间节点，然后根据请求的时间直接跳到对应块进行读取，
// #     如果时间不匹配，就往前或后偏移，直到时间卡到对应位置；

// # 20190302.aof -- 流式数据 

// # idx 20190302.idx
// # 1B 类型 4字节(数据文件序号) start(4) stop(4) + 4字节(偏移位置) [size]
// H = 头描述
// # ...
// # 一个索引记录24个字节 
// # 一年的idx大小为 24*3000*250 = 18M

// # 2、历史数据文件 -- 按品种和结构存储
// # 两个时间纬度  分钟 和 日线
// # # min.sdb 
// # # min.idx  -- 索引可以从sdb中重建

// # # info.sdb
// # # info.idx

// # # day.sdb
// # # day.idx -- 索引可以从sdb中重建

// # idx 最大4G*4G的数据集合
// # [*] sh600600.info 1-255 4字节(数据文件序号) 4字节(偏移位置) [size] + start(4)-stop(4) 单位秒
// # ...
// # 一个索引记录24个字节 
// # 一年的idx大小为 24*3000*250 = 18M

// # sdb 头部是数据结构描述和结构编号, 2B 结构描述长度 + 结构描述与编号 + 
// # 不确定大小的数据块集合，要求每个块可以独立解析,
// # 然后就开始数据区 : 
// # sh600600 + 0 + 1B 数据表 + start(4)-stop(4) 
// #       1bit 是否压缩 250条记录 或 251 + 64K条记录  
// # ***** 若结构不同，重新开始一个新文件 *****

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

// #define  SIS_DISK_MAXLEN_FILE      0xFFFFFFFF  // 4G

// #define  SIS_DISK_MAXLEN_PACKAGE   0x00FFFFFF  // 16M 实时流文件专用

// #define  SIS_DISK_MAXLEN_BLOCK     0xFFFF      // 64K 历史数据专用

// #pragma pack(push,1)
// // 流式文件索引结构体，只允许顺序增加数据
// typedef struct s_sis_avi_head {
//     uint8    version;       // 版本号
//     uint8    scale;         // 时间尺度 毫秒 秒 日期等
//     uint8    ziper;         // 压缩算法
//     uint16   tsize;         // 结构体大小 
//     uint32   csize;         // 代码表的大小 code1+0+code2+0
//     // uint8     table[0];   // 结构体描述
//     // uint8     code[0];   // 结构体描述
// }s_sis_avi_head;

// typedef struct s_sis_avi_body {
// 	uint8   type;    // 类型 D:del S:set A:add
//     uint16  cid;     // 去头描述中找对应代码
//     uint8   tid;     // 去头描述中找对应表
// 	uint16	size;    // 数据大小 最大写入数据不超过64K
//     // uint8   data[0]; // 数据区
// }s_sis_avi_body;

// ////////////////////////////////////////////////
// // disk 仅仅存储历史数据，按单个股票为基本元素进行存储；
// typedef struct s_sis_disk_idx {
//     char    key[32];    // 最大key不超过32的字符
//     uint32  index;      // 在哪个文件中 文件序号 4G*4G = 16P数据
//     uint32  start;      // 开始时间 快速检索
//     uint32  stop;       // 结束时间 快速检索
//     uint32  offset;     // 偏移位置 快速定位
//     uint16  size;       // 数据大小 如果新数据+size < 64K 就在原块上修改，否则生成新的块
// }s_sis_disk_idx;
// // 一共52个字节一条记录

// // 每个文件的头部有一个结构体字典说明，如果结构体不同就需要新生成文件，留待合适时间转换数据格式
// typedef struct s_sis_disk_head {
//     uint8    version;       // 版本号
//     uint8    scale;         // 时间尺度 毫秒 秒 日期等
//     uint8    ziper;         // 压缩算法
//     uint16   size;          // 结构体大小 
//     // uint8     table[0];   // 结构体描述
// }s_sis_disk_head;

// // 流式文件索引结构体，大小限定为64K  SIS_DISK_MAXLEN_BLOCK
// typedef struct s_sis_disk_body {
//     uint8   type;         // B 块标记 当数据紊乱时，通过该标记恢复尚未被破坏的数据
//     char    key[32];      // 最大key不超过32的字符
//     uint32  start;        // 开始时间
//     uint32  stop;         // 结束时间 
//     uint32  count;        // 数据个数
//     uint16  size;         // 数据实际大小，如果新数据+size < 64K 就在原块上修改，否则生成新的块
//     // uint8   data[0];      // 跟数据字节流
// }s_sis_disk_body;


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
