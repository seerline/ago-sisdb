
#include <call.h>

/********************************/
// 一定要用static定义，不然内存混乱
static s_digger_server server = {
    .inited = false,
    .config = NULL,
    .id = 0};
/********************************/
 
int digger_create(const char *conf_)
{
    // 加载可包含的配置文件，方便后面使用
    server.inited = true;
    server.id = 0; // 这里设置当前秒数

    sts_strcpy(server.conf_name, STS_PATH_LEN, conf_);
    sts_file_getpath(server.conf_name, server.conf_path, STS_PATH_LEN);

    server.config = sts_conf_open(conf_);
    if (!server.config)
    {
        printf("load conf %s fail.\n", server.conf_name);
        return STS_MODULE_ERROR;
    }

    return STS_MODULE_OK;
}

int digger_start(s_sts_module_context *ctx_, const char *name_, const char *com_)
{
    sts_module_memory_init(ctx_);

    server.id++;
    // s_sts_module_string *key = sts_module_string_create_printf(ctx_, "%s%06d", name_, server.id);
    s_sts_module_string *key = sts_module_string_create(ctx_, "xx000001", 8);

    sts_module_reply_with_string(ctx_, key);

    return STS_MODULE_OK;
}
int digger_cancel(s_sts_module_context *ctx_, const char *key_)
{
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}
int digger_stop(s_sts_module_context *ctx_, const char *key_)
{
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}

