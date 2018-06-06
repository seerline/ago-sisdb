
#ifndef _STS_LOG_H
#define _STS_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <os_types.h>

// LOG(0) -- 输出内存无法申请的错误，写log后直接退出程序
// LOG(1) -- 程序初始化发生的错误，写log后正常退出程序
// LOG(2..5) -- 程序运行正常应该记录的log信息
// LOG(6..9) -- 其他不太重要的信息

// DEBUG("xxx",char *,size_t) 调试程序的时候用于输出，xxx为文件名，发布版本时DEBUG被屏蔽

// printf( "This is line %d.\n", __LINE__ );
// printf( "This function is %s.\n", __func__ );

#define sts_out_error(a) if(a>10) printf
// sts_out_error 需要判断如果有回车就自动中断 使用("%.*s", 10, "xxxx") 方式

inline void sts_out_binary(const char *key_, const char *val_, int len_)
{
    printf("%s : ", key_);
    char *p = (char *)val_;
    len_ = len_ >30 ? 30 : len_;
    for (int i = 0; i < len_; i++)
    {
        // if (!*p) break;
        printf("%02x ", (unsigned char)*p);
        p++;
    }
    printf("\n");
}

void sts_log_close();
bool sts_log_open(const char *log_);
char *sts_log(int level_, const char *fmt_, ...);

#endif //_STS_LOG_H