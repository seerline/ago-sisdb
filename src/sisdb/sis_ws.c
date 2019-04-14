#include "sis_ws.h"
#include "sis_log.h"
#include "sis_core.h"
#include "sis_thread.h"
#include <sis_json.h>
#include <sisws.h>
#include <sis_bitstream.h>

static const char *__ws_define_hash = "                        258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static char __ws_define_101[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept:                             \r\n\r\n";
// static char* __ws_define_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nContent-Type: text/plain\r\nConnection: Close\r\n\r\n";
// static char* __ws_define_403 = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nContent-Type: text/plain\r\nConnection: Close\r\n\r\n";

static s_sis_ws_server __server = {
};


sis_http_parser_settings __http_settings =
    {
        .on_message_begin = cb_message_begin,
        .on_header_field = cb_header_field,
        .on_header_value = cb_header_value,
        .on_url = cb_url_value,
        .on_headers_complete = cb_headers_complete,
        .on_message_complete = cb_message_complete};

int cb_message_begin(s_sis_http_parser *p)
{
  s_sis_ws_client *client = p->data;
  int i = 0;
  for (i = 0; i < SIS_MAXLEN_WS_HEAD; i++)
  {
    client->request->headers[i][0][0] = 0;
    client->request->headers[i][1][0] = 0;
  }
  client->request->id++;
  client->request->num_headers = 0;
  client->request->last_header_element = NONE;
  client->request->upgrade = 0;
  client->request->handshake = 0;
  client->request->keepalive = 1;
  client->request->url[0] = 0;
  return 0;
}

int cb_url_value(s_sis_http_parser *p, const char *buffer, size_t len)
{
  s_sis_ws_client *client = p->data;
  strncat(client->request->url, buffer, len);
  return 0;
}

int cb_header_field(s_sis_http_parser *p, const char *buffer, size_t len)
{
  s_sis_ws_client *client = p->data;
  s_sis_ws_hand_info *info = client->request;
  if (info->last_header_element != FIELD)
  {
    info->num_headers++;
  }
  strncat(info->headers[info->num_headers - 1][0], buffer, len);
  info->last_header_element = FIELD;
  return 0;
}

int cb_header_value(s_sis_http_parser *p, const char *buffer, size_t len)
{
  s_sis_ws_client *client = p->data;
  s_sis_ws_hand_info *info = client->request;
  strncat(info->headers[info->num_headers - 1][1], buffer, len);
  info->last_header_element = VALUE;
  return 0;
}

int cb_headers_complete(s_sis_http_parser *p)
{
  s_sis_ws_client *client = p->data;
  s_sis_ws_hand_info *info = client->request;
  info->keepalive = sis_http_should_keep_alive(p);
  info->method = p->method;
  info->upgrade = p->upgrade;
  return 0;
}

int cb_message_complete(s_sis_http_parser *p)
{
  s_sis_ws_client *client = p->data;
  s_sis_ws_hand_info *info = client->request;
  s_sis_ws_mess *message = sis_ws_mess_create_hand();
  message->source = client->input; 
  int i;
  for (i = 0; i < info->num_headers; i++)
  {
    if (strncasecmp(info->headers[i][0], "Sec-WebSocket-Key", 17) == 0)
    {
      strncpy(info->wskey, info->headers[i][1], 24);
      break;
    }
  }
  shacalc(info->wskey, __ws_define_101 + 97);
  message->write_buffer = uv_buf_init(__ws_define_101, 129);
  uv_write_t *write = sis_malloc(sizeof(uv_write_t));
  write->data = message;
  if (uv_write(write, message->source, &message->write_buffer, 1, cb_write_after_hand))
  {
    exit(1);
  }
  info->handshake = 1;
  if (!sis_http_should_keep_alive(p))
  {
    uv_close((uv_handle_t *)client->input, cb_close);
  }
  return 0;
}

void cb_close(uv_handle_t *peer)
{
  _client_shutdown_free(peer);
}

