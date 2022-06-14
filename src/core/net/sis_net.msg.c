
#include <sis_net.msg.h>
#include <sis_malloc.h>
#include <sis_time.h>
#include <sis_net.node.h>

// static s_sis_net_errinfo _rinfo[] = {
// 	{ SIS_NET_TAG_INVALID, "ask is invalid."},
// 	{ SIS_NET_TAG_NOAUTH,  "need auth."},
// 	{ SIS_NET_TAG_NIL,     "ask data is nil."},
// 	{ SIS_NET_TAG_ERROR,   "noknown error."},
// };

// 如果把网络类比与找小明拿一份5月的工作计划，那么：
// service 服务名   找什么人 - 找小明
// command 指令名   找小明干什么 - 拿东西
// subject 主体名   拿什么东西 - 拿工作计划
// message 附属信息 什么样的工作计划 - 5月份的工作计划 
// typedef struct s_sis_net_switch {
//     unsigned char service : 1;  // 请求有 service 应答有  --- 
//     unsigned char cmd     : 1;  // 请求有 command 应答有  ---
//     unsigned char subject : 1;  // 请求有 subject 应答有  subject
//     unsigned char answer  : 1;  // 请求有 ---     应答有  answer - 整数 - 由此字段可知道是否应答 有answer一定表示应答包
//     unsigned char msg     : 1;  // 表示有附属信息  0 - 无附属信息 1 - 表示有附属信息
//     unsigned char fmt     : 1;  // msg的信息格式  0 - 字符 1 - 二进制
//     unsigned char more    : 1;  // 表示有扩展字段  0 - 无扩展 1 - 扩展字段存储为 (klen+key+vlen+val)
//     unsigned char crc32   : 1;  // 是否有crc校验  0 - 无 1 - 在数据末尾有4位字节表示crc校验 只有校验无误的包才合法 避免脏数据引起的系统崩溃
// } s_sis_net_switch;
    // unsigned char init    : 1;  // 是否为初始化包  0 - 无需保留 1 - 保存
    // unsigned char next    : 1;  // 是否有分页数据  0 - 无分页 1 表示有分页 格式为 总页数+当前页号+总记录数
    //  分页数据建议放入more中处理
// 字符传输默认为 JSON 字符串 fmt 默认为字符型 其他字段按kv结构存放
// 二进制传输第一个字节为 switch 然后按顺序获取信息填入解析体中 扩展字段第一个为字段数量 然后是kv结构
// 注意 无论什么情况下 都需遵循ws协议包格式 数据头通常为 (size)name:(switch)(data)
// *********************** //
// 如果是转发服务器 需要解析数据包 保存set信息用于初始化客户端信息 对其他命令直接转发 数据做引用计数不做内存拷贝
//        客户端连接成功 首先发送缓存的 set 数据包 然后从当前数据包头开始 转发数据 不做历史存留
//        如果慢客户端 积累500M数据 自动断开连接
// ************************ //
// 内外网穿透 *** 配置映射表后 A P B 只能一一对应 场景P主动连A和B
//    A 一旦联通 P , P 就主动去连接 B, PB 联不通 就挂断 AP
//    AP一通 P就需要缓存A的数据D1 PB联通后把D1原样发送给B
//    BP一通 P就需要缓存B的数据D2 PA联通状态下把D2原样发送给A 完成跳板
// ************************ //
// 点对点微服务原理
// 启动 锁定本机UDP可用端口 并接收该端口消息 写入本地信息 同步远端信息 UDP仅仅用来做服务发现和整体监控
// 同步信息完成后 生成一个服务展示台 提供8000口web本机监控
//      明确哪些服务是可以直接连接的 哪些是需要通过其他IP转连的相当于路由链接
// 默认开放8000-9000的所有端口防火墙
// 用户服务首先登录 如果有登录就必须找到 auth 的服务 并继承 auth 的父节点名
//  register("网络名","根节点IP","username","password");
//         网络名为NULL 默认设置为ZZZ名称
//         根节点为NULL表示仅仅在本网段去发现服务 如果本网段有中继 也可以跳出
//         username 不能为NULL 必须有值
//         password 为NULL 默认找非验证网络 通常用于本机调试适用
//  NULL NULL ding NULL 用于本机测试
//  main 192.168.3.118 ding NULL 首先链接118的8001端口 获取信息 链接不通就UDP获取邻居的信息

