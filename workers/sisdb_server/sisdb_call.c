//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
// email : 48707400@qq.com  BP: 13816575798
//*******************************************************

#include "sisdb_call.h"
#include "sis_net.msg.h"

// 基本都是对数据库数据的整合处理

struct s_sis_method sisdb_call_methods[] = {
    {"inout",     cmd_sisdb_server_inout, 0,  NULL},   // 用户登录
};

s_sis_sds _sisdb_server_inout(s_sisdb_server_cxt *context)
{
    s_sis_sds o = NULL;
    // s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key_);
    // if (collect && collect->obj )
    // {
    //     *format_ = collect->style == SISDB_COLLECT_TYPE_CHARS ? SISDB_FORMAT_CHARS : SISDB_FORMAT_BYTES;
    //     o = sis_sdsdup(SIS_OBJ_SDS(collect->obj));
    // }
    return o;
}

int cmd_sisdb_server_inout(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sis_sds o = _sisdb_server_inout(context);

    sis_net_message_set_char(netmsg, o, sis_sdslen(o));

    return SIS_METHOD_OK;
}