void cb_shutdown_after(uv_shutdown_t *req, int status)
{
  uv_close((uv_handle_t *)req->handle, cb_close);
  sis_free(req);
}

s_sis_ws_messages *sis_ws_messages_create()
{
  s_sis_ws_messages *o = sis_malloc(sizeof(s_sis_ws_messages));
  memset(o, 0, sizeof(s_sis_ws_messages));
  o->lists = sis_pointer_list_create();
  o->buffer = sis_memory_create();
  sis_mutex_create(&o->mutex);
  return o;
}
void sis_ws_messages_destroy(s_sis_ws_messages *ws_)
{
  sis_ws_messages_clear(ws_);

  sis_pointer_list_destroy(ws_->lists);
  sis_memory_destroy(ws_->buffer);
  sis_mutex_destroy(&ws_->mutex);
  sis_free(ws_);
}
void sis_ws_messages_clear(s_sis_ws_messages *ws_)
{
  for (int i = 0; i < ws_->lists->count; i++)
  {
    s_sis_ws_mess *mess = sis_pointer_list_get(ws_->lists, i);
    sis_ws_mess_destroy(mess);
  }
  sis_pointer_list_clear(ws_->lists);
}
// 处睆拆包和粘包
int sis_ws_messages_input(s_sis_ws_messages *ws_, void *source_, const char *in_, size_t ilen_)
{
  sis_mutex_lock(&ws_->mutex);
  sis_memory_cat(ws_->buffer, (char *)in_, ilen_);
  s_sis_bit_stream *stream = sis_bitstream_create(
      (uint8 *)sis_memory(ws_->buffer), sis_memory_get_size(ws_->buffer), 0);

  s_sis_ws_header head;
  int count = 0;
  int isbreak = true;
  int offset = sis_memory_get_address(ws_->buffer);
  // printf("==1==  %d - %d\n", offset, sis_memory_get_size(ws_->buffer));
  while (1)
  {
    if (sis_memory_get_size(ws_->buffer) < 2)
    {
      break;
    }
    head.fin = sis_bitstream_get(stream, 1);
    sis_bitstream_get(stream, 3);
    head.opcode = sis_bitstream_get(stream, 4);
    head.mask = sis_bitstream_get(stream, 1);
    head.length = sis_bitstream_get(stream, 7);

    // printf("==2.1==  %d - %d\n",head.length, sis_memory_get_size(ws_->buffer));

    sis_memory_move(ws_->buffer, 2);
    if (head.length == 126)
    {
      if (sis_memory_get_size(ws_->buffer) < 2)
        break;
      head.length = sis_bitstream_get(stream, 16);
      sis_memory_move(ws_->buffer, 2);
    }
    if (head.length == 127)
    {
      if (sis_memory_get_size(ws_->buffer) < 8)
        break;
      head.length = sis_bitstream_get(stream, 64);
      sis_memory_move(ws_->buffer, 8);
    }
    // printf("==2.2==  %d - %d\n",head.length, sis_memory_get_size(ws_->buffer));
    if (head.mask == 1)
    {
      if (sis_memory_get_size(ws_->buffer) < 4)
        break;
      head.maskkey[0] = sis_bitstream_get(stream, 8);
      head.maskkey[1] = sis_bitstream_get(stream, 8);
      head.maskkey[2] = sis_bitstream_get(stream, 8);
      head.maskkey[3] = sis_bitstream_get(stream, 8);
      sis_memory_move(ws_->buffer, 4);
    }
    // printf("==2.3==  %d - %d\n",head.length, sis_memory_get_size(ws_->buffer));
    if (sis_memory_get_size(ws_->buffer) < head.length)
    {
      break;
    }
    if (head.mask == 1)
    {
      char *ptr = sis_memory(ws_->buffer);
      for (int i = 0; i < head.length; i++)
      {
        ptr[i] = ptr[i] ^ head.maskkey[i % 4];
      }
    }
    // printf("==2.8==  %d - %d -- stream : %d\n", offset, sis_memory_get_size(ws_->buffer),
    //         sis_bitstream_getbytelen(stream));
    sis_bitstream_move(stream, head.length);

    s_sis_ws_mess *mess = sis_ws_mess_create(sis_memory(ws_->buffer), head.length);
    mess->source = source_;

    sis_memory_move(ws_->buffer, head.length);
    count++;
    sis_pointer_list_push(ws_->lists, mess);

    offset = sis_memory_get_address(ws_->buffer);
    printf("==2==  %d - %d -- stream : %d\n", offset, sis_memory_get_size(ws_->buffer),
            sis_bitstream_getbytelen(stream));
    sis_out_binary("--2--", sis_memory(ws_->buffer), sis_memory_get_size(ws_->buffer));
    if (sis_memory_get_size(ws_->buffer) < 1)
    {
      isbreak = false;
      break;
    }
  }
  if (isbreak)
  {
    // printf("==||==  %d - %d\n", offset, sis_memory_get_size(ws_->buffer));
    // 退回上次的佝置
    sis_memory_jumpto(ws_->buffer, offset);
  }
  // sis_memory_pack(ws_->buffer);
  sis_bitstream_destroy(stream);
  sis_mutex_unlock(&ws_->mutex);

  return count;
}
s_sis_sds sis_json_node_get_sds(s_sis_sds s_, s_sis_json_node *node_, const char *key_)
{
  s_ = sis_sdsempty();
  s_sis_json_node *n = sis_json_cmp_child_node(node_, key_);
  if (!n)
  {
    return s_;
  }
  if (n->type == SIS_JSON_INT || n->type == SIS_JSON_DOUBLE || n->type == SIS_JSON_STRING)
  {
    s_ = sis_sdscpy(s_, n->value);
  }
  else if (n->type == SIS_JSON_ARRAY)
  {
    // 暂时只取第一个参数，其余以后在说
    s_sis_json_node *node = sis_json_cmp_child_node(n, "0");
    if(node)
    {
      size_t len = 0;
      char *str = sis_json_output_zip(node, &len);
      s_ = sis_sdscpy(s_, str);
      sis_free(str);
    }
  } else
  // else if( n->type == SIS_JSON_OBJECT)
  {
    size_t len = 0;
    char *str = sis_json_output_zip(n, &len);
    s_ = sis_sdscpy(s_, str);
    sis_free(str);
  }
  return s_;
}
s_sis_ws_mess * sis_ws_mess_create_hand()
{
  s_sis_ws_mess *mess = sis_malloc(sizeof(s_sis_ws_mess));
  memset(mess, 0, sizeof(s_sis_ws_mess));
  mess->style = SIS_WS_STYLE_HAND;
  return mess;  
}
s_sis_ws_mess *sis_ws_mess_create(const char *in_, size_t ilen_)
{
  s_sis_ws_mess *mess = sis_malloc(sizeof(s_sis_ws_mess));
  memset(mess, 0, sizeof(s_sis_ws_mess));

  char *in = sis_malloc(ilen_ + 1);
  memmove(in, in_, ilen_);
  in[ilen_] = 0;

  sis_out_binary("input: ", in_, ilen_);

  mess->in = sis_message_node_create();

  int set = sis_str_pos(in, 64, ':');
  if (set > 0)
  {
    mess->in->source = sis_sdsnewlen(in, set);
  }
  size_t ilen = ilen_ - set + 1;
  s_sis_json_handle *handle = sis_json_load(in + set + 1, ilen);
  if (!handle)
  {
    sis_free(in);
    sis_message_node_destroy(mess->in);
    sis_free(mess);
    LOG(5)
    ("parse %s error.\n", in);
    return NULL;
  }
  sis_free(in);
  // size_t len = 0;
  // char *str = sis_json_output(handle->node, &len);
  // printf("|%s|\n",str);
  // sis_free(str);

  mess->in->command = sis_json_node_get_sds(mess->in->command, handle->node, "cmd");
  mess->in->key = sis_json_node_get_sds(mess->in->key, handle->node, "key");
  mess->in->argv = sis_json_node_get_sds(mess->in->argv, handle->node, "argv");

  printf("%s \n%s \n%s \n%s \n",
         mess->in->source,
         mess->in->command,
         mess->in->key,
         mess->in->argv);
  sis_json_close(handle);
  return mess;
}

