#ifndef _SIS_MESSAGE_H
#define _SIS_MESSAGE_H

#include <sis_core.h>

/////////////////////////////////////////////////
//  message
/////////////////////////////////////////////////

typedef struct s_sis_message {
    void    *out;
    size_t  *olen;
    // 回调函数 如果有值, 返回数据调用该函数返回
    void(*callback)(void *,size_t);  
	
} s_sis_message;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_message *sis_message_create();
void sis_message_destroy(s_sis_message *);

#ifdef __cplusplus
}
#endif


#endif