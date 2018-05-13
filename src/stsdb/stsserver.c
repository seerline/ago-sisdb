

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

/* HELLO.SIMPLE is among the simplest commands you can implement.
* It just returns the currently selected DB id, a functionality which is
* missing in Redis. The command uses two important API calls: one to
* fetch the currently selected DB, the other in order to send the client
* an integer reply as response. */
int HelloSimple_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	_module_not_used(argv);
	_module_not_used(argc);
	//_module_reply_with_int64(ctx, RedisModule_GetSelectedDb(ctx));
	_module_reply_with_int64(ctx, getding());
	return STS_MODULE_OK;
}

/* HELLO.PUSH.NATIVE re-implements RPUSH, and shows the low level modules API
* where you can "open" keys, make low level operations, create new keys by
* pushing elements into non-existing keys, and so forth.
*
* You'll find this command to be roughly as fast as the actual RPUSH
* command. */
int HelloPushNative_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc)
{
	if (argc != 3) return RedisModule_WrongArity(ctx);

	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);

	RedisModule_ListPush(key, REDISMODULE_LIST_TAIL, argv[2]);
	size_t newlen = RedisModule_ValueLength(key);
	RedisModule_CloseKey(key);
	_module_reply_with_int64(ctx, newlen);
	return STS_MODULE_OK;
}

/* HELLO.PUSH.CALL implements RPUSH using an higher level approach, calling
* a Redis command instead of working with the key in a low level way. This
* approach is useful when you need to call Redis commands that are not
* available as low level APIs, or when you don't need the maximum speed
* possible but instead prefer implementation simplicity. */
int HelloPushCall_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc)
{
	if (argc != 3) return RedisModule_WrongArity(ctx);

	RedisModuleCallReply *reply;

	reply = RedisModule_Call(ctx, "RPUSH", "ss", argv[1], argv[2]);
	long long len = RedisModule_CallReplyInteger(reply);
	RedisModule_FreeCallReply(reply);
	_module_reply_with_int64(ctx, len);
	return STS_MODULE_OK;
}

/* HELLO.PUSH.CALL2
* This is exaxctly as HELLO.PUSH.CALL, but shows how we can reply to the
* client using directly a reply object that Call() returned. */
int HelloPushCall2_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc)
{
	if (argc != 3) return RedisModule_WrongArity(ctx);

	RedisModuleCallReply *reply;

	reply = RedisModule_Call(ctx, "RPUSH", "ss", argv[1], argv[2]);
	RedisModule_ReplyWithCallReply(ctx, reply);
	RedisModule_FreeCallReply(reply);
	return STS_MODULE_OK;
}

/* HELLO.LIST.SUM.LEN returns the total length of all the items inside
* a Redis list, by using the high level Call() API.
* This command is an example of the array reply access. */
int HelloListSumLen_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc)
{
	if (argc != 2) return RedisModule_WrongArity(ctx);

	RedisModuleCallReply *reply;

	reply = RedisModule_Call(ctx, "LRANGE", "sll", argv[1], (long long)0, (long long)-1);
	size_t strlen = 0;
	size_t items = RedisModule_CallReplyLength(reply);
	size_t j;
	for (j = 0; j < items; j++) {
		RedisModuleCallReply *ele = RedisModule_CallReplyArrayElement(reply, j);
		strlen += RedisModule_CallReplyLength(ele);
	}
	RedisModule_FreeCallReply(reply);
	_module_reply_with_int64(ctx, strlen);
	return STS_MODULE_OK;
}

