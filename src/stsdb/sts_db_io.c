

#include "sts_db.h"

int stsdb_get(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
{
    sts_module_memory_init(ctx_);

    const char *dbpath = sts_conf_get_str(server.config->node, "dbpath");
    printf("%s/%s/stock/%s.%s\n", server.conf_path, dbpath, db_, key_);
    char *fn = sts_str_sprintf(128, "%s/%s/stock/%s.%s.json",
                               server.conf_path,
                               dbpath,
                               key_,
                               db_
                               );
    size_t size = 0;
    char *buffer = sts_file_open_and_read(fn, &size);
    if (size == 0 || !buffer)
    {
        sts_free(fn);
        return sts_module_reply_with_error(ctx_, "no file!");
        // return STS_MODULE_ERROR;
    }
    sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, buffer, size));
    sts_free(buffer);
    sts_free(fn);
    return STS_MODULE_OK;
}

int stsdb_set_json(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_)
{
    printf("%s.%s\n", db_, key_);
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}

int stsdb_set_struct(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_)
{
    printf("%s.%s\n", db_, key_);
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}