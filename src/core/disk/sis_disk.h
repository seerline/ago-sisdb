#ifndef _SIS_DISK_H
#define _SIS_DISK_H

#include "sis_disk.io.h"
#include "sis_list.h"
#include "sis_db.h"
#include "sis_map.h"
////////////////////////////////////////////////////
// 外部调用接口定义
////////////////////////////////////////////////////

typedef struct s_sis_disk_reader_cb {
    // 用户需要传递的上下文
    void *cb_source; 
    /**
     * @brief 通知文件读取或订阅开始，需要全部转换为毫秒传出 无时序为0,
     * 第一个参数为回调函数提供者（订阅者）的对象指针,
     * 第二个参数为日期
     */
    void (*cb_start)(void *, int);
    // 返回key定义
    void (*cb_dict_keys)(void *, void *key_, size_t);  // 返回key信息
    // 返回sdb定义
    void (*cb_dict_sdbs)(void *, void *sdb_, size_t);  // 返回属性信息
    // 返回解压后的数据(可能一条或多条数据)
    // 支持 SDB SNO 不支持 LOG IDX
    void (*cb_chardata)(void *, const char *kname_, const char *sname_, void *out_, size_t olen_); 
    // 返回解压后的数据(可能一条或多条数据)
    // 支持 SDB SNO 不支持 LOG IDX
    void (*cb_bytedata)(void *, int kidx_, int sidx_, void *out_, size_t olen_); 
    // 返回原始头标记后的原始数据(未解压) PACK时返回该数据结构 速度快
    // 支持 LOG SNO SDB IDX 
    void (*cb_original)(void *, s_sis_disk_head *, void *out_, size_t olen_); // 头描述 实际返回的数据区和大小
    // 通知文件结束了

    /**
     * @brief 通知文件读取或订阅结束
     * 第一个参数为回调函数提供者（订阅者）的对象指针,
     * 第二个参数为日期
     */
    void (*cb_stop)(void *, int);
    /**
     * @brief 通知文件读取或订阅被中断
     * 第一个参数为回调函数提供者（订阅者）的对象指针,
     * 第二个参数为日期
     */
    void (*cb_break)(void *, int); 
} s_sis_disk_reader_cb;


// 通用的读取数据函数 如果有字典数据key就用字典的信息 
// 如果没有字典信息就自己建立一个
typedef struct s_sis_disk_writer {
    // ------input------ //
    int                   style;
    s_sis_sds             fpath;
    s_sis_sds             fname;
    // ------contorl------ //
    int                   status;     // 状态
    s_sis_disk_ctrl      *munit;      // 非时序写入类 sno net log sdb
    s_sis_map_list       *units;      // sdb 的按时序存储的结构映射表
    // 一组操作类 s_sis_disk_ctrl 适用于sdb的时序数据
    // 日时序数据 以起始年为索引
    // 其他时序以日期为索引 
	s_sis_map_list       *map_keys;   // s_sis_sds
	s_sis_map_list       *map_sdbs;   // 全部表结构 s_sis_dynamic_db
    // 以下两个结构为 map_sdbs 的子集
	s_sis_map_list       *map_year;   // 日上时序数据表指针 *s_sis_dynamic_db
	s_sis_map_list       *map_date;   // 日下时序数据表指针 *s_sis_dynamic_db

} s_sis_disk_writer;

typedef struct s_sis_disk_reader_unit {
    struct s_sis_disk_reader    *reader;
    int                   style;
    int                   idate;
    s_sis_disk_ctrl      *ctrl; 
} s_sis_disk_reader_unit;

typedef struct s_sis_disk_reader_sub {
    s_sis_sds             skey;        // key+sdb
    int                   unit_cursor; // 对应文件的指针位置 改变设置 kidx_cursor = 0
    int                   kidx_cursor; // 对应索引的指针位置
    s_sis_pointer_list   *kidxs;       // s_sis_disk_idx * 保存索引指针 不释放
    s_sis_pointer_list   *units;       // ksidxs 对应的 unit 指针
} s_sis_disk_reader_sub;
// 根据key+sdb获取在每个文件的索引 排队进入列表 每个key+sdb只有一条记录
// 先读取所有
typedef struct s_sis_disk_reader {
    // ------init info------ //
    int                   style;
    s_sis_sds             fpath;    // 文件路径
    s_sis_sds             fname;    // 文件名
    s_sis_disk_reader_cb *callback;   // 读文件的回调
    // ------sub info------ //
    s_sis_msec_pair       search_msec;
    s_sis_sds             sub_keys;   // "*" 为全部都要 或者 k1,k2,k3
	s_sis_sds             sub_sdbs;   // "*" 为全部都要 或者 k1,k2,k3
    // ------contorl------ //
    int8                  status_open;// 0 未打开或已关闭 1 已经打开 
    int8                  status_sub; // 0 未订阅或订阅完成 1 订阅中 2 请求停止订阅

    s_sis_disk_ctrl      *munit;      // 非时序类 sno net log sdb
    s_sis_pointer_list   *sunits;     // 当前请求覆盖的文件列表 s_sis_disk_reader_unit 

    uint8                 issub;      // 是否为订阅 不是订阅就一次性返回数据
    uint8                 isone;      // 是否为单键
    // 订阅列表
    s_sis_subdb_cxt      *subcxt;     // 订阅组件   
    s_sis_map_list       *subidxs;    // 订阅索引列表  s_sis_disk_reader_sub
}s_sis_disk_reader;


