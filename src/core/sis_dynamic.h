#ifndef _SIS_DYNAMIC_H
#define _SIS_DYNAMIC_H

#include <sis_core.h>
#include <sis_map.h>
#include <sis_list.h>
#include <sis_conf.h>
#include <sis_csv.h>
#include <sis_math.h>
#include <sis_zint.h>

// 为什么要定义动态结构：
// 每次增加功能都需要同步更新server和client，实际应用中不能保证同步更新，
// 要求是server可以随时升级，client即使不升级也不会导致出错。
// 包含了两个内容： server结构改变，client仍然能够解析出来
//               server结构改变，仍然能够解析client发过来的数据结构
// 流程定义：
// 在server和client端都有一个结构定义表：
//     例如：server.struct.define
//         {   "version":2.0,
//             "s2c":[["time","uint",4],["close","uint",4],["volume","uint",4]], # 比client端多一个字段
//             "c2s":[["open","uint",4]]   # 比client端少一个字段
//         }
//         client.struct.define
//         {   "version":1.0,
//             "s2c":[["time","uint",4],["close","uint",4]],
//             "c2s":[["time","uint",4],["open","uint",4]]
//         }
        
// client连接后，
//     server端会向 client 发送 server.struct.define；
//         client收到后，解析到map的字段数据结构列表中；
//     client向server 发送 client.struct.define;
//         server收到后，解析到map的字段数据结构列表中；

// ###### 双方握手完成 ######

// 当client收到数据后，根据数据头的标志符，调用解析器
// sis_dynamic_analysis(来源数据集名server.s2c，来源数据区，输出数据集名client.s2c，输出数据区)
//     如果 server.s2c == client.s2c 原样返回数据
//     如果 server.s2c != client.s2c 进行数据转换
//         对每个字段 ：style len 相等就直接内存拷贝
//                    其他情况下，需先取值再赋值；
    
//     处理原理：首先配置两个数据集的字段，生成copy方法，
//     不考虑字段名更改的需求，修改字段名成本太高，
//     只考虑字段名增加、删除、字段类型，长度，个数的变化
//     同时，对server方新增结构体发送给客户端，客户端最多返回无法解析，而不会异常
//          对client发给server老的数据结构，只要server的数据集合名还在，就可以解析出来，并为client服务
//     最大的好处是，以后增加服务接口，不用每个接口定义一个数据结构，可以通过 key-value的方式就可以增加接口了
//     只要client收到信息，解析成功后，就直接返回一个 数据集合名 数据区；做好安全检查后，就可以映射到一个数据结构，

#define SIS_DYNAMIC_CHR_LEN   64

#define SIS_DYNAMIC_OK      0
#define SIS_DYNAMIC_ERR     1  // 初始化错误
#define SIS_DYNAMIC_NOKEY   2  // 找不到对应名称结构体
#define SIS_DYNAMIC_NOOPPO  3  // 源头数据没有对应同名结构体
#define SIS_DYNAMIC_SIZE    4  // 数据和字段长度不匹配
#define SIS_DYNAMIC_LEN     5  // 等待转换的数据长度不匹配

#define SIS_DYNAMIC_DICT_LOCAL    0
#define SIS_DYNAMIC_DICT_REMOTE   1

#define SIS_DYNAMIC_FIELD_LIMIT   0xFFFF

// #define SIS_DYNAMIC_METHOD_NONE   0  // 没有关联性
// #define SIS_DYNAMIC_METHOD_COPY   1
// #define SIS_DYNAMIC_METHOD_OWNER  2  // 根据字段各自定义

#define SIS_DYNAMIC_SHIFT_NONE      0  // 类型不同，且双发有一方不是整数类，直接赋值为空或0
#define SIS_DYNAMIC_SHIFT_TYPE    0x1  // 类型相同 
#define SIS_DYNAMIC_SHIFT_UINT    0x2  // 类型不同，但属于整数体系，可以转换 
#define SIS_DYNAMIC_SHIFT_SIZE    0x4  // 长度相同
#define SIS_DYNAMIC_SHIFT_NUMS    0x8  // 数量相同

#define SIS_DYNAMIC_TYPE_NONE   0
#define SIS_DYNAMIC_TYPE_INT    'I'  // 1 2 4 8
#define SIS_DYNAMIC_TYPE_UINT   'U'  // 1 2 4 8
#define SIS_DYNAMIC_TYPE_CHAR   'C'  // 1 -- N
#define SIS_DYNAMIC_TYPE_FLOAT  'F'  // 4 8
#define SIS_DYNAMIC_TYPE_PRICE  'P'  // 4 8
// 时间类型 定义
// 不要想全部用毫秒 来区分如何存储 如何存储应该是上层来决定的
// 比如存储参数增加按年聚合 按月聚合 默认按日聚合数据
#define SIS_DYNAMIC_TYPE_WSEC   'W'  // 微秒 8  
#define SIS_DYNAMIC_TYPE_MSEC   'T'  // 毫秒 8  
#define SIS_DYNAMIC_TYPE_TSEC   'S'  // 秒   4 8  
#define SIS_DYNAMIC_TYPE_MINU   'M'  // 分钟 4 time_t/60
#define SIS_DYNAMIC_TYPE_DATE   'D'  // 天 4 20010101
#define SIS_DYNAMIC_TYPE_YEAR   'Y'  // 年 4 2001

#define SIS_DYNAMIC_DIR_RTOL     0
#define SIS_DYNAMIC_DIR_LTOR     1

#pragma pack(push,1)

typedef struct s_sis_dynamic_field {    
    char           fname[255];  // 字段名 类型不要动 比较时有用
                               // 以字段名为唯一检索标记，如果用户对字段名
    unsigned char  style;      // 数据类型
    unsigned short len;        // 数据长度
    unsigned short count;      // 该字段重复多少次
    unsigned char  dot;        // 输出为字符串时保留的小数点
    unsigned char  mindex;     // 该字段是否为主索引
    unsigned char  solely;     // 该字段是否为唯一值
	unsigned short offset;     // 在该结构的偏移位置
} s_sis_dynamic_field;

//***********************注意***********************//
// 数据转换时，为安全起见，不同类型不能互转，不同类型一律转为空或0，便于排错 
//		（例如：52.75 转整型 52，当发生在卖出时错误严重，设置为0 便于程序员自行处理）
//		 int和uint例外，当转换时越界，一律设置为0 
// -------------------------------------------------
// 这里的转换，是指同一数据类型，长度和数量的变化
//       例如：short 转为 long long 和 float 转为 double
////////////////////////////////////////////////////
typedef struct s_sis_dynamic_db {
    // 描述结构体的标志符号 // 类别 . 名称 . 版本 
	int                       refs;           // 引用数
	s_sis_sds                 name;     
	s_sis_map_list           *fields;         // 字段信息 s_sis_dynamic_field
    unsigned int              size;           // 结构总长度
    s_sis_dynamic_field      *field_time;     // 时间对应字段 根据字段类型对应 仅仅有一个
	s_sis_dynamic_field      *field_mindex;   // 主索引字段 字段索引 仅仅有一个
	s_sis_pointer_list       *field_solely;   // 唯一性字段集合 字段索引  
} s_sis_dynamic_db;

////////////////////////////////////////////////////
// 单个db转换为其他结构 传入需要转换的db
////////////////////////////////////////////////////
typedef struct s_sis_dynamic_field_convert {    
	s_sis_dynamic_field  *in_field;         // 对应的字段指针，s_sis_dynamic_field 为空表示不做转换
	s_sis_dynamic_field  *out_field;         // 对应的字段指针，s_sis_dynamic_field 为空表示不做转换
	void(*cb_shift)(void *, void *, void *);   // 转移方法 == NULL 什么都不做
} s_sis_dynamic_field_convert;

typedef struct s_sis_dynamic_convert {
    int error;                   // 描述错误编号
    s_sis_dynamic_db    *in;     // 输入结构字典 s_sis_dynamic_db
    s_sis_dynamic_db    *out;    // 输出结构字典 s_sis_dynamic_db
    s_sis_map_list      *map_fields;     // 字段的转移回调 s_sis_dynamic_field_convert
    void(*cb_convert)(void *, void *, size_t, void *,size_t);  // 整个数据块的转移回调
} s_sis_dynamic_convert;

