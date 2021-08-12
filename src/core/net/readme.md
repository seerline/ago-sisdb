﻿
文件结构说明：

            sis_net.uv.c 网络层  
            sis_net.uv.h 网络层  
        sis_net.ws.c  协议层  
        sis_net.ws.h  协议层  
        sis_net.rds.c 协议层  
        sis_net.rds.h 协议层  
    sis_net.node.c 应用层  
    sis_net.node.h 应用层  
sis_net.c   提供给外部用户访问的函数定义
sis_net.h   提供给外部用户访问的接口和类型


//////////////////////////////////////////////////////////////////////////////////////////
// 网络通讯协议 主要的关键字
// ver     [非必要] 协议版本 默认 1.0
// fmt     [必要] 数据格式 byte json http 二进制和json数据 为ws格式 http 会把请求和应答数据转换为标准http格式
// name    [必要] 请求的名字 用户名+时间戳+序列号 唯一标志请求的名称 方便无状态接收数据
// service [非必要]   对sisdb来说每个数据集都是一个service 这里指定去哪个service执行命令 http格式这里填路径
// command [非必要]   我要干什么 
// ask     [请求专用｜必要] 表示为请求包
// ans     [应答专用｜必要] 表示为应答包
// msg     [应答专用｜不必要] 表示为应答包
// fuc     [应答专用｜不必要] 表示为后续还有多少数量包
// lnk     [不必要] 数据链路
//////////////////////////////////////////////////////////////////////////////////////////
// 对于 json 格式请求和应答
// 例子 : 
// command:login, ver: 1.0, fmt : bytes, ask : {name:guest, password: guest1234} // 登陆 以二进制格式返回数据
//     默认格式为JSON字符串返回数据 需要支持二进制转BASE64 GBK格式字符串转UTF8才能写入返回数据
//     如果是二进制返回 有什么数据写入什么数据
// 
// 任何时候的命令 如果带了
// service:sisdb, command:create, ask:{ key: sh600600, sdb:info, fields:{...}}
// service:sisdb, command:set,    ask:{ key: sh600600, sdb:info, data:{...}}
// service:sisdb, command:get,    ask:{ key: sh600600, sdb:info, fields:{...}}
// service:sisdb, command:sub,    ask:{ key: sh600600, sdb:info, fields:{...}}
// ---- 应答 ----
// ans : -100,               # 表示请求号已经失效 对于有连续数据的请求
// ans : -1,  msg : xxxxx    # 表示失败 msg为失败原因 
// ans :  0,                 # 表示成功 
// ans :  1,  msg : 10       # 表示成功 msg中数据为整数
// ans :  2,  msg : xxxxx    # 表示成功 msg中数据为字符串  
// ans :  3,  msg : []       # 表示成功 msg中数据为1维array  
// ans :  4,  msg : [[]]     # 表示成功 msg中数据为2维array  
// ans :  5,  msg : {}       # 表示成功 msg中数据为json 
// ans :  6,  msg : xxxx     # 表示成功 msg中数据为base64格式的二进制流
//////////////////////////////////////////////////////////////////////////////////////////
// 对于 二进制 格式请求和应答
// name: + 数据 (name为空时只有而且必须有冒号) 方便数据快速发布和传递
// fmt体现在数据区的第一个字符 除B,J,H外暂不支持其他字符
// *** 首字符为 B | 二进制格式 
// *** 首字符为 { | JSON格式 
// *** 首字符为 H ｜ HTTP格式 ..... 或者redis首字符为R
// name不压缩以 : 分隔 方便数据广播
// 一个字节表示 具备哪些字符 
// ｜ 0 1 2 3 4 5 6 7 ｜
//  0 --> 0 无扩展字段  1 表示有扩展字段
//  1 --> 0 表示请求   1 表示应答
//  10000000   -- > 是否有扩展字段 如果为 1 表示有扩展字段 后面跟1个字节表示扩展字段的个数
//  01000000   -- > 应答包标记 - 表示该数据包为应答 如果其他字段都为0表示OK
// ----------------------------  //
//    000000   -- > 二进制数据 size + data
//    000001   -- > 有 ver 字段
//    000010   -- > 有 command
//    000100   -- > 有 service
//    001000   -- > 有 ask 字段
//    010000   -- > 有 fmt 字段  // 只有请求包有该字段 要求返回的数据格式 
//    100000   -- > 有 lnk 字段  // 表示来源路径 需要一级一级返回
// ----------------------------  //
//    000000   -- > 二进制数据 size+data
//    000001   -- > 有 ver 字段
//    000010   -- > 有 ans 字段
//    000100   -- > 有 msg 字段
//    001000   -- > 有 fuc 字段
//    010000   -- > 备用
//    100000   -- > 有 lnk 字段  // 表示返回路径 需要一级一级返回