// 创建时可能会花一些时间 主要判断用户信息和服务名等是否合法 
// 合法的服务名最后会以 netname.username.servicename 的方式存在map 表中 方便定位和调用
// service = olla_service_create(serviceinfo)
//    serviceinfo 包括 网络信息 netinfo 用户信息 userinfo 权限信息 authinfo 服务名和版本 serviceinfo
// 
// olla_service_register_method(service, methodinfo)
//    methodinfo 包括 方法名 mname 对应函数 cb 简介说明 minfo 权限信息 authinfo 
// 

// olla_service_open(service)
// olla_service_close(service)
// olla_service_start(service)
// olla_service_stop(service)
// olla_service_destroy(service)

// 消息体的公用函数
// olla_message_create()
// olla_message_set()
// olla_message_get()
// olla_message_destroy()

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_net_message -----------------------------//
///////////////////////////////////////////////////////////////////////////
s_sis_net_message *sis_net_message_create()
{
	s_sis_net_message *o = (s_sis_net_message *)sis_malloc(sizeof(s_sis_net_message));
	memset(o, 0, sizeof(s_sis_net_message));
	o->refs = 1;
	return o;
}

void sis_net_message_destroy(void *in_)
{
	sis_net_message_decr(in_);
}
/**
 * @brief 网络消息的引用次数自增+1
 * @param in_ 网络消息
 */
void sis_net_message_incr(s_sis_net_message *in_)
{
    if (in_ && in_->refs != 0xFFFFFFFF) 
    {
        //QQQ 这里需要考虑线程安全么？
        in_->refs++;
    }
}
void sis_net_message_decr(void *in_)
{
    s_sis_net_message *in = (s_sis_net_message *)in_;
    if (!in)
    {
        return ;
    }
    if (in->refs == 1) 
    {
        sis_net_message_clear(in_);
        if (in->map)
        {
            sis_map_pointer_destroy(in->map); 
        }
        if (in->more)
        {
            sis_map_pointer_destroy(in->more); 
        }
        if(in->memory)
        {
            sis_memory_destroy(in->memory);
        }
        sis_free(in_);	
    } 
    else 
    {
        in->refs--;
    }	
}

void sis_net_message_clear(s_sis_net_message *in_)
{
	in_->cid = -1;
	// refs 不能改
	sis_sdsfree(in_->name);
	in_->name = NULL;

    in_->mode = 0;
	in_->format = SIS_NET_FORMAT_CHARS;
    in_->comp = 0;
	if(in_->memory)
	{
		sis_memory_clear(in_->memory);
	}
	memset(&in_->switchs, 0, sizeof(s_sis_net_switch));
    in_->sno = 0;
	sis_sdsfree(in_->service);
	in_->service = NULL;
	sis_sdsfree(in_->cmd);
	in_->cmd = NULL;
	sis_sdsfree(in_->subject);
	in_->subject = NULL;
	in_->tag = 0;
	sis_sdsfree(in_->info);
	in_->info = NULL;

	if(in_->argvs)
	{
		sis_pointer_list_destroy(in_->argvs);
		in_->argvs = NULL;
	}
    if (in_->more)
    {
        sis_map_pointer_clear(in_->more); 
    }
    if (in_->map)
    {
        sis_map_pointer_clear(in_->map); 
    }
}
static size_t _net_message_list_size(s_sis_pointer_list *list_)
{
	size_t size = 0;
	if (list_)
	{
		size += list_->count * sizeof(void *);
		for (int i = 0; i < list_->count; i++)
		{
			s_sis_object *obj = (s_sis_object *)sis_pointer_list_get(list_, i);
			// size += sizeof(s_sis_object); 
            size += SIS_OBJ_GET_SIZE(obj);
		}
	}
	return size;
}
size_t sis_net_message_get_size(s_sis_net_message *msg_)
{
	size_t size = sizeof(s_sis_net_message);
	size += msg_->name ? sis_sdslen(msg_->name) : 0;
	size += msg_->memory ? sis_memory_get_maxsize(msg_->memory) : 0;
	size += msg_->service ? sis_sdslen(msg_->service) : 0;
	size += msg_->cmd ? sis_sdslen(msg_->cmd) : 0;
	size += msg_->subject ? sis_sdslen(msg_->subject) : 0;
	size += msg_->info ? sis_sdslen(msg_->info) : 0;
	size += _net_message_list_size(msg_->argvs);
	return size;
}
// // 只拷贝数据和key
// void sis_net_message_copy(s_sis_net_message *agomsg_, s_sis_net_message *newmsg_, int cid_, s_sis_sds name_)
// {
//     newmsg_->cid = cid_;
//     newmsg_->ver = agomsg_->ver;
//     newmsg_->name = name_ ? sis_sdsdup(name_) : NULL;

