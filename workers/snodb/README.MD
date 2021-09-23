
## 标准低延迟行情架构

要求: 速度第一 压缩第二 原始数据流式保存 

### 简述

   > 顶级服务 所有数据源解析后直接写入 memdb 并提供对外网络服务 memdb 保存 set 信息 并在启动后加载后启动

   > 次级服务 - sno数据落盘 从 memdb 收到数据后 按 sno 压缩后实时写盘

   > 次级服务 - incr数据缓存 从 memdb 收到数据后 按 incr 压缩后保留在内存 以供应 3 级服务定制化获取数据 

   > api接口 - 链接缓存次级服务器 如果是历史数据请求就读盘 如果是实时数据请求就组合 incr 后下发数据

说明:
   1、为了满足速度优先 顶级服务仅仅做了数据格式转换后就广播出去 未做压缩 因此如果内网带宽资源有限 建议顶级和次级服务放在一台主机上 避免网络数据拥堵
   2、支持多行情源数据写入memdb的分离和合并方式，当有多行情源数据写入时，同种数据类型只会取默认

### 程序架构

```plantuml

state forkstatus <<fork>>
state joinstatus <<join>>
state stopstatus <<end>>

datasource1 --> memdb_server
datasource2 --> memdb_server
datasource... --> memdb_server
memdb_server --> forkstatus : top source
forkstatus --> snodb
forkstatus --> incrdb
forkstatus --> sisdb
forkstatus --> ...
snodb --> joinstatus
incrdb --> joinstatus
sisdb --> joinstatus
... --> joinstatus
joinstatus --> api_client
api_client --> stopstatus

```