// // 多个同名的db 转换结构
// typedef struct s_sis_dynamic_class {
//     int error;                 // 描述错误编号
//     s_sis_map_pointer *local;  // 本地的结构字典 s_sis_dynamic_db 以结构名为key
//     s_sis_map_pointer *remote;   // 从网络接收的结构字典 s_sis_dynamic_db 以结构名为key
// } s_sis_dynamic_class;

#pragma pack(pop)

static inline uint64 _sis_field_get_uint(s_sis_dynamic_field *unit_, const char *val_, int index_)
{
	uint64  o = 0;
	uint8  *v8;
	uint16 *v16;
	uint32 *v32;
	uint64 *v64;
	// char info[128];
	// sis_sprintf(info, 128, "%s %d %d %d %d",unit_->name, unit_->offset, unit_->len, unit_->count, index_);
	// sis_out_binary(info, val_ + unit_->offset, 16);
	switch (unit_->len)
	{
	case 1:
		v8 = (uint8 *)(val_ + unit_->offset + index_*sizeof(uint8));
		o = *v8;
		break;
	case 2:
		v16 = (uint16 *)(val_ + unit_->offset + index_*sizeof(uint16));
		o = *v16;
		break;
	case 4:
		v32 = (uint32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = *v32;
		break;
	case 8:
		v64 = (uint64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = *v64;
		break;
	default:
		break;
	}
	return o;
}
static inline int64 _sis_field_get_int(s_sis_dynamic_field *unit_, const char *val_, int index_)
{
	int64  o = 0;
	int8  *v8;
	int16 *v16;
	int32 *v32;
	int64 *v64;

	switch (unit_->len)
	{
	case 1:
		v8 = (int8 *)(val_ + unit_->offset + index_*sizeof(uint8));
		o = *v8;
		break;
	case 2:
		v16 = (int16 *)(val_ + unit_->offset + index_*sizeof(uint16));
		o = *v16;
		break;
	case 4:
		v32 = (int32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = *v32;
		break;
	case 8:
		v64 = (int64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = *v64;
		break;
	default:
		break;
	}
	return o;
}
static inline double _sis_field_get_float(s_sis_dynamic_field *unit_, const char *val_, int index_)
{
	double   o = 0.0;
	float32 *f32;
	float64 *f64;
	switch (unit_->len)
	{
	case 4:
		f32 = (float32 *)(val_ + unit_->offset + index_*sizeof(float32));
		o = (double)*f32;
		break;
	case 8:
		f64 = (float64 *)(val_ + unit_->offset + index_*sizeof(float64));
		o = (double)*f64;
		break;
	default:
		break;
	}
	return o;
}

static inline int _sis_field_get_price_dot(s_sis_dynamic_field *unit_, const char *val_, int index_)
{
	int   o = 0;
	zint32 *v32;
	zint64 *v64;
	switch (unit_->len)
	{
	case 4:
		v32 = (zint32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = sis_zint32_dot(*v32);
		break;
	case 8:
		v64 = (zint64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = sis_zint64_dot(*v64);
		break;
	default:
		break;
	}
	return o;	
}
static inline bool _sis_field_get_price_valid(s_sis_dynamic_field *unit_, const char *val_, int index_)
{
	bool   o = true;
	zint32 *v32;
	zint64 *v64;
	switch (unit_->len)
	{
	case 4:
		v32 = (zint32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = sis_zint32_valid(*v32);
		break;
	case 8:
		v64 = (zint64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = sis_zint64_valid(*v64);
		break;
	default:
		break;
	}
	return o;	
}
static inline double _sis_field_get_price(s_sis_dynamic_field *unit_, const char *val_, int index_)
{
	double   o = 0.0;
	zint32 *v32;
	zint64 *v64;
	switch (unit_->len)
	{
	case 4:
		v32 = (zint32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = sis_zint32_to_double(*v32);
		break;
	case 8:
		v64 = (zint64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = sis_zint64_to_double(*v64);
		break;
	default:
		break;
	}
	return o;
}
static inline void _sis_field_set_uint(s_sis_dynamic_field *unit_, char *val_, uint64 v64_, int index_)
{
	uint8   v8 = 0;
	uint16 v16 = 0;
	uint32 v32 = 0;
	uint64 v64 = 0;
	switch (unit_->len)
	{
	case 1:
		v8 = (uint8)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint8), &v8, unit_->len);
		break;
	case 2:
		v16 = (uint16)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint16), &v16, unit_->len);
		break;
	case 4:
		v32 = (uint32)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint32), &v32, unit_->len);
		break;
	case 8:
		v64 = (uint64)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint64), &v64, unit_->len);
		break;
	default:
		break;
	}	
}

static inline void _sis_field_set_int(s_sis_dynamic_field *unit_, char *val_, int64 v64_, int index_)
{
	int8   v8 = 0;
	int16 v16 = 0;
	int32 v32 = 0;
	int64 v64 = 0;

	switch (unit_->len)
	{
	case 1:
		v8 = (int8)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint8), &v8, unit_->len);
		break;
	case 2:
		v16 = (int16)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint16), &v16, unit_->len);
		break;
	case 4:
		v32 = (int32)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint32), &v32, unit_->len);
		break;
	case 8:
		v64 = (int64)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint64), &v64, unit_->len);
		break;
	default:
		break;
	}	
}

