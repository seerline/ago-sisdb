
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

#ifndef _SISDB_FMAP_H
#define _SISDB_FMAP_H

#include "sis_core.h"
#include "sis_math.h"
#include "sis_malloc.h"

#include "sisdb.h"
#include "sis_map.h"
#include "sis_json.h"
#include "sis_disk.io.h"
#include "sis_dynamic.h"

/////////////////////////////////////////////////////////
//  sisdb 文件和内存映射类
// 解决分块文件读取、修改、删除对应到内存中的一系列关系
// 索引对应于一个列表结构 同时对应于一个磁盘文件
/////////////////////////////////////////////////////////
// 每天需要强制同步一次数据 把内存的数据合并后写入磁盘 清理log
// 存盘时先切换log 然后主程序只接受写数据记入新log 不提供其他服务，直到save完成
// 此时需要重新更新映射表的索引区 然后加载新写入的log 开始下一个周期 
/////////////////////////////////////////////////////////
// 完美的解决方式是新开一个后台数据区，切换log，主程继续执行数据服务
// 后台数据模拟数据变化，然后存盘，直接fork内存直接翻倍 可能有不可预料的错误
// 堵塞处理 可能由于数据量巨大 而耗时
// 后台处理 可以当内存超限后及时存盘 然后重新加载 直到数据处理成功

#define SISDB_FMAP_TYPE_SDB   SIS_SDB_STYLE_SDB  // 有时序结构数据 
#define SISDB_FMAP_TYPE_NON   SIS_SDB_STYLE_NON  // 无时序结构数据 
#define SISDB_FMAP_TYPE_ONE   SIS_SDB_STYLE_ONE  // 无时序单一数据 
#define SISDB_FMAP_TYPE_MUL   SIS_SDB_STYLE_MUL  // 无时序列表数据 

#pragma pack(push,1)

// 如果是只读 moved = writed = 0 那么写盘的时候该键值就不处理
// 通常客户请求什么时间数据 就把相关时间所有文件数据加载到内存 只会多不会少
typedef struct s_sisdb_fmap_idx
{
	// 以下信息来源于 map 记录信息
	int         isign;   // 索引标识 日上为年 日下为日  
	int         start;   // 对应列表的起始索引 -1 表示还没有读过盘 0 在count > 0 时表示索引 在 count = 0时表示已经读盘但没有数据
	int         count;   // 当前块的数量 0 表示没有数据
	// 第一次写入需要建立索引
	uint8       moved;   // 是否被删除或本来就没有数据 删除后写盘时要清理磁盘本来有的数据
	uint8       writed;  // 是否有更新 有数据且被修改过
} s_sisdb_fmap_idx;

// 数据类
typedef struct s_sisdb_fmap_unit
{
	//  要求每个节点时间不能重叠 重叠写入时会覆盖老的数据
    s_sis_object        *kname;  // 可能多次引用 - 指向dict表的name
    s_sis_object        *sname;  // 可能多次引用 - 指向dict表的name
	uint8                reads;  // 读取次数 最大255 每天减少 1/2 销毁时根据该值大小排序
	uint8                ktype;  // 该键值的数据类型
	s_sis_struct_list   *idxs;   // 按时间排序的 索引表 s_sisdb_fmap_idx
	// 以上信息来源于 map 的索引信息
	msec_t               rmsec;  // 最近读的毫秒数 只记录读的毫秒数 大多数键值写入后就不读了，默认36小时没有再读就释放
								 // 写入时rmsec = 0 最长每日会存一次盘 如果没有读只有写那么save时就会从内存清理掉
	uint8                moved;  // 键值直接被删除
	void                *value;  // 数据缓存区 
	// 如果data为时序结构化数据   
	int8                 scale;  // 时序表的时间尺度 SIS_SDB_SCALE_NOTS  SIS_SDB_SCALE_YEAR  SIS_SDB_SCALE_DATE
	msec_t               start;  // 开始时间
	msec_t               stop;   // 结束时间
	double               step;   // 间隔时间，每条记录大约间隔时间，
	s_sis_dynamic_db    *sdb;    // 对应的结构表
} s_sisdb_fmap_unit;

