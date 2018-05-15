

#include "stsserver.h"

struct s_stock_time_series_field {
	uint16 type;  //数据类型 


	dict *fields;//key为字段名，value为字段结构
	dict *keys; //key为股票名或市场名，value为结构化数据
	struct HelloTypeNode *next;
};
struct s_stock_time_series_db {
	dict *fields;//key为字段名，value为字段结构
	dict *keys; //key为股票名或市场名，value为结构化数据
	struct HelloTypeNode *next;
};

struct s_server_object {
	struct RedisModuleType *standard_type;  // 定义标准数据类型
	dict *table;  //按名称对应的表,key为表名，val为s_stock_time_series_db
};

static struct s_server_object server;  //所有变量放server中


/* ========================== Internal data structure  =======================
* This is just a linked list of 64 bit integers where elements are inserted
* in-place, so it's ordered. There is no pop/push operation but just insert
* because it is enough to show the implementation of new data types without
* making things complex. */

struct HelloTypeNode {
	int64_t value;
	struct HelloTypeNode *next;
};

struct HelloTypeObject {
	struct HelloTypeNode *head;
	size_t len; /* Number of elements added. */
};

struct HelloTypeObject *createHelloTypeObject(void) {
	struct HelloTypeObject *o;
	o = RedisModule_Alloc(sizeof(*o));
	o->head = NULL;
	o->len = 0;
	return o;
}

void HelloTypeInsert(struct HelloTypeObject *o, int64_t ele) {
	struct HelloTypeNode *next = o->head, *newnode, *prev = NULL;

	while (next && next->value < ele) {
		prev = next;
		next = next->next;
	}
	newnode = RedisModule_Alloc(sizeof(*newnode));
	newnode->value = ele;
	newnode->next = next;
	if (prev) {
		prev->next = newnode;
	}
	else {
		o->head = newnode;
	}
	o->len++;
}

void HelloTypeReleaseObject(struct HelloTypeObject *o) {
	struct HelloTypeNode *cur, *next;
	cur = o->head;
	while (cur) {
		next = cur->next;
		RedisModule_Free(cur);
		cur = next;
	}
	RedisModule_Free(o);
}

/* ========================= "hellotype" type commands ======================= */

/* HELLOTYPE.INSERT key value */
int HelloTypeInsert_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

	if (argc != 3) return RedisModule_WrongArity(ctx);
	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	int type = RedisModule_KeyType(key);
	if (type != REDISMODULE_KEYTYPE_EMPTY &&
		RedisModule_ModuleTypeGetType(key) != HelloType)
	{
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	long long value;
	if ((RedisModule_StringToLongLong(argv[2], &value) != STS_MODULE_OK)) {
		return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be a signed 64 bit integer");
	}

	/* Create an empty value object if the key is currently empty. */
	struct HelloTypeObject *hto;
	if (type == REDISMODULE_KEYTYPE_EMPTY) {
		hto = createHelloTypeObject();
		RedisModule_ModuleTypeSetValue(key, HelloType, hto);
	}
	else {
		hto = RedisModule_ModuleTypeGetValue(key);
	}

	/* Insert the new element. */
	HelloTypeInsert(hto, value);

	_module_reply_with_int64(ctx, hto->len);
	RedisModule_ReplicateVerbatim(ctx);
	return STS_MODULE_OK;
}

/* HELLOTYPE.RANGE key first count */
int HelloTypeRange_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

	if (argc != 4) return RedisModule_WrongArity(ctx);
	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	int type = RedisModule_KeyType(key);
	if (type != REDISMODULE_KEYTYPE_EMPTY &&
		RedisModule_ModuleTypeGetType(key) != HelloType)
	{
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	long long first, count;
	if (RedisModule_StringToLongLong(argv[2], &first) != STS_MODULE_OK ||
		RedisModule_StringToLongLong(argv[3], &count) != STS_MODULE_OK ||
		first < 0 || count < 0)
	{
		return RedisModule_ReplyWithError(ctx,
			"ERR invalid first or count parameters");
	}

	struct HelloTypeObject *hto = RedisModule_ModuleTypeGetValue(key);
	struct HelloTypeNode *node = hto ? hto->head : NULL;
	RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
	long long arraylen = 0;
	while (node && count--) {
		_module_reply_with_int64(ctx, node->value);
		arraylen++;
		node = node->next;
	}
	RedisModule_ReplySetArrayLength(ctx, arraylen);
	return STS_MODULE_OK;
}

/* HELLOTYPE.LEN key */
int HelloTypeLen_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

	if (argc != 2) return RedisModule_WrongArity(ctx);
	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	int type = RedisModule_KeyType(key);
	if (type != REDISMODULE_KEYTYPE_EMPTY &&
		RedisModule_ModuleTypeGetType(key) != HelloType)
	{
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	struct HelloTypeObject *hto = RedisModule_ModuleTypeGetValue(key);
	_module_reply_with_int64(ctx, hto ? hto->len : 0);
	return STS_MODULE_OK;
}


/* ========================== "hellotype" type methods ======================= */

void *HelloTypeRdbLoad(RedisModuleIO *rdb, int encver) {
	if (encver != 0) {
		/* RedisModule_Log("warning","Can't load data with version %d", encver);*/
		return NULL;
	}
	uint64_t elements = RedisModule_LoadUnsigned(rdb);
	struct HelloTypeObject *hto = createHelloTypeObject();
	while (elements--) {
		int64_t ele = RedisModule_LoadSigned(rdb);
		HelloTypeInsert(hto, ele);
	}
	return hto;
}

void HelloTypeRdbSave(RedisModuleIO *rdb, void *value) {
	struct HelloTypeObject *hto = value;
	struct HelloTypeNode *node = hto->head;
	RedisModule_SaveUnsigned(rdb, hto->len);
	while (node) {
		RedisModule_SaveSigned(rdb, node->value);
		node = node->next;
	}
}

void HelloTypeAofRewrite(RedisModuleIO *aof, s_module_string *key, void *value) {
	struct HelloTypeObject *hto = value;
	struct HelloTypeNode *node = hto->head;
	while (node) {
		RedisModule_EmitAOF(aof, "HELLOTYPE.INSERT", "sl", key, node->value);
		node = node->next;
	}
}

/* The goal of this function is to return the amount of memory used by
* the HelloType value. */
size_t HelloTypeMemUsage(const void *value) {
	const struct HelloTypeObject *hto = value;
	struct HelloTypeNode *node = hto->head;
	return sizeof(*hto) + sizeof(*node)*hto->len;
}

void HelloTypeFree(void *value) {
	HelloTypeReleaseObject(value);
}

void HelloTypeDigest(RedisModuleDigest *md, void *value) {
	struct HelloTypeObject *hto = value;
	struct HelloTypeNode *node = hto->head;
	while (node) {
		RedisModule_DigestAddLongLong(md, node->value);
		node = node->next;
	}
	RedisModule_DigestEndSequence(md);
}

/* This function must be present on each Redis module. It is used in order to
* register the commands into the Redis server. */
int RedisModule_OnLoad(s_module_context *ctx, s_module_string **argv, int argc) {
	_module_not_used(argv);
	_module_not_used(argc);

	if (RedisModule_Init(ctx, "hellotype", 1, REDISMODULE_APIVER_1)
		== STS_MODULE_ERROR) return STS_MODULE_ERROR;

	RedisModuleTypeMethods tm = {
		.version = REDISMODULE_TYPE_METHOD_VERSION,
		.rdb_load = HelloTypeRdbLoad,
		.rdb_save = HelloTypeRdbSave,
		.aof_rewrite = HelloTypeAofRewrite,
		.mem_usage = HelloTypeMemUsage,
		.free = HelloTypeFree,
		.digest = HelloTypeDigest
	};

	HelloType = RedisModule_CreateDataType(ctx, "hellotype", 0, &tm);
	if (HelloType == NULL) return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hellotype.insert",
		HelloTypeInsert_RedisCommand, "write deny-oom", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hellotype.range",
		HelloTypeRange_RedisCommand, "readonly", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hellotype.len",
		HelloTypeLen_RedisCommand, "readonly", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	return STS_MODULE_OK;
}

