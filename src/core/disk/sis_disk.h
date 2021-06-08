#ifndef _SIS_DISK_H
#define _SIS_DISK_H

#include "os_fork.h"
#include "sis_memory.h"
#include "sis_dynamic.h"
#include "sis_obj.h"
#include "sis_file.h"
#include "sis_math.h"
#include "sis_time.h"
#include "sis_snappy.h"
#include "sis_db.h"

#define SIS_DISK_SDB_VER   3    // 数据结构版本

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
// 重建索引规则，同一个key，一旦后面的key时间和前面的数据时间有交叉（不包括粘连），前面的块就直接放弃
// 安全起见，对生成的序列化数据，按接收顺序每5分钟保存一个块，方便数据回放
////////////////////////////////////////////////////
// 一共5中文件格式
// 1. 流式文件 无时序 无索引
// 				此类文件只能追加数据
#define  SIS_DISK_TYPE_STREAM      0  
////////////////////////////////////////////////////
// 2. 实时tick文件 一般作为按顺序实时落盘的临时文件，以毫秒为单位 以天为一个文件
// key - struct(n) 有时序 无索引 - 逐笔顺序写入
// 				此类文件只能追加数据，用于实时数据的写盘记录 通常会在一天结束后合并键值 转换为历史数据格式
// 不同结构的组合体放在不同目录下 同一组合结构体放在一起
#define  SIS_DISK_TYPE_LOG         1 
////////////////////////////////////////////////////
// 3. 历史tick文件 按key+sdb分类存储 以毫秒为单位 以天为一个文件
// key - struct(n) 有时序 数据为结构体 N条记录 有索引 
// 				此类文件同一个key+sdb只有一个包 同一天的每条数据有顺序编号
//              和 TICK 的区别 单个股票各种数据各一个块 根据 TICK 的顺序自动增加顺序号
// 由于毫秒线按日命名，按日存储，一天的数据放一个文件中
// *** 此类文件会开很大的缓存 ***
// 解决方法：以13分钟为一个时间切片，超过13分钟全部写盘一次，并记录13分钟的位置，方便全部提取时排序
// idx增加一个字节标记 0..255 读取的时候排序 
// 单个股票提取一天最多提取19次，这样可以保证内存使用尽量的少
// 13取值随机，但尽量不超过一天255块 也就是不得低于 6 分钟
#define  SIS_DISK_TYPE_SNO         2  
////////////////////////////////////////////////////
// 4. 一键值多数据块文件 每个键值对应一组[start,stop]表示数据的开始结束时间
// 一个key多个数据块，新的时间区间如果和老的时间区间有重合 直接覆盖老的数据 (暂时支持到这里)
// key - struct(n) 有时序 数据为结构体 N条记录 有索引 
// 				此类文件一次传入一整块数据 传入后自动取头记录和尾记录的时间做为索引
// 若单个文件过大 就会生成新的文件.1.2.3 但索引不变，
// 索引文件对每个key需要保存一个hot字节 0..255 每次写盘时如果没有增加就减 1  
// 当索引过大时，会自动取最近更新的数据，除非该key只有一个数据包，其他历史数据放在.1为名的索引文件中
#define  SIS_DISK_TYPE_SDB         5 
////////////////////////////////////////////////////
// 5. 一键值一个数据块文件 每个键值对应一组[start,stop]表示数据的开始结束时间
// 一个key一个数据块，是较为常用的文件格式 键值一样就覆盖旧的
// 主要存分钟线，委托，成交等，是较为常用的文件格式 如果没有时间，索引为[0,0]
// **** 普通的无时序kv结构也用这种类型存储 仅仅是块的标记不同 
// key - struct(n) 有时序 数据为结构体 N条记录 有索引 
// 				此类文件可修改，相同日期后面写入的覆盖前面的
// 若单个文件过大 就会生成新的文件.1.2.3 但索引不变，
// 当索引过大时，会自动取最近更新的数据，除非该key只有一个数据包，其他历史数据放在.1为名的索引文件中
// #define  SIS_DISK_TYPE_SDB_ONCE     6   // 这个和FLOW的区别仅仅是时间字段为日期

