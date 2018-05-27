
#include "sts_db_file.h"

bool stsdb_file_check(const char *service_)
{
    return true;
}

bool stsdb_file_save_aof(const char *service_)
{
    char name[STS_FILE_PATH_LEN];
    sts_sprintf(name,sizeof(name),STS_DB_FILE_AOF,service_);
   
    return true;
}

bool stsdb_file_save(const char *service_)
{
    char name[STS_FILE_PATH_LEN];
    sts_sprintf(name,sizeof(name),);
}
bool stsdb_file_load(const char *dbpath_, s_sts_db *db_)
{
  // 先加载sdb，再加载aof
  // 删除 0 后缀文件
}