/* HELLO.LIST.SPLICE srclist dstlist count
* Moves 'count' elements from the tail of 'srclist' to the head of
* 'dstlist'. If less than count elements are available, it moves as much
* elements as possible. */
int HelloListSplice_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	if (argc != 4) return RedisModule_WrongArity(ctx);

	RedisModuleKey *srckey = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	RedisModuleKey *dstkey = RedisModule_OpenKey(ctx, argv[2],
		REDISMODULE_READ | REDISMODULE_WRITE);

	/* Src and dst key must be empty or lists. */
	if ((RedisModule_KeyType(srckey) != REDISMODULE_KEYTYPE_LIST &&
		RedisModule_KeyType(srckey) != REDISMODULE_KEYTYPE_EMPTY) ||
		(RedisModule_KeyType(dstkey) != REDISMODULE_KEYTYPE_LIST &&
		RedisModule_KeyType(dstkey) != REDISMODULE_KEYTYPE_EMPTY))
	{
		RedisModule_CloseKey(srckey);
		RedisModule_CloseKey(dstkey);
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	long long count;
	if ((RedisModule_StringToLongLong(argv[3], &count) != STS_MODULE_OK) ||
		(count < 0)) {
		RedisModule_CloseKey(srckey);
		RedisModule_CloseKey(dstkey);
		return RedisModule_ReplyWithError(ctx, "ERR invalid count");
	}

	while (count-- > 0) {
		s_module_string *ele;

		ele = RedisModule_ListPop(srckey, REDISMODULE_LIST_TAIL);
		if (ele == NULL) break;
		RedisModule_ListPush(dstkey, REDISMODULE_LIST_HEAD, ele);
		RedisModule_FreeString(ctx, ele);
	}

	size_t len = RedisModule_ValueLength(srckey);
	RedisModule_CloseKey(srckey);
	RedisModule_CloseKey(dstkey);
	_module_reply_with_int64(ctx, len);
	return STS_MODULE_OK;
}

/* Like the HELLO.LIST.SPLICE above, but uses automatic memory management
* in order to avoid freeing stuff. */
int HelloListSpliceAuto_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	if (argc != 4) return RedisModule_WrongArity(ctx);

	RedisModule_AutoMemory(ctx);

	RedisModuleKey *srckey = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	RedisModuleKey *dstkey = RedisModule_OpenKey(ctx, argv[2],
		REDISMODULE_READ | REDISMODULE_WRITE);

	/* Src and dst key must be empty or lists. */
	if ((RedisModule_KeyType(srckey) != REDISMODULE_KEYTYPE_LIST &&
		RedisModule_KeyType(srckey) != REDISMODULE_KEYTYPE_EMPTY) ||
		(RedisModule_KeyType(dstkey) != REDISMODULE_KEYTYPE_LIST &&
		RedisModule_KeyType(dstkey) != REDISMODULE_KEYTYPE_EMPTY))
	{
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	long long count;
	if ((RedisModule_StringToLongLong(argv[3], &count) != STS_MODULE_OK) ||
		(count < 0))
	{
		return RedisModule_ReplyWithError(ctx, "ERR invalid count");
	}

	while (count-- > 0) {
		s_module_string *ele;

		ele = RedisModule_ListPop(srckey, REDISMODULE_LIST_TAIL);
		if (ele == NULL) break;
		RedisModule_ListPush(dstkey, REDISMODULE_LIST_HEAD, ele);
	}

	size_t len = RedisModule_ValueLength(srckey);
	_module_reply_with_int64(ctx, len);
	return STS_MODULE_OK;
}

/* HELLO.RAND.ARRAY <count>
* Shows how to generate arrays as commands replies.
* It just outputs <count> random numbers. */
int HelloRandArray_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	if (argc != 2) return RedisModule_WrongArity(ctx);
	long long count;
	if (RedisModule_StringToLongLong(argv[1], &count) != STS_MODULE_OK ||
		count < 0)
		return RedisModule_ReplyWithError(ctx, "ERR invalid count");

	/* To reply with an array, we call RedisModule_ReplyWithArray() followed
	* by other "count" calls to other reply functions in order to generate
	* the elements of the array. */
	RedisModule_ReplyWithArray(ctx, count);
	while (count--) _module_reply_with_int64(ctx, rand());
	return STS_MODULE_OK;
}