///////////////////////////
//  s_sis_disk_writer
///////////////////////////
// 如果目录下已经有不同类型文件 返回错误
s_sis_disk_writer *sis_disk_writer_create(const char *path_, const char *name_, int style_);
void sis_disk_writer_destroy(void *);

// LOG SNO 为文件实际时间 SDB 为文件修改日期 
int sis_disk_writer_open(s_sis_disk_writer *, int idate_);
// 关闭所有文件 重写索引
void sis_disk_writer_close(s_sis_disk_writer *);

// 写入键值信息 - 可以多次写 新增的添加到末尾 仅支持 SNO SDB
// 无论何时 只要有新的键设置 就直接写盘 
int sis_disk_writer_set_kdict(s_sis_disk_writer *, const char *in_, size_t ilen_);
// 设置表结构体 - 根据不同的时间尺度设置不同的标记 仅支持 SNO SDB
// 无论何时 只要有新结构设置 就直接写盘 
int sis_disk_writer_set_sdict(s_sis_disk_writer *, const char *in_, size_t ilen_);

//////////////////////////////////////////
//                  log 
//////////////////////////////////////////
// 写入数据 仅支持 LOG 不管数据多少 直接写盘 
// sis_disk_writer_open
// sis_disk_writer_log
// sis_disk_writer_close
size_t sis_disk_writer_log(s_sis_disk_writer *, void *in_, size_t ilen_);

//////////////////////////////////////////
//                  net 
//////////////////////////////////////////
// 开始写入数据  支持 SNO NET
int sis_disk_writer_start(s_sis_disk_writer *);
// 数据传入结束 剩余全部写盘 支持 SNO NET
void sis_disk_writer_stop(s_sis_disk_writer *);
// 写入数据 仅支持 NET 必须是原始数据
// sis_disk_writer_open
// sis_disk_writer_set_kdict
// sis_disk_writer_set_sdict
// sis_disk_writer_start
// sis_disk_writer_sic
// ....
// sis_disk_writer_stop
// sis_disk_writer_close
// map速度最快 2500万次查询 耗时1秒 
int sis_disk_writer_sic(s_sis_disk_writer *, const char *kname_, const char *sname_, void *in_, size_t ilen_);
// 写入数据 仅支持 NET 必须是原始数据
// sis_disk_writer_open
// sis_disk_writer_set_kdict
// sis_disk_writer_set_sdict
// sis_disk_writer_start
// sis_disk_writer_sno
// ....
// sis_disk_writer_stop
// sis_disk_writer_close
int sis_disk_writer_sno(s_sis_disk_writer *, const char *kname_, const char *sname_, void *in_, size_t ilen_);
//////////////////////////////////////////
//  sdb 
//////////////////////////////////////////
// 写入标准数据 kname 如果没有就新增 sname 必须字典已经有了数据 
// 需要根据数据的时间字段 确定对应的文件 
// sis_disk_writer_open
// sis_disk_writer_set_kdict
// sis_disk_writer_set_sdict
// sis_disk_writer_sdb
// ...
// sis_disk_writer_one
// ...
// sis_disk_writer_mul
// ...
// sis_disk_writer_close
// 结构化时序和无时序数据 
size_t sis_disk_writer_sdb(s_sis_disk_writer *, const char *kname_, const char *sname_, void *in_, size_t ilen_);
// 单键值单记录数据 
size_t sis_disk_writer_one(s_sis_disk_writer *, const char *kname_, void *in_, size_t ilen_);
// 单键值多记录数据 inlist_ : s_sis_sds 的列表
size_t sis_disk_writer_mul(s_sis_disk_writer *, const char *kname_, s_sis_pointer_list *inlist_);

