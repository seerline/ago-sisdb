
#include "sisdb_file.h"

// bool sisdb_file_check(const char *service_)
// {
//     return true;
// }

bool sisdb_file_save_conf(s_sisdb_server *server_)
{
    char conf[SIS_PATH_LEN];
    sis_sprintf(conf, SIS_PATH_LEN, SIS_DB_FILE_CONF, server_->db_path, server_->db->name);
    sis_file_handle fp = sis_file_open(conf, SIS_FILE_IO_CREATE | SIS_FILE_IO_WRITE | SIS_FILE_IO_TRUCT, 0);
    if (!fp)
    {
        sis_out_log(3)("cann't open file [%s].\n", conf);
        return false;
    }
    sis_file_seek(fp, 0, SEEK_SET);

    size_t len = 0;
    char *str = sis_json_output(server_->db->conf, &len);
    sis_file_write(fp, str, 1, len);
    sis_free(str);
    sis_file_close(fp);

    char sdb[SIS_PATH_LEN];
    sis_sprintf(sdb, SIS_PATH_LEN, "%s/%s/", server_->db_path, server_->db->name);
    sis_path_complete(sdb, SIS_PATH_LEN);

    if (!sis_path_mkdir(sdb))
    {
        sis_out_log(3)("cann't create dir [%s].\n", sdb);
        return false;
    }
    return true;
}

bool _sisdb_file_save_collect_struct(s_sis_sds key_,
                                     s_sisdb_collect *unit_,
                                     sis_file_handle sdbfp_,
                                     sis_file_handle zerofp_)
{
    s_sis_sdb_head head;
    sis_strcpy((char *)&head.key, SIS_MAXLEN_KEY, key_);
    head.format = SIS_DATA_TYPE_STRUCT;
    head.size = unit_->value->count * unit_->value->len;
    // if(head.size <1)
    // {
    //     return true;
    //     // 没有内容
    // }
    sis_file_write(sdbfp_, (const char *)&head, 1, sizeof(s_sis_sdb_head));
    void *val = sis_struct_list_first(unit_->value);
    if(val)
    {
        sis_file_write(sdbfp_, val, 1, head.size);
    }
    return true;
}

bool _sisdb_file_save_collects(s_sis_db *db_, sis_file_handle sdbfp_, sis_file_handle zerofp_)
{
    s_sis_dict_entry *de;
    // 先存有config信息的表
    s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
        if(val->db->control.issys) 
        {
            _sisdb_file_save_collect_struct(sis_dict_getkey(de), val, sdbfp_, zerofp_);
        }
    }
    sis_dict_iter_free(di);
    // 再存普通的表
    di = sis_dict_get_iter(db_->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
        if(!val->db->control.issys) 
        {
            _sisdb_file_save_collect_struct(sis_dict_getkey(de), val, sdbfp_, zerofp_);
        }
    }
    sis_dict_iter_free(di);
    return true;
}
bool _sisdb_file_save_sdb(s_sisdb_server *server_)
{
    // 打开sdb并移到文件尾，准备追加数据
    char sdb[SIS_PATH_LEN];
    sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_MAIN, server_->db_path, server_->db->name);

    sis_file_handle sdb_fp = sis_file_open(sdb, SIS_FILE_IO_CREATE | SIS_FILE_IO_WRITE | SIS_FILE_IO_TRUCT, 0);
    if (!sdb_fp)
    {
        sis_out_log(3)("cann't open file [%s].\n", sdb);
        return false;
    }
    // sis_file_seek(fp, 0, SEEK_END);
    sis_file_seek(sdb_fp, 0, SEEK_SET);

#ifdef SIS_DB_FILE_USE_CATCH
    // 打开sdb.0并截断到文件头，准备写入数据
    char zero[SIS_PATH_LEN];
    sis_sprintf(zero, SIS_PATH_LEN, SIS_DB_FILE_ZERO, server_->db_path, server_->db->name);
    sis_file_handle zero_fp = sis_file_open(zero, SIS_FILE_IO_CREATE | SIS_FILE_IO_WRITE | SIS_FILE_IO_TRUCT, 0);
    if (!zero_fp)
    {
        sis_out_log(3)("cann't open file [%s].\n", zero);
        sis_file_close(sdb_fp);
        return false;
    }
    sis_file_seek(zero_fp, 0, SEEK_SET);
    _sisdb_file_save_collects(tb_, sdb_fp, zero_fp);
    sis_file_close(zero_fp);
#else
    _sisdb_file_save_collects(server_->db, sdb_fp, 0);
#endif

    sis_file_close(sdb_fp);

    return true;
}
bool _sisdb_file_save_collect_other(s_sisdb_server *server_ ,s_sis_sds key_, s_sisdb_collect *unit_)
{
	char code[SIS_MAXLEN_CODE];
	char dbname[SIS_MAXLEN_TABLE];
    sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);
	sis_str_substr(dbname, SIS_MAXLEN_TABLE, key_, '.', 1);

    char sdb[SIS_PATH_LEN];

    void *val = sis_struct_list_first(unit_->value);
    s_sis_sds out = sis_sdsnewlen(val, unit_->value->count * unit_->value->len);
    s_sis_sds other = NULL;

    switch (server_->db->save_format)
    {
    case SIS_DATA_TYPE_JSON:
		other = sisdb_collect_struct_to_json_sds(unit_, out, unit_->db->field_name, false);
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_JSON, server_->db_path, server_->db->name,
                    code, dbname);
		// 带other去写文件
        sis_file_sds_write(sdb, other);
		sis_sdsfree(other);
		break;
	case SIS_DATA_TYPE_ARRAY:
		other = sisdb_collect_struct_to_array_sds(unit_, out, unit_->db->field_name, false);
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_ARRAY, server_->db_path, server_->db->name,
                    code, dbname);
		// 带other去写文件
        sis_file_sds_write(sdb, other);
        sis_sdsfree(other);
		break;
    // case SIS_DATA_TYPE_ZIP:
    //     break;    
    case SIS_DATA_TYPE_CSV:
		other = sisdb_collect_struct_to_csv_sds(unit_, out, unit_->db->field_name, false);
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_CSV, server_->db_path, server_->db->name,
                    code, dbname);
		// 带other去写文件
        sis_file_sds_write(sdb, other);
        sis_sdsfree(other);
        break;  
    }
    sis_sdsfree(out);

    return true;
}
bool _sisdb_file_save_other(s_sisdb_server *server_)
{
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(server_->db->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
        _sisdb_file_save_collect_other(server_, sis_dict_getkey(de), val);
    }
    sis_dict_iter_free(di);
    return true;

}

