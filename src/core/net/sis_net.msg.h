
#ifndef _SIS_NET_MSG_H
#define _SIS_NET_MSG_H

#include <sis_core.h>
#include <sis_memory.h>
#include <sis_list.h>
#include <sis_map.h>

//--------------------------------------------------------------//
// 正常网络包处理流程
// 1.收到数据 判断包是否完整 完整就切断数据原样放到队列中
// 2.处理完整包 以name+:为头 之后第一个字符决定后面数据格式
// 3.{ 表示为字符串的请求 B表示后面数据为二进制格式 这样设计可以无缝对接网页的请求和响应
// 转发网络包处理流程
// 1.如果是转发订阅 直接根据name 从网络层收到数据就转发给其他客户端了 
//--------------------------------------------------------------//
// request 方决定网络数据传输方式 是按字节流还是 JSON 字符串
#define SIS_NET_FORMAT_CHARS   0 
#define SIS_NET_FORMAT_BYTES   1 
// 只有ver和fmt 会作为请求方的默认值保留 除非客户再次更新ver和fmt否则按 默认->上次传送值

// -126..0 为应答的错误定义
#define SIS_NET_TAG_INVALID   -100  // 请求已经失效
#define SIS_NET_TAG_NIL         -3  // 数据为空
#define SIS_NET_TAG_ERROR       -2  // 未知原因错误 
#define SIS_NET_TAG_NOAUTH      -1  // 未登录验证
#define SIS_NET_TAG_OK           0  // 数据正确 通常表示为请求完成的应答
// 1..126 为info字段的数据类型 若没有默认为二进制
#define SIS_NET_TAG_INT          1  // 返回整数 二进制64位整数
#define SIS_NET_TAG_JSON         2  // 返回JSON字符串
#define SIS_NET_TAG_ARRAY        3  // 返回ARRAY字符串
#define SIS_NET_TAG_CHAR         4  // 返回字符串
#define SIS_NET_TAG_CHARS        5  // 返回字符串列表 count+[size+data]
#define SIS_NET_TAG_BYTE         6  // 返回二进制数据流
#define SIS_NET_TAG_BYTES        7  // 返回二进制列表 count+[size+data]

#define SIS_NET_TAG_SUB_OPEN   100  // 订阅打开 字符串日期
#define SIS_NET_TAG_SUB_KEY    101  // 订阅时返回的键值 START 时初始化 然后递增 逗号分隔
#define SIS_NET_TAG_SUB_SDB    102  // 订阅是返回的结构 START 时初始化 然后递增
#define SIS_NET_TAG_SUB_START  103  // 订阅开始 字符串日期
#define SIS_NET_TAG_SUB_WAIT   104  // 订阅缓存数据结束 等待新的数据 字符串日期
#define SIS_NET_TAG_SUB_STOP   105  // 订阅结束 字符串日期
#define SIS_NET_TAG_SUB_CLOSE  106  // 订阅关闭 字符串日期

// 如果把网络类比与找小明拿一份5月的工作计划，那么：
// service 找什么人 - 找小明
// cmd 找小明干什么 - 拿东西
// key 拿什么东西 - 拿工作计划
// ask 什么样的工作计划 - 5月份的工作计划 
typedef struct s_sis_net_switch {
	unsigned char sw_sno     : 1;  // 是否带包序号 为64位整数
	unsigned char sw_service : 1;  // 服务名 寻找哪个服务
	unsigned char sw_cmd     : 1;  // 指令名 寻找该服务下的哪个命令
	unsigned char sw_subject : 1;  // 主体名
	unsigned char sw_tag     : 1;  // 指明数据包的类型 0 表示成功 其他表示各种信息
	unsigned char sw_info    : 1;  // 附属信息
	unsigned char sw_more    : 1;  // 是否有扩展数据 扩展字段存储为 (klen+key+vlen+val)
	unsigned char sw_mark    : 1;  // 记号 == 1 表示为二进制数据, 以区分JSON字符串
} s_sis_net_switch;

