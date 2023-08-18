#ifndef _SIS_DISK_IO_H
#define _SIS_DISK_IO_H

#include "sis_core.h"
#include "sis_snappy.h"
#include "sis_incrzip.h"
#include "sis_memory.h"
#include "sis_dynamic.h"
#include "sis_obj.h"
#include "sis_file.h"
#include "sis_math.h"
#include "sis_time.h"
#include "sis_db.h"
#include "sis_utils.h"

#pragma pack(push, 1)

#define SIS_DISK_SDB_VER   4    // 数据结构版本

////////////////////////////////////////////////////
// 内部标准定义
////////////////////////////////////////////////////
// 一共5种数据文件格式 和 2种索引文件格式 
// 1. 实时日志文件 无时序 无索引 无文件尾 此类文件只能追加数据
//    一般作为记录写入动作的实时记录 按写入顺序落盘 以天为一个文件
//    确认物理写盘后才返回写入成功 重新加载时从头顺序读 直到最后一个合法的数据块
//    通常会在一天结束后 转换为磁盘其他数据格式 并清理删除
#define  SIS_DISK_TYPE_LOG         1 // name.20210121.log
// 2. 带序号的SNO日记文件  有索引 有文件尾 此类文件只能追加数据
//    此类文件只能追加结构化数据 分页条件达成时 压缩所有股票数据 然后写入
//    可以全部或单独获取某个key+sdb得到数据 以天为一个文件
//    每超过 500兆 分页处理 以SIS_DISK_HID_SNO_END 分隔页面
#define  SIS_DISK_TYPE_SNO         2  // name/2021/20210121.sno
////////////////////////////////////////////////////
//    同一文件组中 一个key+sdb只有一个数据块，如果有修改后写的会替代新的 不pack新旧数据都在 区别是内存索引会变
//    若单个文件过大 就会生成新的文件.1.2.3 但索引不变，
//    索引文件对每个key需要保存一个 active 字节 0..255 每次写盘时如果没有增加就减 1 以此来判断key的热度  
//    文件一律分块存储
////////////////////////////////////////////////////
// 2. 统一管理 4 5 6 类型文件 只保留 所有key和sdb最新的结构体
// 所有有效的 key 及相关信息 保存在 SIS_DISK_HID_MSG_MAP 块中
// 删除在 map 中做标记  pack 时才做清理
#define  SIS_DISK_TYPE_SDB         3   // name/name.sdb
// 4. 标准SDB数据文件 无时序 有索引 有文件尾 name.sdb 会有[所有]的key [最新]的结构体名和结构sdb
//    但name的索引只有无时序的数据索引 加载时仅仅加载key和sdb 以方便确定数据是否可能存在
//    根据查询时间再打开对应目录时序文件 才能得到真实的数据节点
#define  SIS_DISK_TYPE_SDB_NOTS    4   // name/nots/name.sdb name.idx
// 5. 大尺度SDB数据文件 有时序 有索引 有文件尾 时间字段为日和以上级别的按每10年为一个区间存储数据
// 按5000key算 一条100 一年250天 = 1.25G 原始数据 支持增量写入 时间有重叠需要合并后写入 PACK时合并数据
#define  SIS_DISK_TYPE_SDB_YEAR    5   // name/year/2010.sdb
// 6. 小尺度SDB数据文件 有时序 有索引 有文件尾 时间字段为日以下的按每天为一个区间存储数据
// 按5000key算 一条100 一天5000条 = 2.5G 原始数据 通常KEY对应一块数据 PACK时清理过期数据块 保留最后一块数据
// 没有对分钟线专门处理 是因为正常分析时通常以天总览 以天为细节分析
#define  SIS_DISK_TYPE_SDB_DATE    6  // name/date/2021/20210606.sdb
// 7. 顺序SIC日记文件 方便网络直接传输 采用增量压缩方式存储数据
//    此类文件只能追加结构化数据 按时序压缩写入 有索引 有文件尾 此类文件只能追加数据
//    存在的价值主要用于历史全盘数据回放 以避免解压压缩过程提高读取速度 以天为一个文件
//    文件可以直接提取磁盘数据进行网络传输  支持压缩期间增加key和sdb
//    文件以16K一个数据块 以1024个数据块为一个数据包 以SIS_DISK_HID_SIC_NEW 分隔
//    数据从头开始的加载 直到读取到文件尾 由于可能定位读取数据 文件分块存储
#define  SIS_DISK_TYPE_SIC         7  // name/2021/20210121.sic  struct incr compress
////////////////////////////////////////////////////
// 1. sno的索引文件 记录key和sdb的信息 以及每个段的 以SIS_DISK_HID_SNO_END 信息 方便按时间点获取数据 时间为毫秒
//    索引文件由于必须全部加载 不分块存储
#define  SIS_DISK_TYPE_SNO_IDX    10 
// 2. sdb的索引文件 记录key和sdb的信息 以及每个key+sdb的位置信息 时间统一为毫秒
//    索引文件由于必须全部加载 不分块存储
#define  SIS_DISK_TYPE_SDB_IDX    11
// 3. sno的索引文件 记录key和sdb的信息 以及每个段的 以SIS_DISK_HID_SIC_NEW 信息 方便按时间点获取数据 时间为毫秒
//    索引文件由于必须全部加载 不分块存储
#define  SIS_DISK_TYPE_SIC_IDX    12 
////////////////////////////////////////////////////

#define  SIS_DISK_IS_IDX(t) (t == SIS_DISK_TYPE_SNO_IDX||t == SIS_DISK_TYPE_SDB_IDX||t == SIS_DISK_TYPE_SIC_IDX)
#define  SIS_DISK_IS_SDB(t) (t == SIS_DISK_TYPE_SDB_NOTS || t == SIS_DISK_TYPE_SDB_YEAR || t == SIS_DISK_TYPE_SDB_DATE)
// 文件后缀名
#define  SIS_DISK_LOG_CHAR      "log"
#define  SIS_DISK_SNO_CHAR      "sno"
#define  SIS_DISK_SIC_CHAR      "sic"
#define  SIS_DISK_SDB_CHAR      "sdb"
#define  SIS_DISK_MAP_CHAR      "map"
#define  SIS_DISK_IDX_CHAR      "idx"
#define  SIS_DISK_TMP_CHAR      "tmp"
// SDB 文件目录名
#define  SIS_DISK_NOTS_CHAR      "nots"
#define  SIS_DISK_YEAR_CHAR      "year"
#define  SIS_DISK_DATE_CHAR      "date"

// key和sdb的关键字 
#define  SIS_DISK_SIGN_KEY      "_keys_"   // 用于索引文件的关键字
#define  SIS_DISK_SIGN_SDB      "_sdbs_"   // 用于索引文件的关键字
#define  SIS_DISK_SIGN_NEW      "_news_"   // 用于索引文件的关键字 记录数据块的开始时间
#define  SIS_DISK_SIGN_END      "_ends_"   // 用于索引文件的关键字 记录数据块的结束时间

// 文件合法性检查
#define  SIS_DISK_CMD_NO_IDX             1
#define  SIS_DISK_CMD_OK                 0
#define  SIS_DISK_CMD_NO_EXISTS        -10
#define  SIS_DISK_CMD_NO_EXISTS_IDX    -11
#define  SIS_DISK_CMD_NO_IGNORE       -100
#define  SIS_DISK_CMD_NO_INIT         -101
#define  SIS_DISK_CMD_NO_VAILD        -102
#define  SIS_DISK_CMD_NO_OPEN         -103
#define  SIS_DISK_CMD_NO_OPEN_IDX     -104
#define  SIS_DISK_CMD_NO_CREATE       -105
// 文件长度预定义
// #define  SIS_DISK_MAXLEN_FILE      0x7F000000  // 数据文件专用  4G - 83M
#define  SIS_DISK_MAXLEN_FILE      0xFF000000  // 数据文件专用  4G - 83M
#define  SIS_DISK_MAXLEN_SDBPAGE   0x00FFFFFF  // 16M SDB文件块大小 超过需分块存储

#define  SIS_DISK_MAXLEN_SICPART   0x00004000  // 16K 每个压缩包大小
#define  SIS_DISK_MAXLEN_SICPAGE   0x00FFFFFF  // 16M 文件块大小 不超过连续压缩 超过重新开始压缩

#define  SIS_DISK_MAXLEN_SNOPAGE   0x1FFFFFFF  // 536M 文件块大小

#define  SIS_DISK_MAXLEN_IDXPAGE   0x00FFFFFF  // 16M 索引文件块大小 

////////////////////////////////////////////////////
// 文件所有数据块都有 SIS_DISK_BLOCK_HEAD 头来描述后面的数据的特性 fin 帧
#define SIS_DISK_BLOCK_HEAD  \
		uint8 fin : 1;    \
		uint8 zip : 2;    \
		uint8 hid : 5;    \
// zip 压缩方法 0 - 不压缩 1 - snappy 2 - 结构增量压缩 3 其他
// 后面紧跟 size(dsize) 为数据区长度 根据实际数据长度可以得到数据有多少条
////////////////////////////////////////////////////
// hid 的定义如下 最多有32种头描述
////////////////////////////////////////////////////
#define  SIS_DISK_HID_NONE        0x0  // 表示该块为删除标记
// 文件头
#define  SIS_DISK_HID_HEAD        0x1  // 文件头描述
// 键值的定义
#define  SIS_DISK_HID_DICT_KEY    0x2  // size(dsize)+key的描述 k1,k2... 以,隔开,可多个key 根据顺序得到索引
// 结构属性定义
#define  SIS_DISK_HID_DICT_SDB    0x3  // size(dsize)+sdb的结构描述 x:{},y:{},z:{} 可以多个sdb 根据顺序得到索引
// LOG的自由数据块                    
#define  SIS_DISK_HID_MSG_LOG     0x4  // size(dsize)+datastream 自行决定数据区数据格式
// SNO的数据块 增加序列号的数据块 默认多个数据
#define  SIS_DISK_HID_MSG_SNO     0x5  // size(dsize)+kid(dsize)+dbid(dsize)+[sno(dsize)+datastream] 一个key+N条记录 
// SNO数据块结束符 收到此消息后 表明数据开始新的一页
#define  SIS_DISK_HID_SNO_END     0x6  // size(dsize)+最新时间+pages(dsize)+序号(dsize)
////// 无时序的 ////////
// 无时序的结构数据 单key单sdb的 通用压缩数据
// 需要有 SIS_DISK_HID_DICT_KEY 和 SIS_DISK_HID_DICT_SDB
#define  SIS_DISK_HID_MSG_NON     0x7  // size(dsize)+kid(dsize)+dbid(dsize)+[datastream] 一个key+N条记录
// 无时序的非结构数据 单key无sdb 单条记录 通用压缩数据
#define  SIS_DISK_HID_MSG_ONE     0x8  // size(dsize)+kid(dsize)+vlen(dsize)+datastream
// 无时序的非结构数据 单key无sdb 多条记录 通用压缩数据
#define  SIS_DISK_HID_MSG_MUL     0x9  // size(dsize)+kid(dsize)+count(dsize)+[vlen(dsize)+datastream...]
////// 有时序的 ////////
// SDB的通用数据块 单key单sdb的增量压缩数据
// 需要有 SIS_DISK_HID_DICT_KEY 和 SIS_DISK_HID_DICT_SDB
#define  SIS_DISK_HID_MSG_SDB     0xA  // size(dsize)+kid(dsize)+dbid(dsize)+[incrzipstream] 一个key+N条记录
// SNO的数据块 默认多key多sdb压缩数据   
#define  SIS_DISK_HID_MSG_SIC     0xB  // size(dsize)+incrzipstream 
// SNO数据块结束符 收到此消息后 表明数据压缩重新开始
#define  SIS_DISK_HID_SIC_NEW     0xC  // size(dsize)+最新时间+pages(dsize)+序号(dsize)
// MAP文件的key+sdb的索引信息 active 在 1.255 之间表示有效 0 表示删除 
// 后写入的 如果 ndate 一样会覆盖前面写入的数据 这样保证 map 只写增量数据 仅在pack 时才清理冗余的数据
#define  SIS_DISK_HID_MSG_MAP     0xD // size(dsize)+klen(dsize)+kname+dblen(dsize)+dname+active(1)+ktype(1)+blocks(dsize)
//        +[active(1)+ndate(dsize)]