bool sisdb_file_save(s_sisdb_server *server_)
{

    sisdb_file_save_conf(server_);

    switch (server_->db->save_format)
    {
    case SIS_DATA_TYPE_JSON:
    case SIS_DATA_TYPE_ARRAY:
    case SIS_DATA_TYPE_CSV:
        _sisdb_file_save_other(server_);
        break;
    case SIS_DATA_TYPE_STRUCT:
    default:
        _sisdb_file_save_sdb(server_);
        break;
    }
    // 最后删除aof文件
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof, SIS_PATH_LEN, SIS_DB_FILE_AOF, server_->db_path, server_->db->name);
    sis_file_delete(aof);

    return true;
}
bool sisdb_file_get_outdisk(const char * key_,  int fmt_, s_sis_sds in_)
{   
    s_sisdb_server *server = sisdb_get_server();

	s_sisdb_collect *collect = sisdb_get_collect(server->db, key_);
	if (!collect)
	{
		sis_out_log(3)("no find %s key.\n", key_);
		return false;
	}
	char code[SIS_MAXLEN_CODE];
	char dbname[SIS_MAXLEN_TABLE];
    sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);
	sis_str_substr(dbname, SIS_MAXLEN_TABLE, key_, '.', 1);

	s_sis_sds other = NULL;
    char sdb[SIS_PATH_LEN];

	switch (fmt_)
	{
    // case SIS_DATA_TYPE_ZIP:
    //     break;    
	case SIS_DATA_TYPE_STRUCT:
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_STRUCT, server->db_path, server->db->name,
                    code, dbname);
        // 直接写文件
        sis_file_sds_write(sdb, in_);
		break;
	case SIS_DATA_TYPE_JSON:
		other = sisdb_collect_struct_to_json_sds(collect, in_, collect->db->field_name, false);
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_JSON, server->db_path, server->db->name,
                    code, dbname);
		// 带other去写文件
        sis_file_sds_write(sdb, other);
		sis_sdsfree(other);
		break;
	case SIS_DATA_TYPE_ARRAY:
		other = sisdb_collect_struct_to_array_sds(collect, in_, collect->db->field_name, false);
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_ARRAY, server->db_path, server->db->name,
                    code, dbname);
		// 带other去写文件
        sis_file_sds_write(sdb, other);
        sis_sdsfree(other);
		break;
    // case SIS_DATA_TYPE_ZIP:
    //     break;    
    case SIS_DATA_TYPE_CSV:
		other = sisdb_collect_struct_to_csv_sds(collect, in_, collect->db->field_name, false);
        sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_OUT_CSV, server->db_path, server->db->name,
                    code, dbname);
		// 带other去写文件
        sis_file_sds_write(sdb, other);
        sis_sdsfree(other);
        break;  
    default:
        sis_out_log(3)("save data type [%d] error.\n", fmt_);
        break;
        // return false;          
	}
    return true;
}


bool sisdb_file_save_aof(s_sisdb_server *server_,
                         int type_, const char *key_,
                         const char *val_, size_t len_)
{
    // 打开sdb并移到文件尾，准备追加数据
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof, SIS_PATH_LEN, SIS_DB_FILE_AOF, server_->db_path, server_->db->name);

    sis_file_handle fp = sis_file_open(aof, SIS_FILE_IO_CREATE | SIS_FILE_IO_WRITE, 0);
    if (!fp)
    {
        printf("open %s error\n", aof);
        return false;
    }
    sis_file_seek(fp, 0, SEEK_END);

    s_sis_aof_head head;
    sis_strncpy((char *)&head.key, SIS_MAXLEN_KEY,
                key_, SIS_MAXLEN_KEY);
    head.type = type_;
    head.size = len_;

    sis_file_write(fp, (const char *)&head, 1, sizeof(s_sis_aof_head));
    if (head.size>0)
    {
        sis_file_write(fp, val_, 1, head.size);
    }

    sis_file_close(fp);
    return true;
}
//结构体存盘必须写结构长度，方便校验
// ------------------- load file -------------------------- //
bool _sisdb_file_load_aof(s_sisdb_server *server_)
{
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof, SIS_PATH_LEN, SIS_DB_FILE_AOF, server_->db_path, server_->db->name);

    sis_file_handle fp = sis_file_open(aof, SIS_FILE_IO_READ, 0);
    if (!fp)
    {
        return false;
    }
    sis_file_seek(fp, 0, SEEK_SET);

    bool hashead = false;
    s_sis_aof_head head;
    s_sis_memory *buffer = sis_memory_create();
    while (1)
    {
        size_t bytes = sis_memory_readfile(buffer, fp, SIS_DB_MEMORY_SIZE);
        // 结构化文件这样读没问题，
        if (bytes <= 0)
        {    break;}
        // printf("read %d\n",(int)bytes);
        while (sis_memory_get_size(buffer) >= sizeof(s_sis_aof_head))
        {
            if (!hashead)
            {
                memmove(&head, sis_memory(buffer), sizeof(s_sis_aof_head));
                sis_memory_move(buffer, sizeof(s_sis_aof_head));
                hashead = true;
            }
            if (sis_memory_get_size(buffer) < head.size)
            {
                break;
            }
            // 不拷贝内存，只是移动指针，但移动后求出的sis_memory_get_size需要减少
            // printf("%s type :%d size: %d\n",head.key, head.type, head.size);
            switch (head.type)
            {
                case SIS_AOF_TYPE_DEL:
                    sisdb_delete(head.key, sis_memory(buffer), head.size);
                    break;
                case SIS_AOF_TYPE_JSET:
                    sisdb_set(SIS_DATA_TYPE_JSON, head.key, sis_memory(buffer), head.size);
                    break;
                case SIS_AOF_TYPE_ASET:
                    sisdb_set(SIS_DATA_TYPE_ARRAY, head.key, sis_memory(buffer), head.size);
                    break;
                case SIS_AOF_TYPE_SSET:
                    sisdb_set(SIS_DATA_TYPE_STRUCT, head.key, sis_memory(buffer), head.size);
                    break;            
                case SIS_AOF_TYPE_CREATE:
                    sisdb_new(head.key, sis_memory(buffer), head.size);
                    break;            
                default:
                    break;
            }
            sis_memory_move(buffer, head.size);
            hashead = false;
        }
    }
    sis_memory_destroy(buffer);
    sis_file_close(fp);
    return true;
}

