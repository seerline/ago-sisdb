
#include "sis_core.h"
#include "sis_sds.h"
#include "sis_list.h"
#include "sis_obj.h"
#include "sis_net.msg.h"
#include "sis_net.h"
#include "sis_math.h"
#include "sis_sha1.h"
#include "sis_memory.h"

#include "sis_log.h"
#include "sis_str.h"
#include <sis_json.h>
#include <sis_bits.h>

#define WS_MAX_HEAD_LEN  (10 + 6)  // 实际是10 保险起见多留几个字节
#define WS_MAX_SEND_BUFF (16 * 1024 - WS_MAX_HEAD_LEN)  // 确保不超过16K
// 浏览器的ws协议不能超过64K数据大小，因此必须拆包，才能发送给浏览器，否则发送失败
// 打包

// 数据体结构定义
typedef  struct s_sis_ws_header {
  uint8  fin;
  uint8  rsv[3];
  uint8  opcode;
  uint64 length;
  uint8  mask;
  uint8  maskkey[4];
}s_sis_ws_header;


// 得到请求连接的缓存
size_t sis_net_ws_get_ask(s_sis_memory *memory_);
// 得到应答信息缓存
size_t sis_net_ws_get_ans(char *key_, s_sis_memory *memory_);
// 判断是否头完整
int sis_net_ws_head_complete(s_sis_memory *in_);
// 得到请求包的key 不得少于32个字节
int sis_net_ws_get_key(s_sis_memory *in_, char *key_);
// 客户端收到回应后 判断服务器是否可用
int sis_net_ws_chk_ans(s_sis_memory *in_);
// 打包时可能数据会拆分多个包 一起写到一个缓存中 
int sis_net_pack_ws(s_sis_memory* in_, s_sis_net_tail *, s_sis_memory *out_);

int sis_net_unpack_ws(s_sis_memory* in_, s_sis_net_tail *, s_sis_memory *out_);

#define sis_net_pack_tcp sis_net_pack_ws
#define sis_net_unpack_tcp sis_net_unpack_ws

//  char r400[96];
//  char r403[90];
//  char r101[129];
