#ifndef _SIS_BITS_H
#define _SIS_BITS_H

#include "sis_core.h"
#include "sis_dynamic.h"
#include "sis_sds.h"

/////////////////////////////////////
// *** 专用于网络结构化数据压缩传输 *** //
// 默认需要key的dict和结构sdb的dict   //
// 如果两端的dict不匹配数据解析错误     //
/////////////////////////////////////

// 索引整数 正整数 0 - 0xFFFFFFFF
// 00 --> 0-F
// 01 --> 10-FF
// 100 --> 100-FFF 
// 101 --> 1000-FFFF  
// 110 --> 10000-FFFFFF 
// 111 --> 1000000-FFFFFFFF  
// 

// 递增整数 带符号位
// 递增整数 带符号位
// 0表示和前值一样
// 10 --> 0-F
// 11 + 000-010
//    000 :  -FF…FF
//    001 :  -FFF…FFF
//    010 :  -FFFF…FFFF
//    011 :  -FFFFF…FFFFF
// 111 + 000-010
//    000 :  -FFFFFF…FFFFFF
//    001 :  -FFFFFFF…FFFFFFF
//    010 :  -FFFFFFFF…FFFFFFFF
//    011 :  -FFFFFFFFF…FFFFFFFFF
//    100 :  -FFFFFFFFFF…FFFFFFFFFF
//    101 :  -FFFFFFFFFFFF…FFFFFFFFFFFF
//    110 :  -FFFFFFFFFFFFFF…FFFFFFFFFFFFFF
//    111 :  -7FFFFFFFFFFFFFFF…7FFFFFFFFFFFFFFF

// 浮点数
// 用小数位乘后得到整数压缩

// 字符串 
// 0 表示和前值一样
// 1 + size长度+字符 ….

typedef int (cb_sis_struct_decode)(void *, int ,int , char *, size_t);

#pragma pack(push,1)

// // 一个数据结构的定义
typedef struct s_sis_struct_unit{
	uint32               offset;
	s_sis_dynamic_db    *sdb;            
}s_sis_struct_unit;

// 可装载多个结构的数据流
typedef struct s_sis_bits_stream{
	uint8                *cur_stream;	// 目标缓冲区
	uint32                bit_maxsize;  // 缓冲区大小，Bit

	uint32                bit_currpos;  // 当前操作位置
	uint32                bit_savepos;	// 上一次保存的位置 Bit

	// 以下是数据缓存定义 根据内存偏移快速定位数据
	uint8                 inited;       // 是否初始化为结构体存储
	s_sis_pointer_list   *units;        // 数据结构说明 s_sis_struct_unit
	uint32                sdbsize;     // 结构体大小
	uint32                max_keynum;    // 最大key的数量 超出不处理
	uint8                *ago_memory;  // 前值数据缓存 大小为 keycount * sdbcount * sdbsize               
	// 数据格式 : kid + sid + count + 数据区
}s_sis_bits_stream;

#pragma pack(pop)

// #define SETBITCODE(_s_,_b_) { ((s_sis_bit_stream *)_s_)->m_nowcode_p = &_b_[0]; ((s_sis_bit_stream *)_s_)->m_nowcode_count = sizeof(_b_)/sizeof(_b_[0]); }

// // #define SETBITCODE(_s_,_b_) {  }

////////////////////////////////////////////////////////
// s_sis_bits_stream
////////////////////////////////////////////////////////

s_sis_bits_stream *sis_bits_stream_create(uint8 *in_, size_t ilen_);
void sis_bits_stream_destroy(s_sis_bits_stream *);
void sis_bits_stream_clear(s_sis_bits_stream *);
// 得到数据的字节长度
size_t sis_bits_stream_getbytes(s_sis_bits_stream *s_);

void sis_bits_stream_checkin(s_sis_bits_stream *s_, int bits_);
// 直接定位到新的位置
int sis_bits_stream_moveto(s_sis_bits_stream *s_, int bitpos_);
//移动内部指针bits_位,可以为负，返回新的位置
int sis_bits_stream_move(s_sis_bits_stream *s_, int bits_);

int sis_bits_stream_move_bytes(s_sis_bits_stream *s_, int bytes_);
// 保存当前位置
void sis_bits_stream_savepos(s_sis_bits_stream *s_);	
//回卷到上一次保存的位置
int sis_bits_stream_restore(s_sis_bits_stream *s_);
// 得到一个整数 bits_ 取值 0..64
uint64 sis_bits_stream_get(s_sis_bits_stream *s_, int bits_);
// 设置一个整数 返回偏移的位
int sis_bits_stream_put(s_sis_bits_stream *s_, uint64 in_, int bits_);
// 写入缓存数据
int sis_bits_stream_put_buffer(s_sis_bits_stream *s_, char *in_, size_t bytes_);

// 以下的都要求能够自解析
// 记录数
int sis_bits_stream_put_nums(s_sis_bits_stream *s_, uint32 in_);
// 短正整数
int sis_bits_stream_put_uint(s_sis_bits_stream *s_, uint32 in_);
// 带符号长整数
int sis_bits_stream_put_int(s_sis_bits_stream *s_, int64 in_);
int sis_bits_stream_put_incr_int(s_sis_bits_stream *s_, int64 in_, int64 ago_);
// 最大9位小数
int sis_bits_stream_put_float(s_sis_bits_stream *s_, double in_, int dot_);
int sis_bits_stream_put_incr_float(s_sis_bits_stream *s_, double in_, double ago_, int dot_);

int sis_bits_stream_put_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_);
int sis_bits_stream_put_incr_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_, char *ago_, size_t alen_);

// 记录数
int sis_bits_stream_get_nums(s_sis_bits_stream *s_);
// 短正整数
uint32 sis_bits_stream_get_uint(s_sis_bits_stream *s_);
// 带符号长整数
int64 sis_bits_stream_get_int(s_sis_bits_stream *s_);
int64 sis_bits_stream_get_incr_int(s_sis_bits_stream *s_, int64 ago_);

double sis_bits_stream_get_float(s_sis_bits_stream *s_, int dot_);
double sis_bits_stream_get_incr_float(s_sis_bits_stream *s_, double ago_, int dot_);

int sis_bits_stream_get_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_);
int sis_bits_stream_get_incr_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_, char *ago_, size_t alen_);

////////////////////////////
// 以下是结构化读取和写入的函数
////////////////////////////
int sis_bits_struct_set_sdb(s_sis_bits_stream *s_, s_sis_dynamic_db *db_);
int sis_bits_struct_set_key(s_sis_bits_stream *s_, int keynum_);

// 关联一个新的数据区 不改变unit和agomemory
void sis_bits_struct_link(s_sis_bits_stream *s_, uint8 *in_, size_t ilen_);
// 重新开始压缩
void sis_bits_struct_flush(s_sis_bits_stream *s_);

int sis_bits_struct_encode(s_sis_bits_stream *s_, int kid_, int sid_, void *in_, size_t ilen_);
int sis_bits_struct_decode(s_sis_bits_stream *s_, void *source_, cb_sis_struct_decode *);



#endif