/////////////////////////////////////////////////////////
// 读取索引文件必须加载全部数据 *** 特别重要 *** 数据文件才有意义
////////////////////////////////////////////////////
// 一个文件可能会出现多次字典信息写入 每次写入都会先写字典表，读取时也是先读字典 后续索引才能一一匹配
// 格式为 blocks(dsize)+[fidx(dsize)+offset(dsize)+size(dsize)]...
#define  SIS_DISK_HID_INDEX_KEY   0x10 // size(dsize)+
// 格式为 blocks(dsize)+[fidx(dsize)+offset(dsize)+size(dsize)]...
#define  SIS_DISK_HID_INDEX_SDB   0x11 // size(dsize)+
// 格式为 klen(dsize)+key+dblen(dsize)+dbn+blocks(dsize)
//         +[active(1)+kdict(dsize)+sdict(dsize)+fidx(dsize)+offset(dsize)+size(dsize)+start(dsize)+page(dsize)]...
#define  SIS_DISK_HID_INDEX_SNO   0x12 // size(dsize)+
// 结构化数据索引 style = 0
// 非结构化数据索引 style != 0
// 合并在一起主要是为了压缩
// stop通常存储时为start的偏移量
// 格式为 klen(dsize)+kname+dblen(dsize)+dname+blocks(dsize)
//         +[active(1)+kdict(dsize)+style(dsize)+sdict(dsize)+fidx(dsize)+offset(dsize)+size(dsize)+start(dsize)+stop(dsize)]...
#define  SIS_DISK_HID_INDEX_MSG   0x13 // size(dsize)+
// NET的块开始块索引 存储当前是第几个块 用于断点续传时使用
// 格式为 blocks(dsize)+[fidx(dsize)+offset(dsize)+size(dsize)+stopmsec(dsize)]
#define  SIS_DISK_HID_INDEX_END   0x14 // size(dsize)+
// NET的块开始块索引 存储当前是第几个块 用于断点续传时使用
// 格式为 blocks(dsize)+[fidx(dsize)+offset(dsize)+size(dsize)+openmsec(dsize)]
#define  SIS_DISK_HID_INDEX_NEW   0x15 // size(dsize)+
// 文件结束块
#define  SIS_DISK_HID_TAIL        0x1F  // 结束块标记

// 定义压缩方法
// 由于 incrzip 的解压速度稍逊于 snappy 所以通用格式采用 snappy 压缩 
// incrzip 仅仅用于 SIS_DISK_TYPE_SIC 类型文件
#define  SIS_DISK_ZIP_NOZIP       0
#define  SIS_DISK_ZIP_SNAPPY      1
#define  SIS_DISK_ZIP_INCRZIP     2
#define  SIS_DISK_ZIP_OTHER       3

// 时序结构数据的时间尺度定义
#define SIS_SDB_SCALE_NOTS   0
#define SIS_SDB_SCALE_YEAR   1
#define SIS_SDB_SCALE_DATE   2

// 不定长非时序数据定义 -- 未来合并到 SDB 中
// 不再区分二进制或JSON统一按二进制处理 
// 传输时检查如果是非全字符就转BASE64格式 传输的数据前导字符S表示原始字符串 B表示BASE64转换 
#define SIS_SDB_STYLE_SDB      0  // 有时序结构数据 
#define SIS_SDB_STYLE_NON      1  // 无时序结构数据
#define SIS_SDB_STYLE_ONE      2  // 无时序单一数据
#define SIS_SDB_STYLE_MUL      3  // 无时序列表数据
#define SIS_SDB_STYLE_SNO      4  // 有时序结构数据 