/* This is a simple command to test replication. Because of the "!" modified
* in the RedisModule_Call() call, the two INCRs get replicated.
* Also note how the ECHO is replicated in an unexpected position (check
* comments the function implementation). */
int HelloRepl1_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc)
{
	_module_not_used(argv);
	_module_not_used(argc);
	RedisModule_AutoMemory(ctx);

	/* This will be replicated *after* the two INCR statements, since
	* the Call() replication has precedence, so the actual replication
	* stream will be:
	*
	* MULTI
	* INCR foo
	* INCR bar
	* ECHO c foo
	* EXEC
	*/
	RedisModule_Replicate(ctx, "ECHO", "c", "foo");

	/* Using the "!" modifier we replicate the command if it
	* modified the dataset in some way. */
	RedisModule_Call(ctx, "INCR", "c!", "foo");
	RedisModule_Call(ctx, "INCR", "c!", "bar");

	_module_reply_with_int64(ctx, 0);

	return STS_MODULE_OK;
}

/* Another command to show replication. In this case, we call
* RedisModule_ReplicateVerbatim() to mean we want just the command to be
* propagated to slaves / AOF exactly as it was called by the user.
*
* This command also shows how to work with string objects.
* It takes a list, and increments all the elements (that must have
* a numerical value) by 1, returning the sum of all the elements
* as reply.
*
* Usage: HELLO.REPL2 <list-key> */
int HelloRepl2_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	if (argc != 2) return RedisModule_WrongArity(ctx);

	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */
	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);

	if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_LIST)
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

	size_t listlen = RedisModule_ValueLength(key);
	long long sum = 0;

	/* Rotate and increment. */
	while (listlen--) {
		s_module_string *ele = RedisModule_ListPop(key, REDISMODULE_LIST_TAIL);
		long long val;
		if (RedisModule_StringToLongLong(ele, &val) != STS_MODULE_OK) val = 0;
		val++;
		sum += val;
		s_module_string *newele = RedisModule_CreateStringFromLongLong(ctx, val);
		RedisModule_ListPush(key, REDISMODULE_LIST_HEAD, newele);
	}
	_module_reply_with_int64(ctx, sum);
	RedisModule_ReplicateVerbatim(ctx);
	return STS_MODULE_OK;
}

/* This is an example of strings DMA access. Given a key containing a string
* it toggles the case of each character from lower to upper case or the
* other way around.
*
* No automatic memory management is used in this example (for the sake
* of variety).
*
* HELLO.TOGGLE.CASE key */
int HelloToggleCase_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	if (argc != 2) return RedisModule_WrongArity(ctx);

	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);

	int keytype = RedisModule_KeyType(key);
	if (keytype != REDISMODULE_KEYTYPE_STRING &&
		keytype != REDISMODULE_KEYTYPE_EMPTY)
	{
		RedisModule_CloseKey(key);
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	if (keytype == REDISMODULE_KEYTYPE_STRING) {
		size_t len, j;
		char *s = RedisModule_StringDMA(key, &len, REDISMODULE_WRITE);
		for (j = 0; j < len; j++) {
			if (isupper(s[j])) {
				s[j] = tolower(s[j]);
			}
			else {
				s[j] = toupper(s[j]);
			}
		}
	}

	RedisModule_CloseKey(key);
	RedisModule_ReplyWithSimpleString(ctx, "OK");
	RedisModule_ReplicateVerbatim(ctx);
	return STS_MODULE_OK;
}

