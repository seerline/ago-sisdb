
#include "sisdb_file.h"

// bool sisdb_file_check(const char *service_)
// {
//     return true;
// }

bool sisdb_file_save_conf(const char *dbpath_, s_sis_db *db_)
{
    char conf[SIS_PATH_LEN];
    sis_sprintf(conf,SIS_PATH_LEN, SIS_DB_FILE_CONF, dbpath_, db_->name);
    sis_file_handle fp = sis_file_open(conf, SIS_FILE_IO_CREATE|SIS_FILE_IO_WRITE|SIS_FILE_IO_TRUCT, 0);
	if (!fp)
	{
		sis_out_log(3)("cann't open file [%s].\n", conf);
		return false;
	}
	sis_file_seek(fp, 0, SEEK_SET);
    sis_file_write(fp, db_->conf, 1, sis_sdslen(db_->conf)); 
    sis_file_close(fp);

    char sdb[SIS_PATH_LEN];
    sis_sprintf(sdb,SIS_PATH_LEN, "%s/%s/", dbpath_, db_->name);
    sis_path_complete(sdb,SIS_PATH_LEN);

    if(!sis_path_mkdir(sdb))
    {
		sis_out_log(3)("cann't create dir [%s].\n", sdb);
		return false;        
    }
    return true;
}
  
bool _sisdb_file_save_collect_struct(s_sis_sds key_, 
        s_sis_collect_unit *unit_,
        sis_file_handle sdbfp_,
        sis_file_handle zerofp_)
{
    s_sis_sdb_head head;
    sis_strncpy((char *)&head.code,9,key_,9);
    head.format = SIS_DATA_STRUCT;
    head.size = unit_->value->count*unit_->value->len;

    sis_file_write(sdbfp_,(const char *)&head,1,sizeof(s_sis_sdb_head));
    void *val = sis_struct_list_first(unit_->value);
    sis_file_write(sdbfp_,val,1,head.size);
    return true;
}
bool _sisdb_file_save_collect(s_sis_sds key_, 
        s_sis_collect_unit *unit_,
        sis_file_handle sdbfp_,
        sis_file_handle zerofp_)
{
    // 得到存盘方式
    int format = unit_->father->father->save_format;
    // 目前仅仅以struct来保存
    bool o = false;
    switch (format) {
        case SIS_DATA_JSON:
        case SIS_DATA_ARRAY:
        case SIS_DATA_ZIP:
            break;
        case SIS_DATA_STRUCT:
        default:
            o = _sisdb_file_save_collect_struct(key_, unit_,sdbfp_,zerofp_);
            break;
    }
    return o;
}
bool _sisdb_file_save_table(s_sis_table *tb_,sis_file_handle sdbfp_,sis_file_handle zerofp_)
{
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->collect_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_collect_unit *val = (s_sis_collect_unit *)sis_dict_getval(de);
        _sisdb_file_save_collect(sis_dict_getkey(de), val, sdbfp_, zerofp_);
	}
	sis_dict_iter_free(di);    
    return true;
}
bool _sisdb_file_save_sdb(const char *dbpath_, s_sis_db *db_,s_sis_table *tb_)
{
    // 打开sdb并移到文件尾，准备追加数据
    char sdb[SIS_PATH_LEN];
    sis_sprintf(sdb,SIS_PATH_LEN, SIS_DB_FILE_MAIN, dbpath_, db_->name, tb_->name);

    sis_file_handle sdb_fp = sis_file_open(sdb, SIS_FILE_IO_CREATE|SIS_FILE_IO_WRITE|SIS_FILE_IO_TRUCT, 0);
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
    sis_sprintf(zero,SIS_PATH_LEN, SIS_DB_FILE_ZERO, dbpath_, db_->name, tb_->name);
    sis_file_handle zero_fp = sis_file_open(zero, SIS_FILE_IO_CREATE|SIS_FILE_IO_WRITE|SIS_FILE_IO_TRUCT, 0);
	if (!zero_fp)
	{
		sis_out_log(3)("cann't open file [%s].\n", zero);
        sis_file_close(sdb_fp);
		return false;
	}
	sis_file_seek(zero_fp, 0, SEEK_SET);
    _sisdb_file_save_table(tb_, sdb_fp, zero_fp);
    sis_file_close(zero_fp);
#else
    _sisdb_file_save_table(tb_, sdb_fp, 0);
#endif

    sis_file_close(sdb_fp);

    return true;
}

bool sisdb_file_save(const char *dbpath_, s_sis_db *db_)
{
    sisdb_file_save_conf(dbpath_,db_);

 	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->db);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_table *val = (s_sis_table *)sis_dict_getval(de);
        _sisdb_file_save_sdb(dbpath_, db_, val);
	}
	sis_dict_iter_free(di);   

    // 最后删除aof文件
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof,SIS_PATH_LEN, SIS_DB_FILE_AOF, dbpath_, db_->name);
    sis_file_delete(aof);

    return true;
}

