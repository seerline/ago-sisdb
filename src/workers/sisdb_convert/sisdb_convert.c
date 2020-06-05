#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_convert.h"
#include "sisdb_collect.h"
#include "sisdb_server.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_convert_methods[] = {
    {"init",  cmd_sisdb_convert_init, NULL, NULL},   
    {"write", cmd_sisdb_convert_write, NULL, NULL},  
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_convert = {
    sisdb_convert_init,
    NULL,
    NULL,
    NULL,
    sisdb_convert_uninit,
    sisdb_convert_method_init,
    sisdb_convert_method_uninit,
    sizeof(sisdb_convert_methods) / sizeof(s_sis_method),
    sisdb_convert_methods,
};

bool sisdb_convert_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_convert_cxt *context = SIS_MALLOC(s_sisdb_convert_cxt, context);
    worker->context = context;

    context->converts = sis_map_pointer_create_v(sis_pointer_list_destroy);

    return true;

}
void sisdb_convert_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_convert_cxt *context = worker->context;
    sis_map_pointer_destroy(context->converts);
}
void sisdb_convert_method_init(void *worker_)
{
}
void sisdb_convert_method_uninit(void *worker_)
{
}
s_sisdb_table *sisdb_find_table(s_sisdb_server_cxt *server_, char *dataset_, char *table_ )
{
    s_sisdb_table *o = NULL;
    s_sis_worker *dataset = sis_map_list_get(server_->datasets, dataset_);
    if (dataset)
    {
        o = sisdb_get_table((s_sisdb_cxt *)dataset->context, table_);
    }
	return o;
}

s_sisdb_convert_unit *sisdb_convert_unit_create(char *dataset_, s_sis_dynamic_convert *convert_)
{
    s_sisdb_convert_unit *o = SIS_MALLOC(s_sisdb_convert_unit, o);
    sis_strcpy(o->dateset, 128, dataset_);
    o->convert = convert_;
    return o;
}
void sisdb_convert_unit_destroy(void *unit_)
{
    s_sisdb_convert_unit *unit = (s_sisdb_convert_unit *)unit_;
    sis_dynamic_convert_destroy(unit->convert);
    sis_free(unit);
}
int cmd_sisdb_convert_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_convert_cxt *context = worker->context;

    s_sis_message *message = (s_sis_message *)argv_;

    s_sis_json_node *node = sis_message_get(message, "node");
    s_sisdb_server_cxt *server = sis_message_get(message, "server");
    if (!node || !server)
    {
        return SIS_METHOD_ERROR;
    }
    context->father  = server;
    s_sis_json_node *next = sis_conf_first_node(node);
    while (next)
    {
        char pub[2][128]; 
        int  pubnums = sis_str_divide(next->key, '.', pub[0], pub[1]);
        if (pubnums == 2)
        {
           s_sisdb_table *pub_tb = sisdb_find_table(server, pub[0], pub[1]);
            if (pub_tb)
            {
                s_sis_pointer_list *sublist = sis_pointer_list_create();
                sublist->vfree = sisdb_convert_unit_destroy;
                s_sis_json_node *subnext = sis_conf_first_node(next);
                while (subnext)
                {
                    char sub[2][128]; 
                    int  subnums = sis_str_divide(subnext->key, '.', sub[0], sub[1]);
                    s_sisdb_table *sub_tb = sisdb_find_table(server, sub[0], sub[1]);
                    if (sub_tb)
                    {
                        s_sis_dynamic_convert *convert = sis_dynamic_convert_create(pub_tb->db, sub_tb->db);
                        s_sisdb_convert_unit *unit = sisdb_convert_unit_create(sub[0], convert);
                        sis_pointer_list_push(sublist, unit);
                    }
                    subnext = subnext->next;
                }
                if (sublist->count > 0)
                {
                    sis_map_pointer_set(context->converts, next->key, sublist);
                }
                else
                {
                    sis_pointer_list_destroy(sublist);
                }
            }

        }
        next = next->next;
    }
    return SIS_METHOD_OK;
}


int cmd_sisdb_convert_write(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_convert_cxt *context = worker->context;

    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    if (netmsg->format != SIS_NET_FORMAT_BYTES)
    {
        // 非二进制暂时不支持播报
        return SIS_METHOD_ERROR;
    }

    char dataset[128]; 
    char command[128]; 
    sis_str_divide(netmsg->cmd, '.', dataset, command);
    char key[128]; 
    char sdb[128]; 
    sis_str_divide(netmsg->key, '.', key, sdb);
    char name[128];
    sis_sprintf(name, 128, "%s.%s", dataset, sdb);

    s_sis_pointer_list *sublist = sis_map_pointer_get(context->converts, name);
    if (!sublist)
    {
        return SIS_METHOD_ERROR;
    }
    for (int i = 0; i < sublist->count; i++)
    {
        s_sisdb_convert_unit *unit = (s_sisdb_convert_unit *)sis_pointer_list_get(sublist, i);
        size_t size = sis_dynamic_convert_length(unit->convert, netmsg->val, sis_sdslen(netmsg->val));
        char *out = sis_malloc(size + 1);
        sis_dynamic_convert(unit->convert, netmsg->val, sis_sdslen(netmsg->val), out, size);
        s_sis_net_message *newmsg = sis_net_message_clone(netmsg);
        sis_sdsfree(newmsg->cmd);
        newmsg->cmd = sis_sdsnew(unit->dateset);
        newmsg->cmd = sis_sdscatfmt(newmsg->cmd , ".%s", command);
        sis_sdsfree(newmsg->key);
        newmsg->key = sis_sdsnew(key);
        newmsg->key = sis_sdscatfmt(newmsg->key , ".%s", unit->convert->out->name);
        sis_sdsfree(newmsg->val);
        newmsg->val = sis_sdsnewlen(out, size);
        // 向上层队列发送信息
        s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, newmsg);
        sis_share_list_push(((s_sisdb_server_cxt *)context->father)->recv_list, obj);
        sis_object_destroy(obj);
    }
    return SIS_METHOD_OK;
}