//     newmsg_->format = agomsg_->format;
//     memmove(&newmsg_->switchs, &agomsg_->switchs, sizeof(s_sis_net_switch));

//     if (agomsg_->argvs && agomsg_->argvs->count > 0)
//     {
//         if (!newmsg_->argvs)
//         {
//             newmsg_->argvs = sis_pointer_list_create();
//             newmsg_->argvs->vfree = sis_object_decr;
//         }
//         else
//         {
//             sis_pointer_list_clear(newmsg_->argvs);
//         }
//         for (int i = 0; i < agomsg_->argvs->count; i++)
//         {
//             s_sis_object *obj = sis_pointer_list_get(agomsg_->argvs, i);
//             sis_object_incr(obj);
//             sis_pointer_list_push(newmsg_->argvs, obj);
//         }
//     }
//     newmsg_->service = agomsg_->service ? sis_sdsdup(agomsg_->service) : NULL;
//     newmsg_->cmd = agomsg_->cmd ? sis_sdsdup(agomsg_->cmd) : NULL;
//     newmsg_->key = agomsg_->key ? sis_sdsdup(agomsg_->key) : NULL;
//     newmsg_->ask = agomsg_->ask ? sis_sdsdup(agomsg_->ask) : NULL;

//     newmsg_->rans = agomsg_->rans;
//     newmsg_->rnext = agomsg_->rnext;
//     newmsg_->rmsg = agomsg_->rmsg ? sis_sdsdup(agomsg_->rmsg) : NULL;
//     newmsg_->rfmt = agomsg_->rfmt;
   
// }

#define SIS_NET_SET_STR(w,s,f) { w = f ? 1 : 0; sis_sdsfree(s); s = f ? sis_sdsnew(f) : NULL; }
#define SIS_NET_SET_SDS(w,s,f) { w = f ? 1 : 0; sis_sdsfree(s); s = f ? sis_sdsdup(f) : NULL; }
#define SIS_NET_SET_BUF(w,s,f,l) { w = (f && l > 0) ? 1 : 0; sis_sdsfree(s); s = (f && l > 0) ? sis_sdsnewlen(f,l) : NULL; }

void sis_net_message_relay(s_sis_net_message *agomsg_, s_sis_net_message *newmsg_, 
  int cid_, s_sis_sds name_, s_sis_sds service_, s_sis_sds cmd_, s_sis_sds subject_)
{
    newmsg_->cid = cid_;
    newmsg_->name = name_ ? sis_sdsdup(name_) : NULL;
    // newmsg_->mode = SIS_MSG_MODE_PUB;

    // 服务名可更改
    SIS_NET_SET_SDS(newmsg_->switchs.sw_service, newmsg_->service, service_);
    SIS_NET_SET_SDS(newmsg_->switchs.sw_cmd, newmsg_->cmd, cmd_);    
    SIS_NET_SET_SDS(newmsg_->switchs.sw_subject, newmsg_->subject, subject_);    
    
    newmsg_->switchs.sw_sno = agomsg_->switchs.sw_sno;
    newmsg_->sno = agomsg_->sno;

    newmsg_->switchs.sw_tag  = agomsg_->switchs.sw_tag;
    newmsg_->tag = agomsg_->tag;

    SIS_NET_SET_SDS(newmsg_->switchs.sw_info, newmsg_->info, agomsg_->info);  
      
    // 操作列表数据
    if (agomsg_->switchs.sw_tag == SIS_NET_TAG_CHARS || agomsg_->switchs.sw_tag == SIS_NET_TAG_BYTES)
    if (agomsg_->argvs && agomsg_->argvs->count > 0)
    {
        newmsg_->switchs.sw_info = 1;
        if (!newmsg_->argvs)
        {
            newmsg_->argvs = sis_pointer_list_create();
            newmsg_->argvs->vfree = sis_object_decr;
        }
        else
        {
            sis_pointer_list_clear(newmsg_->argvs);
        }
        for (int i = 0; i < agomsg_->argvs->count; i++)
        {
            s_sis_object *obj = sis_pointer_list_get(agomsg_->argvs, i);
            sis_object_incr(obj);
            sis_pointer_list_push(newmsg_->argvs, obj);
        }        
    }
}

////////////////////////////////////////////////////////
//  s_sis_net_message 操作类函数
////////////////////////////////////////////////////////

