#ifndef _SIS_OBJECT_H
#define _SIS_OBJECT_H

#define SIS_OBJECT_MAX_REFS MAX_INT

// 定义一个网络数据的结构，减少内存拷贝，方便数据传递，增加程序阅读性
#pragma pack(push,1)

#define SIS_OBJECT_SDS   0  // 字符串
#define SIS_OBJECT_LIST  1  // sis_struct_list 结构体的指针
#define SIS_OBJECT_NODE  2  // sis_socket_node 专用于网络传输的链表

typedef struct s_sis_object {
    unsigned char style;
    unsigned int  refs;  // 引用数 一般为1, 最大嵌套不超过 MAX_INT 超过就会出错 
    void *ptr;
} s_sis_object;

#pragma pack(pop)

#endif
