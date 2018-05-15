
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_ZIP_H
#define _STS_ZIP_H

/////////////////////////////////////////////////////////
//  压缩编码方式定义 保留4种方式
/////////////////////////////////////////////////////////
#define STS_ENCODEING_SRC  0x000  // 无论何时都不压缩  SRC
#define STS_ENCODEING_ROW  0x001  // 只和当前记录同类型数据压缩 ROW
#define STS_ENCODEING_COL  0x010  // 只和上一条记录同一列数据压缩 COL
#define STS_ENCODEING_ALL  0x011  // 以上一条记录同类型数据为参考，压缩当前记录同类型数据  ALL
#define STS_ENCODEING_STR  0x100  // 自己压缩自己，专用于无规律字符串压缩  STR
#define STS_ENCODEING_COD  0x101  // 自己压缩自己，专用于数字和字母的压缩  COD
#define STS_ENCODEING_O22  0x110  // 保留
#define STS_ENCODEING_O33  0x111  // 保留

/////////////////////////////////////////////////////////
//  数据压缩模式
/////////////////////////////////////////////////////////
#define STS_ZIP_NO       0  // 不压缩
#define STS_ZIP_ONE      1  // 1条记录一个包
#define STS_ZIP_BLOCK    2  // 根据数据块大小压缩数据,STS_ZIP_SIZE
#define STS_ZIP_SIZE     (4 * 1024)  //压缩块最大长度，

//压缩数据结构
//第一部分为字段定义，包括字段个数，字段名+字段属性
//........第一部分可以单独传输，也可以和数据区一起传输，取决于请求的方式，默认是一起传输，
//第二部分为数据块，第一个字符表示是否压缩
//                  R...表示压缩
//                  B...表示二进制不数据
//                  {...表示JSON数据格式
//					[...表示数组数据格式
//                  S...表示字符串
//    第二部分数据最大不超过STS_ZIP_SIZE，如果一条记录就超过就分块，记录数写0，分块后，必须保证一块传完再传下一条记录；
//标志符写完写记录个数，250=250 ................ 0 -- 250
//						251+N=250+N+1 .......... 251 -- 506
//						252+N=(250+256)+N+1..... 507 -- 762
//						253+N=(250+2*256)+N+1... 763 -- 1018
//						254+N=(250+3*256)+N+1... 1019 -- 1274
//                      255后面跟2位=65535对4K数据来说，即便1位一个数据也是足够；
//当前数据块起始压缩的数值，不压缩，---- 
//再然后是实际的数据区，存储一个数据包的数据；
//当前数据块结束，用于下一个记录的压缩数值，不压缩 ---- 该块对于网络传输是非必要的
//顺序保存压缩数据

// 在压缩包中，字段的定义

typedef struct sts_fields_zip{
	unsigned index : 6; // 当为up和local的时候，表示和第几个字段比较压缩，local情况下相等为自己压自己
	unsigned type : 2;  // none 不压缩 up 上一条 local 和当前记录某字段比较压缩 
}sts_fields_zip;
// typedef struct sts_fields_zip{
// 	unsigned type : 5;      // 数据类型  一共32种数据类型
// 	unsigned encoding : 3;  // 8种编码方式
// }sts_fields_zip;
// 如果字段中有STS_FIELD_PRC或者STS_FIELD_VOL，那么字段定义完紧跟的就是下面的结构
typedef struct sts_fields_zip_other{
	unsigned prc_decimal : 4;  // 价格保留小数点
	unsigned vol_zoomout : 4;  // 成交量放大倍数
}sts_fields_zip_other;


#endif  /* _STS_ZIP_H */