// 映射表定义
typedef struct s_sisdb_fmap_cxt
{
	s_sis_sds           work_path;
	s_sis_sds           work_name;
	int                 work_date;    // 只有自动存盘后 日期才会切换 满足跨天数据
	// 下面数据永不清理 数据表备份
	s_sis_map_list     *work_sdbs;    // sdb 的结构字典表 s_sis_dynamic_db
	// 所有读取和写入的键值表
	s_sis_map_pointer  *work_keys;    // 数据集合的字典表 s_sisdb_fmap_unit 这里实际存放数据，数量为股票个数x数据表数
									  // SH600600.DAY 
} s_sisdb_fmap_cxt;


////////////////////////////////////////////
// 定义默认的检索工具 还需要定义
// 方法万能方法，不过速度慢，用于检索其他非排序字段的内容
// 有以下运算方法
// group -- field min max 字段内容在min和max之间 【数值】
// same -- field val 字段和变量值相等 【字符串和数值】 根据字段类型比较，不区分大小写
// match -- first val 字段值有val内容 【字符串】val为子集 val:"sh600"
// in -- first val 字段值有val内容 【字符串】val为母集
// contain -- field set 字段内容包含在set集合中 set:"600600,600601" val为母集，相等才返回真【字符串或数值】
//
// $same : { field: code, value:600600 }
// $same : { field: code, value:600600, $scope:{ field: in_time, min: 111, max:222}} //复合条件查询
// 数据操作
// match 按时间信息进行区间查询和删除 start 为时间信息 和数据相匹配  
// start offset count stop
//  0       0     0     0    表示取最新的那一条数据 当日 或 当年最后一条记录 
// day1     0     0    day1  表示取== day1 的数据 可能有多条 也可能一条没有 没有的情况下返回 NULL 
// day1     0     0     0    表示取== day1 的数据 同上
// day1     0     1     0    表示取== day1 的数据 只取第一条 此时忽略 stop
// day1     0    -1     0    表示取== day1 的数据 只取最后一条 此时忽略 stop
// day1    -1     0     0    表示取 < day1 有数据的一天的所有数据 
// day1     0     5    day2  表示取>= day1 开始后5条数据 但不得超过 day2
// day1     0     0    day2  表示取>= day1 - day2 的所有数据 
// day1    -1     0    day2  表示以>= day1 有数据的前一天 的第一条记录开始 - day2 的所有数据 
// day1     0     5    -1    如果匹配不到 start 会自动使用前一天的有效数据 但日线最长不超过1年 其他不超过当日
// 只有stop = -1 且 start 没有对应数据 才会匹配前一天数据 此时如果 offset < 0 
// 有count 按 count数量返回 没有按stop返回 
// 没有 stop 表示取同样day1的数据 
// 以时间为索引 可能会重复 修改即全部替换
////////////////////////////////////////////

////////////////////////////////////////////
// 目前仅支持 start stop 区间查询 时序统一为日期 
// 也就是目前只支持按天修改删除和查询的尺度
// start   stop
//  0       0    表示取最新的那一日数据 当日 或 当年最后一天记录 
// day1     0    表示取 day1 到 当日的所有数据 
// day1    day2  表示取 day1 到 day2 的所有数据
// day1    day1  表示取 == day1 的数据 可能有多条 也可能一条没有 没有的情况下返回 NULL  
// day1    -1    表示取 == day1 的数据 如果没有匹配 用前一个有效日期数据返回 前面没有数据再返回 NULL 
#define SISDB_FMAP_CMP_RANGE   0 // 区间匹配查询和删除
// 用于定位修改 只能定位一条数据 通常用于定位查询、修改或删除  
// start 表示匹配时间 严格匹配 不按日期 找不到就返回 NULL
#define SISDB_FMAP_CMP_SAME    1 // 严格匹配查询和删除


typedef int (cb_fmap_read_def)(void *, char *, size_t, int);

// 日线总是会加载到最新的数据 
// 根据 start 和stop 决定加载到内存的数据量 stop 默认为当天日期 查询日期不早于 start.year - 1 年
// sdb 数据会从 map 文件中获取所有key的数据信息 
typedef struct s_sisdb_fmap_cmd
{
	// 传入的命令要素
	uint8             cmpmode;       // 定位或是模糊查询
	const char       *key;           // 要查询的键值
	uint8             ktype;         // write 时有用
	msec_t            start;         // 定位时间 
	// 非时序表示第几条记录 -1 = 倒数第一条
	// 非日期型一般以一天为区间获取数据 
	// 日期型以 start 为基准日期加载数据 默认stop为当日
	msec_t            stop;          // 结束时间 
	// 如果超过 count 限制 就截断 count 优先级高
	int8              offset;        // 以开始时间为定位 -1 向前一条记录 1 向后一条记录 
	// 如果时间一样就一直到不一样的记录 
	// offset 不能超过 255 基本是一年的数据
	int               count;         // 取多少数据 0 忽略 > = 0
	char             *imem;          // 传入或传出的数据 传出的数据需要拷贝到 data 中 
	// sis_free
	size_t            isize;         // 数据长度
	// 处理后的命令要素 针对已经读入缓存的 unit
	void             *cb_source;     // 返回的对象
	cb_fmap_read_def *cb_fmap_read;  // 返回数据
} s_sisdb_fmap_cmd;

