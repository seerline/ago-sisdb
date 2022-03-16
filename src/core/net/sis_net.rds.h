
#ifndef _SIS_NET_RDS_H
#define _SIS_NET_RDS_H

#include <sis_net.h>
#include <sis_memory.h>

// redis 协议说明: 
// https://blog.csdn.net/u014608280/article/details/84586042

// 格式说明：
			// $  --  二进制流 长度+缓存区 -- 
						// $10\r\n1234567890\r\n  表示10个字节长的字符串
						// $-1\r\n  表示空字符串
			// +  --  成功字符串 以\r\n为结束
			// -  --  串错误字符 以\r\n为结束
			// :  --  整数
			// ++ --  订阅的键
			// *  --  +数量+数组 可以为任何类型

// 请求格式例子:
			// *3\r\n
			// $3\r\n
			// set\r\n
			// $5\r\n
			// key11\r\n
			// $7\r\n
			// value11\r\n

// 应答格式例子:
			// *3\r\n
			// *2\r\n
			// :1\r\n
			// :2\r\n
			// -no\r\n
			// *2\r\n
			// +ok\r\n
			// $5\r\n
			// 12345\r\n		
			// 表示一个由3个元素组成的数组，第一个元素包含2个整数的数组
			//    第二个元素为一个错误字符串,
			//    第三个元素为一个成功字符串和一个5字节长度的数据缓存的数组

			// *2\r\n
			// ++now\r\n  
			// $5\r\n
			// 12345\r\n 
			// 表示收到一个标记为now的广播数据，通常用于sub时的返回数据

// #define REDIS_ERR -1
// #define REDIS_OK 0

// /* When an error occurs, the err flag in a context is set to hold the type of
//  * error that occurred. REDIS_ERR_IO means there was an I/O error and you
//  * should use the "errno" variable to find out what is wrong.
//  * For other values, the "errstr" field will hold a description. */
// #define REDIS_ERR_IO 1 /* Error in read or write */
// #define REDIS_ERR_EOF 3 /* End of file */
// #define REDIS_ERR_PROTOCOL 4 /* Protocol error */
// #define REDIS_ERR_OOM 5 /* Out of memory */
// #define REDIS_ERR_OTHER 2 /* Everything else... */

// #define SIS_NET_REPLY_STRING 1
// #define SIS_NET_REPLY_ARRAY 2
// #define SIS_NET_REPLY_INTEGER 3
// #define SIS_NET_REPLY_NIL 4
// #define SIS_NET_REPLY_STATUS 5
// #define SIS_NET_REPLY_ERROR 6

// #define REDIS_READER_MAX_BUF (1024*16)  /* Default max unused reader buffer. */

// #ifdef __cplusplus
// extern "C" {
// #endif

// typedef struct redisReadTask {
//     int type;
//     int elements; /* number of elements in multibulk container */
//     int idx; /* index in parent (array) object */
//     void *obj; /* holds user-generated value for a read task */
//     struct redisReadTask *parent; /* parent task */
//     void *privdata; /* user-settable arbitrary field */
// } redisReadTask;

// typedef struct redisReplyObjectFunctions {
//     void *(*createString)(const redisReadTask*, char*, size_t);
//     void *(*createArray)(const redisReadTask*, int);
//     void *(*createInteger)(const redisReadTask*, long long);
//     void *(*createNil)(const redisReadTask*);
//     void (*freeObject)(void*);
// } redisReplyObjectFunctions;

// typedef struct redisReader {
//     int err; /* Error flags, 0 when there is no error */
//     char errstr[128]; /* String representation of error when applicable */

//     char *buf; /* Read buffer */
//     size_t pos; /* Buffer cursor */
//     size_t len; /* Buffer length */
//     size_t maxbuf; /* Max length of unused buffer */

//     redisReadTask rstack[9];
//     int ridx; /* Index of current read task */
//     void *reply; /* Temporary reply pointer */

//     redisReplyObjectFunctions *fn;
//     void *privdata;
// } redisReader;

// /* Public API for the protocol parser. */
// redisReader *redisReaderCreateWithFunctions(redisReplyObjectFunctions *fn);
// void redisReaderFree(redisReader *r);
// int redisReaderFeed(redisReader *r, const char *buf, size_t len);
// int redisReaderGetReply(redisReader *r, void **reply);

// #define redisReaderSetPrivdata(_r, _p) (int)(((redisReader*)(_r))->privdata = (_p))
// #define redisReaderGetObject(_r) (((redisReader*)(_r))->reply)
// #define redisReaderGetError(_r) (((redisReader*)(_r))->errstr)

// 根据 s_sis_net_message 中 style 决定如何打包和拆包
// 从用户数据转为 out,len， 返回 非空 表示解析正常
// s_sis_sds sis_net_pack_rds(s_sis_net_class *, s_sis_net_message *in_);
// 如果数据不合法就返回 0 数据解析正确就返回当前数据块大小
// bool sis_net_unpack_rds(s_sis_net_class *, s_sis_net_message *out_, s_sis_memory *in_);

int sis_net_pack_rds(s_sis_memory* in_, s_sis_net_tail *,s_sis_memory *out_);
int sis_net_unpack_rds(s_sis_memory* in_, s_sis_net_tail *, s_sis_memory *out_);

bool sis_net_encoded_rds(s_sis_net_message *in_, s_sis_memory *out_);
bool sis_net_decoded_rds(s_sis_memory* in_, s_sis_net_message *out_);

#ifdef __cplusplus
}
#endif

#endif