void sis_ws_mess_destroy(s_sis_ws_mess *mess_)
{
  if (mess_->sign)
  {
    sis_sdsfree(mess_->sign);
  }
  if (mess_->in)
  {
    sis_message_node_destroy(mess_->in);
  }
  if (mess_->reply)
  {
    sis_object_decr(mess_->reply);
  }
}

s_sis_object *sis_ws_mess_send(s_sis_ws_mess *mess_)
{
  if (!mess_->in)
  {
    return NULL;
  }
  mess_->reply = siscs_send_local_obj(mess_->in->command, mess_->in->key,
                                      mess_->in->argv, mess_->in->argv ? sis_sdslen(mess_->in->argv) : 0);
  if (!mess_->reply)
  {
    mess_->reply = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew("{}"));
  }
  return mess_->reply;
}

s_sis_sds sis_ws_mess_serialize_sds(s_sis_ws_mess *mess_)
{
  size_t msg_size = 1;

  if (mess_->in && mess_->in->source)
    msg_size += sis_sdslen(mess_->in->source);
  if (mess_->reply && mess_->reply->ptr)
  {
    msg_size += sis_sdslen(mess_->reply->ptr);
  }

  size_t maxlen = msg_size + SIS_MAXLEN_WS_HEAD;

  s_sis_sds out = sis_sdsnewlen(NULL, maxlen + SIS_MAXLEN_WS_HEAD);
  s_sis_bit_stream *stream = sis_bitstream_create((uint8 *)out, maxlen, 0);

  sis_bitstream_put(stream, 1, 1); // fin
  sis_bitstream_put(stream, 0, 3); // rsv
  sis_bitstream_put(stream, 1, 4); // opcode
  sis_bitstream_put(stream, 0, 1); // mask
  if (msg_size < 126)
  {
    sis_bitstream_put(stream, msg_size, 7);
  }
  else if (msg_size >= 126 && msg_size <= (size_t)0xFFFF)
  {
    sis_bitstream_put(stream, 126, 7);
    sis_bitstream_put(stream, msg_size, 16);
  }
  else
  {
    sis_bitstream_put(stream, 127, 7);
    sis_bitstream_put(stream, msg_size, 64);
  }
  if (mess_->in && mess_->in->source)
  {
    sis_bitstream_put_buffer(stream, mess_->in->source, sis_sdslen(mess_->in->source));
  }

  sis_bitstream_put_buffer(stream, ":", 1);

  if (mess_->reply)
  {
    sis_bitstream_put_buffer(stream, mess_->reply->ptr, sis_sdslen(mess_->reply->ptr));
  }

  size_t curlen = sis_bitstream_getbytelen(stream);
  // printf("out= %d\n", curlen);
  sis_sdssetlen(out, curlen);
  sis_bitstream_destroy(stream);
  // printf("out= %d  %d\n", sis_sdslen(out), curlen);

  sis_object_set(mess_->reply, SIS_OBJECT_SDS, out);
  // sis_object_decr(mess_->reply);
  // mess_->reply = NULL;

  return out;
}

