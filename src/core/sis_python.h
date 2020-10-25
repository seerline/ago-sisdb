#ifndef _SIS_PYTHON_H
#define _SIS_PYTHON_H

#include <sis_core.h>
#include <sis_sds.h>
#include <sis_map.h>
#include <sis_list.h>
#include <sis_message.h>
#include <sis_method.h>
#include <Python.h>

#define SIS_PY_VARARGS   METH_VARARGS
#define SIS_PY_KEYWORDS  METH_KEYWORDS
#define SIS_PY_NOARGS    METH_NOARGS

#define SIS_PY_NONE        Py_None
// 引用 -1
#define SIS_PY_DECREF      Py_DECREF
// 引用 +1
#define SIS_PY_INCREF      Py_INCREF

#define s_sis_py_obj       PyObject
#define s_sis_py_method    PyMethodDef

#define sis_py_new         PyObject_Malloc
#define sis_py_free        PyObject_Free
// 直接运行python文本代码
#define sis_py_simple_string      PyRun_SimpleString
// 引入模块
#define sis_py_import_module      PyImport_ImportModule
// 获取模块字典属性
#define sis_py_get_dict           PyModule_GetDict
// 从模块中获取函数
#define sis_py_get_func_of_module PyObject_GetAttrString
// 设置函数到模块中
#define sis_py_set_func_to_module PyObject_SetAttrString
//参数类型转换，传递一个字符串。将c类型的字符串转换为python类型，元组中的python类型查看python文档
#define sis_py_get_args           Py_BuildValue
// 调用直接获得的函数，并传递参数 得到python类型的返回值
#define sis_py_run_func           PyEval_CallObject
// 从字典属性中获取函数
#define sis_py_get_func_of_dict   PyDict_GetItemString
// 将python类型的返回值转换为c/c++类型
#define sis_py_parse_args         PyArg_Parse
// 创建一个函数 准备注入python
#define sis_py_new_func           PyCFunction_New
// 向指定的字典中注入函数
#define sis_py_set_func_to_dict   PyDict_SetItemString
// 将python传递来的参数转换为c的类型
#define sis_py_parse_args_tuple   PyArg_ParseTuple

//通过字典属性获取模块中的类
#define sis_py_get_class_of_dict  PyDict_GetItemString
//实例化获取的类
#define sis_py_class_new          PyInstance_New
//调用类的方法
#define sis_py_class_call         PyObject_CallMethod

// 加载指定目录下的python插件
// 检索workpath下所有目录 每个目录代表一个插件
// 加载插件 失败就打印出来 和c插件重复也打印出来
// 插件支持通用的 s_sis_modules 接口和调用参数 按规范命名函数名称 找不到就为空
// 提供调用方法传递给 C 主程序，便于其他模块重复使用 
// 为方便python脚本编写

typedef struct s_sis_pyscript_unit
{
	s_sis_sds          workpath;
	s_sis_sds          workname;
	s_sis_sds          workcmd;
	s_sis_message     *message;
	s_sis_map_pointer *methods;
	s_sis_py_obj      *pmodule;
	s_sis_py_obj      *pdict;
	s_sis_py_obj      *pinit;
	s_sis_py_obj      *puninit;

}s_sis_pyscript_unit;

#ifdef __cplusplus
extern "C" {
#endif

void sis_py_init();
void sis_py_uninit();
bool sis_py_isinit();

// 由于python支持多线程很繁琐只能通过下面三个函数来操作
// **重要** 必须在主线程中调用 加载所有需要的py脚本
s_sis_py_obj *sis_py_mthread_create(const char *workpath_, const char *pyname_);
// **重要** 必须在主线程中调用
void sis_py_mthread_destroy(s_sis_py_obj *obj_);
// **重要** 必须在子线程中调用
s_sis_sds sis_py_mthread_exec(s_sis_py_obj *obj_, const char *func_, const char *argv_);

// 独立执行一段python代码 返回一段字符串
// 文件名 函数名 参数(json格式字符串)
// s_sis_sds sis_py_execute_sds(const char *workpath_, const char *pyname_, const char *func_, const char *argv_);

////////////////////
// s_sis_pyscript_unit
////////////////////

s_sis_pyscript_unit *sis_pyscript_unit_create(const char *workpath_, const char *workname_);
void sis_pyscript_unit_destroy(void *);

int sis_pyscript_unit_load(s_sis_map_pointer *map_, const char *workpath_);

#ifdef __cplusplus
}
#endif



#endif //_SIS_PYTHON_H