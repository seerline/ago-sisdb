#ifndef _API_DMS_
#define _API_DMS_

#ifdef WIN32
#define _API_DMS_DLLEXPORT_ __declspec( dllexport )  //宏定义
#else
#define _API_DMS_DLLEXPORT_  //宏定义
#endif // WIN32

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push,1)

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct s_service_reply
{
    int    style;   // 返回的数据类型
    int    integer; // 返回整数类型
    void  *inmem;   // 返回的数据缓存
    size_t isize;   // 返回的数据缓存大小
    struct s_service_reply *reply; 
    // 返回的列表数据 此时当前节点 integer 中保存的是列表总数 
    // 返回的结构体描述信息 
} s_service_reply;

typedef struct s_service_callback
{
    // 系统回调 表示服务连接成功
    void (*cb_net_open)(int id_);  // id_
    // 系统回调 表示服务连接断开 后台会重新连接服务 并重新从头开始订阅数据
    void (*cb_net_stop)(int id_); // id_
    // 订阅通知信息 订阅日期 
    void (*cb_sub_open )(int id_, long long subdate_); // 订阅打开
    void (*cb_sub_start)(int id_, long long subdate_); // 订阅开始
    void (*cb_sub_wait )(int id_, long long subdate_); // 订阅进入实时状态
    void (*cb_sub_stop )(int id_, long long subdate_); // 订阅结束
    void (*cb_sub_break)(int id_, long long subdate_); // 订阅中断
    void (*cb_sub_close)(int id_, long long subdate_); // 订阅关闭
    // 得到当前键值信息
    void (*cb_sub_dict_keys)(int id_, const char *keys_, size_t ksize_);
    // 得到当前数据表的定义
    void (*cb_sub_dict_sdbs)(int id_, const char *sdbs_, size_t ssize_);
    // 循环返回订阅的数据
    void (*cb_sub_chars)(int id_, const char *key_, const char *sdb_, void *in_, size_t isize_);
    // 非订阅接口命令的返回回调
    void (*cb_reply)(int id_, s_service_reply *reply); 

} s_service_callback;

_API_DMS_DLLEXPORT_ int dms_service_open(const char *initinfo_);
// {
//     "version" : 1,
//     "servicename": "market_cn_v1",
//     "netname": "xxxxxxxxxxxxxxx",
//     "ip": "127.0.0.1",
//     "port":50008,
//     "username":"guest",
//     "password":"guest1234"
// }

_API_DMS_DLLEXPORT_ void dms_service_close(
	int handle_);  
 
// 函数调用
// handle_ 服务的句柄
// command_ 调用服务的命令名
// subject_ 操作的主体名
// in_ 传入的数据缓存
// isize_ 传入数据的大小
// callback_ 有值就按照回调方式返回数据 否则就同步方式返回数据
//    异步返回数据也会返回 s_service_reply 如果成功执行 style == SIS_TAG_OK 
_API_DMS_DLLEXPORT_ s_service_reply *dms_service_command(
	int handle_,           // 句柄
    const char *command_,  // 指令
    const char *subject_,  // 对象
    void *info_,           // 传入数据的指针
    size_t isize_,         // 传入数据的尺寸
	void *callback_);      

_API_DMS_DLLEXPORT_ void dms_service_free_reply(s_service_reply *reply_);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif