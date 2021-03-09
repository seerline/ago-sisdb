
#include "worker.h"
#include "server.h"

#include <sis_modules.h>
#include <sisdb_rsno.h>
#include <sis_obj.h>

// 从行情流文件中获取数据源
static s_sis_method _sisdb_rsno_methods[] = {
  {"sub",    cmd_sisdb_rsno_sub, 0, NULL},
  {"unsub",  cmd_sisdb_rsno_unsub, 0, NULL},
  {"get",    cmd_sisdb_rsno_get, 0, NULL},
  {"setcb",  cmd_sisdb_rsno_setcb, 0, NULL}
};

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
s_sis_modules sis_modules_sisdb_rsno = {
    sisdb_rsno_init,
    NULL,
    sisdb_rsno_working,
    NULL,
    sisdb_rsno_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_rsno_methods)/sizeof(s_sis_method),
    _sisdb_rsno_methods,
};

bool sisdb_rsno_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;

    s_sisdb_rsno_cxt *context = SIS_MALLOC(s_sisdb_rsno_cxt, context);
    worker->context = context;

    context->work_date = sis_json_get_int(node, "work-date", sis_time_get_idate(0));

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
    }
    {
        const char *str = sis_json_get_str(node, "sub-sdbs");
        if (str)
        {
            context->work_sdbs = sis_sdsnew(str);
        }
        else
        {
            context->work_sdbs = sis_sdsnew("*");
        }
    }
    {     
        const char *str = sis_json_get_str(node, "sub-keys");
        if (str)
        {
            context->work_keys = sis_sdsnew(str);
        }
        else
        {
            context->work_keys = sis_sdsnew("*");
        }    
    }
    return true;
}

void sisdb_rsno_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    sisdb_rsno_sub_stop(context);

    sis_sdsfree(context->work_path);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);

    sis_free(context);
    worker->context = NULL;
}


///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////
void _send_rsno_compress(s_sisdb_rsno_cxt *context, int issend)
{
	s_snodb_compress *zipmem = context->rsno_ziper->zip_bits;
	zipmem->size = sis_bits_struct_getsize(context->rsno_ziper->cur_sbits);
	if ((issend && zipmem->size > 0 ) || (int)zipmem->size > context->rsno_ziper->zip_size - 256)
	{
		if (context->cb_snodb_compress)
		{	
			context->cb_snodb_compress(context->cb_source, zipmem);
		}
		if (context->rsno_ziper->cur_size > context->rsno_ziper->initsize)
		{
			snodb_worker_zip_flush(context->rsno_ziper, 1);
		}
		else
		{
			snodb_worker_zip_flush(context->rsno_ziper, 0);
		}
	}
}
static void cb_begin(void *context_, msec_t tt)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    if (context->cb_sub_start)
    {
        char sdate[32];
        sis_llutoa(tt, sdate, 32, 10);
        context->cb_sub_start(context->cb_source, sdate);
    } 
}
static void cb_end(void *context_, msec_t tt)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    
    if (context->cb_snodb_compress)
    {
       // 检查是否有剩余数据
	    _send_rsno_compress(context, 1); 
    }	

    // if (context->cb_sub_stop)
    // {
    //     char sdate[32];
    //     sis_llutoa(tt, sdate, 32, 10);
    //     context->cb_sub_stop(context->cb_source, sdate);
    // } 
}
static void cb_key(void *context_, void *key_, size_t size) 
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
	s_sis_sds srckeys = sis_sdsnewlen((char *)key_, size);
	s_sis_sds keys = sis_match_key(context->work_keys, srckeys);
    if (context->cb_dict_keys)
    {
        context->cb_dict_keys(context->cb_source, keys);
    } 
    if (context->cb_snodb_compress)
    {
    	snodb_worker_set_keys(context->rsno_ziper, keys);
    }
	sis_sdsfree(keys);
	sis_sdsfree(srckeys);
}
static void cb_sdb(void *context_, void *sdb_, size_t size)  
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
	s_sis_sds srcsdbs = sis_sdsnewlen((char *)sdb_, size);
	s_sis_sds sdbs = sis_match_sdb_of_sds(context->work_sdbs, srcsdbs);
    if (context->cb_dict_sdbs)
    {
        context->cb_dict_sdbs(context->cb_source, sdbs);
    } 
    if (context->cb_snodb_compress)
    {
    	snodb_worker_set_sdbs(context->rsno_ziper, sdbs);
    }
	sis_sdsfree(sdbs);
	sis_sdsfree(srcsdbs); 
}

static void cb_read(void *context_, const char *key_, const char *sdb_, void *out_, size_t olen_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;

    if (context->cb_sisdb_bytes)
    {
        s_sis_db_chars inmem = {0};
        inmem.kname= key_;
        inmem.sname= sdb_;
        inmem.data = out_;
        inmem.size = olen_;
        context->cb_sisdb_bytes(context->cb_source, &inmem);
    }
    if (context->cb_snodb_compress)
    {
        int kidx = sis_map_list_get_index(context->rsno_ziper->keys, key_);
        int sidx = sis_map_list_get_index(context->rsno_ziper->sdbs, sdb_);
        if (kidx < 0 || sidx < 0)
        {
            return ;
        }
        snodb_worker_zip_set(context->rsno_ziper, kidx, sidx, out_, olen_);

        _send_rsno_compress(context, 0);
    }

    // int dbid = sis_disk_class_get_sdbi(context->read_class, sdb_); 
} 
///////////////////////////////////////////
//  callback define end.
///////////////////////////////////////////
static void *_thread_snos_read_sub(void *argv_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)argv_;
    context->status = SIS_RSNO_WORK;
	// 开始读盘
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);

    char sdate[32];   
    sis_llutoa(context->work_date, sdate, 32, 10);

    callback->source = context;
    callback->cb_begin = cb_begin;
    callback->cb_key = cb_key;
    callback->cb_sdb = cb_sdb;
    callback->cb_read = cb_read;
    callback->cb_end = cb_end;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    // printf("%s| %s | %s\n", context->write_cb->sub_keys, context->work_sdbs, context->write_cb->sub_sdbs ? context->write_cb->sub_sdbs : "nil");
	if (!sis_strcasecmp("*",context->work_keys) && sis_strcasecmp("*",context->work_sdbs))
	{
		sis_disk_reader_set_sdb(reader, "*");  
		sis_disk_reader_set_key(reader, "*");
	}
	else
	{
		// 获取真实的代码
		s_sis_sds keys = sis_disk_file_get_keys(context->read_class, false);
		s_sis_sds sub_keys = sis_match_key(context->work_keys, keys);
		sis_disk_reader_set_key(reader, sub_keys);
		sis_sdsfree(sub_keys);
		sis_sdsfree(keys);

		// 获取真实的表名
		s_sis_sds sdbs = sis_disk_file_get_sdbs(context->read_class, false);
		s_sis_sds sub_sdbs = sis_match_sdb(context->work_sdbs, sdbs);
		sis_disk_reader_set_sdb(reader, sub_sdbs);  
		sis_sdsfree(sub_sdbs);
		sis_sdsfree(sdbs);
	}

    // sub 是一条一条的输出
    sis_disk_file_read_sub(context->read_class, reader);
    // get 是所有符合条件的一次性输出
    // sis_disk_file_read_get(ctrl->disk_reader, reader);
    sis_disk_reader_destroy(reader);
    sis_free(callback);
	// printf("rson sub end..\n");
    sis_disk_file_read_stop(context->read_class);

    sis_disk_class_destroy(context->read_class);
    context->read_class = NULL;

    context->status = SIS_RSNO_NONE;
    // stop 放这里
    if (context->cb_sub_stop)
    {
        char sdate[32];
        sis_llutoa(context->work_date, sdate, 32, 10);
        context->cb_sub_stop(context->cb_source, sdate);
    } 

    return NULL;
}

void sisdb_rsno_sub_start(s_sisdb_rsno_cxt *context) 
{
    context->read_class = sis_disk_class_create(); 
    context->read_class->isstop = false;

    char sdate[32];   
    sis_llutoa(context->work_date, sdate, 32, 10);

    if (sis_disk_class_init(context->read_class, SIS_DISK_TYPE_SNO, context->work_path, sdate)
     || sis_disk_file_read_start(context->read_class))
    {
        sisdb_rsno_sub_stop(context);
        if (context->cb_sub_stop)
        {
            context->cb_sub_stop(context->cb_source, sdate);
        }
        return ;
    }
	// 启动订阅线程 到这里已经保证了文件存在
	sis_thread_create(_thread_snos_read_sub, context, &context->rsno_thread);

}
void sisdb_rsno_sub_stop(s_sisdb_rsno_cxt *context)
{

	if (context->status == SIS_RSNO_WORK)
	{
		context->read_class->isstop = true;
		while (context->status != SIS_RSNO_NONE)
		{
			// 等待读盘退出
			sis_sleep(100);
		}
	}
    if (context->read_class)
    {
        sis_disk_class_destroy(context->read_class);
        context->read_class = NULL;
    }
    if (context->rsno_ziper)
    {
        snodb_worker_destroy(context->rsno_ziper);
        context->rsno_ziper = NULL;
    }
}
///////////////////////////////////////////
//  method define
/////////////////////////////////////////
int cmd_sisdb_rsno_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    printf("%s = %d\n", __func__, context->status);    
    if (context->status != SIS_RSNO_NONE)
    {
        return SIS_METHOD_ERROR;
    }

    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    // 保存订阅的内容, 回调函数地址, 然后开始订阅
    {
        s_sis_sds str = sis_message_get_str(msg, "sub-keys");
        if (str)
        {
            sis_sdsfree(context->work_keys);
            context->work_keys = sis_sdsdup(str);
        }
    }
    {
        s_sis_sds str = sis_message_get_str(msg, "sub-sdbs");
        if (str)
        {
            sis_sdsfree(context->work_sdbs);
            context->work_sdbs = sis_sdsdup(str);
        }
    }
    if (sis_message_exist(msg, "sub-date"))
    {
        context->work_date = sis_message_get_int(msg, "sub-date");
    }
    else
    {
        context->work_date = sis_time_get_idate(0);
    }

    context->cb_source          = sis_message_get(msg, "source");
    context->cb_sub_start       = sis_message_get_method(msg, "cb_sub_start"     );
    context->cb_sub_stop        = sis_message_get_method(msg, "cb_sub_stop"      );
    context->cb_dict_sdbs       = sis_message_get_method(msg, "cb_dict_sdbs"     );
    context->cb_dict_keys       = sis_message_get_method(msg, "cb_dict_keys"     );
    context->cb_snodb_compress  = sis_message_get_method(msg, "cb_snodb_compress");
    context->cb_sisdb_bytes     = sis_message_get_method(msg, "cb_sisdb_bytes"   );

    LOG(5)("sub market start. [%d]\n", context->work_date);
    if (context->cb_snodb_compress)
    {
        int zip_size = ZIPMEM_MAXSIZE;
        if (sis_message_exist(msg, "zip-size"))
        {
            zip_size = sis_message_get_int(msg, "zip-size");
        }
        int initsize = 4 * 1024 * 1024;
        if (sis_message_exist(msg, "init-size"))
        {
            initsize = sis_message_get_int(msg, "init-size");
        }
        if (!context->rsno_ziper)
        {
            context->rsno_ziper = snodb_worker_create();
        }
        else
        {
            snodb_worker_clear(context->rsno_ziper);
        }
        snodb_worker_zip_init(context->rsno_ziper, zip_size, initsize);
    }
    sisdb_rsno_sub_start(context);

    return SIS_METHOD_OK;
}
int cmd_sisdb_rsno_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    sisdb_rsno_sub_stop(context);

    return SIS_METHOD_OK;
}