// 
typedef struct s_sisdb_fmap_cmp
{
	int               ostart;   	 // 时序表日期
	int               oindex;   	 // 时序表开始索引
	int               ocount;   	 // 时序表操作数量
} s_sisdb_fmap_cmp;

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_fmap_unit --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sisdb_fmap_unit *sisdb_fmap_unit_create(
	s_sis_object *kname_, s_sis_object *sname_,
	int style_, s_sis_dynamic_db *sdb_);
void sisdb_fmap_unit_destroy(void *);
void sisdb_fmap_unit_clear(s_sisdb_fmap_unit *);
// 重新索引
void sisdb_fmap_unit_reidx(s_sisdb_fmap_unit *unit_);
// 得到索引值
msec_t sisdb_fmap_unit_get_mindex(s_sisdb_fmap_unit *unit_, int index_);
// 得到索引值对应日期
int sisdb_fmap_unit_get_start(s_sisdb_fmap_unit *unit_, int index_);
// 快速定位
int sisdb_fmap_unit_goto(s_sisdb_fmap_unit *unit_, msec_t curr_);

// 得到记录数
int sisdb_fmap_unit_count(s_sisdb_fmap_unit *unit_);
// 仅仅增加一条 这里 start 是日期
int sisdb_fmap_unit_push_idx(s_sisdb_fmap_unit *unit_, int start);
// 可以修改同一索引的多条 这里 start 是日期
int sisdb_fmap_unit_update_idx(s_sisdb_fmap_unit *unit_, int start);
// 删除 这里 index 为起始记录 count 为数量
int sisdb_fmap_unit_del_idx(s_sisdb_fmap_unit *unit_, int index, int count);
// 删除键值 需要做标记 仅仅针对时序数据
int sisdb_fmap_unit_move_idx(s_sisdb_fmap_unit *unit_);
// 必须找到一个相等值，否则返回-1
int sisdb_fmap_cmp_same(s_sisdb_fmap_unit *unit_, msec_t  start_, s_sisdb_fmap_cmp *ans_);
// 找到匹配的区间数据，否则返回-1
int sisdb_fmap_cmp_range(s_sisdb_fmap_unit *unit_, msec_t  start_, msec_t  stop_, s_sisdb_fmap_cmp *ans_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_fmap_cxt --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sisdb_fmap_cxt *sisdb_fmap_cxt_create(const char *wpath_, const char *wname_);
void sisdb_fmap_cxt_destroy(s_sisdb_fmap_cxt *);

// // 得到数据 并读入内存 除非磁盘也没有数据 才返回NULL 否则根据cmd参数获取数据
// s_sisdb_fmap_unit * sisdb_fmap_cxt_read_where(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);
// s_sisdb_fmap_unit * sisdb_fmap_cxt_read_match(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);
// 得到数据的索引信息 并读入内存 除非磁盘也没有数据 才返回NULL
s_sisdb_fmap_unit *sisdb_fmap_cxt_getidx(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);
// 把unit 索引中还没有读盘的数据 读盘 
int sisdb_fmap_cxt_getdata(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);

// 读取指定的数据
int sisdb_fmap_cxt_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);
// 插入一条数据
int sisdb_fmap_cxt_push(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);
// 插入多条排好序的数据 不做校验
// int sisdb_fmap_cxt_pushs(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);
// 修改符合条件的数据 没有匹配不修改
int sisdb_fmap_cxt_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);
// 删除符合条件的数据
int sisdb_fmap_cxt_del(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);
// 删除一个键值 同时清除所有 fidx 对应数据
int sisdb_fmap_cxt_move(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_);


int sisdb_fmap_cxt_tsdb_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);

int sisdb_fmap_cxt_tsdb_push(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);
int sisdb_fmap_cxt_tsdb_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);
int sisdb_fmap_cxt_tsdb_del(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_);
int sisdb_fmap_cxt_tsdb_move(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_);


#endif /* _SIS_COLLECT_H */