/* HELLO.MORE.EXPIRE key milliseconds.
*
* If they key has already an associated TTL, extends it by "milliseconds"
* milliseconds. Otherwise no operation is performed. */
int HelloMoreExpire_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */
	if (argc != 3) return RedisModule_WrongArity(ctx);

	mstime_t addms, expire;

	if (RedisModule_StringToLongLong(argv[2], &addms) != STS_MODULE_OK)
		return RedisModule_ReplyWithError(ctx, "ERR invalid expire time");

	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	expire = RedisModule_GetExpire(key);
	if (expire != REDISMODULE_NO_EXPIRE) {
		expire += addms;
		RedisModule_SetExpire(key, expire);
	}
	return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/* HELLO.ZSUMRANGE key startscore endscore
* Return the sum of all the scores elements between startscore and endscore.
*
* The computation is performed two times, one time from start to end and
* another time backward. The two scores, returned as a two element array,
* should match.*/
int HelloZsumRange_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	double score_start, score_end;
	if (argc != 4) return RedisModule_WrongArity(ctx);

	if (RedisModule_StringToDouble(argv[2], &score_start) != STS_MODULE_OK ||
		RedisModule_StringToDouble(argv[3], &score_end) != STS_MODULE_OK)
	{
		return RedisModule_ReplyWithError(ctx, "ERR invalid range");
	}

	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET) {
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	double scoresum_a = 0;
	double scoresum_b = 0;

	RedisModule_ZsetFirstInScoreRange(key, score_start, score_end, 0, 0);
	while (!RedisModule_ZsetRangeEndReached(key)) {
		double score;
		s_module_string *ele = RedisModule_ZsetRangeCurrentElement(key, &score);
		RedisModule_FreeString(ctx, ele);
		scoresum_a += score;
		RedisModule_ZsetRangeNext(key);
	}
	RedisModule_ZsetRangeStop(key);

	RedisModule_ZsetLastInScoreRange(key, score_start, score_end, 0, 0);
	while (!RedisModule_ZsetRangeEndReached(key)) {
		double score;
		s_module_string *ele = RedisModule_ZsetRangeCurrentElement(key, &score);
		RedisModule_FreeString(ctx, ele);
		scoresum_b += score;
		RedisModule_ZsetRangePrev(key);
	}

	RedisModule_ZsetRangeStop(key);

	RedisModule_CloseKey(key);

	RedisModule_ReplyWithArray(ctx, 2);
	RedisModule_ReplyWithDouble(ctx, scoresum_a);
	RedisModule_ReplyWithDouble(ctx, scoresum_b);
	return STS_MODULE_OK;
}

/* HELLO.LEXRANGE key min_lex max_lex min_age max_age
* This command expects a sorted set stored at key in the following form:
* - All the elements have score 0.
* - Elements are pairs of "<name>:<age>", for example "Anna:52".
* The command will return all the sorted set items that are lexicographically
* between the specified range (using the same format as ZRANGEBYLEX)
* and having an age between min_age and max_age. */
int HelloLexRange_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

	if (argc != 6) return RedisModule_WrongArity(ctx);

	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET) {
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	if (RedisModule_ZsetFirstInLexRange(key, argv[2], argv[3]) != STS_MODULE_OK) {
		return RedisModule_ReplyWithError(ctx, "invalid range");
	}

	int arraylen = 0;
	RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
	while (!RedisModule_ZsetRangeEndReached(key)) {
		double score;
		s_module_string *ele = RedisModule_ZsetRangeCurrentElement(key, &score);
		RedisModule_ReplyWithString(ctx, ele);
		RedisModule_FreeString(ctx, ele);
		RedisModule_ZsetRangeNext(key);
		arraylen++;
	}
	RedisModule_ZsetRangeStop(key);
	RedisModule_ReplySetArrayLength(ctx, arraylen);
	RedisModule_CloseKey(key);
	return STS_MODULE_OK;
}

/* HELLO.HCOPY key srcfield dstfield
* This is just an example command that sets the hash field dstfield to the
* same value of srcfield. If srcfield does not exist no operation is
* performed.
*
* The command returns 1 if the copy is performed (srcfield exists) otherwise
* 0 is returned. */
int HelloHCopy_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

	if (argc != 4) return RedisModule_WrongArity(ctx);
	RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1],
		REDISMODULE_READ | REDISMODULE_WRITE);
	int type = RedisModule_KeyType(key);
	if (type != REDISMODULE_KEYTYPE_HASH &&
		type != REDISMODULE_KEYTYPE_EMPTY)
	{
		return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
	}

	/* Get the old field value. */
	s_module_string *oldval;
	RedisModule_HashGet(key, REDISMODULE_HASH_NONE, argv[2], &oldval, NULL);
	if (oldval) {
		RedisModule_HashSet(key, REDISMODULE_HASH_NONE, argv[3], oldval, NULL);
	}
	_module_reply_with_int64(ctx, oldval != NULL);
	return STS_MODULE_OK;
}

