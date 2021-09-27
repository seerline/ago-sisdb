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

#define API_REPLY_INERR  -4   // 传入参数错误
#define API_REPLY_NONET  -3   // 连接不到服务器
#define API_REPLY_NIL    -2   // 返回空
#define API_REPLY_ERROR  -1   // 其他错误
#define API_REPLY_OK      0   // 返回OK

typedef struct s_api_sisdb_client_callback
{
	void *cb_source,                 // 回调传送对象
	// 系统 后台自动重连自动订阅 但订阅信息是回到初始状态
	void (*cb_connected)(int id_);	  // id_
	void (*cb_disconnect)(int id_);   // id_
	// 订阅通知
	void (*cb_sub_start)(int id_, int idate_); // 某一天的数据订阅开始
	void (*cb_sub_stop)(int id_, int idate_);   // 某一天的数据订阅结束
	void (*cb_sub_realtime)(int id_, int idate_); // 通知我进入实时状态
	// 字典信息
	void (*cb_dict_keys)(int id_, const char *omem_, int osize_); // 订阅返回代码表
	void (*cb_dict_sdbs)(int id_, const char *omem_, int osize_); // 订阅返回表信息
	// 按名字返回的数据 
	void (*cb_sub_chars)(int id_, const char *kname_, const char *sname_, const char *omem_, int osize_); 
	// 按索引返回的数据 
	void (*cb_sub_bytes)(int id_, int kidx_, int sidx_, const char *omem_, int osize_); 
} s_api_sisdb_client_callback;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// 创建客户端实例 返回 实例 id
_API_SISDB_DLLEXPORT_ int  api_sisdb_client_create(s_api_sisdb_client_callback *);
// 销毁指定实例
_API_SISDB_DLLEXPORT_ void api_sisdb_client_destroy(int id_);  

// argv_ 为json格式的配置文件
// { "ip" : "127.0.0.1",
//   "port" : 10000,
//   "username" : "guest",
//   "password" : "guest1234",
// }
// 返回 >=0 为正常 返回 < 0 为错误信息
_API_SISDB_DLLEXPORT_ void api_sisdb_client_open(int id_, const char *argv_);  
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
//         {"sub-date":20201012, "ishead":0}
// 取消订阅  cmd : unsub    | key ： NULL | val : NULL
_API_SISDB_DLLEXPORT_ int api_sisdb_client_command(int id_, const char *cmd_, const char *key_, const char *ask_);

// 可以传递用户的数据 这里只保存指针 用户如果要使用 必须自己保持指针一直存活
_API_SISDB_DLLEXPORT_ void *api_sisdb_client_get_userdata(int id_, const char *name_);  // 得到用户数据
_API_SISDB_DLLEXPORT_ void  api_sisdb_client_set_userdata(int id_, const char *name_, void *data_);  // 设置用户数据

#ifdef __cplusplus
}
#endif // __cplusplus

#endif