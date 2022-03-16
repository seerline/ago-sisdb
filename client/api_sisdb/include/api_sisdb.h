#ifndef _API_SISDB_
#define _API_SISDB_

#ifdef WIN32
#define _API_SISDB_DLLEXPORT_ __declspec( dllexport )  //宏定义
#else
#define _API_SISDB_DLLEXPORT_  //宏定义
#endif // WIN32

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push,1)


#define API_REPLY_NOANS    -104     // 未等到数据
#define API_REPLY_NOWORK   -103     // 接口没有工作
#define API_REPLY_INERR    -102     // 传入参数错误
#define API_REPLY_NONET    -101     // 连接不到服务器

// -100 .. 100 为服务端返回 ID
#define API_REPLY_INVALID  -100     // 请求已经失效
#define API_REPLY_NIL        -3     // 返回空
#define API_REPLY_ERROR      -2     // 未知原因错误 
#define API_REPLY_ERROR      -1     // 未登录验证

#define API_REPLY_OK          0     // 返回OK
#define API_REPLY_SUB_OPEN    5     // 订阅开始
#define API_REPLY_SUB_WAIT    6     // 订阅缓存数据结束 等待新的数据
#define API_REPLY_SUB_STOP    7     // 订阅结束

typedef struct s_api_sisdb_cb_sys
{
	void *cb_source;                 // 回调传送对象
	// 系统 后台自动重连自动订阅 但订阅信息是回到初始状态
	void (*cb_connected)(void *);	 
	void (*cb_disconnect)(void *);   
} s_api_sisdb_cb_sys;

typedef int (cb_api_sisdb_ask)(void *source_, int rans_, const char *key_, const char *omem_, int osize_);

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// 创建客户端实例 返回 实例 id
_API_SISDB_DLLEXPORT_ int  api_sisdb_client_create(s_api_sisdb_cb_sys *);
// 销毁指定实例
_API_SISDB_DLLEXPORT_ void api_sisdb_client_destroy(int id_);  

// argv_ 为json格式的配置文件
// { "ip" : "127.0.0.1",
//   "port" : 10000,
//   "username" : "guest",
//   "password" : "guest1234",
// }
// 返回 >=0 为正常 返回 < 0 为错误信息
_API_SISDB_DLLEXPORT_ int api_sisdb_client_open(int id_, const char *argv_);  
// 关闭实例
_API_SISDB_DLLEXPORT_ void api_sisdb_client_close(int id_);  

// 发送请求统一接口 返回一个请求ID在 reply 回调中对应
// 订阅行情  cmd : sub | key : *.* 表示全部行情 | val : NULL 表示从尾部订阅当日行情
//   全部行情 key = *.*
//   单票单表 key = "SH688012.stk_transact"
//   单票多表 key = "SH688012.stk_snapshot,stk_transact"
//   单票全表 key = "SH688012.*"
//   多票单表 key = "SH688012,SH600600.stk_transact"
//   多票多表 key = "SH688012,SH600600,SZ300300.stk_snapshot,stk_transact"
//   多票多表（头匹配） key = "SH688,SZ300,SH000001.stk_snapshot,stk_transact"
//   全票单表 key = "*.stk_transact"
//   val : json格式参数 
//         {"sub-date":20201012, "sub-head":0}
// 取消订阅  cmd : unsub    | key ： NULL | val : NULL
_API_SISDB_DLLEXPORT_ int api_sisdb_client_cmd(
	int                 id_,             // 句柄
	const char         *cmd_,            // 请求的参数
	const char         *key_,            // 请求的key
	const char         *value_,            // 请求的参数
	size_t              vsize_,
	void               *cb_source_,
	void               *cb_reply_);  // cb_api_sisdb_ask 类型的回调函数

// 堵塞知道 reply 返回数据
_API_SISDB_DLLEXPORT_ int api_sisdb_client_cmd_wait(
	int                 id_,             // 句柄
	const char         *cmd_,            // 请求的参数
	const char         *key_,            // 请求的key
	const char         *value_,            // 请求的参数
	size_t              vsize_,
	void               *cb_source_,
	void               *cb_reply_);  // cb_api_sisdb_ask 类型的回调函数

// 可以传递用户的数据 这里只保存指针 用户如果要使用 必须自己保持指针一直存活
_API_SISDB_DLLEXPORT_ void *api_sisdb_client_get_userdata(int id_, const char *name_);  // 得到用户数据
_API_SISDB_DLLEXPORT_ void  api_sisdb_client_set_userdata(int id_, const char *name_, void *data_);  // 设置用户数据

#ifdef __cplusplus
}
#endif // __cplusplus

#endif