// 位域定义长度必须和类型匹配 否则长度不对
typedef struct s_sis_disk_main_head {
	SIS_DISK_BLOCK_HEAD
    char     sign[3];             // 标志符 "SIS"
    uint32   wdate;               // 是起始数据的日期 不是创建的时间  
    uint16   version       :   5; // 版本号 最多 32 个版本
    uint16   style         :   5; // 文件类型 最多 32 种类型
    uint16   iszip         :   2; // 是否压缩 0 其后的数据都不压缩 1 按文件类型不同用不同的压缩方式
    uint16   index         :   1; // [数据文件专用] 1 有索引 0 没有索引文件 
    uint16   nouse         :   3; // 开关类保留 (3)
    uint16   workers;             // [索引文件专用] 对应work文件数量
    uint8    switchs[4];          // 开关类保留 (9)
}s_sis_disk_main_head;            // 16个字节头 128

// 索引或者工作文件的总数只有在关闭文件时才能确定 所以尾部保存总的文件数
typedef struct s_sis_disk_main_tail {
	SIS_DISK_BLOCK_HEAD
    uint32   fcount;       // 当前关联文件总数 
    uint32   novalid;      // 无效块数 最小的块一定大于1个字节 根据该值判定是否需要pack
    uint32   validly;      // 有效块数 
    uint8    other[19];    // 备用 多留16字节备用
    char     crc[16];      // 数据检验 索引应该和工作crc一致 创建时生成 后面不变 比较IDX和SDB可以大致知道文件是否匹配
}s_sis_disk_main_tail;     // 32个字节尾


// 定义读取缓存最小字节
// 除非到了文件尾，否则最小的in大小不得低于16个字节 这样就不用判断 dsize 的尺寸了
#define  SIS_DISK_MIN_RSIZE   sizeof(s_sis_disk_main_tail)

// 公用头 1 个字节
typedef struct s_sis_disk_head {
    SIS_DISK_BLOCK_HEAD
}s_sis_disk_head;

#define  SIS_DISK_MIN_WSIZE   (sizeof(s_sis_disk_head) + sizeof(s_sis_disk_main_tail))

////////////////////////////////////////////////////
// 单文件读写定义
////////////////////////////////////////////////////
// 文件状态
#define  SIS_DISK_STATUS_CLOSED    0x0
#define  SIS_DISK_STATUS_OPENED    0x1
#define  SIS_DISK_STATUS_RDOPEN    0x2  // 文件只读打开 

#define  SIS_DISK_STATUS_APPEND    0x10  // 文件可写打开 从文件尾写入数据
#define  SIS_DISK_STATUS_CREATE    0x20  // 新文件打开 从第一个文件写入
#define  SIS_DISK_STATUS_NOSTOP    0x40  // 文件写打开 已经写了文件头 但未写结束标记
#define  SIS_DISK_STATUS_CHANGE    0x80  // 文件有变化 文件有除了文件头的更新

typedef struct s_sis_disk_files_unit {
    s_sis_sds             fn;         // 文件名
    s_sis_file_handle     fp_1;       // 文件句柄
    size_t                offset;     // 当前偏移位置
    uint32                novalid;    // 无效数量
    uint32                validly;    // 有效数量
    uint8                 status;     // 文件状态
    s_sis_memory         *fcatch;     // 必须加写缓存 如果写入块太小 写盘耗时会成倍增加
} s_sis_disk_files_unit;

#define  SIS_DISK_ACCESS_NOLOAD    0  // 没有这类文件 
#define  SIS_DISK_ACCESS_RDONLY    1  // 文件只读打开 
#define  SIS_DISK_ACCESS_APPEND    2  // 文件可写打开 从文件尾写入数据
#define  SIS_DISK_ACCESS_CREATE    3  // 新文件打开 从第一个文件写入

// 多文件管理类
typedef struct s_sis_disk_files {
    uint8                 access;         // 只有三种状态
    s_sis_disk_main_head  main_head;      // 文件头
    s_sis_disk_main_tail  main_tail;      // 文件尾
    s_sis_sds             cur_name;       // 工作文件名
    uint8                 cur_unit;       // 当前是哪个文件
	size_t                max_file_size;  // 文件最大尺寸 大于就换文件
	size_t                max_page_size;  // 块最大尺寸 大于就换分块存储 暂时不用
	s_sis_pointer_list   *lists;          // s_sis_disk_files_unit 文件信息 有几个就打开几个
} s_sis_disk_files;

//////////////////////////////////////////////////////
// 索引表 只有具备索引功能的文件 必须先加载字典 再加载索引 
// 索引文件写入时要先写字典信息
//////////////////////////////////////////////////////
typedef struct s_sis_disk_idx_unit
{
    uint8             active; // 活跃记数 据此可判断是否需要加载到内存
    uint8             ktype;  // key对应异构类型时的 数据类型 SIS_SDB_STYLE_SDB SIS_SDB_STYLE_NON ...
    uint8             sdict;  // 结构字典对应的索引 - 默认为一个文件同一个键值不超过255次改变 0 表示没有sdb
    uint16            fidx;   // 在哪个文件中 文件序号 文件名.1
    uint64            offset; // 文件偏移位置
    uint64            size;   // 数据长度
    uint64            start;  // 开始时间 不同时间尺度 全部转换为毫秒
    uint64            stop;   // 结束时间 检索时也全部转换为毫秒 统一单位
    uint16            ipage;  // sno专用 页面索引
} s_sis_disk_idx_unit;

// 索引依赖于字典表 索引文件总是会把字典表写在前面 读取字典后 后续数据才能正确获取和读取 
typedef struct s_sis_disk_idx {
    uint32              cursor;    // 当前读到第几条记录
    s_sis_object       *kname;     // 可能多次引用 - 指向dict表的name
    s_sis_object       *sname;     // 可能多次引用 - 指向dict表的name
    s_sis_struct_list  *idxs;      // 索引的列表 s_sis_disk_idx_unit
}s_sis_disk_idx;