static inline void _sis_field_set_float(s_sis_dynamic_field *unit_, char *val_, double f64_, int index_)
{
	float32 f32 = 0.0;
	float64 f64 = 0.0;
	switch (unit_->len)
	{
	case 4:
		f32 = (float32)f64_;
		memmove(val_ + unit_->offset + index_ * sizeof(float32), &f32, unit_->len);
		break;
	case 8:
	default:
		f64 = (float64)f64_;
		memmove(val_ + unit_->offset + index_ * sizeof(float64), &f64, unit_->len);
		break;
	}
}

static inline void _sis_field_set_price(s_sis_dynamic_field *unit_, char *val_, double f64_, int dot_, int valid_, int index_)
{
	zint32 v32 = {0};
	zint64 v64 = {0};
	int dot = dot_ < 0 ? unit_->dot : dot_;
	switch (unit_->len)
	{
	case 4:
		v32 = sis_double_to_zint32(f64_, dot, valid_);
		memmove(val_ + unit_->offset + index_*sizeof(float32), &v32, unit_->len);
		break;
	case 8:
	default:
		v64 = sis_double_to_zint64(f64_, dot, valid_);
		memmove(val_ + unit_->offset + index_*sizeof(float64), &v64, unit_->len);
		break;
	}

}
static inline s_sis_sds sis_dynamic_field_to_csv(s_sis_sds in_, s_sis_dynamic_field *field_, const char *val_)
{
	if(field_) 
	{
		for(int index = 0; index < field_->count; index++)
		{
			switch (field_->style)
			{
			case SIS_DYNAMIC_TYPE_INT:
				in_ = sis_csv_make_int(in_, _sis_field_get_int(field_, val_, index));
				break;
			case SIS_DYNAMIC_TYPE_TSEC:
			case SIS_DYNAMIC_TYPE_MSEC:
			case SIS_DYNAMIC_TYPE_MINU:
			case SIS_DYNAMIC_TYPE_DATE:
			case SIS_DYNAMIC_TYPE_UINT:
				in_ = sis_csv_make_uint(in_, _sis_field_get_uint(field_, val_, index));
				break;
			case SIS_DYNAMIC_TYPE_FLOAT:
				{
					in_ = sis_csv_make_double(in_, _sis_field_get_float(field_, val_, index), field_->dot);
				}
				break;
			case SIS_DYNAMIC_TYPE_PRICE:
				{
					int dot = _sis_field_get_price_dot(field_, val_, index);
					in_ = sis_csv_make_double(in_, _sis_field_get_price(field_, val_, index), dot > 0 ? dot : field_->dot);
				}
				break;
			case SIS_DYNAMIC_TYPE_CHAR:
				in_ = sis_csv_make_str(in_, val_ + field_->offset + index*field_->len, field_->len);
				break;
			default:
				in_ = sis_csv_make_str(in_, " ", 1);
				break;
			}
		}
	}
	return in_;
}

