#ifndef _SIS_COMMAND_H
#define _SIS_COMMAND_H

#include <sis_core.h>
#include <sis_map.h>

/////////////////////////////////////////////////
//  command
/////////////////////////////////////////////////
typedef void *_sis_method_define(void *, void *);

typedef struct s_sis_method
{
    const char *command;                  // 方法的名字
    void *(*proc)(void *cls, void *argv); // 方法的调用实体 操作对象 参数
    // 下面两项用户填空
    const char *style;                    // 方法属于的类别，相当于命名空间 subscribe append zip 等
    const char *explain;                  // 方法的说明
} s_sis_method;

// 方法的映射表, 通过映射表快速调用对应函数 s_sis_method
#define s_sis_methods s_sis_map_pointer

#ifdef __cplusplus
extern "C"
{
#endif
    s_sis_methods *sis_methods_create(s_sis_method *methods_, int count_);
    void sis_methods_destroy(s_sis_methods *);

    s_sis_method *sis_methods_get(s_sis_methods *map_, const char *name_, const char *style_);

#ifdef __cplusplus
}
#endif

#endif