////////////////////////////////////////////////////
// 读写catch定义
////////////////////////////////////////////////////
// 通用的读取数据函数 如果有字典数据key就用字典的信息 
// 如果没有字典信息就自己建立一个
typedef struct s_sis_disk_wcatch {
    s_sis_object         *kname;    // 可能多次引用 - 指向dict表 因为可能文件没有索引
    s_sis_object         *sname;    // 可能多次引用 - 指向dict表
    s_sis_disk_head       head;     // 块头信息
    s_sis_memory         *memory;   // 可能数据缓存指针
    s_sis_disk_idx_unit   winfo;    // 写入时的索引信息
}s_sis_disk_wcatch;

struct s_sis_disk_reader_cb;

typedef struct s_sis_disk_rcatch {
    // 按索引信息获取数据 只是指针
    s_sis_disk_idx_unit         *rinfo;    // 按索引信息获取数据 只是指针
    // 按顺序读取文件 根据回调返回数据
    s_sis_msec_pair              search_msec;
    int                          iswhole;
    s_sis_sds                    sub_keys;   // "*" 为全部都要 或者 k1,k2,k3
	s_sis_sds                    sub_sdbs;   // "*" 为全部都要 或者 k1,k2,k3
    struct s_sis_disk_reader_cb *callback;   // 读文件的回调函数组合    
    // 返回的数据
    s_sis_disk_head              head;     // 块头信息
    int                          kidx;     // key的索引信息
    int                          sidx;     // sdb的索引信息
    int                          series;   // sno的索引信息
    s_sis_memory                *memory;   // 数据缓存指针
    s_sis_pointer_list          *mlist;    // 异构类型时的数据链表
}s_sis_disk_rcatch;

//////////////////////////////////////////////////////
// s_sis_disk_kdict 
//////////////////////////////////////////////////////
// key 的索引传递信息 只有这里的name是原始的
typedef struct s_sis_disk_kdict {
    int                 index;  // 索引号
	s_sis_object       *name;   // 名字
}s_sis_disk_kdict;
//////////////////////////////////////////////////////
// s_sis_disk_sdict 
//////////////////////////////////////////////////////
// sdb 的索引传递信息 只有这里的name是原始的
typedef struct s_sis_disk_sdict {
    int                 index;  // 索引号
	s_sis_object       *name;   // 名字
    s_sis_pointer_list *sdbs;   // 信息 s_sis_dynamic_db
}s_sis_disk_sdict;
//////////////////////////////////////////////////////
// s_sis_disk_map 
//////////////////////////////////////////////////////
typedef struct s_sis_disk_map_unit
{
    uint8               active; // 读者数 0 表示该块被删除 1 - 255 表示读者数
    uint32              idate;  // 日上为年 日下未日期
    // 写入和读取时没有用 单次写如果时间不重叠 会直接增加一个数据块 此时数量是对不上的
    // uint32              count;  // 数据个数 仅仅在读取成立时
} s_sis_disk_map_unit;
// map 的索引传递信息 这里的name是原始的
typedef struct s_sis_disk_map {
    s_sis_object       *kname;  // 可能多次引用 - 指向dict表的name
    s_sis_object       *sname;  // 可能多次引用 - 指向dict表的name
    uint8               active; // 读者数 0 表示该键值被删除 1 - 255 表示读者数 每天减 1
    uint8               ktype;  // SIS_SDB_STYLE_SDB SIS_SDB_STYLE_NON ...
    s_sis_sort_list    *sidxs;  // s_sis_disk_map_unit
}s_sis_disk_map;

//////////////////////////////////////////////////////
// s_sis_disk_sno sno读取文件控制类
//////////////////////////////////////////////////////
// 既然读取的数据都有序号那么就直接写到对应位置
// 
typedef struct s_sis_disk_sno_rctrl {
    ///// sno 专用工具 //////
    int                 count;
    int                 pagenums;     // 
    int                 cursor_blk;   // 第几个数据块
    int                 cursor_rec;   // 数据块的第几条记录
    s_sis_pointer_list *rsno_idxs;    // 读数据区 s_sis_struct_list s_sis_db_chars 都是指针
    s_sis_pointer_list *rsno_mems;    // 读数据区 s_sis_memory 内存区
} s_sis_disk_sno_rctrl;

//////////////////////////////////////////////////////
// s_sis_disk_ctrl  单组文件控制类
//////////////////////////////////////////////////////

