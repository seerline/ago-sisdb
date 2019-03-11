
#include "siscs_io.h"
#include "siscs.h"
#include "sis_thread.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_siscs_server server = {
    .status = SIS_SERVER_STATUS_NOINIT,
    .log_level = 5,
    .log_size = 5};
/********************************/

int siscs_open(const char *conf_)
{

    s_sis_conf_handle *config = sis_conf_open(conf_);
    if (!config)
    {
        sis_out_log(1)("load conf %s fail.\n", conf_);
        return 2;
    }

    char conf_path[SIS_PATH_LEN];
    sis_file_getpath(conf_, conf_path, SIS_PATH_LEN);

    s_sis_json_node *lognode = sis_json_cmp_child_node(config->node, "log");
    if (lognode)
    {
        sis_get_fixed_path(conf_path, sis_json_get_str(lognode, "path"),
                           server.log_path, SIS_PATH_LEN);
        server.log_level = sis_json_get_int(lognode, "level", 5);
        server.log_size = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
    }

    server.services = sis_struct_list_create(sizeof(s_sis_url), NULL, 0);
    s_sis_json_node *service = sis_json_cmp_child_node(config->node, "services");
    if (service)
    {
        s_siscs_service url;
        s_sis_json_node *node = sis_json_first_node(service);
        while (node)
        {
            memset(&url ,0, sizeof(s_siscs_service));
            sis_strcpy(url.protocol, 128, node->key);
            url.port = sis_json_get_int(node, "port", 7329);
            sis_struct_list_push(server.services, &url);
            node = sis_json_next_node(node);
        }
    }
    if (server.services->count < 1)
    {
        goto error;
    }

    for(int i = 0; i < server.services->count; i++)
    {
        s_siscs_service *url = sis_struct_list_get(server.services, i);
        //****** 临时处理，后期这里需要完善 ******//
        siscs_service_start(url);
    }
    
    server.status = SIS_SERVER_STATUS_INITED;

    sis_conf_close(config);

    return 0;

error:
    sis_conf_close(config);
    return 1;
}

void siscs_close()
{
    if (server.status == SIS_SERVER_STATUS_CLOSE)
    {
        return;
    }
    server.status = SIS_SERVER_STATUS_CLOSE;

    for(int i = 0; i < server.services->count; i++)
    {
        s_siscs_service *url = sis_struct_list_get(server.services, i);
        siscs_service_stop(url);
    }    
    sis_struct_list_destroy(server.services);
}

void *siscs_get_server()
{
    return &server;
}