static inline void sis_dynamic_field_to_array(s_sis_json_node *in_, s_sis_dynamic_field *field_, const char *val_)
{
	if(field_) 
	{
		for(int index = 0; index < field_->count; index++)
		{
			switch (field_->style)
			{
			case SIS_DYNAMIC_TYPE_INT:
				sis_json_array_add_int(in_, _sis_field_get_int(field_, val_, index));
				break;
			case SIS_DYNAMIC_TYPE_TSEC:
			case SIS_DYNAMIC_TYPE_MSEC:
			case SIS_DYNAMIC_TYPE_MINU:
			case SIS_DYNAMIC_TYPE_DATE:
			case SIS_DYNAMIC_TYPE_UINT:
				sis_json_array_add_uint(in_, _sis_field_get_uint(field_, val_, index));
				break;
			case SIS_DYNAMIC_TYPE_FLOAT:
				{
					sis_json_array_add_double(in_, _sis_field_get_float(field_, val_, index), field_->dot);
				}
				break;
			case SIS_DYNAMIC_TYPE_PRICE:
				{
					int dot = _sis_field_get_price_dot(field_, val_, index);
					sis_json_array_add_double(in_, _sis_field_get_price(field_, val_, index), dot > 0 ? dot : field_->dot);
				}
				break;
			case SIS_DYNAMIC_TYPE_CHAR:
				sis_json_array_add_string(in_, val_ + field_->offset + index*field_->len, field_->len);
				break;
			default:
				sis_json_array_add_string(in_, " ", 1);
				break;
			}
		}
	}
}
// index_ 表示为当前字段的第几个 默认为 0 必须保证 out足够大
static inline void sis_dynamic_field_json_to_struct(s_sis_sds out_, s_sis_dynamic_field *field_, int index_,
			char *key_, s_sis_json_node *innode_)
{

	int64  i64 = 0;
	double f64 = 0.0;
	const char *str;
	switch (field_->style)
	{
	case SIS_DYNAMIC_TYPE_CHAR:
		str = sis_json_get_str(innode_, key_);
		if (str)
		{
			int len = sis_min(field_->len, strlen(str));
			memmove(out_ + field_->offset + index_ *field_->len, str, len);
			if (len < field_->len)
			{
				out_[field_->offset + index_*field_->len + len] = 0;
			}
		}
		// sis_out_binary("update 0 ", in_, 60);
		break;
    case SIS_DYNAMIC_TYPE_TSEC:
    case SIS_DYNAMIC_TYPE_MSEC:
    case SIS_DYNAMIC_TYPE_MINU:
    case SIS_DYNAMIC_TYPE_DATE:
    case SIS_DYNAMIC_TYPE_UINT:
		if (sis_json_find_node(innode_, key_))
		{
			i64 = sis_json_get_int(innode_, key_, 0);
			_sis_field_set_uint(field_, out_, (uint64)i64, index_);
		}
		break;
	case SIS_DYNAMIC_TYPE_INT:
		if (sis_json_find_node(innode_, key_))
		{
			i64 = sis_json_get_int(innode_, key_, 0);
			_sis_field_set_int(field_, out_, i64, index_);
		}
		break;
	case SIS_DYNAMIC_TYPE_FLOAT:
		if (sis_json_find_node(innode_, key_))
		{
			f64 = sis_json_get_double(innode_, key_, 0.0);
			_sis_field_set_float(field_, out_, f64, index_);
		}
		break;
	case SIS_DYNAMIC_TYPE_PRICE:
		if (sis_json_find_node(innode_, key_))
		{
			f64 = sis_json_get_double(innode_, key_, 0.0);
			int valid = sis_json_get_valid(innode_, key_);
			_sis_field_set_price(field_, out_, f64, -1, valid, index_);
		}
		break;
	default:
		break;
	}
}
//////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////
s_sis_dynamic_field *sis_dynamic_field_create(const char *name_);
void sis_dynamic_field_destroy(void *db_);