typedef struct s_sis_disk_ctrl {
    /////// 初始化文件的信息 ////////
    s_sis_sds            fpath;
    s_sis_sds            fname;
    int                  style;       // 0 表示未初始化 其他表示文件类型
    int                  open_date;   // 数据时间 - 日期
    int                  stop_date;   // 数据时间 - 日期
    int                  status;

    ////// sno sdb net 都会用的到 ////////
    bool                 isstop;       // 是否中断读取 读文件期间是否停止 如果为 1 立即退出
    // 读时为本地字典 写时为新旧合计字典
	s_sis_map_list      *map_kdicts;   // s_sis_disk_kdict
	s_sis_map_list      *map_sdicts;   // s_sis_disk_sdict
    // 读时不用 写时为新增的字典 按增加顺序写入  
	s_sis_pointer_list  *new_kinfos;   // s_sis_object 
	s_sis_pointer_list  *new_sinfos;   // s_sis_dynamic_db * 
    // map 的索引 
	s_sis_map_list      *map_maps;     // s_sis_disk_map
    // 读写控制器
    s_sis_disk_rcatch   *rcatch;       // 读数据的缓存 
    s_sis_disk_wcatch   *wcatch;       // 写数据的缓存
    // 工作文件
    s_sis_disk_files    *work_fps;     // 工作文件组
    // 索引文件和索引内存表
    s_sis_disk_files    *widx_fps;     // 索引文件组
    s_sis_map_list      *map_idxs;     // s_sis_disk_idx 索引信息列表

    ///// sdb 专用工具 //////
    s_sis_incrzip_class *sdb_incrzip;   // 需要初始化
    ///// sno 专用工具 //////
    int                  sno_pages;     // 最大的分页数
    int                  sno_series;    // SIS_DISK_HID_MSG_SNO 当前最新计数
    msec_t               sno_msec;      // 最后一条有时间序列记录的时间  
    int64                sno_count;     // 记录数
    size_t               sno_size;      // SIS_DISK_HID_MSG_SNO 当前累计数据量
    s_sis_map_list      *sno_wcatch;    // 从实时文件转盘后文件 临时存放数据 s_sis_disk_wcatch
    s_sis_disk_sno_rctrl*sno_rctrl;     // 分块读取时的缓存 根据sno进行排序
    ///// net 专用工具 //////
    int                  net_pages;     // 块数
    msec_t               net_msec;      // 第一条有时间序列记录的时间  
    int64                net_count;     // 记录数
    size_t               net_zipsize;   // net 当前压缩的尺寸
    s_sis_incrzip_class *net_incrzip;   // 增量压缩组件 用后即释放
} s_sis_disk_ctrl;

#pragma pack(pop)

///////////////////////////
//  sis_disk_files.c
///////////////////////////
//** s_sis_disk_files  **//

s_sis_disk_files *sis_disk_files_create();
void sis_disk_files_destroy(void *);

void sis_disk_files_clear(s_sis_disk_files *);

int sis_disk_files_init(s_sis_disk_files *cls_, char *fn_);

int sis_disk_files_inc_unit(s_sis_disk_files *cls_);

int sis_disk_files_remove(s_sis_disk_files *cls_);

int sis_disk_files_open(s_sis_disk_files *cls_, int access_);
void sis_disk_files_close(s_sis_disk_files *cls_);

bool sis_disk_files_seek(s_sis_disk_files *cls_);

size_t sis_disk_files_offset(s_sis_disk_files *cls_);

size_t sis_disk_files_write_sync(s_sis_disk_files *cls_);

size_t sis_disk_files_write(s_sis_disk_files *cls_, s_sis_disk_head  *head_, void *in_, size_t ilen_);

size_t sis_disk_files_write_saveidx(s_sis_disk_files *cls_, s_sis_disk_wcatch *wcatch_);

// 回调返回为 -1 表示已经没有读者了,停止继续读文件
typedef int (cb_sis_disk_files_read)(void *, s_sis_disk_head *, char *, size_t );
// 全文读取 数据后通过回调输出数据
size_t sis_disk_files_read_fulltext(s_sis_disk_files *cls_, void *, cb_sis_disk_files_read *callback);
// 定位读去某一个块
size_t sis_disk_files_read(s_sis_disk_files *, int fidx_, size_t offset_, size_t size_, 
    s_sis_disk_head  *head_, s_sis_memory *omemory_);
// 从索引信息读取数据
size_t sis_disk_files_read_fromidx(s_sis_disk_files *cls_, s_sis_disk_rcatch *);

size_t sis_disk_io_unzip_widx(s_sis_disk_head  *head_, char *in_, size_t ilen_, s_sis_memory *memory_);


///////////////////////////
//  sis_disk_ctrl.c
///////////////////////////
void sis_disk_set_search_msec(s_sis_msec_pair *src_, s_sis_msec_pair *des_);
int sis_disk_get_sdb_scale(s_sis_dynamic_db *db_);

//**  s_sis_disk_idx  **//
s_sis_disk_idx *sis_disk_idx_create(s_sis_object *kname_, s_sis_object *sname_);
void sis_disk_idx_destroy(void *in_);
int sis_disk_idx_set_unit(s_sis_disk_idx *cls_, s_sis_disk_idx_unit *unit_);
s_sis_disk_idx_unit *sis_disk_idx_get_unit(s_sis_disk_idx *cls_, int index_);
s_sis_disk_idx *sis_disk_idx_get(s_sis_map_list *map_, s_sis_object *kname_,  s_sis_object *sname_);

//** s_sis_disk_wcatch **//
s_sis_disk_wcatch *sis_disk_wcatch_create(s_sis_object *kname_, s_sis_object *sname_);
void sis_disk_wcatch_destroy(void *in_);
void sis_disk_wcatch_init(s_sis_disk_wcatch *in_);
void sis_disk_wcatch_clear(s_sis_disk_wcatch *in_);
void sis_disk_wcatch_setname(s_sis_disk_wcatch *in_, s_sis_object *kname_, s_sis_object *sname_);

//** s_sis_disk_rcatch **//
s_sis_disk_rcatch *sis_disk_rcatch_create(s_sis_disk_idx_unit *rinfo_);
void sis_disk_rcatch_destroy(void *in_);
void sis_disk_rcatch_clear(s_sis_disk_rcatch *in_);
// 根据索引读取数据
void sis_disk_rcatch_init_of_idx(s_sis_disk_rcatch *in_, s_sis_disk_idx_unit *rinfo_);
// 根据订阅信息读取数据
void sis_disk_rcatch_init_of_sub(s_sis_disk_rcatch *in_, const char *subkeys_, const char *subsdbs_,
    s_sis_msec_pair *search_, void *cb_);

//** s_sis_disk_kdict **//
s_sis_disk_kdict *sis_disk_kdict_create(const char *name_);
void sis_disk_kdict_destroy(void *in_);

//** s_sis_disk_sdict **//
s_sis_disk_sdict *sis_disk_sdict_create(const char *name_);
void sis_disk_sdict_destroy(void *in_);
s_sis_dynamic_db *sis_disk_sdict_last(s_sis_disk_sdict *dict_);
s_sis_dynamic_db *sis_disk_sdict_get(s_sis_disk_sdict *dict_, int index_);
// 找一找 最后一条记录是否结构一致 如果不一致就返回true 一样就返回false
bool sis_disk_sdict_isnew(s_sis_disk_sdict *dict_, s_sis_dynamic_db *db_);

s_sis_object *sis_disk_kdict_get_name(s_sis_map_list *map_kdict_, int keyi_);
s_sis_object *sis_disk_sdict_get_name(s_sis_map_list *map_sdict_, int sdbi_);
int sis_disk_kdict_get_idx(s_sis_map_list *map_kdict_, const char *kname_);
int sis_disk_sdict_get_idx(s_sis_map_list *map_sdict_, const char *sname_);

int sis_disk_reader_set_kdict(s_sis_map_list *map_kdict_, const char *in_, size_t ilen_);
int sis_disk_reader_set_sdict(s_sis_map_list *map_sdict_, const char *in_, size_t ilen_);

s_sis_disk_kdict *sis_disk_map_get_kdict(s_sis_map_list *map_kdict_, const char *kname_);
s_sis_disk_sdict *sis_disk_map_get_sdict(s_sis_map_list *map_sdict_, const char *sname_);

//** s_sis_disk_map **//
s_sis_disk_map *sis_disk_map_create(s_sis_object *kname_, s_sis_object *sname_);
void sis_disk_map_destroy(void *in_);

int sis_disk_map_merge(s_sis_disk_map *agomap_, s_sis_disk_map *newmap_);

///////////////////////////
//  sis_disk.io.c
///////////////////////////
//**  s_sis_disk_ctrl  **//
// 根据传入的参数创建目录或确定文件名(某些类型的文件名是系统默认的，用户只能指定目录) 
// 路径和文件名分开是为了根据不同种类增加中间目录 文件过大时自动分割子文件 
s_sis_disk_ctrl *sis_disk_ctrl_create(int style_, const char *fpath_, const char *fname_, int wdate_);
void sis_disk_ctrl_destroy(void *cls_);

void sis_disk_ctrl_clear(s_sis_disk_ctrl *cls_);

int sis_disk_ctrl_work_zipmode(s_sis_disk_ctrl *cls_);
int sis_disk_ctrl_widx_zipmode(s_sis_disk_ctrl *cls_);
// 设置文件大小
void sis_disk_ctrl_set_size(s_sis_disk_ctrl *cls_,size_t fsize_, size_t psize_);
// 检查文件是否有效
int sis_disk_ctrl_valid_work(s_sis_disk_ctrl *cls_);
// 检查索引是否有效
int sis_disk_ctrl_valid_widx(s_sis_disk_ctrl *cls_);

// 读取工作文件中的实际字典信息
int sis_disk_ctrl_read_kdict(s_sis_disk_ctrl *cls_, s_sis_disk_idx *node_);
int sis_disk_ctrl_read_sdict(s_sis_disk_ctrl *cls_, s_sis_disk_idx *node);

// 主文件有最全的key和每次变化的sdb信息
// 只要打开子文件就同步总的key,但除非有数据写入才会把更新的key写入子文件
void sis_disk_ctrl_cmp_kdict(s_sis_disk_ctrl *munit_, s_sis_disk_ctrl *sunit_);
// 只要打开子文件就同步最新的对应时区sdb,但除非有数据写入才会把更新的sdb写入子文件
void sis_disk_ctrl_cmp_sdict(s_sis_disk_ctrl *munit_, s_sis_disk_ctrl *sunit_);
// 写入新增字典信息到磁盘 写完后清空新增列
void sis_disk_ctrl_write_kdict(s_sis_disk_ctrl *cls_);
// 写入新增字典信息到磁盘 写后清空新增列
void sis_disk_ctrl_write_sdict(s_sis_disk_ctrl *cls_);

// 读取keys信息
s_sis_sds sis_disk_ctrl_get_keys_sds(s_sis_disk_ctrl *cls_);
// 读取sdbs信息
s_sis_sds sis_disk_ctrl_get_sdbs_sds(s_sis_disk_ctrl *cls_);

// 调用以下函数要检查是否为新键 时序是否一致 结构是否有变化
s_sis_disk_kdict *sis_disk_ctrl_set_kdict(s_sis_disk_ctrl *cls_, const char *kname_);
s_sis_disk_sdict *sis_disk_ctrl_set_sdict(s_sis_disk_ctrl *cls_, s_sis_dynamic_db *sdb_);

// 解压数据块
size_t sis_disk_ctrl_unzip(s_sis_disk_ctrl *cls_, s_sis_disk_head *head_, char *imem_, size_t isize_, s_sis_memory *out_);
// 解压work文件读取的内容
size_t sis_disk_ctrl_unzip_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_);