//  标准字段读完后 就需要读扩展字段 扩展字段最多255个
//  标准字段格式 直接为数据区大小+数据区
//  扩展字段格式 字段大小+字段名 数据区大小+数据区
//////////////////////////////////////////////////////////////////////////////////////////
// 对于http格式的请求和应答
// 例如 https://api.com/data/v1/api/master/getSecID.json?assetClass=E&exchangeCD=XSHE,XSHG 
// fmt = http
// service = https://api.com/data/v1/api/master/ # 这里是链接的url 路由
// command = getSecID.json  # 这里是api的方法
// ask :{ assetClass: E, exchangeCD : "XSHE,XSHG", post:{...}, headers:{...}, ...}
//       # ask 中 如果有 post字段表示 post 方式 否则以get方式获取数据
//       # ask 中 如果有 headers字段表示 请求中国要增加 headers 字典中的内容
// ---- 应答时 解析http返回数据为 JSON 应答格式----
// 处理http流程：C端拼接标准json命令 传入中间件 如果fmt是http就解析为http请求包去获取数据 
// 并解析返回数据为标准JSON应答格式数据提交给C端
//////////////////////////////////////////////////////////////////////////////////////////
// 解决代理数据传递的问题 - 类比于分布式数据传递机制
// C发送请求 O 增加一级 oname:cname:{} 后传递给 S
// S响应后 返回给 O ， O脱壳oname后 返回 :cname:{} 传递给 C
// 其中如果 C 的请求信息完全一致 O 代理层 会把返回的数据分发给相同请求的客户端
// 此时后续的C请求公用一个 oname 
// 同时这也是解决分布式重复计算的方案
// 注意对于有分包数据返回或订阅数据返回 需要在O端保持一定时间的catch



// 阻塞直到有返回 超过 wait_msec 没有数据返回就超时退出
// int sis_net_class_ask_wait(s_sis_net_class *, s_sis_object *send_, s_sis_object *recv_);

/////////////////////////////////////////////////////////////
// 协议说明
/////////////////////////////////////////////////////////////
// 1、底层协议格式 以ws为基础协议 实际数据协议 二进制和JSON混杂格式两种
// 客户端发起连接 连接认证通过后, 
// 客户端 发送 1:{ cmd:login, argv:{version : 1, compress : snappy, format: json, username : xxx, password : xxx}}
// 服务端 成功返回 1: { login : ok }
//       失败返回 1: { logout : "format 不支持."}
// 对回调没有要求的 不填来源 则包以:开头
// 所有标志符不能带 :

// 2、二进制协议格式 主要应用于快速数据交互 
// 二进制协议交互 握手后 必须交互一次结构化数据的字典表 
// 客户端凡事需要发送的二进制结构体都需要首先发送给服务端 并唯一命名 服务器亦然 以便对方能认识自己
// 客户端 发送 2: { cmd:dict, argv:{info:{fields:[[],[]]},market:{fields:[[],[]]},trade:{fields:[[],[]]}} }
// 服务端 发送 2: { cmd:dict, argv:{info:{fields:[[],[]]},market:{fields:[[],[]]},trade:{fields:[[],[]]}} }
// 之后客户端发送格式为 3:set:key:sdb:+..... 对应json的几个参数 key sdb 

// 这些交互底层处理掉 上层收到的就是字符串数据 

// 3、JSON协议格式 主要应用于 ws 协议下的web应用 
// 基于json数据格式，通过驱动来转换数据格式，从协议层来说相当于底层协议的解释层，

// 格式说明：
// id: 以冒号为ID结束符，以确定信息的一一对应关系 不能有{}():符号
// {} json 数据 -- 字符串
// [] array 数据 -- 字符串
	
	// id  -- 谁干的事结果给谁 针对异步问题数据的对应关系问题 [必选]
	// cmd -- 干什么 [必选]
	// key -- 对谁干 [可选]
	// sdb -- 对什么属性干 [可选] 
	// argv -- 怎么干... [可选]

// 请求格式例子:
// id:{"cmd":"sisdb.get","key":"sh600601","sdb":"info","argv":[11,"sssss",{"min":16}]}
	// argv参数只能为数值、字符串，[],{}，必须能够被js解析，
// id:{"cmd":"sisdb.set","key":"sh600601","sdb":"info","argv":[{"time":100,"newp":100.01}]}

// 应答格式例子:
// id:{} 通常返回信息类查询
// id:[[],[]] 单key单sdb
// id:{"000001.info":[[],[]],"000002.info":[[],[]]} 多key或多sdb 
// id:{"info.fields":[[],[]]} 返回字段信息 也可以混合在以上的返回结构


// id的意义如下：
// 	所有的消息都以字符串或数字为开始，以冒号为结束；
// 	system:为系统类消息，比如链接、登录、PING等信息；
// 	不放在{}中的原因是解析简单，快速定位转发
// 	1.作为转发中间件时，aaa.bbb.ccc的方式一级一级下去，返回数据后再一级一级上来，最后返回给发出请求的端口，
// 	  方便做分布式应用的链表；因为该值会有变化，和请求数据分开放置；只有内存拷贝没有json解析
// 	2.作为订阅和发布，当客户订阅了某一类数据，异步返回数据时可以准确的定位数据源，
// 	3.对于一应一答的阻塞请求，基本可以忽略不用；
//  4.对于客户端发出多个请求，服务端返回数据时以stock来定位请求；