
#ifndef _STS_CORE_H
#define _STS_CORE_H

#include <sts_os.h>

// LOG(0) -- 输出内存无法申请的错误，写log后直接退出程序
// LOG(1) -- 程序初始化发生的错误，写log后正常退出程序
// LOG(2..5) -- 程序运行正常应该记录的log信息
// LOG(6..9) -- 其他不太重要的信息

// DEBUG("xxx",char *,size_t) 调试程序的时候用于输出，xxx为文件名，发布版本时DEBUG被屏蔽

#endif //_STS_CORE_H
