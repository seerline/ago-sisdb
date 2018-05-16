
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_FIELDS_H
#define _STS_FIELDS_H

#include "sds.h"
#include "dict.h"
#include "sts_map.h"
#include <assert.h>
/////////////////////////////////////////////////////////
//  字段类型定义
/////////////////////////////////////////////////////////
//关于时间的定义
#define STS_FIELD_NONE    0 // "TIDX"  //int32 表示序号 0.....1000....
#define STS_FIELD_INDEX   1 // "TIDX"  //int32 表示序号 0.....1000....
#define STS_FIELD_SECOND  2 // "TSEC"  //int32 time_t格式，精确到秒  
#define STS_FIELD_MINUTE  3 // "TMIN"  //int32 time_t格式 / 60，精确到分
#define STS_FIELD_MIN5    4 // "TMIN"  //int32 time_t格式 / 60，精确到分
#define STS_FIELD_DAY     5 // "TDAY"  //int32 20170101格式，精确到天
#define STS_FIELD_MONTH   6 // "TMON"  //int32 201703格式，精确到月
#define STS_FIELD_YEAR    7 // "TYEAR"  //int16 2017格式，精确到年
#define STS_FIELD_TIME    8// "TTIME"  //int64 精确到毫秒的时间  
//8位代码定义
#define STS_FIELD_CODE    9 // "TCODE"  //int64 -- char[8] 8位字符转换为一个int64为索引
//其他类型定义
#define STS_FIELD_CHAR    10 // "CHAR"  //string 类型 后面需跟长度;
//传入格式为 field名称:数据类型:长度; STS_FIELD_CHAR不填长度默认为16;
#define STS_FIELD_INT1    11 // "INT1"  //int 类型 
#define STS_FIELD_INT2    12 // "INT2"  //int 类型 
#define STS_FIELD_INT4    13 // "INT4"  //int 类型 
#define STS_FIELD_INT8    14 // "INT8"  //int 类型 
#define STS_FIELD_INTZ    15 // "INTZ"  //int 类型 压缩整数类型
//--------------------------------------------------------//
// 特殊类型定义的规则是针对价格和成交量的定义，传上来是浮点数或者大整数，需要根据info信息进行放大或缩小
// 用户传来的数据都是原始数据，比如价格，上传为浮点数，但是存储的时候，
// 需要根据info.SH600600 中定义的小数点位数对该数据进行放大，取出数据时把对应数据缩小后返回给用户，
#define STS_FIELD_PRC     18  // "PRC"  //后面需跟长度，默认4，最大不超过8;
// 仅针对json数据格式需要转换
// 针对价格float或double，存储数据前需要针对股票代码到code表中获取对应的prc_decimal 
// 然后对来源的json数据进行放大然后取整保存，取数据时如果要求转为json就对应转为浮点数
// prc_decimal取值范围是 0...15,保留小数位,0表示数据不做处理，直接取整保存
#define STS_FIELD_VOL     19  // "VOL"  //后面需跟长度，默认4，最大不超过8;
// 仅针对json数据格式需要转换
// 针对成交量int32或int64，存储数据前需要针对股票代码到code表中获取对应的vol_zoomout 
// 然后对来源的json数据进行缩小后取整保存，取数据时如果要求转为json就对应放大
// vol_zoomout取值范围是 0..15 默认不缩放，最大放大10^15

#define STS_FIELD_FLOAT   20 // "FLOAT"  //float 
#define STS_FIELD_DOUBLE  21 // "DOUBLE"  //double

//--------32种类型，保留4种类型-------//

#pragma pack(push,1)
#if 0
typedef struct sts_int_packed{
	unsigned len : 3;   // 字段长度(按字节)  0:1,1:2,2:4,3:8,4:16,5:32,6:64,7:128
	unsigned zoom : 5;  // 放大缩小比例,  
	// 0x00000 0x10000 比例不变
	// 0x00001...0x01111 0x0001:缩小10倍1位小数点,0x0010:缩小100倍 ... 
	//                   0x1111:可缩小10^15倍，小数点后15位 （主要为支持比特币）
	//                   超出32位整数范围，即便缩小比例再小，也无法表示，此时数据类型需要为int64
	// 0x10001...0x11111 0x10001:放大10倍,0x10010:放大100倍, ... 最大放大10^15倍
}sts_int_packed;

