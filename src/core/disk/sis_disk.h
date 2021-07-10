#ifndef _SIS_DISK_H
#define _SIS_DISK_H

#include "sis_disk.io.h"

////////////////////////////////////////////////////
// 外部调用接口定义
////////////////////////////////////////////////////

typedef struct s_sis_disk_reader_cb {
    // 用户需要传递的上下文
    void *cb_source; 
    // 通知文件开始读取了或订阅开始 需要全部转换为毫秒传出 无时序为0
    void (*cb_start)(void *, int);  // 日期
    // 返回key定义
    void (*cb_dict_keys)(void *, void *key_, size_t);  // 返回key信息
    // 返回sdb定义
    void (*cb_dict_sdbs)(void *, void *sdb_, size_t);  // 返回属性信息
    // 返回解压后的数据(可能一条或多条数据)
    // 支持 SDB SNO 不支持 LOG IDX
    void (*cb_userdate)(void *, const char *kname_, const char *sname_, void *out_, size_t olen_); 
    // 返回解压后的数据(可能一条或多条数据)
    // 支持 SDB SNO 不支持 LOG IDX
    void (*cb_realdate)(void *, int kidx_, int sidx_, void *out_, size_t olen_); 
    // 返回原始头标记后的原始数据(未解压) PACK时只能返回该数据结构 速度快
    // 支持 LOG SNO SDB IDX 
    void (*cb_original)(void *, s_sis_disk_head *, void *out_, size_t olen_); // 头描述 实际返回的数据区和大小
    // 通知文件结束了
    void (*cb_stop)(void *, int); //第一个参数就是用户传递进来的指针
} s_sis_disk_reader_cb;


// 通用的读取数据函数 如果有字典数据key就用字典的信息 
// 如果没有字典信息就自己建立一个
typedef struct s_sis_disk_writer {
    int                   style;
    s_sis_sds             fpath;
    s_sis_sds             fname;
    //////////////////////////////////////////////////////////////
    int                   status;
    s_sis_disk_ctrl      *wunit;      // sno和log和sdb的非时序写入类
    // ---------------------------------- //
    s_sis_map_kint       *units;  
    // 一组操作类 s_sis_disk_ctrl 适用于sdb的时序数据
    // 日时序数据 以起始年为索引
    // 其他时序以日期为索引
    // sdb 的按时序存储的结构映射表
	s_sis_disk_map       *wdict;  
    s_sis_incrzip_class  *incrzip; // = NULL 当前只接收增量压缩数据
    s_sis_memoey         *zipmem;
    s_sis_memoey         *memory;
    // sno 专用 //
    size_t                sno_max_size;  // 最大压缩数据大小 默认16M
    size_t                sno_cur_size;  // 当前已存数据大小
    // 参数传递 // 
    s_sis_disk_head       whead;
    int                   kidx;
    int                   sidx;
    s_sis_dynamic_db     *sdb;
    s_sis_object         *kname;
    s_sis_object         *sname;

} s_sis_disk_writer;


typedef struct s_sis_disk_reader {
    // ------input------ //
    s_sis_sds             fpath;
    s_sis_sds             fname;
    s_sis_msec_pair       search_msec;
 
    s_sis_sds             sub_keys;   // "*" 为全部都要 或者 k1,k2,k3
	s_sis_sds             sub_sdbs;   // "*" 为全部都要 或者 k1,k2,k3

    s_sis_disk_reader_cb *callback;   // 读文件的回调
    // ------contorl------ //
    int                   status;     // 0 未初始化 1 可以订阅 2 不能订阅

    // ------output------ //
    uint8                issub;   // 是否为订阅 不是订阅就一次性返回数据
                                  // 如果是订阅 根据whole判断是否整块输出
    uint8                whole;   // 是否一次性输出 

    // int                  rsize; // 当前的记录长度
    s_sis_disk_dict     *kdict;
    s_sis_disk_dict     *sdict;
    s_sis_disk_head      rhead;    // 块头信息
    s_sis_disk_idx_unit  rinfo;    // 读入时的索引信息
    s_sis_memory        *memory; 
    ////////// 读取数据范围  //////////
}s_sis_disk_reader;



// //////////////////////////////////////////////////////
// 索引表 只有具备索引功能的文件 必须先加载字典 再加载索引 
// 索引文件写入时要先写字典信息
//////////////////////////////////////////////////////

// typedef struct s_sis_disk_reader {
//     uint8                issub;   // 是否为订阅 不是订阅就一次性返回数据
//                                   // 如果是订阅 根据whole判断是否整块输出
//     uint8                whole;   // 是否一次性输出 