//////////// 以下是索引文件定义 ////////////////
// 文件记录格式为 klen(dsize)+key+slen(dsize)+sdb+blocks(dsize)
//         +[active(1)+kdict(dsize)+sdict(dsize)+fidx(dsize)+offset(dsize)+size(dsize)+start(dsize)+stop(dsize)]...
// 直接用字符串不用id是为了适应其他没有字典的文件格式
// kdict 为 当前key对应的字典单元 sdict 当前sdb对应的字典单元
// fidx 为 文件序号 数据过大，会分块存储 此为编号
// start 为开始时间 可以为秒 年 毫秒 但同一组数据该值最好保持一致
// stop 为 start 的时间偏移
// 索引文件为一次性写入 文件名以默认规则来命名 避免索引文件过大
// 用户检索时只需要打开对应时段的索引即可
// 增加一个字节标记 active 1..255 表示活跃度 整理历史数据和加载内存时可参考 
// 刚写入时 = 1 第二次写索引时 如果没有操作就 = 0 
// 255 表示经常读写 
// 最近一次写索引 如果该键值没有被读写就减 1
// ---------- //
// 时间为毫秒
// msec索引名为 20200120.idx
// 读取该文件时，对N个key先全部读取第一个块，然后按sno排序，广播出去，再读第二个块....
// 内存加载仅仅加载当天的数据，过24点即从内存清除昨日数据， 
#define  SIS_DISK_TYPE_SNO_IDX    10 
// 内存加载时仅仅加载最近活跃的数据 
// dict 针对key和sdb的字典表不同 dict 表示不同的字典表 读取时对应dict的字典表为当前的数据
#define  SIS_DISK_TYPE_SDB_IDX    11

#define  SIS_DISK_SCALE_NONE       0  // 和时间无关,仅仅为kv结构 不设置为0 ，是当用户传入0时，不处理
#define  SIS_DISK_SCALE_MSEC       1  // 毫秒  多条记录 
#define  SIS_DISK_SCALE_MINU       2  // 1分钟  8 位 201002381425  14:25分
#define  SIS_DISK_SCALE_DATE       3  // 日期   多条记录      20100238

#define  SIS_DISK_LOG_CHAR      "log"
#define  SIS_DISK_SNO_CHAR      "sno"
#define  SIS_DISK_SDB_CHAR      "sdb"
#define  SIS_DISK_IDX_CHAR      "idx"
#define  SIS_DISK_TMP_CHAR      "tmp"

#define  SIS_DISK_SIGN_KEY      "_keys_"   // 用于索引文件的关键字
#define  SIS_DISK_SIGN_SDB      "_sdbs_"   // 用于索引文件的关键字

#define  SIS_DISK_CMD_NO_IDX             1
#define  SIS_DISK_CMD_OK                 0
#define  SIS_DISK_CMD_NO_EXISTS        -10
#define  SIS_DISK_CMD_NO_EXISTS_IDX    -11
#define  SIS_DISK_CMD_NO_IGNORE       -100
#define  SIS_DISK_CMD_NO_INIT         -101
#define  SIS_DISK_CMD_NO_VAILD        -102
#define  SIS_DISK_CMD_NO_OPEN         -103
#define  SIS_DISK_CMD_NO_OPEN_IDX     -104


// #define  SIS_DISK_MAXLEN_FILE      0x7FFFFFFF  // 2G - 2147483647 
// #define  SIS_DISK_MAXLEN_INDEX     0x07FFFFFF  // 2G -  134217727
#define  SIS_DISK_MAXLEN_FILE      0xFFFFFFFF  //   4G  数据文件专用
#define  SIS_DISK_MAXLEN_INDEX     0x08FFFFFF  // 150M  索引文件专用
#define  SIS_DISK_MAXLEN_MINPAGE   0x000FFFFF  //   1M 索引文件块大小
#define  SIS_DISK_MAXLEN_MIDPAGE   0x00FFFFFF  //  16M 实时文件块大小
#define  SIS_DISK_MAXLEN_MAXPAGE   0x1FFFFFFF  // 536M 实时文件块大小

#pragma pack(push,1)

////////////////////////////////////////////////
// disk_sdb 流式文件索引结构体，只允许顺序增加数据
//        多种结构体按时间序列混合写入，不允许插入修改，只能append
////////////////////////////////////////////////
// 定义一个动态的长度 dsize 表示所有数量类数据
// 0 - 250 一个字节表示
// 第一个字节为251, 后面跟1个字节X 250 + X为实际数量
// 第一个字节为252, 后面跟2个字节X 250 + X为实际数量
// 第一个字节为253, 后面跟4个字节X X为实际数量

#define SIS_DISK_BLOCK_HEAD  \
		uint8 fin : 1;    \
		uint8 zip : 1;    \
		uint8 hid : 6;    \
// 后面紧跟 len(dsize) 为数据区长度 根据实际数据长度可以得到数据有多少条
// hid 的定义如下 总共有64中头描述
// 不考虑key sdb 异构问题
#define  SIS_DISK_HID_NONE        0x0  // 表示该块为删除标记