typedef struct sts_fields_packed{
	unsigned type : 5;      // 数据类型  一共32种数据类型
	unsigned encoding : 3;  // 8种编码方式 
	union {
		sts_int_packed iflag;
		uint8_t slen;   //数据类型为字符串时，用一个字节约表示字符串长度，定义如下：
		// 0 ... 127 表示 1...128个字节长度
		// 128...255 长度计算公式为 =（128-127）*255，最大可以表示长度为32640个字节长度，满足普通文章需求；
	};
}sts_fields_packed;
#endif

// typedef struct s_sts_fields_flags{
// 	unsigned type : 5;      // 数据类型  一共32种数据类型
// 	unsigned encoding : 3;  // 8种编码方式
// 	unsigned len : 6;       // 字段长度(按字节)  0...63 最大不超过64个字符串，公司名称最大限制
// 	unsigned group : 2;     // 一条记录同一类型数据最多分为4组，一般用不到，
// }s_sts_fields_flags;
// typedef struct s_sts_field_unit{
// 	uint8_t  index;   // 位置，结构中第几个字段 最多64个字段
// 	uint16_t offset;  // 偏移位置，结构中第几个字段，偏移多少字节
// 	char name[STS_FIELD_MAXLEN];  // 字段名，
// 	s_sts_fields_flags flags; // attribute字段属性
// }s_sts_field_unit;

typedef struct s_sts_fields_flags{
	unsigned type : 5;    // 数据类型  一共32种数据类型
	unsigned len : 6;     // 字段长度(按字节)  0...63 最大不超过64个字符串，公司名称最大限制
	unsigned io : 1;      // 0 输入时放大 输出时缩小，float类型取小数点 int 取整
	unsigned zoom : 4;    // zoom = 0 不缩放，无论io为何值，实际保存的数据都是int32整型 
	// io = 0 输入时乘以zoom后取整，输出时除以zoom后保留的小数点个数 对float&double有效
	// 最大支持15位小数, 
	// io = 1 输入时除以zoom后取整，输出时乘以zoom后取整 对int有效
	// 最大支持10^15次方,足够表示极大数 
	unsigned ziper : 2;  // none 不压缩 up 上一条 local 和当前记录某字段比较压缩 multi 和指定字段相乘
	unsigned refer : 6;  // 当为up和local的时候，表示和第几个字段比较压缩，local情况下相等为自己压自己
				// ziper==refer==0 表示不压缩 
}s_sts_fields_flags;

#define STS_FIELD_MAXLEN  32

typedef struct s_sts_field_unit{
	uint8_t  index;   // 位置，结构中第几个字段 最多64个字段
	uint16_t offset;  // 偏移位置，结构中第几个字段，偏移多少字节
	char name[STS_FIELD_MAXLEN];  // 字段名，
	s_sts_fields_flags flags; // attribute字段属性
}s_sts_field_unit;

//对table来说，定义一个map指向一个多记录sts_field_unit数据结构

#pragma pack(pop)


//用户传入字段定义时的结构，根据类型来确定其他
//示例："fields":[{"name":"time","type":"TT","zip":"ROW"},
//				  {"name":"close","type":"FL","zip":"ALL"},
//                {"name":"vol","type":"I8","zip":"ROW"},
//                {"name":"name","type":"CH","len":16}];
//
//     会生成第1个时序字段time，长度8B，按列压缩
//			 第2个字段为浮点字段close，值除以100，保留两位小数,压缩方式为all
//           第3个为整型字段vol，长度8B  按列压缩
//			 第2个为字符串name，长度为16 不压缩

s_sts_field_unit *sts_field_unit_create(int index, const char *name_, const char *type_, const char *zip, int len_, int group_);
void sts_field_unit_destroy(s_sts_field_unit *);

bool sts_field_is_times(int t_);

#endif  /* _STS_FIELDS_H */
