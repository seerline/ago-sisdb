
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

// ans 的定义
#define SIS_NET_ANS_INVALID   -100  // 请求已经失效
#define SIS_NET_ANS_NIL         -3  // 数据为空
#define SIS_NET_ANS_ERROR       -2  // 未知原因错误 
#define SIS_NET_ANS_NOAUTH      -1  // 未登录验证

#define SIS_NET_ANS_OK           0  // 数据正确
#define SIS_NET_ANS_SUB_OPEN     5  // 订阅开始
#define SIS_NET_ANS_SUB_WAIT     6  // 订阅缓存数据结束 等待新的数据
#define SIS_NET_ANS_SUB_STOP     7  // 订阅结束

// 如果把网络类比与找小明拿一份5月的工作计划，那么：
// service 找什么人 - 找小明
// cmd 找小明干什么 - 拿东西
// key 拿什么东西 - 拿工作计划
// ask 什么样的工作计划 - 5月份的工作计划 
typedef struct s_sis_net_switch {
	unsigned char is_publish  : 1;  // 是否为广播包 如果是 不再解析其他 直接准备传递 
	unsigned char is_inside   : 1;  // 是否为内部包 如果是 不扩散
	unsigned char is_reply    : 1;  // 表示为应答包  应答有  ans ** 字符格式以此来判定是否为应答
	unsigned char is_1        : 1;  // 备用
	unsigned char has_ver     : 1;  // 请求有 ver     应答有  ver
	unsigned char has_fmt     : 1;  // 请求有 fmt     应答有  fmt
	unsigned char has_service : 1;  // 请求有 service 应答有  --- 
	unsigned char has_cmd     : 1;  // 请求有 cmd     应答有  ---

	unsigned char has_key     : 1;  // 请求有 key     应答有  key
	unsigned char has_ask     : 1;  // 请求有 ask     应答有  ---
	unsigned char has_msg     : 1;  // 请求有 ---     应答有  msg
	unsigned char has_next    : 1;  // 请求有 ---     应答有  next 有后续包
	unsigned char has_argvs   : 1;  // 请求有 argvs   应答有  argvs
	unsigned char has_more    : 1;  // 表示有扩展字段  0 - 无扩展
	unsigned char fmt_msg     : 1;  // 信息格式 0 - 字符 1 - 二进制
	unsigned char fmt_argv    : 1;  // 数据格式 0 - 字符 1 - 二进制
} s_sis_net_switch;

// 定义错误信息返回
typedef struct s_sis_net_errinfo {
	int      rno;
	char    *rinfo;
} s_sis_net_errinfo;

// 把请求和应答统一结合到 s_sis_net_message 中
// 应答目前约定只支持一级数组，
// 说明：没有和组件通讯合并 是担心耦合度过高 和 交互信息过重
//      建议到了实际请求可以把 s_sis_net_message 作为参数传递到 s_sis_message 中以实现信息共享
typedef struct s_sis_net_message {
    // 公共部分
    int                 cid;       // 哪个客户端的信息 -1 表示向所有用户发送
    int8                ver;       // [非必要] 协议版本
	uint32              refs;      // 引用次数
    s_sis_sds	        name;      // [必要] 请求的名字 用户名+时间戳+序列号 唯一标志请求的名称，需要原样返回；
	// 用户请求的投递地址 方便无状态接收数据  不超过128个字符    
    int8                comp;      // 如果包不完整需要等到完整包到达再解析
	s_sis_memory       *memory;    // 网络来去的原始数据包 转发时直接发送该数据

    int8                format;    // 根据此标记进行打包和拆包 默认 SIS_NET_FORMAT_CHARS

    s_sis_net_switch    switchs;   // 字典开关
    // 当发送和接收到的是不定长列表数据时 写入argvs中 
    s_sis_pointer_list *argvs;     // 按顺序获取 s_sis_object -> s_sis_sds 
    // 请求相关部分
	s_sis_sds	        service;   // [非必要] 请求信息专用,指定去哪个service执行命令 http格式这里填路径
	s_sis_sds	        cmd;       // [非必要] 请求信息专用,要执行什么命令  get set....
    s_sis_sds	        key;       // [请求专用｜不必要] 对谁执行命令
    s_sis_sds	        ask;       // [请求专用｜不必要] 执行命令的参数，必须为字符类型 
    // 应答相关部分
    int                 rans;      // [应答专用｜必要] 表示为应答编号 整数
	int                 rnext;     // 后续的数据
    s_sis_sds           rmsg;      // [应答专用｜不必要] 表示为字符类型的返回数据
    int8                rfmt;      // [应答专用] rmsg 数据的类型和格式 特指 rmsg 整数 数组 ...
	// 有扩展字典时有值
	s_sis_map_pointer  *map;       // 
} s_sis_net_message;