#define  SIS_DISK_HID_HEAD        0x1  // 表示为头描述
// 键值 - {}中放置游离的个性属性，仅仅属于该key  主要存储 价格小数位数 decimal places "dp" = 2
#define  SIS_DISK_HID_DICT_KEY    0x2  // key的描述 k1:{},k2:{} 以,隔开,可多个key 根据顺序得到索引
// 键值的通用结构属性 同一个文件不允许有同名的 sdb
#define  SIS_DISK_HID_DICT_SDB    0x3  // sdb的结构描述 x:{},y:{},z:{} 可以多个sdb 根据顺序得到索引
// 增加不带序列号的数据块
#define  SIS_DISK_HID_MSG_LOG     0x4  // [kid(dsize)+dbid(dsize)+datastream] 多组 (一个key+1条记录)
// 增加序列号的数据块 默认多个数据
#define  SIS_DISK_HID_MSG_SNO     0x5  // kid(dsize)+dbid(dsize)+[sno(dsize)+datastream] 一个key+N条记录
// 增加序列号的数据块结束符 收到此消息排序后发送信息
#define  SIS_DISK_HID_SNO_END     0x6  // pages(dsize)
// 增加不带序列号的数据块
#define  SIS_DISK_HID_MSG_SDB     0x8  // kid(dsize)+dbid(dsize)+[datastream] 一个key+N条记录
// 所有的sdb文件 最多就4种格式 
// 一种是有key和sdb字典的 
// 一种是只有key字典的 
// 一种是只有sdb字典的 
// 最后一种是没有字典的
#define  SIS_DISK_HID_MSG_KEY     0x9  // kid(dsize)+vlen(dsize)+value
// KDB结构数据 前面需要有 SIS_DISK_HID_DICT_SDB 结构说明
#define  SIS_DISK_HID_MSG_KDB     0xA  // klen(dsize)+key+dbid(dsize)+datastream
#define  SIS_DISK_HID_MSG_ANY     0xB  // klen(dsize)+key+vlen(dsize)+value 一个任意长度字符串
// #define  SIS_DISK_HID_MSG_LIST    0xA  // klen(dsize)+key+[vlen(dsize)+value] count个不定长字符串
// #define  SIS_DISK_HID_MSG_JSON    0xB  // klen(dsize)+key+vlen(dsize)+value 直接存放kv数据 1个

// 增加通用流式数据块, 数据可能是多个标准块的组合，也可以是任意数据
#define  SIS_DISK_HID_STREAM      0x10  // datastream 纯数据 不能被索引

// O为中间代理层 S为源头 C为接收
#define  SIS_NET_HID_PRIVATE      0x20  // SO 或 CO 的私有交互
#define  SIS_NET_HID_SUB          0x21  // C-->O 向O订阅需要的数据 以便O按需分配
#define  SIS_NET_HID_PUB          0x22  // S-->O O缓存定量数据 数据随到随分发给 C 
#define  SIS_NET_HID_GET          0x23  // C-->O 只能获取S端set的数据
#define  SIS_NET_HID_SET          0x24  // S-->O O保存和更新该数据，等待C的get

// 增加通用索引流式数据块, 数据可能是多个标准块的组合，也可以是任意数据
// 读取索引文件必须先加载dict模块 *** 特别重要 ***
// 格式为 klen(dsize)+key+dblen(dsize)+dbn+blocks(dsize)
//         +[active(1)+kdict(dsize)+sdict(dsize)+fidx(dsize)+offset(dsize)+size(dsize)+start(dsize)+stop(dsize)]...
#define  SIS_DISK_HID_INDEX_MSG   0x30 
// 格式为 klen(dsize)+key+dblen(dsize)+dbn+blocks(dsize)
//         +[active(1)+kdict(dsize)+sdict(dsize)+fidx(dsize)+offset(dsize)+size(dsize)+start(dsize)+page(dsize)]...
#define  SIS_DISK_HID_INDEX_SNO   0x31
// 一个文件可能会出现多次字典信息写入 每次写入都会先写字典表，读取时也是先读字典 后续索引才能一一匹配
// 格式为 blocks(dsize)+[fidx(dsize)+offset(dsize)+size(dsize)]...
#define  SIS_DISK_HID_INDEX_KEY   SIS_DISK_HID_DICT_KEY
// 格式为 blocks(dsize)+[fidx(dsize)+offset(dsize)+size(dsize)]...
#define  SIS_DISK_HID_INDEX_SDB   SIS_DISK_HID_DICT_SDB