/* HELLO.LEFTPAD str len ch
* This is an implementation of the infamous LEFTPAD function, that
* was at the center of an issue with the npm modules system in March 2016.
*
* LEFTPAD is a good example of using a Redis Modules API called
* "pool allocator", that was a famous way to allocate memory in yet another
* open source project, the Apache web server.
*
* The concept is very simple: there is memory that is useful to allocate
* only in the context of serving a request, and must be freed anyway when
* the callback implementing the command returns. So in that case the module
* does not need to retain a reference to these allocations, it is just
* required to free the memory before returning. When this is the case the
* module can call RedisModule_PoolAlloc() instead, that works like malloc()
* but will automatically free the memory when the module callback returns.
*
* Note that PoolAlloc() does not necessarily require AutoMemory to be
* active. */
int HelloLeftPad_RedisCommand(s_module_context *ctx, s_module_string **argv, int argc) {
	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */
	long long padlen;

	if (argc != 4) return RedisModule_WrongArity(ctx);

	if ((RedisModule_StringToLongLong(argv[2], &padlen) != STS_MODULE_OK) ||
		(padlen< 0)) {
		return RedisModule_ReplyWithError(ctx, "ERR invalid padding length");
	}
	size_t strlen, chlen;
	const char *str = _module_get_string_and_len(argv[1], &strlen);
	const char *ch = _module_get_string_and_len(argv[3], &chlen);

	/* If the string is already larger than the target len, just return
	* the string itself. */
	if (strlen >= (size_t)padlen)
		return RedisModule_ReplyWithString(ctx, argv[1]);

	/* Padding must be a single character in this simple implementation. */
	if (chlen != 1)
		return RedisModule_ReplyWithError(ctx,
		"ERR padding must be a single char");

	/* Here we use our pool allocator, for our throw-away allocation. */
	padlen -= strlen;
	char *buf = RedisModule_PoolAlloc(ctx, padlen + strlen);
	for (long long j = 0; j < padlen; j++) buf[j] = *ch;
	memcpy(buf + padlen, str, strlen);

	RedisModule_ReplyWithStringBuffer(ctx, buf, padlen + strlen);
	return STS_MODULE_OK;
}

#include <conf.h>

/* This function must be present on each Redis module. It is used in order to
* register the commands into the Redis server. */
int _module_on_load(s_module_context *ctx, s_module_string **argv, int argc) {
	if (_module_init(ctx, "stsdb", 1, STS_MODULE_VER)
		== STS_MODULE_ERROR) return STS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	for (int i = 0; i < argc; i++) {
		const char *s = _module_get_string_and_len(argv[i], NULL);
		printf("Module loaded with ARGV[%d] = %s\n", i, s);
	}

	if (_module_create_command(ctx, "dp.get",`
		HelloSimple_RedisCommand, "readonly", 0, 0, 0) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;
	/*
	if (_module_create_command(ctx, "hello.push.native",
		HelloPushNative_RedisCommand, "write deny-oom", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.push.call",
		HelloPushCall_RedisCommand, "write deny-oom", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.push.call2",
		HelloPushCall2_RedisCommand, "write deny-oom", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.list.sum.len",
		HelloListSumLen_RedisCommand, "readonly", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.list.splice",
		HelloListSplice_RedisCommand, "write deny-oom", 1, 2, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.list.splice.auto",
		HelloListSpliceAuto_RedisCommand,
		"write deny-oom", 1, 2, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.rand.array",
		HelloRandArray_RedisCommand, "readonly", 0, 0, 0) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.repl1",
		HelloRepl1_RedisCommand, "write", 0, 0, 0) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.repl2",
		HelloRepl2_RedisCommand, "write", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.toggle.case",
		HelloToggleCase_RedisCommand, "write", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.more.expire",
		HelloMoreExpire_RedisCommand, "write", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.zsumrange",
		HelloZsumRange_RedisCommand, "readonly", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.lexrange",
		HelloLexRange_RedisCommand, "readonly", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.hcopy",
		HelloHCopy_RedisCommand, "write deny-oom", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	if (_module_create_command(ctx, "hello.leftpad",
		HelloLeftPad_RedisCommand, "", 1, 1, 1) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;
*/
	return STS_MODULE_OK;
}
