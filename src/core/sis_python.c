
#include <sis_python.h>
#include <sis_file.h>

static int __init_pyscripe = 0;

void sis_py_init()
{
	if (!__init_pyscripe)
	{
		Py_Initialize();
		__init_pyscripe = 1;
	}
}
void sis_py_uninit()
{
	if (__init_pyscripe)
	{
		Py_Finalize();
		__init_pyscripe = 0;
	}
}
bool sis_py_isinit()
{
	return (__init_pyscripe == 1);
}
s_sis_sds sis_py_execute_sds(const char *workpath_, const char *pyname_, const char *func_, const char *argv_)
{
	s_sis_sds o = NULL;
	// sis_py_init();
	sis_py_simple_string("import sys");
	char workpath[255];
	sis_sprintf(workpath, 255, "sys.path.append('%s')", workpath_);
	sis_py_simple_string(workpath);

	s_sis_py_obj *pmodule = sis_py_import_module(pyname_);
	if (!pmodule) goto fail;
	s_sis_py_obj *pdict = sis_py_get_dict(pmodule);
	if (!pdict) goto fail;
	s_sis_py_obj *pfunc = sis_py_get_func_of_dict(pdict, func_);
	if (!pfunc) goto fail;

	s_sis_py_obj *pargs = sis_py_get_args("(s)", argv_); 
	// s_sis_py_obj *pargs = sis_py_get_args("(i)", 1000); 
	s_sis_py_obj *preply = sis_py_run_func(pfunc, pargs);

    char  *reply;
    sis_py_parse_args(preply, "s", &reply);
	o = sis_sdsnew(reply);

fail:
	// sis_py_uninit();
	// 删除中间目录
	sis_sprintf(workpath, 255, "%s/__pycache__/", workpath_);
	sis_path_del_files(workpath);
	return o;
}
////////////////////
// s_sis_py_method
////////////////////
// 向主程序注册一个方法
// source cmd_mapname cmd_pyname
static s_sis_py_obj* _sis_py_method_add(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    char  *cmd;
    char  *cmdname;
 	uint64 source = 0;
	sis_py_parse_args_tuple(args_, "Kss", &source, &cmd, &cmdname);
	s_sis_pyscript_unit *unit = (s_sis_pyscript_unit *)source;

	
	printf("c: _sis_py_method_add %s %s %d %s\n", cmd, cmdname, (int)source, unit->workpath);
    
	
	SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;
}
// 向主程序删除一个方法
static s_sis_py_obj* _sis_py_method_del(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;
}
// 发现主程序中提供的方法 classname + '.' + methodname
static s_sis_py_obj* _sis_py_method_find(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    // char *ptr;
    // char str[50];
    // size_t len;
    // int size = 0;
    // PyArg_ParseTuple(args, "s#i", &ptr, &len ,&size);
    // strcpy(str, ptr); // 实际数据还在py那里，这里只是把指针指向过去
    // // PyArg_ParseTuple(args, "s#", &str, &len);
    // printf("c:_call_ding called [%s] [%s] %d %d %s\n", ptr, str, len, size, "000");
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;

}

// 调用主程序中提供的方法 classname + '.' + methodname 方法必须已注册并启动
static s_sis_py_obj* _sis_py_method_call(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;
}