s_sis_dynamic_field *sis_dynamic_db_add_field(s_sis_dynamic_db *db_, const char *fname_, int style, int ilen, int count, int dot);
// 得到时间字段的精度
int sis_dynamic_field_scale(int style_);

s_sis_dynamic_db *sis_dynamic_db_create(s_sis_json_node *node_);
// 一种无结构的动态表
s_sis_dynamic_db *sis_dynamic_db_create_none(const char *name_, size_t size_);
void sis_dynamic_db_destroy(void *db_);

void sis_dynamic_db_setname(s_sis_dynamic_db *db_, const char *name_);
void sis_dynamic_db_incr(s_sis_dynamic_db *db_);
void sis_dynamic_db_decr(s_sis_dynamic_db *db_);

s_sis_dynamic_field *sis_dynamic_db_get_field(s_sis_dynamic_db *db_, int *index_, const char *field_);

// 比较两个表的结构 一样返回 true
bool sis_dynamic_dbinfo_same(s_sis_dynamic_db *db1_, s_sis_dynamic_db *db2_);

// 获得当前缓存的时间
msec_t sis_dynamic_db_get_time(s_sis_dynamic_db *db_, int index_, void *in_, size_t ilen_);
// 获得当前索引的值 以长整数返回
uint64 sis_dynamic_db_get_mindex(s_sis_dynamic_db *db_, int index_, void *in_, size_t ilen_);

///////////////////////////////
// 转换对象定义
///////////////////////////////
s_sis_dynamic_convert *sis_dynamic_convert_create(s_sis_dynamic_db *in_, s_sis_dynamic_db *out_);

void sis_dynamic_convert_destroy(void *);
// 得到转换目标的长度
size_t sis_dynamic_convert_length(s_sis_dynamic_convert *cls_, const char *in_, size_t ilen_);
// 得到转换数据
int sis_dynamic_convert(s_sis_dynamic_convert *cls_, 
		const char *in_, size_t ilen_, char *out_, size_t olen_);

uint64 sis_time_unit_convert(int instyle, int outstyle, uint64 in64);

// // 参数为json结构的数据表定义,必须两个数据定义全部传入才创建成功
// // 同名的自动生成link信息，不同名的没有link信息
// // 同一个来源其中的 数据集合名称 不能重复  remote_ 表示对方配置  local_ 表示本地配置
// s_sis_dynamic_class *sis_dynamic_class_create_of_json(
//     const char *remote_,size_t rlen_,
//     const char *local_,size_t llen_);
// s_sis_dynamic_class *sis_dynamic_class_create_of_conf(
//     const char *remote_,size_t rlen_,
//     const char *local_,size_t llen_);

// // 参数为conf结构的配置文件,必须两个数据定义全部传入才创建成功
// s_sis_dynamic_class *sis_dynamic_class_create_of_conf_file(
//     const char *rfile_,const char *lfile_);
// s_sis_dynamic_class *sis_dynamic_class_create_of_json_file(
//     const char *rfile_,const char *lfile_);

// void sis_dynamic_class_destroy(s_sis_dynamic_class *);

// // 返回0表示数据处理正常，解析数据要记得释放内存
// // 可以对远端和本地的数据结构相互转换，对于没有对应关系的结构体，求长度时返回的是 0 
// // 仅仅只处理服务端下发的结构集合，对于服务端已经不支持的结构，客户端应该强制升级处理
// // 对于服务端有,客户端没有的结构体，客户端应该收到后直接抛弃

// // 本地数据转远程结构 得到长度 
// size_t sis_dynamic_analysis_ltor_length(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_);

// int sis_dynamic_analysis_ltor(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_,
//         char *out_, size_t olen_);

// // 远程结构转本地结构
// size_t sis_dynamic_analysis_rtol_length(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_);

// int sis_dynamic_analysis_rtol(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_,
//         char *out_, size_t olen_);

// s_sis_sds sis_dynamic_struct_to_json(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_, int dir_,
//         const char *in_, size_t ilen_);


#endif //_SIS_DYNAMIC_H


// ###### 动态结构 + 持久化文件处理流程和格式 ######
// 每个子包数据块大小不超过