//     // int                  rsize; // 当前的记录长度
//     s_sis_disk_dict     *kdict;
//     s_sis_disk_dict     *sdict;
//     s_sis_disk_head      rhead;    // 块头信息
//     s_sis_disk_idx_unit  rinfo;    // 读入时的索引信息
//     s_sis_memory        *memory; 
//     ////////// 读取数据范围  //////////
//     s_sis_msec_pair      search_msec;

//     s_sis_sds            sub_keys;   // "*" 为全部都要 或者 k1,k2,k3
// 	s_sis_sds            sub_sdbs;   // "*" 为全部都要 或者 k1,k2,k3

//     s_sis_disk_callback *callback;   // 读文件的回调
// }s_sis_disk_reader;

typedef struct s_sis_disk_ctrl {
    /////// 初始化文件的信息 ////////
    int                  style;       // 0 表示未初始化 其他表示文件类型
    char                 fpath[255];
    char                 fname[255];
    int                  open_date;   // 数据时间
    int                  stop_date;   // 数据时间
    int                  status;

    ////// sno sdb 都会用的到 ////////
    bool                 isstop;       // 是否中断读取 读文件期间是否停止 如果为 1 立即退出

	s_sis_map_list      *map_kdicts;   // s_sis_disk_kdict
	s_sis_map_list      *map_sdicts;   // s_sis_disk_sdict

    s_sis_map_list      *map_idxs;     // s_sis_disk_idx 索引信息列表

    s_sis_disk_files    *work_fps;     // 工作文件组
    s_sis_disk_files    *widx_fps;     // 索引文件组

    s_sis_disk_reader   *reader;       // 读取数据时保存的指针

    s_sis_disk_rcatch   *rcatch;       // 读数据的缓存 
    s_sis_disk_wcatch   *wcatch;       // 写数据的缓存
    ///// sno 专用 //////
    int                  sno_count;   
    size_t               sno_zipsize;   // sno 当前压缩的尺寸
    s_sis_incrzip_class *sno_incrzip;   // sno 压缩组件
} s_sis_disk_ctrl;

///////////////////////////
//  s_sis_disk_idx
///////////////////////////
s_sis_disk_idx *sis_disk_idx_create(s_sis_object *kname_, s_sis_object *sname_);
void sis_disk_idx_destroy(void *in_);
int sis_disk_idx_set_unit(s_sis_disk_idx *cls_, s_sis_disk_idx_unit *unit_);
s_sis_disk_idx_unit *sis_disk_idx_get_unit(s_sis_disk_idx *cls_, int index_);


///////////////////////////
//  s_sis_disk_reader
///////////////////////////
s_sis_disk_reader *sis_disk_reader_create(s_sis_disk_callback *); 
void sis_disk_reader_destroy(void *in_);
void sis_disk_reader_init(s_sis_disk_reader *in_);
void sis_disk_reader_clear(s_sis_disk_reader *in_);
void sis_disk_reader_set_key(s_sis_disk_reader *in_, const char *);
void sis_disk_reader_set_sdb(s_sis_disk_reader *in_, const char *);
// 设置搜索时间范围 start 一般从最小的值 stop最大值
void sis_disk_reader_set_stime(s_sis_disk_reader *cls_, msec_t start_, msec_t stop_);


///////////////////////////
//  s_sis_disk_ctrl
///////////////////////////
// 根据传入的参数创建目录或确定文件名(某些类型的文件名是系统默认的，用户只能指定目录) 
// 路径和文件名分开是为了根据不同种类增加中间目录 文件过大时自动分割子文件 
s_sis_disk_ctrl *sis_disk_class_create();
void sis_disk_class_destroy(s_sis_disk_ctrl *cls_);

int sis_disk_class_init(s_sis_disk_ctrl *cls_, int style_, const char *fpath_, const char *fname_, int wdate_);

// 关闭文件
void sis_disk_class_clear(s_sis_disk_ctrl *cls_);
// 设置文件大小
void sis_disk_class_set_size(s_sis_disk_ctrl *,size_t fsize_, size_t psize_);

// 检查文件是否有效
int sis_disk_file_valid_work(s_sis_disk_ctrl *cls_);
// 检查索引
int sis_disk_file_valid_widx(s_sis_disk_ctrl *cls_);

// 读写数据
int sis_disk_file_write_start(s_sis_disk_ctrl *cls_);
//写文件尾 只读文件不做 
int sis_disk_file_write_stop(s_sis_disk_ctrl *cls_);

