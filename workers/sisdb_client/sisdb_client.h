#ifndef _SISDB_CLIENT_H
#define _SISDB_CLIENT_H

#include "sis_method.h"
#include "sis_net.h"
#include "sis_list.h"
#include "sis_map.lock.h"

typedef int (sis_reply_define)(void *, int , const char *, const char *, size_t);

typedef struct s_sisdb_client_ask {
	int32              sno;            // 数据来源标识
        
    bool               issub;          // 是否订阅
    int8               format;         // 字节还是字符
        
	s_sis_sds          cmd;            // 请求的参数
	s_sis_sds          key;            // 请求的key
	s_sis_sds          val;            // 请求的val

    void              *cb_source;
	sis_reply_define  *cb_reply;    // 回调的数据 (void *source, int rid, void *key, void *val);
}s_sisdb_client_ask;

#define SIS_CLI_STATUS_INIT  0   // 初始化完成
#define SIS_CLI_STATUS_AUTH  1   // 进行验证
#define SIS_CLI_STATUS_WORK  2   // 正常工作
#define SIS_CLI_STATUS_STOP  3   // 断开
#define SIS_CLI_STATUS_EXIT  4   // 退出
// 客户的结构体
typedef struct s_sisdb_client_cxt
{
	int                 status;
    int                 cid;
    s_sis_url           url_cli;
    s_sis_net_class    *session;
    bool                auth;
	char                username[32]; 
	char                password[32];
    s_sis_wait_handle   wait;
    uint32              ask_sno;  // 请求序列号    
	// 订阅的数据列表
    s_sis_safe_map     *asks;     // 订阅的信息 断线后需要重新发送订阅信息 以 key 为索引 s_sisdb_client_ask

    void              *cb_source;
    sis_method_define *cb_connected;
    sis_method_define *cb_disconnect;

}s_sisdb_client_cxt;

bool  sisdb_client_init(void *, void *);
void  sisdb_client_uninit(void *);
void  sisdb_client_method_init(void *);
void  sisdb_client_method_uninit(void *);

s_sisdb_client_ask *sisdb_client_ask_create(
    const char   *cmd_,                // 请求的参数
	const char   *key_,                // 请求的key
	void         *val_,                // 请求的参数
    size_t        vlen_,
	void         *cb_source_,          // 回调传送对象
	void         *cb_reply             // 回调的数据
);

void sisdb_client_ask_destroy(void *);

s_sisdb_client_ask *sisdb_client_ask_cmd(s_sisdb_client_cxt *context, 
    const char   *cmd_,                // 请求的参数
	const char   *key_,                // 请求的key
	void         *val_,                // 请求的参数
    size_t        vlen_,
    void         *cb_source_,          // 回调传送对象
    void         *cb_reply);           // 回调的数据

void sisdb_client_ask_del(s_sisdb_client_cxt *, s_sisdb_client_ask *);

s_sisdb_client_ask *sisdb_client_ask_get(
    s_sisdb_client_cxt *, 	
    const char   *source_         // 来源信息
);

void sisdb_client_ask_unsub(s_sisdb_client_cxt *, s_sisdb_client_ask *);

bool sisdb_client_ask_sub_exists(
    s_sisdb_client_cxt *context, 	
    const char   *cmd_,         // 来源信息
    const char   *key_         // 来源信息
);

// fast query
int cmd_sisdb_client_setcb(void *worker_, void *argv_);
int cmd_sisdb_client_status(void *worker_, void *argv_);
// slow query
int cmd_sisdb_client_chars_nowait(void *worker_, void *argv_);
int cmd_sisdb_client_bytes_nowait(void *worker_, void *argv_);
// fast query
int cmd_sisdb_client_chars_wait(void *worker_, void *argv_);
int cmd_sisdb_client_bytes_wait(void *worker_, void *argv_);


#endif