#define  SIS_DISK_HID_TAIL        0x3F  // 结束块标记

// 定义压缩方法
#define  SIS_DISK_ZIP_NONE        0
#define  SIS_DISK_ZIP_SNAPPY      1

// 位域定义长度必须和类型匹配 否则长度不对
typedef struct s_sis_disk_main_head {
	SIS_DISK_BLOCK_HEAD
    char     sign[3];        // 标志符 "SIS" (4)
    uint16   version       :   5; // 版本号 最多 32 个版本
    uint16   style         :   4; // 文件类型 最多 16 种类型
    uint16   compress      :   3; // 压缩方法 0 - 不压缩 1 - 各个结构自己的压缩 2 - 3 其他
    uint16   index         :   1; // [工程文件专用] 1 有索引 0 没有索引文件 
    uint16   nouse         :   3; // 开关类保留 (9)
    uint8    switchs[3];          // 开关类保留 (9)
    uint8    workers;             // [索引专用] 当前关联的工作文件总数 编号大的比前一个文件大一倍 2^(n-1)
    uint32   wtime;               // 是数据的日期 不是创建的时间 统一秒 
    uint16   other;               // 
}s_sis_disk_main_head;            // 16个字节头 128

// 对于sno文件每个索引都有一个page，在重建索引时根据sno的结束块来确定page的数量
//        获取数据时同样的page取完数据后，再排序送出数据
// 对于字典的问题 情况复杂 

// 索引或者工作文件的总数只有在关闭文件时才能确定 所以尾部保存总的文件数
typedef struct s_sis_disk_main_tail {
	SIS_DISK_BLOCK_HEAD
    uint8    count;        // 当前关联文件总数 编号大的比前一个文件大一倍 2^(n-1)
    char     crc[16];      // 数据检验
}s_sis_disk_main_tail;     // 18个字节尾

// 定义读取缓存最小字节
// 除非到了文件尾，否则最小的in大小不得低于16个字节 这样就不用判断 dsize 的尺寸了
#define  SIS_DISK_MIN_BUFFER   sizeof(s_sis_disk_main_tail)

// 公用头 1 个字节

typedef struct s_sis_disk_head {
    SIS_DISK_BLOCK_HEAD
}s_sis_disk_head;


typedef struct s_sis_disk_callback {
    void *source; // 用户需要传递的数据
    // 通知文件已经打开或网络已经链接
    // void (*cb_open)(void *); //第一个参数就是用户传递进来的指针
    // 通知文件开始读取了或订阅开始
    void (*cb_begin)(void *, msec_t); // 第一个参数就是用户传递进来的指针, 第二个为开始时间第一个时间
    // 返回key定义
    void (*cb_key)(void *, void *key_, size_t);  // 返回key信息
    // 返回sdb定义 key属性的定义
    void (*cb_sdb)(void *, void *sdb_, size_t);  // 返回属性信息
    // 返回读到的数据(解压后)
    void (*cb_read)(void *, const char *key_, const char *sdb_, void *out_, size_t olen_); // 头描述 实际返回的数据区和大小
    // 返回读到的数据(解压后)
    //void (*cb_onebyone)(void *, int kidx_, int sidx_, void *out_, size_t olen_); 
    // 通知文件结束了
    void (*cb_end)(void *, msec_t); //第一个参数就是用户传递进来的指针
    // 通知文件已经关闭或网络已经断开
    // void (*cb_close)(void *); //第一个参数就是用户传递进来的指针
}s_sis_disk_callback;

//////////////////////////////////////////////////////
// 字典表 多数文件的信息都从这里找到对应关系
//////////////////////////////////////////////////////
typedef struct s_sis_disk_dict_unit {
    uint8              writed; // 是否已经写盘
	s_sis_dynamic_db  *db;     // 如果是数据结构 表示结构的长度 若没有为NULL
}s_sis_disk_dict_unit;

// key 和 sdb 的基本信息
typedef struct s_sis_disk_dict {
    int                 index;  // 索引号
	s_sis_object       *name;   // 名字
    s_sis_pointer_list *units;  // 信息
}s_sis_disk_dict;

//////////////////////////////////////////////////////
// 索引表 只有具备索引功能的文件 必须先加载字典 再加载索引 
// 索引文件写入时要先写字典信息
//////////////////////////////////////////////////////

typedef struct s_sis_disk_index_unit
{
    // uint8             hid;    // 索引没有hid是因为定位读取时可从文件中获取
    uint8             active; // 活跃记数 据此可判断是否存放入冷文件中 sno文件中 按从小到大排列
    uint8             kdict;  // 键值字典对应的索引 - 默认为一个文件同一个键值不超过255次改变 0 表示没有key
    uint8             sdict;  // 结构字典对应的索引 - 默认为一个文件同一个键值不超过255次改变 0 表示没有sdb
    uint8             fidx;   // 在哪个文件中 文件序号 4G*4G = 16P数据 文件名.1
    size_t            offset; // 文件偏移位置
    size_t            size;   // 数据长度
    msec_t            start;  // 开始时间 不同时间尺度 时间单位不同 秒 秒 日
    msec_t            stop;   // 结束时间
    uint16            ipage;  // sno 文件专用 标记数 按从小到大排列 收到一个结束标记就增加1
} s_sis_disk_index_unit;

#define SIS_DISK_IDX_TYPE_NONE  '0'
#define SIS_DISK_IDX_TYPE_INT   SIS_DYNAMIC_TYPE_INT
#define SIS_DISK_IDX_TYPE_MSEC  SIS_DYNAMIC_TYPE_TICK  
#define SIS_DISK_IDX_TYPE_SEC   SIS_DYNAMIC_TYPE_SEC   
#define SIS_DISK_IDX_TYPE_MIN   SIS_DYNAMIC_TYPE_MINU 
#define SIS_DISK_IDX_TYPE_DAY   SIS_DYNAMIC_TYPE_DATE 

// 索引依赖于字典表 索引文件总是会把字典表写在前面 读取字典后 后续数据才能正确获取和读取 
typedef struct s_sis_disk_index {
    uint32              cursor;    // 当前读到第几条记录
    s_sis_object       *key;       // 可能多次引用 - 指向dict表 因为可能文件没有索引
    s_sis_object       *sdb;       // 可能多次引用 - 指向dict表
    s_sis_struct_list  *index;     // 索引的列表 s_sis_disk_index_unit
}s_sis_disk_index;

// 专门用来缓存sno数据的
typedef struct s_sis_disk_rcatch {
    int                rsize;    // 当前的记录长度
    s_sis_disk_dict   *kdict;
    s_sis_disk_dict   *sdict;
    s_sis_memory      *omem;    // 数据缓存指针 可能多条记录 已经移动到数据区
}s_sis_disk_rcatch;

typedef struct s_sis_disk_rsno_unit {
    s_sis_object      *keyn;     // 可能多次引用 - 指向dict表 因为可能文件没有索引
    s_sis_object      *sdbn;     // 可能多次引用 - 指向dict表
    int                size;     // 当前的数据长度
    char*              optr;     // 指向omem数据指针
}s_sis_disk_rsno_unit;

typedef struct s_sis_disk_rsno {
    uint64                sno;     // 用来排序的标志号
    s_sis_disk_rsno_unit *unit;    
}s_sis_disk_rsno;
// 通用的读取数据函数 如果有字典数据key就用字典的信息 
// 如果没有字典信息就自己建立一个
typedef struct s_sis_disk_wcatch {
    s_sis_object         *key;      // 可能多次引用 - 指向dict表 因为可能文件没有索引
    s_sis_object         *sdb;      // 可能多次引用 - 指向dict表
    s_sis_memory         *memory;   // 可能数据缓存指针
    s_sis_disk_index_unit winfo;    // 写入时的索引信息
}s_sis_disk_wcatch;


typedef struct s_sis_disk_reader {
    uint8                issub;
    uint8                isone;   // 是否一次性输出
////////// ask  //////////
    s_sis_msec_pair      search_int;

    s_sis_msec_pair      search_day;
    s_sis_msec_pair      search_min;
    s_sis_msec_pair      search_sec;
    s_sis_msec_pair      search_sno;

    s_sis_sds            keys;      // key name "*" 为全部都要 或者 k1,k2,k3
	s_sis_sds            sdbs;      // sdb name "*" 为全部都要 或者 k1,k2,k3

    s_sis_disk_callback *callback;  // 读文件的回调
}s_sis_disk_reader;

// 文件状态
#define  SIS_DISK_STATUS_CLOSED    0x0
#define  SIS_DISK_STATUS_OPENED    0x1
#define  SIS_DISK_STATUS_RDOPEN    0x2  // 文件只读打开 
// #define  SIS_DISK_STATUS_RDREAD    0x2  // 文件只读并且已经读取相关基础信息 

#define  SIS_DISK_STATUS_APPEND    0x10  // 文件可写打开 从文件尾写入数据
#define  SIS_DISK_STATUS_CREATE    0x20  // 新文件打开 从第一个文件写入
#define  SIS_DISK_STATUS_NOSTOP    0x40  // 文件写打开 已经写了文件头 但未写结束标记
#define  SIS_DISK_STATUS_CHANGE    0x80  // 文件有变化 文件有除了文件头的更新