void cb_write_after_hand(uv_write_t *write, int status)
{
  if (status < 0)
  {
    LOG(5)("uv_write error: %s\n", uv_err_name(status));
  }
  printf("write ok. %p \n", write->data);
  s_sis_ws_mess *message = (s_sis_ws_mess *)write->data;
  if (message)
  {
    sis_ws_mess_destroy(message);
  }
  sis_free(write);
}
void cb_write_after_body(uv_write_t *write, int status)
{
  if (status < 0)
  {
    LOG(5)("uv_write error: %s\n", uv_err_name(status));
  }
  printf("write ok. %p \n", write->data);
  // 这里释放
  s_sis_ws_mess *message = (s_sis_ws_mess *)write->data;
  if (message)
  {
    sis_ws_mess_destroy(message);
  }
  sis_free(write);
}
static void after_shutdown(uv_shutdown_t* req, int status) {
  uv_close((uv_handle_t*) req->handle, cb_close);
  sis_free(req);
}
// void cb_read_after(uv_stream_t *handle, ssize_t insize, uv_buf_t buffer)
static void cb_read_after(uv_stream_t* handle,
                       ssize_t nread,
                       const uv_buf_t* buf)
{
  // printf("----------%d----------%p -- %p\n", (int)nread, handle, buf);

  uv_shutdown_t* sreq;
  if (nread < 0)
  {
    ASSERT(nread == UV_EOF);
    sis_free(buf->base);
    sreq = sis_malloc(sizeof(uv_shutdown_t));
    ASSERT(0 == uv_shutdown(sreq, handle, after_shutdown));
    return;
  }
  if (nread == 0)
  {
    sis_free(buf->base);
    return;
  }
  s_sis_ws_client *client = (s_sis_ws_client *)handle->data;

  if (client->request->handshake == 0)
  {
    size_t bytes = sis_http_parser_execute(client->parser, &__http_settings, buf->base, nread);
    
    // sis_out_binary("hand", buf->base, nread);

    sis_free(buf->base);
    if (bytes != nread)
    {
      uv_shutdown_t *com;
      com = (uv_shutdown_t *)sis_malloc(sizeof(uv_shutdown_t));
      uv_shutdown(com, handle, cb_shutdown_after);
    }
  }
  else
  {

    // sis_out_binary("source: ", buf->base, nread);

    sis_ws_messages_input(client->messages, handle, buf->base, nread);

    for (int i = 0; i < client->messages->lists->count;)
    {
      s_sis_ws_mess *message = (s_sis_ws_mess *)sis_pointer_list_get(client->messages->lists, i);
      printf("client->messages: count = %d: %d %s\n", client->messages->lists->count, i,
             message->in->source);
      sis_ws_mess_send(message);
      if (message->reply)
      {
        s_sis_sds reply = sis_ws_mess_serialize_sds(message);
        sis_out_binary("reply: ", reply, sis_sdslen(reply));
        message->write_buffer = uv_buf_init(reply, sis_sdslen(reply));
        uv_write_t *write = sis_malloc(sizeof(uv_write_t));
        write->data = message;
        printf("write ..... %p \n", write->data);
        if (uv_write(write, message->source, &message->write_buffer, 1, cb_write_after_body))
        {
          exit(1);
        }
      }
      sis_pointer_list_delete(client->messages->lists, i, 1);
    }
    sis_free(buf->base);
  }
}

