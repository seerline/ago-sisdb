#ifndef _WORKER_H
#define _WORKER_H

#include <sts_core.h>
#include <sts_json.h>
#include <sts_net.h>
#include <sts_list.h>

#include <os_thread.h>

typedef struct s_sts_worker {
	int    status;         //当前的工作状态
    int    work_mode;

    char   workname[STS_NAME_LEN];
    char   classname[STS_NAME_LEN];

    int                 work_gaps;
    s_sts_wait          wait;
    s_sts_struct_list  *work_plans; 
    s_sts_thread_id_t   thread_id;

	s_sts_url    in_url;
	s_sts_url    out_url;

	bool(*init)(struct s_sts_worker *, s_sts_json_node *);

	void(*open)(struct s_sts_worker *);
	void(*close)(struct s_sts_worker *);

    void(*working)(struct s_sts_worker *);
	
} s_sts_worker;

s_sts_worker *sts_worker_create(s_sts_json_node *);
void sts_worker_destroy(s_sts_worker *);

#endif