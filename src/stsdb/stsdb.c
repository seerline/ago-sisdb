
#include <sts_db.h>
#include <sts_db_io.h>

static s_sts_module_type *__stsdb_type;

int call_stsdb_get(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ < 2)
	{
		return sts_module_wrong_arity(ctx_);
	}

	int o;
	const char *key = sts_module_string_get(argv_[1], NULL);
	char db[32];
	char code[16];
	sts_str_substr(db, 32, key, '.', 1);
	sts_str_substr(code, 16, key, '.', 0);

	if (argc_ == 3)
	{
		o = stsdb_get(ctx_, db, code, sts_module_string_get(argv_[2], NULL));
	}
	else
	{
		o = stsdb_get(ctx_, db, code, "{\"format\":\"json\"}");
	}
	return o;
}
int call_stsdb_set(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 4)
	{
		return sts_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sts_module_string_get(argv_[1], NULL), sts_module_string_get(argv_[2], NULL));
	// sts_module_reply_with_simple_string(ctx_, "OK");
	char * dt = sts_module_string_get(argv_[2],NULL);
 	if (sts_str_subcmp(dt ,STS_DB_DATETYPE_STR,',') < 0){
		return sts_module_reply_with_error(ctx_, "set data type error.\n", dt);
	}
	int o;
	const char *key = sts_module_string_get(argv_[1], NULL);
	char db[32];
	char code[16];
	sts_str_substr(db, 32, key, '.', 1);
	sts_str_substr(code, 16, key, '.', 0);

	if (sts_strcasecmp(dt,"struct")) {
		o = stsdb_set_struct(ctx_, db, code, sts_module_string_get(argv_[3], NULL));
	} else {
		o = stsdb_set_json(ctx_, db, code, sts_module_string_get(argv_[3], NULL));
	}

	return o;
}


//////////////////////////////////////////////////////////////
//  type define begin.
/////////////////////////////////////////////////////////////
void *sts_type_rdb_load(s_sts_module_io *rdb, int ver_) {
    printf("load data start\n");
    if (ver_ != 0) {
        return NULL;
    }
	struct sts_db *db = sts_db_create();
	sts_db_load(db);
    return db;
}

void sts_type_rdb_save(s_sts_module_io *rdb_, void *value_) {
	sts_db_save((struct sts_db *)value_);
}

void sts_type_aof_write(s_sts_module_io *aof, s_sts_module_string *key, void *value) {
//
}

size_t sts_type_mem_usage(const void *value) {
    return 0;
}

void sts_type_free(void *value_) {
    sts_db_destroy((struct sts_db *)value_);
}

void sts_type_digest(s_sts_module_digest *md, void *value) {
//
}
//////////////////////////////////////////////////////////////
//    type define end.
/////////////////////////////////////////////////////////////
int sts_module_on_load(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (sts_module_init(ctx_, "stsdb", 1, STS_MODULE_VER) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	for (int k = 0; k < argc_; k++)
	{
		const char *s = sts_module_string_get(argv_[k], NULL);
		printf("module loaded with argv_[%d] = %s\n", k, s);
	}
	int o;
	if (argc_ == 1)
	{
		o = call_digger_init(sts_module_string_get(argv_[0], NULL));
	}
	else
	{
		o = call_digger_init("../conf/stsdb.conf");
	}
	if (o != STS_MODULE_OK)
	{
		return STS_MODULE_ERROR;
	}

	s_sts_module_type_methods tm = {
		.version = STS_MODULE_TYPE_METHOD_VERSION,
		.rdb_load = sts_type_rdb_load,
		.rdb_save = sts_type_rdb_save,
		.aof_rewrite = sts_type_aof_write,
		.mem_usage = sts_type_mem_usage,
		.free = sts_type_free,
		.digest = sts_type_digest
	};

	__stsdb_type = sts_module_type_create(ctx, "stsdb", 0, &tm);
	if (__stsdb_type == NULL) return STS_MODULE_ERROR;


	if (sts_module_create_command(ctx_, "stsdb.get", call_stsdb_get,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "stsdb.set", call_stsdb_set,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}

	return STS_MODULE_OK;
}

