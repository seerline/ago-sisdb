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

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// 函数返回 不能等于 0 SIS_METHOD_ERROR
#define API_REPLY_ERR    -1   // 返回失败
#define API_REPLY_OK      1   // 返回OK
#define API_REPLY_SIGN    2   // 返回操作信号
#define API_REPLY_DATA    3   // 返回实际数据

// 创建客户端实例 返回 实例 id
_API_SISDB_DLLEXPORT_ int  api_sisdb_client_create(
	const char *ip_,
	int         port_,
	const char *username_, 
	const char *password_
);

// 销毁指定实例
_API_SISDB_DLLEXPORT_ void api_sisdb_client_destroy(int id_);  

// 发送请求并返回数据 堵塞知道数据返回
// 仅对非订阅的命令有效
_API_SISDB_DLLEXPORT_ int api_sisdb_command_ask(
	int           id_,             // 句柄
	const char   *cmd_,            // 请求的参数
	const char   *key_,            // 请求的key
	const char   *val_,            // 请求的参数
	void         *cb_reply         // 回调的数据
);

// 发送请求并通过回调返回数据 立即返回 
// 仅对订阅的命令有效
_API_SISDB_DLLEXPORT_ int api_sisdb_command_sub(
	int           id_,             // 句柄
	const char   *cmd_,            // 请求的参数
	const char   *key_,            // 请求的key
	const char   *val_,            // 请求的参数
	//////////////////
	void   *cb_source_,          // 回调传送对象
    void   *cb_sub_start,        // 订阅开始
	void   *cb_sub_realtime,     // 订阅进入实时状态
	void   *cb_sub_stop,         // 订阅结束 自动取消订阅
	void   *cb_dict_reply        // 回调的数据
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif