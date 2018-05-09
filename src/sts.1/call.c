
#include <call.h>

int sts_db_create(s_sts_module_context *ctx_, const char *path_)
{
    char dname[255];
    char rname[255];
    if (path_ == NULL)
    {
        sprintf(dname, "day.sdb");
        sprintf(rname, "right.sdb");
    }
    else
    {
        sprintf(dname, "%s/day.sdb", path_);
        sprintf(rname, "%s/right.sdb", path_);
    }
    sts_module_memory_init(ctx_);
    int count = 0;
    count += sts_db_load_from_file(ctx_, "day", dname, sizeof(s_stock_day));
    count += sts_db_load_from_file(ctx_, "right", rname, sizeof(s_stock_right));
    return count;
}
int sts_db_load_from_file(s_sts_module_context *ctx_, const char *dbname_, const char *filename_, size_t slen_)
{

    FILE *fp = fopen(filename_, "rb");
    if (!fp)
        return 0;

    fseek(fp, 0, SEEK_END);
    size_t filelen = ftell(fp);
    if (filelen == 0)
    {
        fclose(fp);
        return 0;
    }

    fseek(fp, 0, SEEK_SET);

    s_sts_module_key *key;
    s_sts_module_string *code, *value;
    int count;
    char str[10];
    char *ptr;
    size_t l1, l2, l3, pos = 0, len;

    while (1)
    {
        l1 = 0, l2 = 0, l3 = 0;
        l1 = fread(str, 1, 8, fp);
        str[8] = 0;

        l2 = fread(&count, sizeof(int), 1, fp) * sizeof(int);
        len = count * slen_;
        ptr = (char *)malloc(len + 1);
        l3 = fread(ptr, 1, len, fp);
        pos = pos + l1 + l2 + l3;
        printf("[%s.%s],%ld : %d [%ld,%ld][%ld,%ld,%ld]\n", dbname_, str, slen_, count, filelen, pos, l1, l2, l3);
        code = sts_module_string_create_printf(ctx_, "%s.%s", dbname_, str);
        // printf("%p---%p\n",code);
        key = sts_module_key_open(ctx_, code, STS_MODULE_READ | STS_MODULE_WRITE);
        value = sts_module_string_create(ctx_, ptr, len);
        sts_module_key_set_value(key, value);
        sts_module_key_close(key);

        free(ptr);
        // break;

        if (pos >= filelen)
            break;
    }
    fclose(fp);
    return count;
}

int sts_db_get(s_sts_module_context *ctx_, const char *dbname_, const char *key_, const char *command)
{
    sts_module_memory_init(ctx_);
    printf("%s.%s\n", dbname_, key_);
    s_sts_module_string *code = sts_module_string_create_printf(ctx_, "%s.%s", dbname_, key_);

    s_sts_module_key *key = sts_module_key_open(ctx_, code, STS_MODULE_READ);
    if (!key)
        return sts_module_reply_with_null(ctx_);
    int keytype = sts_module_key_type(key);
    if (keytype != STS_MODULE_KEYTYPE_STRING &&
        keytype != STS_MODULE_KEYTYPE_EMPTY)
    {
        sts_module_key_close(key);
        return sts_module_reply_with_error(ctx_, STS_MODULE_ERROR_WRONGTYPE);
    }
    size_t len;
    char *value = sts_module_key_get_value(key, &len, STS_MODULE_READ);
    sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, value, len));
    sts_module_key_close(key);
    return 0;
}
