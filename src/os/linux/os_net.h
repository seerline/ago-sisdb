
#ifndef _OS_NET_H
#define _OS_NET_H

// #include <sys/time.h> 
// #include <unistd.h>
// #include <strings.h>
#include <signal.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>     // sockaddr_in, "man 7 ip" ,htons
#include <netinet/tcp.h>
#include <arpa/inet.h>   //inet_addr,inet_aton
#include <poll.h>             //poll,pollfd
#include <unistd.h>        //read,write
#include <netdb.h>         //gethostbyname

#include <os_str.h>         //gethostbyname

#define SIS_NET_MAX_SEND_LEN   32*1024
#define SIS_NET_MAX_RECV_LEN   32*1024

#ifdef __cplusplus
extern "C" {
#endif

void sis_socket_init();
void sis_socket_uninit();

int sis_socket_getip4(const char *name_, char *ip_, size_t ilen_);

#define sis_net_recv(a,b,c,d) read(a,b,c)
#define sis_net_send(a,b,c,d) write(a,b,c)

#define sis_net_close close

#ifdef __cplusplus
}
#endif

//    web-socket 头定义
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-------+-+-------------+-------------------------------+
//    |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
//    |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
//    |N|V|V|V|       |S|             |   (if payload len==126/127)   |
//    | |1|2|3|       |K|             |                               |
//    +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
//    |     Extended payload length continued, if payload len == 127  |
//    + - - - - - - - - - - - - - - - +-------------------------------+
//    |                               |Masking-key, if MASK set to 1  |
//    +-------------------------------+-------------------------------+
//    | Masking-key (continued)       |          Payload Data         |
//    +-------------------------------- - - - - - - - - - - - - - - - +
//    :                     Payload Data continued ...                :
//    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
//    |                     Payload Data continued ...                |
//    +---------------------------------------------------------------+
////////////////////////////////////////////////////////////////////////
//    sis-stream 二进制流的定义
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +---+---+---+---+-+-+-----------+-------------------------------+
//    | C | S | Z | R |K|L| key len   | key ...
//    | R | S | I | S |E|I|  (64)     |
//    | C | L | P | V |Y|N|           |
//    |   |   |   |   | |K|           |
//    |   |   |   |   | |S|           |
//    +---+---+---+---+-+-+-----------+ - - - - - - - - - - - - - - - +
//    | data ....                              | CRC(16-64)| 
//    + - - - - - - - - - - - - - - - - - - - -+-----------+- - - - - +
//    | links count   | link1 len     | link1 info....
//    +---------------+---------------+ - - - - - - - - - - - - - - - +
//    | links info....
//    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
//    二进制结构体说明：
//    CRC是标志校验位数   0：不校验 0x01: crc16 0x10:crc32 0x11：crc64   
//    SSL是加密标志      0：不加密 0x11：保留
//                     0x01: openssl 传输前需要交换密钥
//                     0x10: 对称加密，双发约定的密钥          
//    ZIP是压缩标志      0：不压缩 
//                     0x01: 专用压缩格式 --  针对key特定的结构压缩
//                     0x10: 通用压缩 -- 要求高速轻便 
//                     0x11: 保留

//    KEY   表示数据区的格式定义，我们把各种数据都定义为一个唯一的标志符，这样便于扩展 
//    KEY LEN 表示key的长度 不能超过64个字符 后面紧跟key的值，不带结束符号

//    LINKS 表示是否有源头数据
//    LINKS COUNT 表示数据需要一层一层传递下去 的 数量
//       最多传递255层，超过就丢弃，
//    LINKS INFO 为一个类似key的结构， 最大长度为255，后面为 KEY.ID
//    上传时，每多一级传输，就在本地生成key+id，然后追加到数据包末尾，然后一层层传递
//    数据处理完返回数据时，需要该数据原样返回，到上一级server时，根据最后一个key+id
//    定位需要回传给上一级哪台服务器，然后去掉最后一条纪录，上传数据，以此类推，知道links count = 0
//    
//   如果CRC有定义，全部数据结束后，计算CRC值，并对应写到数据最后，
//   *** CRC 仅仅校验数据，并不对其他数据进行校验 （提高效率，避免频繁操作）***
//   *** 分析：key 如果被破坏，最坏情况和其他key重叠，解析后字段长度也不同，数据被丢弃 ***
//   ***      link信息紊乱，数据被丢弃，其他数据破坏也会被丢弃，不会解析出渣数据 ****
//   接收端收到数据后，用ws头的数据总长度 减去 二进制控制信息，就是实际数据区，
//   首先校验CRC，然后解密，最后再解压缩，就还原到实际的二进制数据了
////////////////////////////////////////////////////////////////////////
//   接口需要提供数据打包的入口函数， 数据拆包 
//
//   cs 中间件机制
//   本地服务和远程服务，对于本地服务，直接是方法名进行调用，即时返回，
//   客户首先登录，对于生产者登录后提供 服务名 cs会把远程服务保存在远程服务列表中
//   
//   消费者登录后，发出指令，服务名.method key com ，cs收到后，查询服务，添加服务到等待队列中，
//      返回确认信息给消费者，直到收到生产者的回应数据后，再发送给消费者，消费者通过回调获取数据
////////////////////////////////////////////////////////////////////////
//  1、内部通讯，默认采用二进制传输，仅仅在 客户请求 json array csv 格式时才返回文本型 
//  2、字符串全部用utf8  
//   
//  3、有三个角色，server、client、middle、 
//
#endif //_OS_NET_H
