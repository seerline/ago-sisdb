#ifndef _SIS_SINCRC_H
#define _SIS_SINCRC_H

#include "sis_core.h"
#include "sis_dynamic.h"
#include "sis_sds.h"
#include "sis_obj.h"
#include "sis_bits.h"

/////////////////////////////////////
// ***     结构化数据增量压缩     *** //
/////////////////////////////////////
// 1.用于网络结构化数据压缩传输
// 通讯双方需要先互换 key的dict和结构sdb的dict   
// 接收端用收到的字典对数据解析 并用发送端的字典打包数据发送给接收端    
// 2.用于结构化数据压缩存储
// 写数据前先写key和sdb的字典信息，然后写入数据，新写入的数据字典会覆盖老的数据字典
/////////////////////////////////////

// 新的数据+原长度大于缓存区大小 就返回该回调 返回回调后清空当前缓存 把新的数据写入当前缓存
typedef int (cb_incrzip_encode)(void *, char *, size_t);
// 返回key和sdb的索引 以及对应的数据
typedef int (cb_incrzip_decode)(void *, int ,int , char *, size_t);

#pragma pack(push,1)

typedef struct s_unzip_unit
{
	uint32  kidx; // 支持40亿不同键值
	uint16  sidx; // 支持6万不同结构体或字段
	uint16  size;
	char   *data;    
} s_unzip_unit;

// 一个数据结构的定义
typedef struct s_sis_incrzip_dbinfo{
	uint32               offset;  // 当前偏移位置
	uint16               fnums;   // 字段数
	s_sis_dynamic_db    *lpdb;    // 不用释放       
}s_sis_incrzip_dbinfo;

#define SIS_SIC_STATUS_NONE     0
#define SIS_SIC_STATUS_INITKEY  1
#define SIS_SIC_STATUS_INITSDB  2
#define SIS_SIC_STATUS_INITED   3 
#define SIS_SIC_STATUS_ENCODE   7
#define SIS_SIC_STATUS_DECODE  11

// 可装载多个结构的数据流
typedef struct s_sis_incrzip_class{
	uint8                 status;       // 工作状态      
	 
	s_sis_pointer_list   *dbinfos;      // s_sis_incrzip_dbinfo
	uint32                sumdbsize;    // 所有结构体的大小
	uint32                maxdbsize;    // 最大的结构体的大小

	uint32                cur_keys;     // 最大key的数量 超出不处理
	s_sis_bits_stream    *cur_stream;   // 字节流操作指针
	uint8                *cur_memory;   // 前值数据缓存 大小为 keycount * sdbcount * sdbsize               

    int8                  zip_init;     // 1 表示初始块 0 表示连续块 2 表示已经写了块头
	uint32                zip_size;     // 压缩用最大尺寸
	uint8                *zip_memory;   // 压缩用外部缓存
	// 数据格式 : kid + sid + count + 数据区
	uint32                zip_bags;       // 当前压缩的包数量
	// 最后一个字节如果 <= 250 表示实际长度 251 再向前取两位 252 再向前取4位....
	uint8                 zip_bagsize;    // 数量长度 字节
	
	char                 *unzip_catch;   // 避免解压频繁申请内存 

	void                 *cb_source;     // 
	cb_incrzip_encode    *cb_compress;   // 数据以回调方式返回 默认为 maxsize < currpos + 2 * newdbsize	
	cb_incrzip_decode    *cb_uncompress;  // 解压用
}s_sis_incrzip_class;

#pragma pack(pop)

////////////////////////////////////////////////////////
// s_sis_incrzip_class
////////////////////////////////////////////////////////
s_sis_incrzip_class *sis_incrzip_class_create();
void sis_incrzip_class_destroy(s_sis_incrzip_class *);
void sis_incrzip_class_clear(s_sis_incrzip_class *);
// 必须在解压或者压缩前设置数据结构 返回结构索引
int sis_incrzip_set_key(s_sis_incrzip_class *s_, int maxkeys_);
int sis_incrzip_set_sdb(s_sis_incrzip_class *s_, s_sis_dynamic_db *db_);

// 得到数据的字节长度 结构化压缩会保存当前压缩包的数量 因此必须用这个函数才能获得实际的长度 否则在解压时会报错
size_t sis_incrzip_getsize(s_sis_incrzip_class *s_);

// 写在当前位置curpos处
void sis_incrzip_set_bags(s_sis_incrzip_class *s_);

/////////////////////
//  单数据单股票整体压缩
/////////////////////
// sis_incrzip_class_create
// sis_incrzip_set_sdb
// sis_incrzip_compress
// ...
// sis_incrzip_class_destroy

int sis_incrzip_compress(s_sis_incrzip_class *s_, char *in_, size_t ilen_, s_sis_memory *out_);

/////////////////////
//  多数据顺序压缩
/////////////////////

// sis_incrzip_class_create
// sis_incrzip_set_key
// sis_incrzip_set_sdb
// ...
// sis_incrzip_set_sdb
// sis_incrzip_compress_start
// sis_incrzip_compress_step
// ...
// sis_incrzip_compress_step
// sis_incrzip_compress_restart
// sis_incrzip_compress_step
// ...
// sis_incrzip_compress_step
// sis_incrzip_compress_restart
// sis_incrzip_compress_step
// ...
// sis_incrzip_compress_step
// sis_incrzip_compress_stop
// sis_incrzip_class_destroy

// 如果未设置结构体返回错误 压缩数据直接写入 cin_ 避免内存多次拷贝
int sis_incrzip_compress_start(s_sis_incrzip_class *, int maxsize_, void *source_, cb_incrzip_encode *);
// 压缩过程中增加key 
int sis_incrzip_compress_addkey(s_sis_incrzip_class *, int newnums);
// 强制从头开始压缩
void sis_incrzip_compress_restart(s_sis_incrzip_class *s_, int init_);
// 返回成功压缩的数量（包数量，不是记录数量）
int sis_incrzip_compress_step(s_sis_incrzip_class *s_, int kid_, int sid_, char *in_, size_t ilen_);
// 把最后的压缩包返回给调用端
int sis_incrzip_compress_stop(s_sis_incrzip_class *);

// 判断是否起始包
int sis_incrzip_isinit(uint8 *in_, size_t ilen_);

/////////////////////
//  单数据单股票整体解开压缩
/////////////////////
// sis_incrzip_class_create
// sis_incrzip_set_sdb
// sis_incrzip_uncompress
// ...
// sis_incrzip_class_destroy

int sis_incrzip_uncompress(s_sis_incrzip_class *s_, char *cin_, size_t csize_, s_sis_memory *out_);

//////////////////////
//  多数据解开压缩
//////////////////////
// 设置解压回调
int sis_incrzip_uncompress_start(s_sis_incrzip_class *, void *source_, cb_incrzip_decode *);
// 解压 由于并不能事先知道解压的数据结构大小 所以
int sis_incrzip_uncompress_step(s_sis_incrzip_class *, char *cin_, size_t csize_);
// 清理解压痕迹
int sis_incrzip_uncompress_stop(s_sis_incrzip_class *);



#endif