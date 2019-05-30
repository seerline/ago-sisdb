
#ifndef _SIS_NET_TCP_H
#define _SIS_NET_TCP_H

#include <sis_net.h>

// #define SIS_NET_REPLY_SUB     7

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
s_sis_sds sis_net_pack_tcp(struct s_sis_net_class *, s_sis_net_message *in_);
// 如果数据不合法就返回 0 数据解析正确就返回当前数据块大小
size_t sis_net_unpack_tcp(struct s_sis_net_class *, s_sis_net_message *out_, char* in_, size_t ilen_);

#ifdef __cplusplus
}
#endif

#endif
