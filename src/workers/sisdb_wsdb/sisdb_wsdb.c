#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_wsdb.h"
#include "sisdb.h"
#include "sisdb_server.h"
#include "sisdb_collect.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_wsdb_methods[] = {
    {"save", cmd_sisdb_wsdb_save, 0, NULL},  // json 格式
    {"pack", cmd_sisdb_wsdb_pack, 0, NULL},  // json 格式
};
// 通用文件存取接口
s_sis_modules sis_modules_sisdb_wsdb = {
    sisdb_wsdb_init,
    NULL,
    NULL,
    NULL,
    sisdb_wsdb_uninit,
    sisdb_wsdb_method_init,
    sisdb_wsdb_method_uninit,
    sizeof(sisdb_wsdb_methods) / sizeof(s_sis_method),
    sisdb_wsdb_methods,
};

bool sisdb_wsdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_wsdb_cxt *context = SIS_MALLOC(s_sisdb_wsdb_cxt, context);
    worker->context = context;
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "work-path");
        if (sonnode)
        {
            context->work_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->work_path = sis_sdsnew("data/");
        }
        if (!sis_path_exists(context->work_path))
        {
            sis_path_mkdir(context->work_path);
        }
    }
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "safe-path");
        if (sonnode)
        {
            context->safe_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->safe_path = sis_sdsnew("data/safe/");
        }
        if (!sis_path_exists(context->safe_path))
        {
            sis_path_mkdir(context->safe_path);
        }
    }
    context->page_size = sis_json_get_int(node, "page-size", 500) * 1000000;

    return true;
}
void sisdb_wsdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;
    sis_sdsfree(context->work_path);
    sis_sdsfree(context->safe_path);
    sis_free(context);

}
void sisdb_wsdb_method_init(void *worker_)
{
}
void sisdb_wsdb_method_uninit(void *worker_)
{
}

s_sis_sds _sisdb_get_keys(s_sisdb_cxt *cxt)
{
    int nums = 0;
    s_sis_sds msg = sis_sdsempty();
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(cxt->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_TABLE)
            {
                char keyn[128], sdbn[128]; 
                int cmds = sis_str_divide(collect->name, '.', keyn, sdbn);
                if (cmds == 2)
                {
                    if (nums > 0)
                    {
                        msg = sis_sdscat(msg, ",");
                    }
                    nums++;
                    msg = sis_sdscatfmt(msg, "%s", keyn);
                }                
            }
        }
        sis_dict_iter_free(di);
    }  
    printf("keys = %s\n", msg);
    return msg;    
}

s_sis_sds _sisdb_get_sdbs(s_sisdb_cxt *cxt)
{
    int count = sis_map_list_getsize(cxt->work_sdbs);
    s_sis_json_node *sdbs_node = sis_json_create_object();
    {
        for(int i = 0; i < count; i++)
        {
            s_sisdb_table *sdb = (s_sisdb_table *)sis_map_list_geti(cxt->work_sdbs, i);
            sis_json_object_add_node(sdbs_node, sdb->db->name, sis_dynamic_dbinfo_to_json(sdb->db));
        }
    }
    s_sis_sds msg = sis_json_to_sds(sdbs_node, true);
    printf("sdbs = %s\n", msg);
    sis_json_delete_node(sdbs_node);
    return msg;
}

int cmd_sisdb_wsdb_save(void *worker_, void *argv_)
{
    s_sis_message *msg = (s_sis_message *)argv_;
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)sis_message_get(msg, "sisdb");
    if (!sisdb)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

    if (sis_map_pointer_getsize(sisdb->work_keys) > 0)
    {
        s_sis_disk_class *sdbfile = sis_disk_class_create();
        sis_disk_class_init(sdbfile, SIS_DISK_TYPE_SDB, context->work_path, sisdb->dbname, 0);
        // 不能删除老文件的信息
        sis_disk_file_write_start(sdbfile);
        {
            s_sis_sds keys = _sisdb_get_keys(sisdb);
            sis_disk_class_set_key(sdbfile, true, keys, sis_sdslen(keys));
            s_sis_sds sdbs = _sisdb_get_sdbs(sisdb);
            sis_disk_class_set_sdb(sdbfile, true, sdbs, sis_sdslen(sdbs));
            sis_sdsfree(keys);
            sis_sdsfree(sdbs);
        }

        // 写入自由和结构键值
        {
            char keyn[128], sdbn[128]; 
            s_sis_memory *memory = sis_memory_create();
            s_sis_dict_entry *de;
            s_sis_dict_iter *di = sis_dict_get_iter(sisdb->work_keys);
            while ((de = sis_dict_next(di)) != NULL)
            {
                s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
                sis_str_divide(collect->name, '.', keyn, sdbn); 
                if (collect->style == SISDB_COLLECT_TYPE_TABLE)
                {
                    sis_disk_file_write_sdb(sdbfile, keyn, collect->sdb->db->name, 
                        SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
                }
                else 
                if (collect->style == SISDB_COLLECT_TYPE_CHARS || collect->style == SISDB_COLLECT_TYPE_BYTES)
                {
                    sis_memory_clear(memory);
                    sis_memory_cat_byte(memory, collect->style, 1);
                    sis_memory_cat(memory, SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
                    sis_disk_file_write_any(sdbfile, sis_dict_getkey(de), sis_memory(memory), sis_memory_get_size(memory));
                }
            }
            sis_dict_iter_free(di);
            sis_memory_destroy(memory);
        }
        sis_disk_file_write_stop(sdbfile);
        sis_disk_class_destroy(sdbfile);
    }

    return SIS_METHOD_OK;
}

int cmd_sisdb_wsdb_pack(void *worker_, void *argv_)
{
    s_sis_message *msg = (s_sis_message *)argv_;
    s_sis_sds dbname = sis_message_get_str(msg, "dbname");
    if (!dbname)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

    // 只处理 sdb 的数据 sno 数据本来就是没有冗余的
    s_sis_disk_class *srcfile = sis_disk_class_create();
    sis_disk_class_init(srcfile, SIS_DISK_TYPE_SDB, context->work_path, dbname, 0);
    sis_disk_file_move(srcfile, context->safe_path);
    sis_disk_class_init(srcfile, SIS_DISK_TYPE_SDB, context->safe_path, dbname, 0);

    s_sis_disk_class *desfile = sis_disk_class_create();
    sis_disk_class_init(desfile, SIS_DISK_TYPE_SDB, context->work_path, dbname, 0);

    size_t size = sis_disk_file_pack(srcfile, desfile);

    if (size == 0)
    {
        sis_disk_file_delete(desfile);
        sis_disk_file_move(srcfile, context->work_path);
    }   
    sis_disk_class_destroy(desfile);
    sis_disk_class_destroy(srcfile);

    if (size == 0)
    {
        return SIS_METHOD_ERROR;
    }
    return SIS_METHOD_OK;
}