// 二进制协议会在每个数据包最后放入此结构 解包时先从最后取出该结构 
// 该结构可以增加crc数据校验等功能
// 因为数据是否被压缩只有压缩后才能直到是否成功 不成功用原文 所以这个结构必须放到数据末尾
// 也可用此数据判断二进制数据是否合法
// 字符串数据没有该结构 统一不压缩 不加密
// 如果需要压缩加密把字符串当字节流处理就可以了
// 根据ws协议头就知道后面的数据是什么格式 二进制就跟这个结构 字符出就没有这个结构
// 二进制数据包最后一个字节 - 检查只对 s_sis_net_switch 后的实体数据进行处理
typedef struct s_sis_net_tail {
	unsigned char bytes    : 1;  // 数据以什么格式传播
	unsigned char crc16    : 1;  // 是否有crc16数据校验 如果有去前面取16偏移用于校验
	unsigned char compress : 2;  // 是否被压缩 0:｜1:snappy｜2:incrzip｜3:前置一个字节中支持255种压缩方式 
	unsigned char crypt    : 4;  // 是否被加密 0:｜1..7 | 15:前置一个字节中支持255种加解密方式 
	// compress的前置信息 + crypt的前置信息 + s_sis_net_tail 为最后的信息描述
} s_sis_net_tail;

// 定义错误信息返回
typedef struct s_sis_net_errinfo {
	int      rno;
	char    *rinfo;
} s_sis_net_errinfo;



#define SIS_MSG_MODE_NORMAL   0  // 此数据为信息包 下级连接按接收顺序传递 数据流
#define SIS_MSG_MODE_INSIDE   1  // 此数据为信息包 下级连接按接收顺序传递 数据流
#define SIS_MSG_MODE_SET     10  // 此数据为信息包 下级连接按接收顺序传递 数据流
#define SIS_MSG_MODE_PUB     11  // 此数据为广播包 下级连接直接广播 数据流

// 把请求和应答统一结合到 s_sis_net_message 中
// 应答目前约定只支持一级数组，
// 说明：没有和组件通讯合并 是担心耦合度过高 和 交互信息过重
//      建议到了实际请求可以把 s_sis_net_message 作为参数传递到 s_sis_message 中以实现信息共享
typedef struct s_sis_net_message {
    // 公共部分
    int                 cid;       // 哪个客户端的信息 -1 表示向所有用户发送
	//QQQ 引用次数干什么用的？
	uint32              refs;      // 引用次数
	// 用户请求的投递地址 方便无状态接收数据  不超过128个字符    
    s_sis_sds	        name;      // [必要] 请求的名字 用户名+时间戳+序列号 唯一标志请求的名称，需要原样返回；
	int8                mode;      // 是否为内部通讯包
    
    int8                format;    // 是否以字符串方式进行打包和拆包 默认 SIS_NET_FORMAT_CHARS
    int8                comp;      // 如果包不完整需要等到完整包到达再解析 主要用于拆包后的组合复原 该值决定了WSS的头标记
	s_sis_memory       *memory;    // 网络来去的原始数据包 转发时直接发送该数据

    // 数据包解析后的信息
    s_sis_net_switch    switchs;   // 字典开关
	int64               sno;       // 包序号
	s_sis_sds	        service;   // 请求信息专用,指定去哪个service执行命令 http格式这里填路径
	s_sis_sds	        cmd;       // 请求信息专用,要执行什么命令  get set....
	s_sis_sds           subject;   // 
	int8                tag;       // 标签

    s_sis_sds           info;      // 
	// 当发送和接收到的是不定长列表数据时 写入 infos 中 tag = SIS_NET_TAG_CHARS SIS_NET_TAG_BYTES
    s_sis_pointer_list *argvs;     // 按顺序获取 s_sis_object 

	// 这里通常是网络进程之间交互的扩展信息
	s_sis_map_pointer  *more;      // 扩展字段定义 网络通讯时 有值就扩展出来 ｜ 暂时不用
	// 这里通常是线程之间交互的扩展信息
	s_sis_map_pointer  *map;       // 这里是用户自定义的kv
} s_sis_net_message;