// 只读数据
int sis_disk_file_read_start(s_sis_disk_ctrl *cls_);
//写文件尾 只读文件不做 
int sis_disk_file_read_stop(s_sis_disk_ctrl *cls_);

///////////////////////////
//  index option
///////////////////////////

s_sis_disk_idx *sis_disk_idx_get(s_sis_map_list *map_, s_sis_object *kname_, s_sis_object *sname_);

// 根据 reader_ 和索引生成 需要查询的数据列表
// key sdb 为 * 表示全部查 
// sdb 为 NULL 表示只有key起作用 
// list_ 为 s_sis_disk_idx 结构队列 其中 index 仅仅为指向索引的指针 销毁时要注意
int sis_reader_sub_filters(s_sis_disk_ctrl *cls_, s_sis_disk_reader *reader_, s_sis_pointer_list *list_);

///////////////////////////
//  write
///////////////////////////
s_sis_sds sis_disk_file_get_keys(s_sis_disk_ctrl *cls_, bool onlyincr_);
s_sis_sds sis_disk_file_get_sdbs(s_sis_disk_ctrl *cls_, bool onlyincr_);

size_t sis_disk_file_write_key_dict(s_sis_disk_ctrl *cls_);
size_t sis_disk_file_write_sdb_dict(s_sis_disk_ctrl *cls_);

// 重新生成索引文件，需要读取原始文件
size_t sis_disk_file_write_index(s_sis_disk_ctrl *cls_);

// 写入 stream 接口
size_t sis_disk_file_write_log(s_sis_disk_ctrl *cls_, void *in_, size_t ilen_);
// 仅仅对 sdb 有效 必须有 key 和 sdb 的字典 否则无效
size_t sis_disk_file_write_sdbi(s_sis_disk_ctrl *cls_,
                                int keyi_, int sdbi_, void *in_, size_t ilen_);
// 通用写入接口
size_t sis_disk_file_write_sdb(s_sis_disk_ctrl *cls_, 
    const char *key_, const char *sdb_, void *in_, size_t ilen_);

size_t sis_disk_file_write_kdb(s_sis_disk_ctrl *cls_,
                                const char *key_, const char *sdb_, void *in_, size_t ilen_);

size_t sis_disk_file_write_key(s_sis_disk_ctrl *cls_,
                                const char *key_, void *in_, size_t ilen_);

size_t sis_disk_file_write_any(s_sis_disk_ctrl *cls_,
                                const char *key_, void *in_, size_t ilen_);

// size_t sis_disk_file_write_original(s_sis_disk_ctrl *cls_, 
//     s_sis_disk_head *head_, void *in_, size_t ilen_)

void sis_disk_file_delete(s_sis_disk_ctrl *cls_);

void sis_disk_file_move(s_sis_disk_ctrl *cls_, const char *path_);
///////////////////////////
//  read
///////////////////////////
// 读取索引后加载字典信息
int sis_disk_file_read_dict(s_sis_disk_ctrl *cls_);
// 加载索引文件到内存 方便检索
int cb_sis_disk_file_read_idx(void *cls_, s_sis_disk_head *, char *, size_t );
// 读取log文件
int cb_sis_disk_file_read_log(void *cls_, s_sis_disk_head *, char *, size_t );
// 读取sno文件
int cb_sis_disk_file_read_sno(void *cls_, s_sis_disk_head *, char *, size_t );

// 以流的方式读取文件 从文件中一条一条发出 
// 必须是同一个时间尺度的数据 否则无效
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_file_read_sub(s_sis_disk_ctrl *cls_, s_sis_disk_reader *reader_);

// 直接获取数据 需要索引 只能获取单一key的数据 可指定时间段 k1 db1
s_sis_object *sis_disk_file_read_get_obj(s_sis_disk_ctrl *cls_, s_sis_disk_reader *reader_);

///////////////////////////
//  pack
///////////////////////////