int digger_get(s_sts_module_context *ctx_, const char *key_, const char *com_)
{
    sts_module_memory_init(ctx_);

    const char *dbpath = sts_conf_get_str(server.config->node, "dbpath");
    printf("%s/%s/%s\n", server.conf_path, dbpath, key_);
    char *fn = sts_str_sprintf(128, "%s/%s/%s.json",
                               server.conf_path,
                               dbpath,
                               sts_str_replace(key_,'.','/'));
	s_sts_sds buffer = sts_file_read_to_sds(fn);
	if (!buffer)
    {
        sts_free(fn);
        sts_module_reply_with_error(ctx_, "no file!");
        return STS_MODULE_ERROR;
    }
    sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, buffer, sts_sdslen(buffer)));
    sts_sdsfree(buffer);
    sts_free(fn);
    return STS_MODULE_OK;
}
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
	s_sts_sds buffer = sts_file_read_to_sds(fn);
	if (!buffer)
    {
        sts_free(fn);
        sts_module_reply_with_error(ctx_, "no file!");
        return STS_MODULE_ERROR;
    }
    sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, buffer, sts_sdslen(buffer)));
    sts_sdsfree(buffer);
    sts_free(fn);
    return STS_MODULE_OK;
}
int digger_set(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
{
    printf("%s.%s\n", db_, key_);
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}
/////////////////
int _digger_block_sub_reply(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
    sts_module_not_used(argv_);
    sts_module_not_used(argc_);
    s_sts_module_string *s = sts_module_block_getdata(ctx_);
    sts_module_reply_with_string(ctx_, s);
    // sts_module_string_free(ctx_, s);
    return STS_MODULE_OK;
}

/* Timeout callback for blocking command HELLO.BLOCK */
int _digger_block_sub_timeout(s_sts_module_context *ctx, s_sts_module_string **argv, int argc)
{
    sts_module_not_used(argv);
    sts_module_not_used(argc);
    return sts_module_reply_with_simple_string(ctx, "sub timedout\n");
}

/* Private data freeing callback for HELLO.BLOCK command. */
void _digger_block_sub_free(void *buffer_)
{
    sts_module_free(buffer_);
}

/* The thread entry point that actually executes the blocking part
 * of the command HELLO.BLOCK. */
void *_digger_block_sub_thread(void *argv_)
{
    void **argv = argv_;
    s_sts_module_block_client *bc = argv[0];
    s_sts_module_string *code = argv[1];
    // sts_module_free(argv);

    s_sts_module_context *ctx = sts_module_get_safe_context(bc);

    while (1)
    {
        s_sts_module_key *key = sts_module_key_open(ctx, code,
                                                    STS_MODULE_READ | STS_MODULE_WRITE);
        s_sts_module_string *e = sts_module_list_pop(key, STS_MODULE_LIST_HEAD);
        if (e)
        {
            // sts_module_reply_with_string(ctx, e);
            // sts_module_string_free(ctx, e);
            sts_module_key_close(key);
            sts_module_free_safe_context(ctx);
            sts_module_free(argv);
            sts_module_unblock_client(bc, e);
            break;
        }
        else
        {
            sts_module_key_close(key);
            usleep(1000*1000);
        }
    }

    return NULL;
}

int digger_sub(s_sts_module_context *ctx_, const char *db_, const char *key_)
{
    pthread_t pid;
    s_sts_module_block_client *bc = sts_module_block_client(
        ctx_,
        _digger_block_sub_reply,
        _digger_block_sub_timeout,
        _digger_block_sub_free,
        0);
    void **argv = RedisModule_Alloc(sizeof(void *) * 2);
    argv[0] = bc;
    argv[1] = sts_module_string_create_printf(ctx_, "%s.%s", db_, key_);

    if (pthread_create(&pid, NULL, _digger_block_sub_thread, argv) != 0)
    {
        sts_module_block_abort(bc);
        return sts_module_reply_with_error(ctx_, "can't start thread\n");
    }
    return 0;
}
// int digger_sub(s_sts_module_context *ctx_, const char *db_, const char *key_)
// {
//     printf("%s.%s\n", db_, key_);

//     s_sts_module_call_reply *reply;
//     reply = sts_module_call(ctx_, "BLPOP %s.%s %d", db_, key_, 0);
//     if(!reply) {
//         // 似乎不会阻塞在这里
//         sts_module_reply_with_simple_string(ctx_, "ERROR");
//         return STS_MODULE_ERROR;
//     }
//     size_t len = 0;
//     size_t items = sts_module_call_reply_len(reply);
//     for (size_t k = 0; k < items; k++) {
//         s_sts_module_call_reply *e = sts_module_call_reply_array(reply,k);
//         len += sts_module_call_reply_len(e);
//     }
//     sts_module_call_reply_free(reply);
//     sts_module_reply_with_int64(ctx_,len);

//     return STS_MODULE_OK;
// }
// int digger_pub(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
// {
//     printf("%s.%s\n", db_, key_);

//     char *key = sts_str_sprintf(128, "%s.%s", db_, key_);
//     s_sts_module_call_reply *reply;
//     reply = sts_module_call(ctx_,"RPUSH",key);
//     sts_free(key);

//     long long len = sts_module_call_reply_int64(reply);
//     sts_module_call_reply_free(reply);
//     sts_module_reply_with_int64(ctx_,len);

//     return STS_MODULE_OK;
// }
// int digger_sub(s_sts_module_context *ctx_, const char *db_, const char *key_)
// {
//     printf("%s.%s\n", db_, key_);

//     s_sts_module_string *code = sts_module_string_create_printf(ctx_, "%s.%s", db_, key_);
//     s_sts_module_key *key = sts_module_key_open(ctx_, code,
//                                                 STS_MODULE_READ | STS_MODULE_WRITE);

//     s_sts_module_string *e = sts_module_list_pop(key, STS_MODULE_LIST_HEAD);
//     if (e)
//     {
//         sts_module_reply_with_string(ctx_, e);
//         sts_module_string_free(ctx_, e);
//     }
//     else
//     {
//         sts_module_reply_with_null(ctx_);
//     }
//     sts_module_key_close(key);
//     return STS_MODULE_OK;
// }
int digger_pub(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
{
    printf("%s.%s\n", db_, key_);

    s_sts_module_string *code = sts_module_string_create_printf(ctx_, "%s.%s", db_, key_);
    s_sts_module_key *key = sts_module_key_open(ctx_, code,
                                                STS_MODULE_READ | STS_MODULE_WRITE);

    s_sts_module_string *info = sts_module_string_create(ctx_, com_, strlen(com_));
    sts_module_list_push(key, STS_MODULE_LIST_TAIL, info);
    size_t newlen = sts_module_value_len(key);
    sts_module_key_close(key);
    sts_module_reply_with_int64(ctx_, newlen);

    return STS_MODULE_OK;
}
