
#include "sts_db_file.h"

// bool sts_db_file_check(const char *service_)
// {
//     return true;
// }

bool sts_db_file_save_conf(const char *dbpath_, s_sts_db *db_)
{
    char conf[STS_PATH_LEN];
    sts_sprintf(conf,STS_PATH_LEN, STS_DB_FILE_CONF, dbpath_, db_->name);
    sts_file_handle fp = sts_file_open(conf, STS_FILE_IO_CREATE|STS_FILE_IO_WRITE|STS_FILE_IO_TRUCT, 0);
	if (!fp)
	{
		sts_out_error(3)("cann't open file [%s].\n", conf);
		return false;
	}
	sts_file_seek(fp, 0, SEEK_SET);
    sts_file_write(fp, db_->conf, 1, sts_sdslen(db_->conf)); 
    sts_file_close(fp);

    char sdb[STS_PATH_LEN];
    sts_sprintf(sdb,STS_PATH_LEN, "%s/%s/", dbpath_, db_->name);
    sts_path_complete(sdb,STS_PATH_LEN);

    if(!sts_path_mkdir(sdb))
    {
		sts_out_error(3)("cann't create dir [%s].\n", sdb);
		return false;        
    }
    return true;
}
  
bool _sts_db_file_save_collect_struct(s_sts_sds key_, 
        s_sts_collect_unit *unit_,
        sts_file_handle sdbfp_,
        sts_file_handle zerofp_)
{
    s_sts_sdb_head head;
    sts_strncpy((char *)&head.code,9,key_,9);
    head.format = STS_DATA_STRUCT;
    head.size = unit_->value->count*unit_->value->len;

    sts_file_write(sdbfp_,(const char *)&head,1,sizeof(s_sts_sdb_head));
    void *val = sts_struct_list_first(unit_->value);
    sts_file_write(sdbfp_,val,1,head.size);
    return true;
}
bool _sts_db_file_save_collect(s_sts_sds key_, 
        s_sts_collect_unit *unit_,
        sts_file_handle sdbfp_,
        sts_file_handle zerofp_)
{
    // 得到存盘方式
    int format = unit_->father->father->save_format;
    // 目前仅仅以struct来保存
    bool o = false;
    switch (format) {
        case STS_DATA_JSON:
        case STS_DATA_ARRAY:
        case STS_DATA_ZIP:
            break;
        case STS_DATA_STRUCT:
        default:
            o = _sts_db_file_save_collect_struct(key_, unit_,sdbfp_,zerofp_);
            break;
    }
    return o;
}
bool _sts_db_file_save_table(s_sts_table *tb_,sts_file_handle sdbfp_,sts_file_handle zerofp_)
{
	s_sts_dict_entry *de;
	s_sts_dict_iter *di = sts_dict_get_iter(tb_->collect_map);
	while ((de = sts_dict_next(di)) != NULL)
	{
		s_sts_collect_unit *val = (s_sts_collect_unit *)sts_dict_getval(de);
        _sts_db_file_save_collect(sts_dict_getkey(de), val, sdbfp_, zerofp_);
	}
	sts_dict_iter_free(di);    
    return true;
}
bool _sts_db_file_save_sdb(const char *dbpath_, s_sts_db *db_,s_sts_table *tb_)
{
    // 打开sdb并移到文件尾，准备追加数据
    char sdb[STS_PATH_LEN];
    sts_sprintf(sdb,STS_PATH_LEN, STS_DB_FILE_MAIN, dbpath_, db_->name, tb_->name);

    sts_file_handle sdb_fp = sts_file_open(sdb, STS_FILE_IO_CREATE|STS_FILE_IO_WRITE|STS_FILE_IO_TRUCT, 0);
	if (!sdb_fp)
	{
		sts_out_error(3)("cann't open file [%s].\n", sdb);
		return false;
	}
	// sts_file_seek(fp, 0, SEEK_END);
	sts_file_seek(sdb_fp, 0, SEEK_SET);
 
#ifdef STS_DB_FILE_USE_CATCH
    // 打开sdb.0并截断到文件头，准备写入数据
    char zero[STS_PATH_LEN];
    sts_sprintf(zero,STS_PATH_LEN, STS_DB_FILE_ZERO, dbpath_, db_->name, tb_->name);
    sts_file_handle zero_fp = sts_file_open(zero, STS_FILE_IO_CREATE|STS_FILE_IO_WRITE|STS_FILE_IO_TRUCT, 0);
	if (!zero_fp)
	{
		sts_out_error(3)("cann't open file [%s].\n", zero);
        sts_file_close(sdb_fp);
		return false;
	}
	sts_file_seek(zero_fp, 0, SEEK_SET);
    _sts_db_file_save_table(tb_, sdb_fp, zero_fp);
    sts_file_close(zero_fp);
#else
    _sts_db_file_save_table(tb_, sdb_fp, 0);
#endif

    sts_file_close(sdb_fp);

    return true;
}

bool sts_db_file_save(const char *dbpath_, s_sts_db *db_)
{
    sts_db_file_save_conf(dbpath_,db_);

 	s_sts_dict_entry *de;
	s_sts_dict_iter *di = sts_dict_get_iter(db_->db);
	while ((de = sts_dict_next(di)) != NULL)
	{
		s_sts_table *val = (s_sts_table *)sts_dict_getval(de);
        _sts_db_file_save_sdb(dbpath_, db_, val);
	}
	sts_dict_iter_free(di);   

    // 最后删除aof文件
    char aof[STS_PATH_LEN];
    sts_sprintf(aof,STS_PATH_LEN, STS_DB_FILE_AOF, dbpath_, db_->name);
    sts_file_delete(aof);

    return true;
}