#define  SIS_DISK_NAME_LEN  255
typedef struct s_sis_files_unit {
    char                  fn[SIS_DISK_NAME_LEN];    // 文件名
    s_sis_handle          fp;         // 文件句柄
    size_t                offset;     // 当前偏移位置
    uint8                 status;     // 文件状态
    s_sis_memory         *wcatch;     // 必须加写缓存 如果写入块太小 写盘耗时会成倍增加
} s_sis_files_unit;

#define  SIS_DISK_ACCESS_NOLOAD    0  // 没有这类文件 
#define  SIS_DISK_ACCESS_RDONLY    1  // 文件只读打开 
#define  SIS_DISK_ACCESS_APPEND    2  // 文件可写打开 从文件尾写入数据
#define  SIS_DISK_ACCESS_CREATE    3  // 新文件打开 从第一个文件写入
// 多文件管理类
typedef struct s_sis_files {
    uint8                 access;     // 只有三种状态
    s_sis_memory         *zip_memory;     // 压缩用缓存区
    s_sis_disk_main_head  main_head;      // 文件头
    s_sis_disk_main_tail  main_tail;      // 文件尾
    char                  cur_name[SIS_DISK_NAME_LEN];  // 工作文件名
    uint8                 cur_unit;       // 当前是哪个文件
	size_t                max_file_size;  // 文件最大尺寸 大于就换文件
	size_t                max_page_size;  // 块最大尺寸 大于就换文件
	s_sis_pointer_list   *lists;          // s_sis_files_unit 文件信息 有几个就打开几个
} s_sis_files;

typedef struct s_sis_disk_class {
    s_sis_mutex_t        mutex;  // 多线程写入时需要加锁，不然报错
    bool                 isinit; // 必须初始化后才能使用
    // uint8                isstop;  // 读文件期间是否停止 如果为 1 立即退出
    bool                 isstop; // 是否中断读取
    s_sis_disk_reader   *reader; // 读取数据时保存的指针


    int   style;
    char  fpath[255];
    char  fname[255];
    int   wtime;   // 数据时间

    int   status;
    // 字典表应该是这样的
    // 第一次写入的为全字典 后续写的如果内容一样 不增加 如果新的数据字典为新的就直接写入
    // 注意 新写入的数据总是以当前key值最大的索引为参考 
    // 如果内容不同 在同名下增加 一个属性内容 并做好索引 然后把新增的信息字典 写盘 不用写全部字典
    // 对已经存在的文件 打开文件时需要加载全部的字典表 
    // 读取数据时如果根据 dict 不同加载不同信息 但 name 一致  
    // 这样即使前后结构不同 通过格式转换 老的数据结构也可以解析成新的结构返回给前端
    // 通过pack可以把旧结构数据转成新的数据结构 字典表也就只有一个完整的了 
	s_sis_map_list      *keys;   // s_sis_disk_dict
	s_sis_map_list      *sdbs;   // s_sis_disk_dict

    s_sis_disk_wcatch  *src_wcatch;     // 需要转入公共区的 wcatch
  
    s_sis_map_list     *index_infos;      // 以key.sdb 为索引的 s_sis_struct_list 列表 存储 s_sis_disk_index

    size_t              sno_size;       // SIS_DISK_HID_MSG_SNO 当前累计数据量
    size_t              sno_series;     // SIS_DISK_HID_MSG_SNO 当前最新计数
    uint32              sno_pages;      // 页面计数
    s_sis_map_list     *sno_wcatch;     // 从实时文件转盘后文件 临时存放数据 s_sis_disk_wcatch
    s_sis_pointer_list *sno_rcatch;     // 分块读取时的缓存 s_sis_disk_rcatch 需要根据sno进行快速排序

    s_sis_files        *work_fps;      // 工作文件组
    s_sis_files        *index_fps;     // 索引文件组

} s_sis_disk_class;

#pragma pack(pop)

///////////////////////////
//  s_sis_disk_index
///////////////////////////
s_sis_disk_index *sis_disk_index_create(s_sis_object *key_, s_sis_object *sdb_);
void sis_disk_index_destroy(void *in_);
int sis_disk_index_set_unit(s_sis_disk_index *cls_, s_sis_disk_index_unit *unit_);
s_sis_disk_index_unit *sis_disk_index_get_unit(s_sis_disk_index *cls_, int index_);