// 结构化时序和无时序数据 
int sis_disk_writer_sdb_remove(s_sis_disk_writer *, const char *kname_, const char *sname_, int isign_);
// 单键值单记录数据 
int sis_disk_writer_one_remove(s_sis_disk_writer *, const char *kname_);
// 单键值多记录数据 inlist_ : s_sis_sds 的列表
int sis_disk_writer_mul_remove(s_sis_disk_writer *, const char *kname_);

s_sis_disk_reader *sis_disk_reader_create(const char *path_, const char *name_, int style_, s_sis_disk_reader_cb *cb_);
void sis_disk_reader_destroy(void *);

// 顺序读取 仅支持 LOG  通过回调的 cb_original 返回数据
int sis_disk_reader_sub_log(s_sis_disk_reader *, int idate_);

// 顺序读取 仅支持 NET  通过回调的 cb_original 或 cb_bytedata 返回数据
// 如果定义了 cb_bytedata 就解压数据再返回
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_reader_sub_sic(s_sis_disk_reader *, const char *keys_, const char *sdbs_, int idate_);

// 读取 仅支持 SNO  
// 如果定义了 cb_bytedata cb_chardata 就解压数据再返回
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_reader_sub_sno(s_sis_disk_reader *, const char *keys_, const char *sdbs_, int idate_);

// 取消一个正在订阅的任务 只有处于非订阅状态下才能订阅 避免重复订阅
void sis_disk_reader_unsub(s_sis_disk_reader *);
/////////////////////////////////
// 以下是对sdb的文件读取操作
// 仅仅对应sdb文件
int sis_disk_reader_open(s_sis_disk_reader *reader_);

// 仅仅对应sdb文件
void sis_disk_reader_close(s_sis_disk_reader *reader_);

s_sis_dynamic_db *sis_disk_reader_getdb(s_sis_disk_reader *reader_, const char *sname_);
// 获取单值和列表值 返回值需要释放 数据实际类型 s_sis_memory
s_sis_object * sis_disk_reader_get_one(s_sis_disk_reader *, const char *kname_);
// 返回值 单条数据类型为 s_sis_sds
s_sis_node *sis_disk_reader_get_mul(s_sis_disk_reader *, const char *kname_);

// 下面都是结构化数据包括时序和非时序的数据
// 从对应文件中获取数据 拼成完整的数据返回 只支持 SDB 单键单表
// 多表按时序输出通过该函数获取全部数据后 排序输出 时间范围单位为毫秒
// sis_disk_reader_open
// sis_disk_reader_get_obj
// sis_disk_reader_close
// sname_ smsec_ = NULL 表示仅仅获取无时序的单键数据（单键数据不支持订阅）
// 返回值为 s_sis_memory
s_sis_object *sis_disk_reader_get_obj(s_sis_disk_reader *, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_);

// 获取sno键值
s_sis_object *sis_disk_reader_get_keys(s_sis_disk_reader *, int idate);
// 获取sno数据库
s_sis_object *sis_disk_reader_get_sdbs(s_sis_disk_reader *, int idate);

// 仅仅支持同一个数据集合的订阅 多集合订阅需要通过 sis_disk_reader_get_obj 合并后输出
// 以流的方式读取文件 从文件中一条一条发出 按时序 无时序的会最先发出 只支持 SDB SNO 时间范围单位为毫秒
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
// 按磁盘存储的块 所有键无时序的先发 然后有时序读取第一块 然后排序返回 依次回调 cb_bytedata 直到所有数据发送完毕 
// sis_disk_reader_open
// sis_disk_reader_sub
// sis_disk_reader_close
int sis_disk_reader_sub_sdb(s_sis_disk_reader *, const char *keys_, const char *sdbs_, s_sis_msec_pair *smsec_);


///////////////////////////
//  s_sis_disk_control
///////////////////////////
// 删除 log net sno 文件
int sis_disk_control_remove(const char *path_, const char *name_, int style_, int idate_);
// 是否存在 log net sno 文件
int sis_disk_control_exist(const char *path_, const char *name_, int style_, int idate_);
// 移动文件至目标目录
int sis_disk_control_move(const char *srcpath_, const char *name_, int style_, int idate_, const char *dstpath_);
// 复制文件至目标目录
int sis_disk_control_copy(const char *srcpath_, const char *name_, int style_, int idate_, const char *dstpath_);
// 复制文件至目标目录
int sis_disk_control_(const char *srcpath_, const char *name_, int style_, int idate_, const char *dstpath_);
// 判断指定日期的log是否存在
int sis_disk_log_exist(const char *path_, const char *name_, int idate_);
// 判断指定日期的net是否存在
int sis_disk_sic_exist(const char *path_, const char *name_, int idate_);
// 判断指定日期的sno是否存在
int sis_disk_sno_exist(const char *path_, const char *name_, int idate_);


#endif

