#ifndef _SERVICE_CORE_H
#define _SERVICE_CORE_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "worker.h"

// 微服务架构核心组件
// 1.只有同名的微服务才能够交互服务内容
// 2.信息表需要记录 服务名 版本号 IP PORT 权限 IP实例CPU MEM占用 等信息
// 若请求未指定版本号就以最大的版本号请求 
// 服务中心优先找到符合条件服务、空闲状态的IP进行数据请求 并返回数据
// 3.寻找服务优先局域网UDP方式获取7330的服务信息（3次） 其次按照服务列表中的外网IP地址通过TCP拿到服务 
// 


typedef struct s_service_core_reader
{
	s_sis_sds             sub_mkey; // 匹配字符
	s_sis_pointer_list   *netmsgs;    // s_sis_net_message
} s_service_core_reader;

typedef struct s_service_core_cxt
{
	int                 status;      // 工作状态

	s_sis_sds           dbname;      // 数据库名字 service_core
	s_sis_map_pointer  *work_keys;   // 数据集合的字典表 char -> sis_object

	// 多个 client 订阅的列表 需要一一对应发送
	// 同一 mkey 可能有多个用户订阅 如果一个用户有单键值订阅并且和模糊订阅重叠 从单键值订阅表中删除
	s_sis_pointer_list  *msub_readers; // s_service_core_reader 
	// 以 key 为索引的 s_service_core_reader 同一key可能有多个用户订阅
	s_sis_map_pointer   *sub_readers;   // s_service_core_reader

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

}s_service_core_cxt;

s_service_core_reader *service_core_reader_create(s_sis_net_message *netmsg_);
void service_core_reader_destroy(void *);

bool  service_core_init(void *, void *);
void  service_core_uninit(void *);
void  service_core_working(void *);  

// 获得一个set写入的数据
int service_core_get(void *worker_, void *argv_);
// 写入一个数据 数据会在重启清理
int service_core_set(void *worker_, void *argv_);
// 订阅一个键值
int service_core_sub(void *worker_, void *argv_);
// 发布一个信息
int service_core_pub(void *worker_, void *argv_);
// 订阅多个符合条件的键值 头匹配
int service_core_msub(void *worker_, void *argv_);
// 取消订阅
int service_core_unsub(void *worker_, void *argv_);

#endif