static s_sis_py_obj* _sis_py_message_new(s_sis_py_obj *self_, s_sis_py_obj *args_) 
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;
}   
static s_sis_py_obj* _sis_py_message_del(s_sis_py_obj *self_, s_sis_py_obj *args_)    
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_get(s_sis_py_obj *self_, s_sis_py_obj *args_)    
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_set(s_sis_py_obj *self_, s_sis_py_obj *args_)    
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_get_cb(s_sis_py_obj *self_, s_sis_py_obj *args_) 
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_set_cb(s_sis_py_obj *self_, s_sis_py_obj *args_) 
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_get_int(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_set_int(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_get_str(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
static s_sis_py_obj* _sis_py_message_set_str(s_sis_py_obj *self_, s_sis_py_obj *args_)
{
    SIS_PY_INCREF(SIS_PY_NONE);
    return SIS_PY_NONE;	
} 
// 提供给python的方法 python 获取数据可能需要用到主程序的提供的功能
// 所有的方法参数 都是 json 的数据字符串
// 无论是返回数据 还是传入数据都以json格式为中间格式

static s_sis_py_method __merge_methods[] = 
{
    {"sis_py_method_add",      _sis_py_method_add,       SIS_PY_VARARGS, "register own method to server."},
    {"sis_py_method_del",      _sis_py_method_del,       SIS_PY_VARARGS, "delete own method from server."},
    {"sis_py_method_find",     _sis_py_method_find,      SIS_PY_VARARGS, "find method from server."},
    {"sis_py_method_call",     _sis_py_method_call,      SIS_PY_VARARGS, "call method from server."},
    {"sis_py_message_new",     _sis_py_message_new,      SIS_PY_VARARGS, ""},
    {"sis_py_message_del",     _sis_py_message_del,      SIS_PY_VARARGS, ""},
    {"sis_py_message_get",     _sis_py_message_get,      SIS_PY_VARARGS, ""},
    {"sis_py_message_set",     _sis_py_message_set,      SIS_PY_VARARGS, ""},
    {"sis_py_message_get_cb",  _sis_py_message_get_cb,   SIS_PY_VARARGS, ""},
    {"sis_py_message_set_cb",  _sis_py_message_set_cb,   SIS_PY_VARARGS, ""},
    {"sis_py_message_get_int", _sis_py_message_get_int,  SIS_PY_VARARGS, ""},
    {"sis_py_message_set_int", _sis_py_message_set_int,  SIS_PY_VARARGS, ""},
    {"sis_py_message_get_str", _sis_py_message_get_str,  SIS_PY_VARARGS, ""},
    {"sis_py_message_set_str", _sis_py_message_set_str,  SIS_PY_VARARGS, ""},
    { NULL },
};

////////////////////
// s_sis_pyscript_unit
////////////////////

s_sis_pyscript_unit *sis_pyscript_unit_create(const char *workpath_, const char *workname_)
{
	s_sis_pyscript_unit *o = SIS_MALLOC(s_sis_pyscript_unit, o);
	o->workpath = sis_sdsnew(workpath_);
	o->workname = sis_sdsnew(workname_);
	o->workcmd = sis_sdsempty();
	o->workcmd = sis_sdscatfmt(o->workcmd, "sys.path.append('%s%s')", workpath_, workname_);

	// sis_py_init();
	sis_py_simple_string("import sys");
	sis_py_simple_string(o->workcmd);

	char str[255];
	o->pmodule = sis_py_import_module(workname_);
	if (!o->pmodule) goto fail;
	o->pdict = sis_py_get_dict(o->pmodule);
	if (!o->pdict) goto fail;
	sis_sprintf(str, 255, "sis_%s_init", workname_);
	o->pinit = sis_py_get_func_of_dict(o->pdict, str);
	if (!o->pinit) goto fail;
	sis_sprintf(str, 255, "sis_%s_uninit", workname_);
	o->puninit = sis_py_get_func_of_dict(o->pdict, str);
	if (!o->puninit) goto fail;

	s_sis_py_method *cur;
	for (cur = __merge_methods; cur->ml_name != NULL; cur++) 
	{
		s_sis_py_obj *pfunc = sis_py_new_func(cur, NULL);
		sis_py_set_func_to_dict(o->pdict, cur->ml_name, pfunc);
		SIS_PY_DECREF(pfunc);
	}

	SIS_PY_INCREF(o->pmodule);
	SIS_PY_INCREF(o->pdict);
	SIS_PY_INCREF(o->pinit);
	SIS_PY_INCREF(o->puninit);
	// s_sis_py_obj *init_args = sis_py_get_args("(K,s)", (uint64)o, "{}"); // L->int64 K->uint64
	// s_sis_py_obj *init_result = sis_py_run_func(pinit, init_args);
	// // 加载所有的方法
    // int ok = 0;
    // sis_py_parse_args(init_result, "i", &ok);
	// printf("c: %d %p, %p %p\n", ok, o, init_args, init_result);

	LOG(5)("install script %s ok.\n", workname_);
	return o;	
fail:
	LOG(5)("install script %s fail.\n", workname_);
	sis_pyscript_unit_destroy(o);
	return NULL;	
}
void sis_pyscript_unit_destroy(void *unit_)
{
	s_sis_pyscript_unit *unit = (s_sis_pyscript_unit *)unit_;
	sis_sdsfree(unit->workpath);
	sis_sdsfree(unit->workname);
	sis_sdsfree(unit->workcmd);
	// 删除所有的方法

	SIS_PY_DECREF(unit->pmodule);
	SIS_PY_DECREF(unit->pdict);
	SIS_PY_DECREF(unit->pinit);
	SIS_PY_DECREF(unit->puninit);
	// sis_py_uninit();
	sis_free(unit);
}
////////////////////
// s_sis_pyscript_cxt
////////////////////
int sis_pyscript_unit_load(s_sis_map_pointer *map_, const char *workpath_)
{
	if(!sis_path_exists(workpath_))
	{
		return 0;
	}
	sis_file_fixpath((char *)workpath_);
	char *dirlist = sis_path_get_files(workpath_, SIS_FINDPATH);
	int count = sis_str_substr_nums(dirlist, ':');
	char workname[64];
	for (int i = 0; i < count; i++)
	{
		sis_str_substr(workname, 64, dirlist, ':', i);
		s_sis_pyscript_unit *unit = sis_pyscript_unit_create(workpath_, workname);
		// printf("%s\n", workname ? workname : "nil");
		if (unit)
		{
			sis_map_pointer_set(map_, workname, unit);
		}
	}
	sis_free(dirlist);

	return sis_map_pointer_getsize(map_);
}

#if 0
void *thread_pop(void *arg)
{
	int count = 30000;
    s_sis_map_pointer *map = (s_sis_map_pointer *)arg;

    while(count)
    {
		s_sis_pyscript_unit *unit = (s_sis_pyscript_unit *)sis_map_pointer_get(map, "demo_method");
		if (unit)
		{
			s_sis_py_obj *init_args = sis_py_get_args("(K,s)", (uint64)unit, "{}"); // L->int64 K->uint64
			s_sis_py_obj *init_result = sis_py_run_func(unit->pinit, init_args);
			// 加载所有的方法
			int ok = 0;
			sis_py_parse_args(init_result, "i", &ok);
			printf("c: [%d] %d %p, %p %p\n", count, ok, unit, init_args, init_result);
		}
		count--;
        sis_sleep(1);
    }
    return NULL;
}

int main()
{
	sis_py_init();
	s_sis_map_pointer *map = sis_map_pointer_create_v(sis_pyscript_unit_destroy);
	int count = sis_pyscript_unit_load(map, "../pyscript/");

	pthread_t read;
	pthread_create(&read, NULL, thread_pop, map);

	pthread_join(read, NULL);

	sis_map_pointer_destroy(map);
	sis_py_uninit();
	return 0;
}
int main1()
{
	s_sis_sds str = sis_py_execute_sds(
		"../pyscript/",
		"api_web",
		"web_api_get",
		"{\"url\":\"https://api.wmcloud.com/data/v1/api/master/getSecID.json?assetClass=E&exchangeCD=XSHE,XSHG\",\"headers\":{\"Authorization\":\"Bearer c03ed0c0ce9e78106ae10d87f627696477d85bd4a2069312f8c898522c32f3d8\"}}");
	printf("%30s\n", str);
	sis_sdsfree(str);
	return 0;
}
#endif