int cmd_sisdb_rsno_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_; 

    char *sdate = sis_message_get_str(msg, "get-date");
    s_sis_disk_class *read_class = sis_disk_class_create(); 
    if (sis_disk_class_init(read_class, SIS_DISK_TYPE_SNO, context->work_path, sdate) 
        || sis_disk_file_read_start(read_class))
    {
        sis_disk_class_destroy(read_class);
        return SIS_METHOD_ERROR;
    }
    char *kname = sis_message_get_str(msg, "get-keys");
    char *sname = sis_message_get_str(msg, "get-sdbs");

    s_sis_disk_reader *reader = sis_disk_reader_create(NULL);
    sis_disk_reader_set_sdb(reader, sname);
    sis_disk_reader_set_key(reader, kname); 
   // get 是所有符合条件的一次性输出
    s_sis_object *obj = sis_disk_file_read_get_obj(read_class, reader);
    sis_disk_reader_destroy(reader);

    if (obj)
    {
        sis_message_set(msg, "omem", obj, sis_object_destroy);
        s_sis_disk_dict *sdict = sis_map_list_get(read_class->sdbs, sname);
        if (sdict)
        {
            s_sis_disk_dict_unit *sunit = sis_disk_dict_last(sdict);
            s_sis_json_node *node = sis_dynamic_dbinfo_to_json(sunit->db);
            s_sis_dynamic_db *diskdb = sis_dynamic_db_create(node);
            sis_json_delete_node(node);
            sis_message_set(msg, "diskdb", diskdb, sis_dynamic_db_destroy);
        }
    }
    sis_disk_file_read_stop(read_class);
    sis_disk_class_destroy(read_class); 
    return SIS_METHOD_OK;
}

int cmd_sisdb_rsno_setcb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    if (context->status != SIS_RSNO_INIT && context->status != SIS_RSNO_NONE)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_; 

    s_sis_sds wpath = sis_message_get_str(msg, "work-path");
    if (wpath)
    {
        sis_sdsfree(context->work_path);
        context->work_path = sis_sdsdup(wpath);
    }
    context->cb_source          = sis_message_get(msg, "source");
    context->cb_sub_start       = sis_message_get_method(msg, "cb_sub_start"     );
    context->cb_sub_stop        = sis_message_get_method(msg, "cb_sub_stop"      );
    context->cb_dict_sdbs       = sis_message_get_method(msg, "cb_dict_sdbs"     );
    context->cb_dict_keys       = sis_message_get_method(msg, "cb_dict_keys"     );
    context->cb_snodb_compress  = sis_message_get_method(msg, "cb_snodb_compress");
    context->cb_sisdb_bytes     = sis_message_get_method(msg, "cb_sisdb_bytes"   );

    context->status = SIS_RSNO_INIT;
    return SIS_METHOD_OK;
}

void sisdb_rsno_working(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;
    
    SIS_WAIT_OR_EXIT(context->status == SIS_RSNO_INIT);  

    if (context->status == SIS_RSNO_INIT)
    {
        if (context->work_date > 20100101 && context->work_date < 20250101)
        {
            LOG(5)("sub history start. [%d]\n", context->work_date);
            sisdb_rsno_sub_start(context);
            LOG(5)("sub history end. [%d]\n", context->work_date);
        }
    }
}