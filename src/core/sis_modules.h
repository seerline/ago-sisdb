#ifndef _MFM_CORE_H
#define _MFM_CORE_H

#include "sis_core.h"
#include "sis_method.h"

typedef struct s_sis_modules
{
  // char *classname;
  // 读取配置，申请内存，失败就跳出，
  bool(*init)(void *, void *); 

  // 线程工作前的准备工作 打开socket，设置工作模式，可能耗时长
	void(*work_init)(void *);  

  // 线程工作主体 为空表示不循环 
  void(*working)(void *);       
  
  // 结束工作线程前的准备工作 关闭socket，清除和通知其他依赖项，可能耗时长
	void(*work_uninit)(void *); 
  
  // 释放内存
  void(*uninit)(void *); 

  // 提供调用方法前的工作
	void(*method_init)(void *);  
  // 结束调用方法后的释放
	void(*method_uninit)(void *);  

  // 方法数量
  int mnums; 

  // 方法数组
  s_sis_method *methods;

}s_sis_modules;


#endif