#define SIS_NET_SHOW_MSG(_s_,_n_) { uint16 *sw = (uint16 *)&netmsg->switchs; \
	if (netmsg->switchs.is_reply) {\
		printf("%s: [%d] %x : %d %s %s  argvs :%d\n", _s_, netmsg->cid, *sw, \
			netmsg->rans, netmsg->key ? netmsg->key : "nil",\
			netmsg->rmsg ? netmsg->rmsg : "nil",\
			netmsg->argvs ? netmsg->argvs->count : 0);\
	} else {\
		printf("%s: [%d] %x : %s %s %s %s argvs :%d\n", _s_, netmsg->cid, *sw, \
			netmsg->service ? netmsg->service : "nil",\
			netmsg->cmd ? netmsg->cmd : "nil",\
			netmsg->key ? netmsg->key : "nil",\
			netmsg->ask ? netmsg->ask : "nil",\
			netmsg->argvs ? netmsg->argvs->count : 0);\
	}}

////////////////////////////////////////////////////////
//  标准网络格式的消息结构
////////////////////////////////////////////////////////

s_sis_net_message *sis_net_message_create();

void sis_net_message_destroy(void *in_);

void sis_net_message_incr(s_sis_net_message *);
void sis_net_message_decr(void *);

void sis_net_message_clear(s_sis_net_message *);
size_t sis_net_message_get_size(s_sis_net_message *);

void sis_net_message_copy(s_sis_net_message *, s_sis_net_message *, int cid_, s_sis_sds name_);

////////////////////////////////////////////////////////
//  s_sis_net_message 操作类函数
////////////////////////////////////////////////////////

void sis_net_ask_with_chars(s_sis_net_message *netmsg_, 
    char *cmd_, char *key_, char *val_, size_t vlen_);

void sis_net_ask_with_bytes(s_sis_net_message *netmsg_, 
    char *cmd_, char *key_, char *val_, size_t vlen_);

void sis_net_ask_with_argvs(s_sis_net_message *netmsg_, const char *in_, size_t ilen_);

// in_被吸入
void sis_net_ans_with_chars(s_sis_net_message *, const char *in_, size_t ilen_);
// *** 这个函数需要检查
void sis_message_set_key(s_sis_net_message *netmsg_, const char *kname_, const char *sname_);
void sis_message_set_cmd(s_sis_net_message *netmsg_, const char *cmd_);

void sis_net_ans_with_bytes(s_sis_net_message *, const char *in_, size_t ilen_);
void sis_net_ans_with_argvs(s_sis_net_message *, const char *in_, size_t ilen_);
// 获取数据流
s_sis_sds sis_net_get_argvs(s_sis_net_message *netmsg_, int index);

void sis_net_ans_with_noreply(s_sis_net_message *);

void sis_net_ans_with_int(s_sis_net_message *, int in_);
void sis_net_ans_with_ok(s_sis_net_message *);
void sis_net_ans_with_error(s_sis_net_message *, char *rval_, size_t vlen_);
void sis_net_ans_with_null(s_sis_net_message *);
void sis_net_ans_with_sub_start(s_sis_net_message *, const char *info_);
void sis_net_ans_with_sub_wait(s_sis_net_message *,  const char *info_);
void sis_net_ans_with_sub_stop(s_sis_net_message *,  const char *info_);

#endif //_SIS_CRYPT_H
