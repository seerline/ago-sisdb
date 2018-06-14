
#include <worker.h>
#include <work_east2day.h>
#include <work_zh2cw.h>
#include <market_shmd.h>
#include <market_szdb.h>

s_sts_worker *sts_worker_create(s_sts_json_node *node_)
{
    s_sts_worker *worker = (s_sts_worker *)sts_malloc(sizeof(s_sts_worker));
    memset(worker, 0, sizeof(s_sts_worker));

    const char *classname = sts_json_get_str(node_, "classname");
    sts_strcpy(worker->workname, STS_NAME_LEN, node_->key);
    sts_strcpy(worker->classname, STS_NAME_LEN, classname);

    if (!sts_strcasecmp(classname, "market-shmd"))
    {
        printf("run %s\n",classname);
    }
    else if (!sts_strcasecmp(classname, "market-szhq"))
    {
        printf("run %s\n",classname);
    }
    else if (!sts_strcasecmp(classname, "zh2cw"))
    {
        // worker->init = 
        // worker->open = 
        // worker->close = 
        // worker->working = 
        printf("run %s\n",classname);
    }
    else if (!sts_strcasecmp(classname, "east2day"))
    {
        // worker->init = 
        // worker->open = 
        // worker->close = 
        // worker->working = 
        printf("run %s\n",classname);
    }
    else
    {
        printf("error %s\n",classname);
        sts_free(worker);
        return NULL;
    }
    worker->work_plans = sts_struct_list_create(sizeof(uint16), NULL, 0);

    return worker;
}

void sts_worker_destroy(s_sts_worker *worker_)
{
    sts_struct_list_destroy(worker_->work_plans);
    sts_free(worker_);
}