void _client_connected_init(uv_stream_t *input_)
{
  s_sis_ws_client *client = sis_malloc(sizeof(s_sis_ws_client));
  
  client->messages = sis_ws_messages_create();

  client->parser = sis_malloc(sizeof(s_sis_http_parser));
  client->request = sis_malloc(sizeof(s_sis_ws_hand_info)); 

  strcpy(client->request->wskey, __ws_define_hash);
  client->request->id = 0;
  assert(client->request);
  client->request->handshake = 0;
  client->input = input_;
  input_->data = client;
  http_parser_init(client->parser, HTTP_REQUEST);
  client->parser->data = client;
}

void _client_shutdown_free(uv_handle_t *handle)
{
  s_sis_ws_client *client = handle->data;
  if (client)
  {
    sis_free(client->request);
    sis_free(client->parser);
    sis_ws_messages_destroy(client->messages);
    sis_free(client);
  }
  sis_free(handle);
}
// uv_buf_t cb_read_alloc(uv_handle_t *handle, size_t suggested_size)
static void cb_read_alloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf)
{
  buf->base = sis_malloc(suggested_size);
  buf->len = suggested_size;
  // return uv_buf_init(sis_malloc(suggested_size), suggested_size);
}

void cb_connection(uv_stream_t *server, int status)
{
  uv_stream_t *client;
  int r;
  if (status != 0)
  {
    LOG(1)("connect error %s\n", uv_err_name(status));
  }
  assert(status == 0);
  
  client = sis_malloc(sizeof(uv_tcp_t));

  assert(client != NULL);
  r = uv_tcp_init(__server.loop, (uv_tcp_t *)client);
  assert(r == 0);
  r = uv_accept(server, client);
  assert(r == 0);

  _client_connected_init(client);
  
  r = uv_read_start(client, cb_read_alloc, cb_read_after);
  assert(r == 0);
}