// 读文件操作
int sis_disk_ctrl_read_start(s_sis_disk_ctrl *cls_);
int sis_disk_ctrl_read_stop(s_sis_disk_ctrl *cls_);
// 写文件操作 默认追加写入 想新建可以先删除
int sis_disk_ctrl_write_start(s_sis_disk_ctrl *cls_);
int sis_disk_ctrl_write_stop(s_sis_disk_ctrl *cls_);

// 删除文件组
void sis_disk_ctrl_remove(s_sis_disk_ctrl *cls_);
// 合并日上可能存在的分段数据
int sis_disk_ctrl_merge(s_sis_disk_ctrl *src_);
// 清理文件中无效数据 
int sis_disk_ctrl_pack(s_sis_disk_ctrl *src_, s_sis_disk_ctrl *des_);
// 把源文件移动到指定位置  0 成功
int sis_disk_ctrl_move(s_sis_disk_ctrl *cls_, const char *path_);

///////////////////////////
//  sis_disk.io.log.c
///////////////////////////
// 读取时 可由外部通过 s_sis_disk_reader 直接访问数据
////////////////////////////////////////////
// write
size_t sis_disk_io_write_log(s_sis_disk_ctrl *cls_, void *in_, size_t ilen_);
// read
int sis_disk_io_sub_log(s_sis_disk_ctrl *cls_, void *cb_);

///////////////////////////
//  sis_disk.io.sic.c
///////////////////////////
// 读取时 可由外部通过 s_sis_disk_reader 直接访问数据
////////////////////////////////////////////
// write
size_t sis_disk_io_write_sic_work(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_);

int sis_disk_io_write_sic_start(s_sis_disk_ctrl *cls_);
int sis_disk_io_write_sic(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_);
int sis_disk_io_write_sic_stop(s_sis_disk_ctrl *cls_);
size_t sis_disk_io_write_sic_widx(s_sis_disk_ctrl *cls_);
// read
int sis_disk_io_sub_sic(s_sis_disk_ctrl *cls_, const char *subkeys_, const char *subsdbs_,
                    s_sis_msec_pair *search_, void *cb_);
// 读所有索引信息
int sis_disk_io_read_sic_widx(s_sis_disk_ctrl *cls_);

///////////////////////////
//  sis_disk.io.sdb.c
///////////////////////////
// 读取时 不能被外部直接访问 必须由高层获取数据处理后返回
////////////////////////////////////////////
// write
size_t sis_disk_io_write_noidx(s_sis_disk_files *files_, s_sis_disk_wcatch *wcatch_);
size_t sis_disk_io_write_sdb_work(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_);

int sis_disk_io_write_one(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, void *in_, size_t ilen_);
int sis_disk_io_write_mul(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_pointer_list *ilist_);

int sis_disk_io_write_non(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_);
int sis_disk_io_write_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_);

int sis_disk_io_write_map(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, int style, int idate, int moved);

size_t sis_disk_io_write_sdb_widx(s_sis_disk_ctrl *cls_);

void sis_disk_io_write_widx_tail(s_sis_disk_ctrl *cls_);

// read
// 只支持获取一个键值的数据
int sis_disk_io_read_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_);
// 订阅整个文件 通常用于pack
int sis_disk_io_sub_sdb(s_sis_disk_ctrl *cls_, void *cb_);
// 读所有索引信息
int sis_disk_io_read_sdb_widx(s_sis_disk_ctrl *cls_);
// 读取map文件信息
int sis_disk_io_read_sdb_map(s_sis_disk_ctrl *cls_);

///////////////////////////
//  sis_disk.io.sno.c
///////////////////////////
// 读取时 可由外部通过 s_sis_disk_reader 直接访问数据
////////////////////////////////////////////
// write
#define sis_disk_io_write_sno_work sis_disk_io_write_sdb_work

int sis_disk_io_write_sno_start(s_sis_disk_ctrl *cls_);
int sis_disk_io_write_sno(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_);
int sis_disk_io_write_sno_stop(s_sis_disk_ctrl *cls_);
size_t sis_disk_io_write_sno_widx(s_sis_disk_ctrl *cls_);
// read
// 只支持获取一个键值的数据
#define sis_disk_io_read_sno sis_disk_io_read_sdb
// 订阅文件 中符合条件的数据 返回真实数据 只订阅SNO单文件时有效
int sis_disk_io_sub_sno(s_sis_disk_ctrl *cls_, const char *subkeys_, const char *subsdbs_, s_sis_msec_pair *search_, void *cb_);
// 读所有索引信息
int sis_disk_io_read_sno_widx(s_sis_disk_ctrl *cls_);

///////////////////////////
//  sis_disk.io.sno.c
///////////////////////////

s_sis_disk_sno_rctrl *sis_disk_sno_rctrl_create(int pagenums_);
void sis_disk_sno_rctrl_destroy(s_sis_disk_sno_rctrl *);
// 清理所有数据
void sis_disk_sno_rctrl_clear(s_sis_disk_sno_rctrl *);

// 放入一个标准块 返回实际的数量
int sis_disk_sno_rctrl_push(s_sis_disk_sno_rctrl *, const char *kname_, const char *sname_, int dbsize_, s_sis_memory *imem_);
// 从头开始读数据
void sis_disk_sno_rctrl_init(s_sis_disk_sno_rctrl *);
// 弹出一条记录 只是移动 cursor 指针 
s_sis_db_chars *sis_disk_sno_rctrl_rpop(s_sis_disk_sno_rctrl *);

#endif

