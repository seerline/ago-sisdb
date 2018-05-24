sts  stock time series

变量定义规范:

__xxxx 开头表示全局变量
aaa_xxxx_b  aaa表示类别 b表示数据类型

函数定义规范：

_xxxxx 表示仅在当前c中作用的函数
aaa_xxxx aaa表示类别，作用于整个工程
aaa_xxxx_m 表示该函数返回值需要手动释放，释放方法根据返回类型来确定
## 不再返回_m,凡是需要返回新内存的，必须使用会话内存池的方式申请内存，在会话结束后，释放内存

申请内存方法：
zmalloc z开头表示基础malloc函数集合
pmalloc p开头表示内存池，可重复利用，申请的内存大小不定， 对应 pfree只是把内存还到链表中

**xmalloc x开头表示自动内存回收，执行某段代码前，执行create_auto_memory,结束后执行destroy_auto_memory,中间不用释放内存 
