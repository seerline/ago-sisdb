
def sis_demo_service_init(source, config):    
    print("py: sis_api_web_init", source, config)
    #  do somthing...
    return 1

def sis_demo_service_uninit(source):    
    print("py: sis_api_web_uninit", source)


# 以下3个函数会根据配置定时调用 当该组件为微服务的时候起作用
def sis_demo_service_work_init(source):    
    print("py: sis_demo_service_work_init", source)

def sis_demo_service_working(source):    
    print("py: sis_demo_service_working", source)
    #  do somthing...

def sis_demo_service_work_uninit(source):    
    print("py: sis_demo_service_work_uninit", source)