#define SIS_NET_SHOW_MSG(_s_,_n_) { s_sis_net_message *_msg_ = (s_sis_net_message *)_n_; \
	uint8 *sw = (uint8 *)&_msg_->switchs; \
	printf("net %s: [%d] %x [%d:%d]: %lld | %d %s %s %s [%zu]%s argvs :%d\n", _s_, \
	    _msg_->cid, *sw, _msg_->mode, _msg_->format, _msg_->sno, _msg_->tag,\
		_msg_->service ? _msg_->service : "nil",\
		_msg_->cmd ? _msg_->cmd : "nil",\
		_msg_->subject ? _msg_->subject : "nil",\
		_msg_->info ? sis_sdslen(_msg_->info) : 0,\
		_msg_->switchs.sw_mark == 0 ? _msg_->info : "...",\
		_msg_->argvs ? _msg_->argvs->count : 0);\
	}

////////////////////////////////////////////////////////
//  标准网络格式的消息结构
////////////////////////////////////////////////////////

s_sis_net_message *sis_net_message_create();

void sis_net_message_destroy(void *in_);

void sis_net_message_incr(s_sis_net_message *);
void sis_net_message_decr(void *);

void sis_net_message_clear(s_sis_net_message *);
size_t sis_net_message_get_size(s_sis_net_message *);

// 拷贝需要广播的数据
void sis_net_message_relay(s_sis_net_message *, s_sis_net_message *, int cid_, s_sis_sds name_, 
	s_sis_sds service_, s_sis_sds cmd_, s_sis_sds subject_);

// 以下函数 只检查相关字段 其他都不管
void sis_net_message_set_subject(s_sis_net_message *netmsg_, const char *kname_, const char *sname_);
void sis_net_message_set_scmd(s_sis_net_message *netmsg_, const char *scmd_);
void sis_net_message_set_cmd(s_sis_net_message *netmsg_, const char *cmd_);
void sis_net_message_set_service(s_sis_net_message *netmsg_, const char *service_);
void sis_net_message_set_tag(s_sis_net_message *netmsg_, int tag_);
void sis_net_message_set_notag(s_sis_net_message *netmsg_);

void sis_net_message_set_info(s_sis_net_message *netmsg_, void *val_, size_t vlen_);

void sis_net_message_set_info_i(s_sis_net_message *netmsg_, int64 val_);

void sis_net_message_init_chars(s_sis_net_message *netmsg_);

void sis_net_message_set_chars(s_sis_net_message *netmsg_, char *val_, size_t vlen_);

void sis_net_message_init_bytes(s_sis_net_message *netmsg_);

void sis_net_message_set_bytes(s_sis_net_message *netmsg_, char *val_, size_t vlen_);

void sis_net_message_set_char(s_sis_net_message *netmsg_, char *val_, size_t vlen_);

void sis_net_message_set_byte(s_sis_net_message *netmsg_, char *val_, size_t vlen_);
////////////////////////////////////////////////////////
//  s_sis_net_message 提取数据函数
////////////////////////////////////////////////////////
void sis_net_msg_set_inside(s_sis_net_message *);

// 获取字符数据
int64 sis_net_msg_info_as_int(s_sis_net_message *netmsg_);

int sis_net_msg_info_as_date(s_sis_net_message *netmsg_);

////////////////////////////////////////////////////////
//  s_sis_net_message 操作类函数
////////////////////////////////////////////////////////
void sis_net_msg_tag_ok(s_sis_net_message *);
void sis_net_msg_tag_int(s_sis_net_message *, int64 in_);
void sis_net_msg_tag_error(s_sis_net_message *, char *rval_, size_t vlen_);
void sis_net_msg_tag_null(s_sis_net_message *);
void sis_net_msg_tag_sub_start(s_sis_net_message *, const char *info_);
void sis_net_msg_tag_sub_wait(s_sis_net_message *,  const char *info_);
void sis_net_msg_tag_sub_stop(s_sis_net_message *,  const char *info_);
void sis_net_msg_tag_sub_open(s_sis_net_message *, const char *info_);
void sis_net_msg_tag_sub_close(s_sis_net_message *, const char *info_);

void sis_net_msg_clear(s_sis_net_message *);
void sis_net_msg_clear_service(s_sis_net_message *);
void sis_net_msg_clear_cmd(s_sis_net_message *);
void sis_net_msg_clear_info(s_sis_net_message *);
void sis_net_msg_clear_subject(s_sis_net_message *);

#endif //_SIS_CRYPT_H