int sis_ws_server_start(int port)
{
  // strncpy(r101,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept:                             \r\n\r\n",129);
  // 有一个新链接就创建一个，
  __server.loop = uv_default_loop();

  int r;
  struct sockaddr_in addr;
  ASSERT(0 == uv_ip4_addr("0.0.0.0", port, &addr));
  // struct sockaddr_in address = uv_ip4_addr("0.0.0.0", port);

  r = uv_tcp_init(__server.loop, &__server.server);
  if (r)
  {
    LOG(1)
    ("socket create error.\n");
    return 1;
  }
  r = uv_tcp_bind(&__server.server, (const struct sockaddr*) &addr, 0);
  if (r)
  {
    LOG(1)
    ("bind error. [%d]\n", port);
    return 1;
  }
  r = uv_listen((uv_stream_t *)&__server.server, 4096, cb_connection);
  if (r)
  {
    LOG(1)
    ("listen error. %s\n", uv_err_name(r));
    return 1;
  }
  uv_run(__server.loop, UV_RUN_DEFAULT);
  return 0;
}

#if 0
int main(int argc, char **argv) {
  loop = uv_default_loop();
  if (server_start(80)) return 1;
  uv_run(loop);
  return 0;
}
#endif

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <uv.h>

// uv_loop_t *loop;

// typedef struct {
//     uv_write_t req;
//     uv_buf_t buf;
// } write_req_t;

// void free_write_req(uv_write_t *req) {
//     write_req_t *wr = (write_req_t*) req;
//     sis_free(wr->buf.base);
//     sis_free(wr);
// }

// void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {//用于读数杮时的缓冲区分酝
//   buf->base = (char*)sis_malloc(suggested_size);
//   buf->len = suggested_size;
// }

// void echo_write(uv_write_t *req, int status) {
//     if (status < 0) {
//         fprintf(stderr, "Write error %s\n", uv_err_name(status));
//     }
//     free_write_req(req);
// }

// void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
//     if (nread > 0) {
//         write_req_t *req = (write_req_t*) sis_malloc(sizeof(write_req_t));
//         req->buf = uv_buf_init(buf->base, nread);//这是个构造函数，这里仅仅是浅拷贝＝指针坪是进行了赋值而已＝＝＝
//         uv_write((uv_write_t*) req, client, &req->buf, 1, echo_write);
//         return;
//     }

//     if (nread < 0) {
//         if (nread != UV_EOF)
//             fprintf(stderr, "Read error %s\n", uv_err_name(nread));
//         uv_close((uv_handle_t*) client, NULL);
//     }

//     sis_free(buf->base);
// }

// void on_new_connection(uv_stream_t *server, int status) {
//     if (status == -1) {
//         // error!
//         return;
//     }

//     uv_pipe_t *client = (uv_pipe_t*) sis_malloc(sizeof(uv_pipe_t));
//     uv_pipe_init(loop, client, 0);
//     if (uv_accept(server, (uv_stream_t*) client) == 0) {//accept戝功之坎开始读
//         uv_read_start((uv_stream_t*) client, alloc_buffer, echo_read);//开始从pipe读（在loop中注册读事件），读完回调
//     }
//     else {
//         uv_close((uv_handle_t*) client, NULL);//出错关闭
//     }
// }

