#ifndef _SIS_DYNAMIC_H
#define _SIS_DYNAMIC_H

#include <sis_core.h>
#include <sis_map.h>
#include <sis_list.h>
#include <sis_conf.h>
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



#define SIS_DYNAMIC_CHR_LEN   32

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
#define SIS_DYNAMIC_TYPE_INT    'I'
#define SIS_DYNAMIC_TYPE_UINT   'U'
#define SIS_DYNAMIC_TYPE_CHAR   'C'
#define SIS_DYNAMIC_TYPE_FLOAT  'F'

#define SIS_DYNAMIC_DIR_RTOL     0
#define SIS_DYNAMIC_DIR_LTOR     1

#pragma pack(push,1)

typedef struct s_sis_dynamic_unit {    
    char  name[SIS_DYNAMIC_CHR_LEN];  // 字段名
                                        // 以字段名为唯一检索标记，如果用户对字段名
    unsigned char  style;      // 数据类型
    unsigned short len;        // 数据长度
    unsigned short count;      // 该字段重复多少次
    unsigned char  dot;        // 输出为字符串时保留的小数点
	unsigned short offset;     // 在该结构的偏移位置
	struct s_sis_dynamic_unit *map_unit;      // 对应的字段指针，s_sis_dynamic_unit 为空表示不做转换
	void(*shift)(void *, void *, void *);      // 转移方法 == NULL 什么都不做
} s_sis_dynamic_unit;

//***********************注意***********************//
// 数据转换时，为安全起见，不同类型不能互转，不同类型一律转为空或0，便于排错 
//		（例如：52.75 转整型 52，当发生在卖出时错误严重，设置为0 便于程序员自行处理）
//		 int和uint例外，当转换时越界，一律设置为0 
// -------------------------------------------------
// 这里的转换，是指同一数据类型，长度和数量的变化
//       例如：short 转为 long long 和 float 转为 double
////////////////////////////////////////////////////
typedef struct s_sis_dynamic_db {
	char   name[SIS_DYNAMIC_CHR_LEN];    // 描述结构体的标志符号
    //                  // 类别 . 名称 . 版本 
	s_sis_struct_list        *fields;     // 用顺序结构体来保存字段信息 s_sis_dynamic_unit
    unsigned short            size;       // 结构总长度
    // unsigned char             method;     // 转移方法  0 - 没有关联 1 - 直接拷贝 2 - 根据字段定义的方法取值
	struct s_sis_dynamic_db  *map_db;     // 对应的 s_sis_dynamic_db 为空表示不做转换
    void(*method)(void *, void *, size_t, void *,size_t); 
    int(*compress)(void *, size_t, void *); 
    int(*uncompress)(void *, size_t, void *);  // 没有info信息
    int(*refer_compress)(void *,void *, size_t, void *); 
    int(*refer_uncompress)(void *,void *, size_t, void *);  // 没有info信息
} s_sis_dynamic_db;

typedef struct s_sis_dynamic_class {
    // int dot;                   // 转字符串时，float保留的小数位
    int error;                 // 描述错误编号
    s_sis_map_pointer *local;  // 本地的结构字典 s_sis_dynamic_db 以结构名为key
    s_sis_map_pointer *remote;   // 从网络接收的结构字典 s_sis_dynamic_db 以结构名为key
} s_sis_dynamic_class;

#pragma pack(pop)

s_sis_dynamic_db *sis_dynamic_db_create(s_sis_json_node *node_);
void sis_dynamic_db_destroy(s_sis_dynamic_db *db_);

s_sis_sds sis_dynamic_db_to_conf(s_sis_dynamic_db *db_, s_sis_sds in_);

// 参数为json结构的数据表定义,必须两个数据定义全部传入才创建成功
// 同名的自动生成link信息，不同名的没有link信息
// 同一个来源其中的 数据集合名称 不能重复  remote_ 表示对方配置  local_ 表示本地配置
s_sis_dynamic_class *sis_dynamic_class_create_of_json(
    const char *remote_,size_t rlen_,
    const char *local_,size_t llen_);
s_sis_dynamic_class *sis_dynamic_class_create_of_conf(
    const char *remote_,size_t rlen_,
    const char *local_,size_t llen_);

// 参数为conf结构的配置文件,必须两个数据定义全部传入才创建成功
s_sis_dynamic_class *sis_dynamic_class_create_of_conf_file(
    const char *rfile_,const char *lfile_);
s_sis_dynamic_class *sis_dynamic_class_create_of_json_file(
    const char *rfile_,const char *lfile_);

void sis_dynamic_class_destroy(s_sis_dynamic_class *);

// 返回0表示数据处理正常，解析数据要记得释放内存
// 可以对远端和本地的数据结构相互转换，对于没有对应关系的结构体，求长度时返回的是 0 
// 仅仅只处理服务端下发的结构集合，对于服务端已经不支持的结构，客户端应该强制升级处理
// 对于服务端有,客户端没有的结构体，客户端应该收到后直接抛弃

// 本地数据转远程结构
size_t sis_dynamic_analysis_ltor_length(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_);

int sis_dynamic_analysis_ltor(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_,
        char *out_, size_t olen_);

// 远程结构转本地结构
size_t sis_dynamic_analysis_rtol_length(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_);

int sis_dynamic_analysis_rtol(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_,
        char *out_, size_t olen_);

s_sis_sds sis_dynamic_struct_to_json(
		s_sis_dynamic_class *cls_, 
        const char *key_, int dir_,
        const char *in_, size_t ilen_);

s_sis_sds sis_dynamic_conf_to_array_sds(const char *dbstr_, void *in_, size_t ilen_); 

s_sis_sds sis_dynamic_db_to_array_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_); 

#endif //_SIS_DYNAMIC_H


// ###### 动态结构 + 持久化文件处理流程和格式 ######
// 每个子包数据块大小不超过