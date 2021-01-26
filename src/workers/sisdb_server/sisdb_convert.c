#include "sisdb_collect.h"
#include "sisdb_server.h"

s_sis_dynamic_db *sisdb_find_db(s_sisdb_server_cxt *server_, const char *dataset_, const char *table_)
{
    s_sis_dynamic_db *o = NULL;
    s_sis_worker *dataset = sis_map_list_get(server_->datasets, dataset_);
    if (dataset)
    {
        s_sis_message *msg = sis_message_create();
        sis_message_set_str(msg, "dbname", table_, sis_strlen(table_));
        sis_worker_command(dataset, "getdb", msg);
        o = sis_message_get(msg, "db");
        sis_message_destroy(msg);        
    }
	return o;
}
s_sisdb_convert *sisdb_convert_create(const char *dataset_, s_sis_dynamic_convert *convert_)
{
    s_sisdb_convert *o = SIS_MALLOC(s_sisdb_convert, o);
    sis_strcpy(o->dateset, 128, dataset_);
    o->convert = convert_;
    return o;
}
void sisdb_convert_destroy(void *unit_)
{
    s_sisdb_convert *unit = (s_sisdb_convert *)unit_;
    sis_dynamic_convert_destroy(unit->convert);
    sis_free(unit);
}

int sisdb_convert_init(s_sisdb_server_cxt *server_, s_sis_json_node *node_)
{
    s_sis_json_node *next = sis_conf_first_node(node_);
    while (next)
    {
        char pub[2][128]; 
        int  pubnums = sis_str_divide(next->key, '.', pub[0], pub[1]);
        if (pubnums == 2)
        {
            s_sis_dynamic_db *pub_db = sisdb_find_db(server_, pub[0], pub[1]);
            if (pub_db)
            {
                s_sis_pointer_list *sublist = sis_pointer_list_create();
                sublist->vfree = sisdb_convert_destroy;
                s_sis_json_node *subnext = sis_conf_first_node(next);
                while (subnext)
                {
                    char sub[2][128]; 
                    int  subnums = sis_str_divide(subnext->key, '.', sub[0], sub[1]);
                    if (subnums == 2)
                    {
                        s_sis_dynamic_db *sub_db = sisdb_find_db(server_, sub[0], sub[1]);
                        if (sub_db)
                        {
                            s_sis_dynamic_convert *convert = sis_dynamic_convert_create(pub_db, sub_db);
                            s_sisdb_convert *unit = sisdb_convert_create(sub[0], convert);
                            sis_pointer_list_push(sublist, unit);
                        }
                    }
                    subnext = subnext->next;
                }
                if (sublist->count > 0)
                {
                    sis_map_pointer_set(server_->converts, next->key, sublist);
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


int sisdb_convert_working(s_sisdb_server_cxt *server_, s_sis_net_message *netmsg_)
{
    s_sisdb_server_cxt *context = server_;
    s_sis_net_message *netmsg = netmsg_;

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
        s_sisdb_convert *unit = (s_sisdb_convert *)sis_pointer_list_get(sublist, i);
        size_t size = sis_dynamic_convert_length(unit->convert, netmsg->val, sis_sdslen(netmsg->val));
        char *out = sis_malloc(size + 1);
        sis_dynamic_convert(unit->convert, netmsg->val, sis_sdslen(netmsg->val), out, size);
        s_sis_net_message *newmsg = sis_net_message_clone(netmsg);
        newmsg->style |= SIS_NET_INSIDE; // 内部传递
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
        sis_lock_list_push(context->recv_list, obj);
        sis_object_destroy(obj);
    }
    return SIS_METHOD_OK;
}

