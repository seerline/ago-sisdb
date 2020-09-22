
# 方法命名没有限制 只需使用 sis_py_method_add 注册就可以被其他服务调用
def cmd_method_get(source, message):
    print("py: cmd_method_get", source, message)
        

def cmd_method_set(source, message):
    print("py: cmd_method_set", source, message)

# ---------  以下函数命名必须严格按照 sis_xxxxxx_init ----------#
# 初始化接口 所有初始化工作这里处理 config 为一个json格式字符串 内含配置该工作的配置
# 返回 0 表示初始化失败 1 表示初始化成功 
# 需要在这里注册所有能够被系统调用的api
def sis_demo_method_init(source, config):    
    print("py: sis_demo_method_init", source, config)
    return 1

def sis_demo_method_uninit(source):    
    print("py: sis_demo_method_uninit", source)

# 以下两个函数是在提供方法调用前 和 停止方法调用后调用
def sis_demo_method_method_init(source):    
    print("py: sis_demo_method_method_init", source)
    sis_py_method_add(source, "get", "cmd_method_get")
    sis_py_method_add(source, "set", "cmd_method_set")

def sis_demo_method_method_uninit(source):    
    print("py: sis_demo_method_method_uninit", source)
    sis_py_method_del(source, "get")
    sis_py_method_del(source, "set")

