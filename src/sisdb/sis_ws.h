#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "uv.h"

#include "sis_core.h"
#include "sis_sds.h"
#include "sis_list.h"
#include "sis_net.node.h"
#include "sis_net.obj.h"

#include "sis_http.h"
#include "sis_sha1.h"
#include "sis_memory.h"

#define SIS_MAXLEN_WS_HEAD 255
#define SIS_MAXLEN_ELEMENT 500
#define SIS_MAXLEN_URL     255

// 数据体结构定义
typedef  struct s_sis_ws_header {
  uint8  fin;
  uint8  rsv[3];
  uint8  opcode;
  uint64 length;
  uint8  mask;
  uint8  maskkey[4];
}s_sis_ws_header;

#define SIS_WS_STYLE_BODY     0  // 正常通讯
#define SIS_WS_STYLE_HAND     1  // 握手消息
// 单一数据包体结构定义
typedef struct s_sis_ws_mess {
  int        style;   // 数据类型，以便释放 source
  void      *source;   //  该信息所关联的端口等信息
  // void      *whither;  //  向何处去，以及向何处去的数据等
  uv_buf_t   write_buffer; //
  s_sis_sds sign;
  s_sis_net_message *in;
  s_sis_object *reply;
} s_sis_ws_mess;

s_sis_ws_mess * sis_ws_mess_create_hand();
s_sis_ws_mess * sis_ws_mess_create(const char *in_, size_t ilen_);
void sis_ws_mess_destroy(s_sis_ws_mess *mess_);
// int sis_ws_mess_input(s_sis_ws_mess *mess_, const char *in_, size_t ilen_);
s_sis_object *sis_ws_mess_send(s_sis_ws_mess *mess_);
s_sis_memory *sis_ws_mess_serialize_sds(s_sis_ws_mess *mess_);


/////////////////////////
//  一个链接一个消息列表
/////////////////////////
// 传递给底层的结构体，主程序只需要不断监听该消息队列即可获取请求，处理完后放进发送队列，
// 底层会通过信号量，实时发送顺序发送这些消息了，

typedef struct s_sis_ws_messages {
  s_sis_mutex_t mutex;
  // 还应该有处理队列的回调
  s_sis_memory *buffer;
  s_sis_pointer_list *lists;  // 一个信息列表 s_sis_ws_mess
  // 增加读写锁
  s_sis_pointer_list *replys; // 应答队列，提供方法写入，
} s_sis_ws_messages;

s_sis_ws_messages * sis_ws_messages_create();
void sis_ws_messages_destroy(s_sis_ws_messages *ws_);
// 只管生成lists
int sis_ws_messages_input(s_sis_ws_messages *ws_, void *source_, const char *in_, size_t ilen_);
void sis_ws_messages_clear(s_sis_ws_messages *ws_);

/////////////////////////
// 数据体结构定义
/////////////////////////
typedef struct s_sis_ws_hand_info {
  uint64_t id;
  char url[SIS_MAXLEN_URL];
  enum http_method method;

  int num_headers;
  enum { NONE=0, FIELD, VALUE } last_header_element;
  char headers [SIS_MAXLEN_WS_HEAD][2][SIS_MAXLEN_ELEMENT];
  char wskey[61];
  uint8_t upgrade;
  uint8_t handshake;
  uint8_t keepalive;
}s_sis_ws_hand_info;

#define s_sis_http_parser http_parser
#define sis_http_parser_execute  http_parser_execute
#define sis_http_parser_settings http_parser_settings
#define sis_http_should_keep_alive http_should_keep_alive

typedef struct s_sis_ws_client {
  s_sis_ws_messages *messages;  // 消息队列
  // int fd;                // 句柄
  s_sis_http_parser   *parser;  
  s_sis_ws_hand_info  *request;
  uv_stream_t* input;
} s_sis_ws_client;

typedef struct s_sis_ws_server {
  uv_tcp_t   server;
  uv_loop_t* loop;
  s_sis_pointer_list *clients;  // 一个信息列表 s_sis_ws_client
} s_sis_ws_server;

#define FATAL(msg)                                        \
  do {                                                    \
    fprintf(stderr,                                       \
            "Fatal error in %s on line %d: %s\n",         \
            __FILE__,                                     \
            __LINE__,                                     \
            msg);                                         \
    fflush(stderr);                                       \
    abort();                                              \
  } while (0)

/* Have our own assert, so we are sure it does not get optimized away in
 * a release build.
 */
#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)


 int cb_message_begin (http_parser *p);
 int cb_url_value (http_parser *p, const char *buf, size_t len);
 int cb_header_field (http_parser *p, const char *buf, size_t len);
 int cb_header_value (http_parser *p, const char *buf, size_t len);
 int cb_headers_complete (http_parser *p);
 int cb_message_complete (http_parser *p);

void cb_write_after_hand(uv_write_t *write, int status);

void cb_close(uv_handle_t* peer);

void _client_shutdown_free(uv_handle_t *handle);

int sis_ws_server_start(int port);

// int sis_ws_server_create();


//  char r400[96];
//  char r403[90];
//  char r101[129];