///////////////////////////
//  s_sis_disk_dict
///////////////////////////
s_sis_disk_dict *sis_disk_dict_create(const char *name_, bool iswrite_, s_sis_dynamic_db * db_);
void sis_disk_dict_destroy(void *in_);

void sis_disk_dict_clear_units(s_sis_disk_dict *dict_);

s_sis_disk_dict_unit *sis_disk_dict_last(s_sis_disk_dict *);
s_sis_disk_dict_unit *sis_disk_dict_get(s_sis_disk_dict *, int);
// 传递一个 dict 单元 返回值 = 0 表示没有新增 1.n表示新增
int sis_disk_dict_set(s_sis_disk_dict *, bool iswrite_, s_sis_dynamic_db * db_); 

///////////////////////////
//  s_sis_disk_rcatch
///////////////////////////
// 先把磁盘读取的数据 保存在列表中
s_sis_disk_rcatch *sis_disk_rcatch_create(int size_, s_sis_memory *omem_); 
void sis_disk_rcatch_destroy(void *in_);

///////////////////////////
//  s_sis_disk_wcatch
///////////////////////////
s_sis_disk_wcatch *sis_disk_wcatch_create(s_sis_object *key_, s_sis_object *sdb_); 
void sis_disk_wcatch_destroy(void *in_);
void sis_disk_wcatch_clear(s_sis_disk_wcatch *in_);

///////////////////////////
//  s_sis_disk_reader
///////////////////////////
s_sis_disk_reader *sis_disk_reader_create(s_sis_disk_callback *); 
void sis_disk_reader_destroy(void *in_);
void sis_disk_reader_clear(s_sis_disk_reader *in_);
void sis_disk_reader_set_key(s_sis_disk_reader *in_, const char *);
void sis_disk_reader_set_sdb(s_sis_disk_reader *in_, const char *);
void sis_disk_reader_set_stime(s_sis_disk_reader *cls_, int scale_, msec_t start_, msec_t stop_);
void sis_disk_reader_get_stime(s_sis_disk_reader *cls_, int scale_, s_sis_msec_pair *pair_);

int  sis_disk_get_idx_style(s_sis_disk_class *cls_,const char *sdb_, s_sis_disk_index_unit *iunit_);

///////////////////////////
//  s_sis_files
///////////////////////////
s_sis_files *sis_files_create();
void sis_files_destroy(void *);

void sis_files_clear(s_sis_files *);

void sis_files_init(s_sis_files *cls_, char *fn_);

int sis_files_inc_unit(s_sis_files *cls_);

int sis_files_delete(s_sis_files *cls_);

int sis_files_open(s_sis_files *cls_, int access_);
void sis_files_close(s_sis_files *cls_);

bool sis_files_seek(s_sis_files *cls_);

size_t sis_files_offset(s_sis_files *cls_);

size_t sis_files_write_sync(s_sis_files *cls_);

size_t sis_files_write(s_sis_files *cls_, int hid_, s_sis_disk_wcatch *wcatch_);


// // 移动指针到第几个文件 某个位置
// long long sis_files_seek(s_sis_files *, int fidx_, size_t offset_, int where_);
// // 从当前位置读取数据

// 回调返回为 -1 表示已经没有读者了,停止继续读文件
typedef int (cb_sis_files_read)(void *, s_sis_disk_head *, s_sis_memory *);
// 全文读取 数据后通过回调输出数据
size_t sis_files_read_fulltext(s_sis_files *cls_, void *, cb_sis_files_read *callback);
// 定位读去某一个块
size_t sis_files_read(s_sis_files *, int fidx_, size_t offset_, size_t size_, 
    uint8 *hid_, s_sis_memory *out_);

///////////////////////////
//  s_sis_disk_class
///////////////////////////
// 根据传入的参数创建目录或确定文件名(某些类型的文件名是系统默认的，用户只能指定目录) 
// 路径和文件名分开是为了根据不同种类增加中间目录 文件过大时自动分割子文件 
s_sis_disk_class *sis_disk_class_create();
void sis_disk_class_destroy(s_sis_disk_class *cls_);

int sis_disk_class_init(s_sis_disk_class *cls_, int style_, const char *fpath_, const char *fname_, int wtime_);

// 关闭文件
void sis_disk_class_clear(s_sis_disk_class *cls_);
// 设置文件大小
void sis_disk_class_set_size(s_sis_disk_class *,size_t fsize_, size_t psize_);