bool sts_db_file_save_aof(const char *dbpath_, s_sts_db *db_, 
            int format_, const char *tb_, const char *key_, 
            const char *val_, size_t len_)
{
    // 打开sdb并移到文件尾，准备追加数据
    char aof[STS_PATH_LEN];
    sts_sprintf(aof,STS_PATH_LEN, STS_DB_FILE_AOF, dbpath_, db_->name);

    sts_file_handle fp = sts_file_open(aof, STS_FILE_IO_CREATE|STS_FILE_IO_WRITE, 0);
	if (!fp)
	{
        printf("open %s error\n",aof);
		return false;
	}
    sts_file_seek(fp, 0 ,SEEK_END);

    s_sts_aof_head head;
    sts_strncpy((char *)&head.table,STS_TABLE_MAXLEN,tb_,STS_TABLE_MAXLEN);
    sts_strncpy((char *)&head.code,9,key_,9);
    head.format = STS_DATA_STRUCT;
    head.size = len_;

    sts_file_write(fp,(const char *)&head,1,sizeof(s_sts_aof_head));
    sts_file_write(fp,val_,1,head.size);
    
    sts_file_close(fp);
    return true;
}
// ------------------- load file -------------------------- //
bool sts_db_file_load_aof(const char *dbpath_, s_sts_db *db_)
{
    char aof[STS_PATH_LEN];
    sts_sprintf(aof,STS_PATH_LEN, STS_DB_FILE_AOF, dbpath_, db_->name);

    sts_file_handle fp = sts_file_open(aof, STS_FILE_IO_READ, 0);
	if (!fp)
	{
		return false;
	}
    sts_file_seek(fp, 0 ,SEEK_SET);

    bool hashead =false;
    s_sts_aof_head head;
    s_sts_memory *buffer = sts_memory_create();    
	while (1)
	{
		size_t bytes = sts_memory_readfile(buffer, fp, STS_DB_MEMORY_SIZE);
		if (bytes <= 0) break;
		while (sts_memory_get_size(buffer) > sizeof(s_sts_aof_head))
		{
            if (!hashead) {
                memmove(&head, sts_memory(buffer),sizeof(s_sts_aof_head));
                sts_memory_move(buffer, sizeof(s_sts_aof_head));
                hashead = true;    
            }
			if (sts_memory_get_size(buffer) < head.size) 
            {
                break;
            }
            // 不拷贝内存，只是移动指针，但移动后求出的sts_memory_get_size需要减少
            stsdb_set_format(head.format, head.table, head.code, sts_memory(buffer), head.size);
            sts_memory_move(buffer, head.size);
            sts_memory_pack(buffer);
            hashead=false;
        }
    }  
    sts_memory_destroy(buffer);
    sts_file_close(fp);
    return true;
    
}
bool _sts_db_file_load_table(s_sts_table *tb_,sts_file_handle fp_)
{
    bool hashead =false;
    s_sts_sdb_head head;
    s_sts_memory *buffer = sts_memory_create();    
	while (1)
	{
		size_t bytes = sts_memory_readfile(buffer, fp_, STS_DB_MEMORY_SIZE);
		if (bytes <= 0) break;
		while (sts_memory_get_size(buffer) > sizeof(s_sts_sdb_head))
		{
            if (!hashead) {
                memmove(&head, sts_memory(buffer),sizeof(s_sts_sdb_head));
                sts_memory_move(buffer, sizeof(s_sts_sdb_head));
                hashead = true;    
            }
			if (sts_memory_get_size(buffer) < head.size) 
            {
                break;
            }
            // 不拷贝内存，只是移动指针，但移动后求出的sts_memory_get_size需要减少
            stsdb_set_format(head.format, tb_->name, head.code, sts_memory(buffer), head.size);
            sts_memory_move(buffer, head.size);
            sts_memory_pack(buffer);
            hashead=false;
            // sts_sleep(1000*100);
        }
    }  
    sts_memory_destroy(buffer);
    // 释放没有移动前的指针
    return true;
}
bool sts_db_file_load(const char *dbpath_, s_sts_db *db_)
{
    // 遍历加载所有数据表
    char sdb[STS_PATH_LEN];

 	s_sts_dict_entry *de;
	s_sts_dict_iter *di = sts_dict_get_iter(db_->db);
	while ((de = sts_dict_next(di)) != NULL)
	{
		s_sts_table *val = (s_sts_table *)sts_dict_getval(de);
        sts_sprintf(sdb,STS_PATH_LEN, STS_DB_FILE_MAIN, dbpath_, db_->name, val->name);
        sts_file_handle fp = sts_file_open(sdb, STS_FILE_IO_READ, 0);
	    if (fp)
	    {
	        sts_file_seek(fp, 0, SEEK_SET);
            _sts_db_file_load_table(val, fp);
            sts_file_close(fp);
	    }
	}
	sts_dict_iter_free(di);   
    // 加载完毕再加载aof文件
    // 如果有aof文件表示非正常退出，需要执行存盘操作，
    if (sts_db_file_load_aof(dbpath_,db_)) {
        sts_db_file_save(dbpath_, db_);
    }
    return true;
}