// void remove_sock(int sig) {
//     uv_fs_t req;
//     uv_fs_unlink(loop, &req, "echo.sock", NULL);//删除擝作
//     exit(0);
// }

// int main() {
//     loop = uv_default_loop();

//     uv_pipe_t server;
//     uv_pipe_init(loop, &server, 0);

//     signal(SIGINT, remove_sock);//收到结束信坷顺便删除相关pipe文件

//     int r;
//     if ((r = uv_pipe_bind(&server, "echo.sock"))) {//朝务器端绑定unix域地址
//         fprintf(stderr, "Bind error %s\n", uv_err_name(r));
//         return 1;
//     }
//     if ((r = uv_listen((uv_stream_t*) &server, 128, on_new_connection))) {//开始连接，出现回调，enent loop由此开始
//         fprintf(stderr, "Listen error %s\n", uv_err_name(r));
//         return 2;
//     }
//     return uv_run(loop, UV_RUN_DEFAULT);
// }

///// --- client ----

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <uv.h>

// uv_loop_t * loop;
// uv_tty_t tty_stdin,tty_stdout;
// uv_pipe_t server;

// typedef struct {//该结构存在的愝义就是坑回调函数传递缓冲区地址，并坊时释放
//     uv_write_t req;
//     uv_buf_t buf;
// } write_req_t;

// void free_write_req(uv_write_t *req) {
//     write_req_t *wr = (write_req_t*) req;
//     sis_free(wr->buf.base);
//     sis_free(wr);
// }

// void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
//   buf->base = (char*)sis_malloc(suggested_size);
//   buf->len = suggested_size;
// }

// void write_to_stdout_cb(uv_write_t* req, int status){
//     if(status){
//         fprintf(stderr, "Write error %s\n", uv_strerror(status));
//         exit(0);
//     }
//     free_write_req(req);
// }

// void read_from_pipe_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
//     write_req_t *wri = (write_req_t *)sis_malloc(sizeof(write_req_t));
//     wri->buf = uv_buf_init(buf->base,nread);
//     uv_write((uv_write_t*)wri,(uv_stream_t*)&tty_stdout,&wri->buf,1,write_to_stdout_cb);
// }

// void write_to_pipe_cb(uv_write_t* req, int status){
//     if(status){
//         fprintf(stderr, "Write error %s\n", uv_strerror(status));
//         exit(0);
//     }
//     uv_read_start((uv_stream_t*)&server,alloc_buffer,read_from_pipe_cb);//冝一次构造缓冲区
//     free_write_req(req);//释放动思分酝的所有数杮
// }

// void read_from_input_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
//     write_req_t *wri = (write_req_t *)sis_malloc(sizeof(write_req_t));//实例化新结构，主覝2个作用：产生write所需的uv_write_t;临时存储buf（坌时杝供给回调函数枝构方法）便于该数杮的坊时free
//     wri->buf=uv_buf_init(buf->base, nread);//buf夝制
//     uv_write((uv_write_t*)wri,(uv_stream_t*)&server,&wri->buf,1,write_to_pipe_cb);//需覝注愝的是write调用的时候&wri->buf必须依然有效＝所以这里直接用buf会出现问题＝
//     //write完戝之坎当剝缓冲区也就失去了愝义，于是坑回调函数传递buf指针，并由回调函数负责枝构该缓冲区
// }

// int main() {
//     loop = uv_default_loop();

//     uv_pipe_init(loop, &server, 1);
//     uv_tty_init(loop,&tty_stdin,0,1);
//     uv_tty_init(loop,&tty_stdout,1,0);

//     uv_connect_t conn;
//     uv_pipe_connect((uv_connect_t*)&conn,&server,"echo.sock",NULL);//连接pipe
//     uv_read_start((uv_stream_t*)&tty_stdin,alloc_buffer,read_from_input_cb);//从stdin读数杮，并由此触坑回调,毝次alloc_buffer回调产生一个缓冲数杮结构uv_buf_t并在堆上分酝数杮＝

//     return uv_run(loop, UV_RUN_DEFAULT);
// }