void sis_net_message_set_subject(s_sis_net_message *netmsg_, const char *kname_, const char *sname_)
{
    sis_sdsfree(netmsg_->subject);
    netmsg_->subject = NULL;
    if (kname_ || sname_)
    {
        if (sname_)
        {
            char skey[128];
            sis_sprintf(skey, 128, "%s.%s", kname_, sname_);
            netmsg_->subject = sis_sdsnew(skey);
            // sis_sdscatfmt 这个方式似乎有问题
        }
        else
        {
            netmsg_->subject = sis_sdsnew(kname_);
        }
    }
    netmsg_->switchs.sw_subject = netmsg_->subject ? 1 : 0;
}
void sis_net_message_set_scmd(s_sis_net_message *netmsg_, const char *scmd_)
{
    sis_sdsfree(netmsg_->service);
    sis_sdsfree(netmsg_->cmd);
    netmsg_->service = NULL;
    netmsg_->cmd = NULL;
    if (scmd_)
    {
        s_sis_sds service = NULL;
        s_sis_sds command = NULL; 
        int cmds = sis_str_divide_sds(scmd_, '.', &service, &command);
        if (cmds == 2)
        {
            netmsg_->service = service;
            netmsg_->cmd = command;
        }
        else
        {
            netmsg_->cmd = service;
        }
    }
    netmsg_->switchs.sw_service = netmsg_->service ? 1 : 0;
    netmsg_->switchs.sw_cmd = netmsg_->cmd ? 1 : 0;
}
void sis_net_message_set_cmd(s_sis_net_message *netmsg_, const char *cmd_)
{
    SIS_NET_SET_STR(netmsg_->switchs.sw_cmd, netmsg_->cmd, cmd_);
}
void sis_net_message_set_service(s_sis_net_message *netmsg_, const char *service_)
{
    SIS_NET_SET_STR(netmsg_->switchs.sw_service, netmsg_->service, service_);
}
void sis_net_message_set_tag(s_sis_net_message *netmsg_, int tag_)
{
    netmsg_->tag = tag_;
    netmsg_->switchs.sw_tag = 1;
}
void sis_net_message_set_notag(s_sis_net_message *netmsg_)
{
    netmsg_->tag = 0;
    netmsg_->switchs.sw_tag = 0;
}

void sis_net_message_set_info(s_sis_net_message *netmsg_, void *val_, size_t vlen_)
{
    SIS_NET_SET_BUF(netmsg_->switchs.sw_info, netmsg_->info, (char *)val_, vlen_);
}

void sis_net_message_set_info_i(s_sis_net_message *netmsg_, int64 i64_)
{
    netmsg_->switchs.sw_info = 1;
    sis_sdsfree(netmsg_->info);

    char sint[32];
    sis_llutoa(i64_, sint, 32, 10);
    netmsg_->info = sis_sdsnewlen(sint, sis_strlen(sint));
}

// 准备写入数据列表
void sis_net_message_init_chars(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    sis_net_message_set_tag(netmsg_, SIS_NET_TAG_CHARS);
    if (!netmsg_->argvs)
	{
		netmsg_->argvs = sis_pointer_list_create();
		netmsg_->argvs->vfree = sis_object_decr;
	}
    else
    {
        sis_pointer_list_clear(netmsg_->argvs);
    }  
}
void sis_net_message_set_chars(s_sis_net_message *netmsg_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    if (netmsg_->tag != SIS_NET_TAG_CHARS)
    {
        sis_net_message_init_chars(netmsg_);
    }
    // SIS_NET_SET_STR(netmsg_->switchs.sw_service, netmsg_->service, service_);
    // SIS_NET_SET_STR(netmsg_->switchs.sw_cmd, netmsg_->cmd, cmd_);
    // SIS_NET_SET_STR(netmsg_->switchs.sw_subject, netmsg_->subject, subject_);
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(val_, vlen_));
	sis_pointer_list_push(netmsg_->argvs, obj);    
}
void sis_net_message_init_bytes(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    sis_net_message_set_tag(netmsg_, SIS_NET_TAG_BYTES);
    if (!netmsg_->argvs)
	{
		netmsg_->argvs = sis_pointer_list_create();
		netmsg_->argvs->vfree = sis_object_decr;
	}
    else
    {
        sis_pointer_list_clear(netmsg_->argvs);
    }  
}
void sis_net_message_set_bytes(s_sis_net_message *netmsg_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    if (netmsg_->tag != SIS_NET_TAG_BYTES)
    {
        sis_net_message_init_bytes(netmsg_);
    }
    // SIS_NET_SET_STR(netmsg_->switchs.sw_service, netmsg_->service, service_);
    // SIS_NET_SET_STR(netmsg_->switchs.sw_cmd, netmsg_->cmd, cmd_);
    // SIS_NET_SET_STR(netmsg_->switchs.sw_subject, netmsg_->subject, subject_);
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(val_, vlen_));
	sis_pointer_list_push(netmsg_->argvs, obj);    
}
void sis_net_message_set_char(s_sis_net_message *netmsg_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    SIS_NET_SET_BUF(netmsg_->switchs.sw_info, netmsg_->info, val_, vlen_);
}
void sis_net_message_set_byte(s_sis_net_message *netmsg_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    SIS_NET_SET_BUF(netmsg_->switchs.sw_info, netmsg_->info, val_, vlen_);
}