size_t sis_disk_file_pack(s_sis_disk_ctrl *src_, s_sis_disk_ctrl *des_);


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
int sis_disk_writer_set_kdict(s_sis_disk_writer *, const char *in_, size_t ilen_);
// 设置表结构体 - 根据不同的时间尺度设置不同的标记 仅支持 SNO SDB
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
//                  sno 
//////////////////////////////////////////
// 开始写入数据 后面的数据只有符合条件才会写盘 支持SNO 
// sendzip_ = 0 后续只接收 sis_disk_writer_sno 没有压缩的原始数据
// sendzip_ = 1 后续只接收 sis_disk_writer_snozip 已经压缩好的数据
int sis_disk_writer_start(s_sis_disk_writer *, int sendzip_);
// 数据传入结束 剩余全部写盘 支持SNO
void sis_disk_writer_stop(s_sis_disk_writer *);
// 写入数据 仅支持 SNO 必须是压缩数据
// sis_disk_writer_open
// sis_disk_writer_set_kdict
// sis_disk_writer_set_sdict
// sis_disk_writer_start
// sis_disk_writer_snozip
// ....
// sis_disk_writer_stop
// sis_disk_writer_close
size_t sis_disk_writer_snozip(s_sis_disk_writer *, void *in_, size_t ilen_);
// 写入数据 仅支持 SNO 必须是原始数据
// sis_disk_writer_open
// sis_disk_writer_set_kdict
// sis_disk_writer_set_sdict
// sis_disk_writer_start
// sis_disk_writer_sno
// ....
// sis_disk_writer_stop
// sis_disk_writer_close
int sis_disk_writer_sno(s_sis_disk_writer *, const char *kname_, const char *sname_, void *in_, size_t ilen_);

size_t sis_disk_writer_sno_one(s_sis_disk_writer *writer_, const char *kname_, int style_, void *in_, size_t ilen_);

size_t sis_disk_writer_sno_mul(s_sis_disk_writer *writer_, const char *kname_, int style_, s_sis_pointer_list *inlist_);

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
// sis_disk_writer_sdb_idx
// ...
// sis_disk_writer_non
// ...
// sis_disk_writer_close
size_t sis_disk_writer_sdb(s_sis_disk_writer *, const char *kname_, const char *sname_, void *in_, size_t ilen_);
// 按索引写入数据 kidx_ sidx_ 必须都有效
size_t sis_disk_writer_sdb_idx(s_sis_disk_writer *, int kidx_, int sidx_, void *in_, size_t ilen_);
// 需要根据数据 默认指无时序的数据 
// style_ = SIS_SDB_STYLE_BYTE  SIS_SDB_STYLE_JSON  SIS_SDB_STYLE_CHAR 
size_t sis_disk_writer_sdb_one(s_sis_disk_writer *, const char *kname_, int style_, void *in_, size_t ilen_);
// style_ = SIS_SDB_STYLE_BYTES  SIS_SDB_STYLE_JSONS  SIS_SDB_STYLE_CHARS 
// inlist_ : s_sis_sds 的列表
size_t sis_disk_writer_sdb_mul(s_sis_disk_writer *, const char *kname_, int style_, s_sis_pointer_list *inlist_);

///////////////////////////
//  s_sis_disk_reader
///////////////////////////
s_sis_disk_reader *sis_disk_reader_create(const char *path_, const char *name_, s_sis_disk_reader_cb *cb_);
void sis_disk_reader_destroy(void *);

// 打开 准备读 首先加载IDX到内存中 就知道目录下支持哪些数据了 LOG SNO SDB
int sis_disk_reader_open(s_sis_disk_reader *);
// 关闭所有文件 设置了不同订阅条件后可以重新
void sis_disk_reader_close(s_sis_disk_reader *);

// 从对应文件中获取数据 拼成完整的数据返回 只支持 SDB 单键单表
// 多表按时序输出通过该函数获取全部数据后 排序输出
s_sis_object *sis_disk_reader_get_obj(s_sis_disk_reader *, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_);

// 以流的方式读取文件 从文件中一条一条发出 按时序 无时序的会最先发出 只支持 SDB 
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
// 按磁盘存储的块 所有键无时序的先发 然后有时序读取第一块 然后排序返回 依次回调 cb_realdate 直到所有数据发送完毕 
int sis_disk_reader_sub(s_sis_disk_reader *, const char *keys_, const char *sdbs_, s_sis_msec_pair *smsec_);

// 顺序读取 仅支持 LOG  通过回调的 cb_original 返回数据
int sis_disk_reader_sub_log(s_sis_disk_reader *, int idate_);

// 顺序读取 仅支持 SNO  通过回调的 cb_original 或 cb_realdate 返回数据
// 如果定义了 cb_realdate 就解压数据再返回
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_reader_sub_sno(s_sis_disk_reader *, const char *keys_, const char *sdbs_, int idate_);

// 取消一个正在订阅的任务 只有处于非订阅状态下才能订阅 避免重复订阅
int sis_disk_reader_unsub(s_sis_disk_reader *);

///////////////////////////
//  s_sis_disk_control
///////////////////////////

// 不论该目录下有任何类型文件 全部删除
int sis_disk_io_clear(const char *path_, const char *name_);


#endif