int _sisdb_file_load_collect_alone(s_sis_db *db_, const char *key_, const char *in_, size_t ilen_)
{
    int o = 0; 
    s_sisdb_collect *collect = sisdb_get_collect(db_, key_);
    if (!collect)
    {
        collect = sisdb_collect_create(db_, key_);
        if (!collect)
        {
            return o;
        }
        // 进行其他的处理
    }
    // 先储存上一次的数据，
    o = sisdb_collect_update_block(collect, in_, ilen_);

    sisdb_sys_check_write(db_, key_, collect);        
 
    return o;
}
bool _sisdb_file_load_collects(s_sis_db *db_, sis_file_handle fp_)
{
    bool hashead = false;
    s_sis_sdb_head head;
    s_sis_memory *buffer = sis_memory_create();
    while (1)
    {
        size_t bytes = sis_memory_readfile(buffer, fp_, SIS_DB_MEMORY_SIZE);
        if (bytes <= 0)
            break;
        while (sis_memory_get_size(buffer) > sizeof(s_sis_sdb_head))
        {
            if (!hashead)
            {
                memmove(&head, sis_memory(buffer), sizeof(s_sis_sdb_head));
                sis_memory_move(buffer, sizeof(s_sis_sdb_head));
                hashead = true;
            }
            // printf("load table %lu size=%d\n",sis_memory_get_size(buffer) , head.size);
            if (sis_memory_get_size(buffer) < head.size)
            {
                break;
            }
            // 不拷贝内存，只是移动指针，但移动后求出的sis_memory_get_size需要减少
            // printf("load table name=%s  %s size=%d\n",tb_->name,head.code,head.size);
            // sisdb_set_direct(head.format, tb_->name, head.code, sis_memory(buffer), head.size);
            _sisdb_file_load_collect_alone(db_, head.key, sis_memory(buffer), head.size);

            sis_memory_move(buffer, head.size);
            // sis_memory_pack(buffer);
            hashead = false;
            // sis_sleep(1000*100);
        }
    }
    sis_memory_destroy(buffer);
    // 释放没有移动前的指针
    // 这里重建info信息就不用担心写入时key的写入顺序了，可以全部加载完后再统一处理info信息

    return true;
}
bool sisdb_file_load(s_sisdb_server *server_)
{
    // 遍历加载所有数据表
    char sdb[SIS_PATH_LEN];

    sis_sprintf(sdb, SIS_PATH_LEN, SIS_DB_FILE_MAIN, server_->db_path, server_->db->name);

    server_->db->loading = true;
    sis_file_handle fp = sis_file_open(sdb, SIS_FILE_IO_READ, 0);
    if (fp)
    {
        sis_file_seek(fp, 0, SEEK_SET);
        _sisdb_file_load_collects(server_->db, fp);
        sis_file_close(fp);
    }
    // 加载完毕再加载aof文件
    // 如果有aof文件表示非正常退出，需要执行存盘操作，
    if (_sisdb_file_load_aof(server_))
    {
        sisdb_file_save(server_);
    }
    server_->db->loading = false;
    return true;
}