//////////////////////////////////////////
//
//////////////////////////////////////////
void sis_net_msg_set_inside(s_sis_net_message *netmsg_)
{
    netmsg_->mode = SIS_MSG_MODE_INSIDE;
}

int64 sis_net_msg_info_as_int(s_sis_net_message *netmsg_)
{
    return netmsg_->info ? sis_atoll(netmsg_->info) : 0;
}

int sis_net_msg_info_as_date(s_sis_net_message *netmsg_)
{
    int idate = sis_net_msg_info_as_int(netmsg_); 
	return idate ? idate : sis_time_get_idate(0);
}

//////////////////////////////////////////
//
//////////////////////////////////////////

void sis_net_msg_tag_ok(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_OK;
    netmsg_->switchs.sw_tag = 1;
}
void sis_net_msg_tag_int(s_sis_net_message *netmsg_, int64 i64_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_INT;
    netmsg_->switchs.sw_tag = 1;
    sis_sdsfree(netmsg_->info);
    netmsg_->info = sis_sdsnewlong(i64_);
    netmsg_->switchs.sw_info = 1;
}
void sis_net_msg_tag_error(s_sis_net_message *netmsg_, char *rval_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_ERROR;
    netmsg_->switchs.sw_tag = 1;
    if (vlen_ == 0)
    {
        vlen_ = sis_strlen(rval_);
    }
    SIS_NET_SET_BUF(netmsg_->switchs.sw_info, netmsg_->info, rval_, vlen_);
}
void sis_net_msg_tag_null(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_NIL;
    netmsg_->switchs.sw_tag = 1;
}
void sis_net_msg_tag_sub_start(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_SUB_START;
    netmsg_->switchs.sw_tag = 1;
    SIS_NET_SET_STR(netmsg_->switchs.sw_info, netmsg_->info, info_);
}
void sis_net_msg_tag_sub_wait(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_SUB_WAIT;
    netmsg_->switchs.sw_tag = 1;
    SIS_NET_SET_STR(netmsg_->switchs.sw_info, netmsg_->info, info_);
}
void sis_net_msg_tag_sub_stop(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_SUB_STOP;
    netmsg_->switchs.sw_tag = 1;
    SIS_NET_SET_STR(netmsg_->switchs.sw_info, netmsg_->info, info_);
}

void sis_net_msg_tag_sub_open(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_SUB_OPEN;
    netmsg_->switchs.sw_tag = 1;
    SIS_NET_SET_STR(netmsg_->switchs.sw_info, netmsg_->info, info_);
}
void sis_net_msg_tag_sub_close(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->tag = SIS_NET_TAG_SUB_CLOSE;
    netmsg_->switchs.sw_tag = 1;
    SIS_NET_SET_STR(netmsg_->switchs.sw_info, netmsg_->info, info_);
}

void sis_net_msg_clear(s_sis_net_message *netmsg_)
{
    sis_net_msg_clear_service(netmsg_);
    sis_net_msg_clear_cmd(netmsg_);
    sis_net_msg_clear_info(netmsg_);
    sis_net_msg_clear_subject(netmsg_);
}
void sis_net_msg_clear_service(s_sis_net_message *netmsg_)
{
    netmsg_->switchs.sw_service = 0;
    sis_sdsfree(netmsg_->service);
    netmsg_->service = NULL;
}
void sis_net_msg_clear_cmd(s_sis_net_message *netmsg_)
{
    netmsg_->switchs.sw_cmd = 0;
    sis_sdsfree(netmsg_->cmd);
    netmsg_->cmd = NULL;
}
void sis_net_msg_clear_info(s_sis_net_message *netmsg_)
{
    netmsg_->switchs.sw_info = 0;
    sis_sdsfree(netmsg_->info);
    netmsg_->info = NULL;
}
void sis_net_msg_clear_subject(s_sis_net_message *netmsg_)
{
    netmsg_->switchs.sw_subject = 0;
    sis_sdsfree(netmsg_->subject);
    netmsg_->subject = NULL;
}