// 得到key的索引
int sis_disk_class_get_keyi(s_sis_disk_class *cls_, const char *key_);
// 得到sdb的索引
int sis_disk_class_get_sdbi(s_sis_disk_class *cls_, const char *sdb_);
// 得到key的名字
const char *sis_disk_class_get_keyn(s_sis_disk_class *cls_, int keyi_);
// 得到sdb的名字
const char *sis_disk_class_get_sdbn(s_sis_disk_class *cls_, int sdbi_);
// 增加 key
int sis_disk_class_add_key(s_sis_disk_class *cls_, const char *key_);
// 设置 key
int sis_disk_class_set_key(s_sis_disk_class *cls_, bool iswrite_, const char *in_, size_t ilen_);
// 设置结构体
int sis_disk_class_set_sdb(s_sis_disk_class *cls_, bool iswrite_, const char *in_, size_t ilen_);
// 检查文件是否有效
int sis_disk_file_valid(s_sis_disk_class *cls_);
// 检查索引
int sis_disk_file_valid_idx(s_sis_disk_class *cls_);

// 读写数据
int sis_disk_file_write_start(s_sis_disk_class *cls_);
//写文件尾 只读文件不做 
int sis_disk_file_write_stop(s_sis_disk_class *cls_);

// 只读数据
int sis_disk_file_read_start(s_sis_disk_class *cls_);
//写文件尾 只读文件不做 
int sis_disk_file_read_stop(s_sis_disk_class *cls_);

///////////////////////////
//  index option
///////////////////////////

s_sis_disk_index *sis_disk_index_get(s_sis_map_list *map_, s_sis_object *key_, s_sis_object *sdb_);

// 根据 reader_ 和索引生成 需要查询的数据列表
// key sdb 为 * 表示全部查 
// sdb 为 NULL 表示只有key起作用 
// list_ 为 s_sis_disk_index 结构队列 其中 index 仅仅为指向索引的指针 销毁时要注意
int sis_reader_sub_filters(s_sis_disk_class *cls_, s_sis_disk_reader *reader_, s_sis_pointer_list *list_);

///////////////////////////
//  write
///////////////////////////
s_sis_sds sis_disk_file_get_keys(s_sis_disk_class *cls_, bool onlyincr_);
s_sis_sds sis_disk_file_get_sdbs(s_sis_disk_class *cls_, bool onlyincr_);

size_t sis_disk_file_write_key_dict(s_sis_disk_class *cls_);
size_t sis_disk_file_write_sdb_dict(s_sis_disk_class *cls_);

// 写入可能剩余的数据
size_t sis_disk_file_write_surplus(s_sis_disk_class *cls_);

// 重新生成索引文件，需要读取原始文件
size_t sis_disk_file_write_index(s_sis_disk_class *cls_);

// 写入 stream 接口
size_t sis_disk_file_write_stream(s_sis_disk_class *cls_, void *in_, size_t ilen_);
// 仅仅对 sdb 有效 必须有 key 和 sdb 的字典 否则无效
size_t sis_disk_file_write_sdbi(s_sis_disk_class *cls_,
                                int keyi_, int sdbi_, void *in_, size_t ilen_);
// 通用写入接口
size_t sis_disk_file_write_sdb(s_sis_disk_class *cls_, 
    const char *key_, const char *sdb_, void *in_, size_t ilen_);

size_t sis_disk_file_write_kdb(s_sis_disk_class *cls_,
                                const char *key_, const char *sdb_, void *in_, size_t ilen_);

size_t sis_disk_file_write_key(s_sis_disk_class *cls_,
                                const char *key_, void *in_, size_t ilen_);

size_t sis_disk_file_write_any(s_sis_disk_class *cls_,
                                const char *key_, void *in_, size_t ilen_);

void sis_disk_file_delete(s_sis_disk_class *cls_);

void sis_disk_file_move(s_sis_disk_class *cls_, const char *path_);
///////////////////////////
//  read
///////////////////////////
// 读取索引后加载字典信息
int sis_disk_file_read_dict(s_sis_disk_class *cls_);
// 加载索引文件到内存 方便检索
int cb_sis_disk_file_read_index(void *cls_, s_sis_disk_head *, s_sis_memory *);

// 以流的方式读取文件 从文件中一条一条发出 
// 必须是同一个时间尺度的数据 否则无效
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_file_read_sub(s_sis_disk_class *cls_, s_sis_disk_reader *reader_);

// 直接获取数据 需要索引 只能获取单一key的数据 可指定时间段 k1 db1
s_sis_object *sis_disk_file_read_get_obj(s_sis_disk_class *cls_, s_sis_disk_reader *reader_);

///////////////////////////
//  pack
///////////////////////////
size_t sis_disk_file_pack(s_sis_disk_class *src_, s_sis_disk_class *des_);

#endif