bool sisdb_file_saveto(const char *dbpath_, s_sis_db *db_, int format_, const char *tb_)
{
    //???
    sisdb_file_save_conf(dbpath_,db_);

 	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->db);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_table *val = (s_sis_table *)sis_dict_getval(de);
        _sisdb_file_save_sdb(dbpath_, db_, val);
	}
	sis_dict_iter_free(di);   

    // 最后删除aof文件
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof,SIS_PATH_LEN, SIS_DB_FILE_AOF, dbpath_, db_->name);
    sis_file_delete(aof);

    return true;    
}

bool sisdb_file_save_aof(const char *dbpath_, s_sis_db *db_, 
            int format_, const char *tb_, const char *key_, 
            const char *val_, size_t len_)
{
    // 打开sdb并移到文件尾，准备追加数据
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof,SIS_PATH_LEN, SIS_DB_FILE_AOF, dbpath_, db_->name);

    sis_file_handle fp = sis_file_open(aof, SIS_FILE_IO_CREATE|SIS_FILE_IO_WRITE, 0);
	if (!fp)
	{
        printf("open %s error\n",aof);
		return false;
	}
    sis_file_seek(fp, 0 ,SEEK_END);

    s_sis_aof_head head;
    sis_strncpy((char *)&head.table,SIS_TABLE_MAXLEN,tb_,SIS_TABLE_MAXLEN);
    sis_strncpy((char *)&head.code,SIS_CODE_MAXLEN,key_,SIS_CODE_MAXLEN);
    head.format = format_;
    head.size = len_;

    sis_file_write(fp,(const char *)&head,1,sizeof(s_sis_aof_head));
    sis_file_write(fp,val_,1,head.size);
    
    sis_file_close(fp);
    return true;
}
// ------------------- load file -------------------------- //
bool sisdb_file_load_aof(const char *dbpath_, s_sis_db *db_)
{
    char aof[SIS_PATH_LEN];
    sis_sprintf(aof,SIS_PATH_LEN, SIS_DB_FILE_AOF, dbpath_, db_->name);

    sis_file_handle fp = sis_file_open(aof, SIS_FILE_IO_READ, 0);
	if (!fp)
	{
		return false;
	}
    sis_file_seek(fp, 0 ,SEEK_SET);

    bool hashead =false;
    s_sis_aof_head head;
    s_sis_memory *buffer = sis_memory_create();    
	while (1)
	{
		size_t bytes = sis_memory_readfile(buffer, fp, SIS_DB_MEMORY_SIZE);
		if (bytes <= 0) break;
		while (sis_memory_get_size(buffer) > sizeof(s_sis_aof_head))
		{
            if (!hashead) {
                memmove(&head, sis_memory(buffer),sizeof(s_sis_aof_head));
                sis_memory_move(buffer, sizeof(s_sis_aof_head));
                hashead = true;    
            }
			if (sis_memory_get_size(buffer) < head.size) 
            {
                break;
            }
            // 不拷贝内存，只是移动指针，但移动后求出的sis_memory_get_size需要减少
            sisdb_set_format(head.format, head.table, head.code, sis_memory(buffer), head.size);
            sis_memory_move(buffer, head.size);
            hashead=false;
        }
    }  
    sis_memory_destroy(buffer);
    sis_file_close(fp);
    return true;
    
}
bool _sisdb_file_load_table(s_sis_table *tb_,sis_file_handle fp_)
{
    tb_->loading = true;  // 为true时不做links工作
    bool hashead =false;
    s_sis_sdb_head head;
    s_sis_memory *buffer = sis_memory_create();    
	while (1)
	{
		size_t bytes = sis_memory_readfile(buffer, fp_, SIS_DB_MEMORY_SIZE);
		if (bytes <= 0) break;
		while (sis_memory_get_size(buffer) > sizeof(s_sis_sdb_head))
		{
            if (!hashead) {
                memmove(&head, sis_memory(buffer),sizeof(s_sis_sdb_head));
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
            // sisdb_set_format(head.format, tb_->name, head.code, sis_memory(buffer), head.size);
            sis_table_update_load(head.format, tb_, head.code, sis_memory(buffer), head.size);
            sis_memory_move(buffer, head.size);
            // sis_memory_pack(buffer);
            hashead=false;
            // sis_sleep(1000*100);
        }
    }  
    sis_memory_destroy(buffer);
    tb_->loading = false;
    // 释放没有移动前的指针
    return true;
}
bool sisdb_file_load(const char *dbpath_, s_sis_db *db_)
{
    // 遍历加载所有数据表
    char sdb[SIS_PATH_LEN];

 	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->db);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_table *val = (s_sis_table *)sis_dict_getval(de);
        sis_sprintf(sdb,SIS_PATH_LEN, SIS_DB_FILE_MAIN, dbpath_, db_->name, val->name);
        sis_file_handle fp = sis_file_open(sdb, SIS_FILE_IO_READ, 0);
	    if (fp)
	    {
	        sis_file_seek(fp, 0, SEEK_SET);
            _sisdb_file_load_table(val, fp);
            sis_file_close(fp);
	    }
	}
	sis_dict_iter_free(di);   
    // 加载完毕再加载aof文件
    // 如果有aof文件表示非正常退出，需要执行存盘操作，
    if (sisdb_file_load_aof(dbpath_,db_)) {
        sisdb_file_save(dbpath_, db_);
    }
    return true;
}
