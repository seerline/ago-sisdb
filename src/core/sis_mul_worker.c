
#include "worker.h"
#include <sis_mul_worker.h>
#include <sis_math.h>
#include <sis_ai_list.h>

void *_thread_mul_worker(void *argv_)
{
    s_sis_mul_worker_thread *context = (s_sis_mul_worker_thread *)argv_; 
    sis_wait_thread_start(context->work_thread);
    while (sis_wait_thread_noexit(context->work_thread))
    {
        if (!sis_wait_thread_iswork(context->work_thread))
        {
            break;
        }
        while(context->tasks->count > 0)
        {
            sis_mutex_lock(&context->wlock);
            s_sis_mul_worker_task *task = sis_pointer_list_get(context->tasks, 0);
            sis_mutex_unlock(&context->wlock);

            task->method(task->context, task->argv);
            if (task->vfree)
            {
                task->vfree(task->argv);
            }
            sis_mutex_lock(&context->wlock);
            sis_pointer_list_delete(context->tasks, 0, 1);
            sis_mutex_unlock(&context->wlock);
        }
        sis_wait_thread_wait(context->work_thread, 1000);
    }
    sis_wait_thread_stop(context->work_thread);
    return NULL;
}

s_sis_mul_worker_thread *sis_mul_worker_thread_create()
{
    s_sis_mul_worker_thread *o = SIS_MALLOC(s_sis_mul_worker_thread, o);
    o->tasks = sis_pointer_list_create();
    o->tasks->vfree = sis_free_call;
    o->work_thread = sis_wait_thread_create(1);
    if (!sis_wait_thread_open(o->work_thread, _thread_mul_worker, o))
    {
        sis_wait_thread_destroy(o->work_thread);
        sis_free(o);
        return NULL;
    }
    sis_mutex_init(&o->wlock, NULL);
    return o;
}
void sis_mul_worker_thread_destroy(s_sis_mul_worker_thread *cxt)
{
    // sis_wait_thread_close(cxt->work_thread);
    sis_wait_thread_destroy(cxt->work_thread);
    sis_pointer_list_destroy(cxt->tasks);
    sis_free(cxt);
}
void sis_mul_worker_thread_add_task(s_sis_mul_worker_thread *cxt, sis_method_define *method, void *context,void *argv, void *vfree)
{
    s_sis_mul_worker_task *task = SIS_MALLOC(s_sis_mul_worker_task, task);
    task->context = context;
    task->argv = argv;
    task->method = method;
    task->vfree = vfree;
    sis_mutex_lock(&cxt->wlock);
    sis_pointer_list_push(cxt->tasks, task);
    sis_mutex_unlock(&cxt->wlock);
    sis_wait_thread_notice(cxt->work_thread);
}

s_sis_mul_worker_cxt *sis_mul_worker_cxt_create(int nums)
{
    s_sis_mul_worker_cxt *o = SIS_MALLOC(s_sis_mul_worker_cxt, o);
    o->wnums = sis_between(nums, 1, 64);
    o->wthreads = sis_malloc(sizeof(void *) * o->wnums);
    memset(o->wthreads, 0, sizeof(void *) * o->wnums);
    for (int i = 0; i < o->wnums; i++)
    {
        o->wthreads[i] = sis_mul_worker_thread_create();
    }
    return o;
}

void sis_mul_worker_cxt_destroy(s_sis_mul_worker_cxt *cxt)
{
    for (int i = 0; i < cxt->wnums; i++)
    {
        if (cxt->wthreads[i])
        {
            sis_mul_worker_thread_destroy(cxt->wthreads[i]);
            cxt->wthreads[i] = NULL;
        }
    }
    sis_free(cxt->wthreads);
    sis_free(cxt);
}
void sis_mul_worker_cxt_add_task(s_sis_mul_worker_cxt *cxt, const char *hashstr,sis_method_define *method, void *context,void *argv,void *vfree)
{
    int idx = sis_ai_get_hash(hashstr, sis_strlen(hashstr), cxt->wnums);
    // printf("idx = %d %s %d %d\n", idx, hashstr, sis_strlen(hashstr), cxt->wnums);
    if (cxt->wthreads[idx])
    {
        sis_mul_worker_thread_add_task(cxt->wthreads[idx], method, context, argv, vfree);
    }
    else
    {
        LOG(5)("add task fail, %d\n", idx);
    }
}
void sis_mul_worker_cxt_add_task_i(s_sis_mul_worker_cxt *cxt, int tidx,
    sis_method_define *method, void *context,void *argv, void *vfree)
{
    int idx = sis_between(tidx, 0, cxt->wnums - 1);
    if (cxt->wthreads[idx])
    {
        sis_mul_worker_thread_add_task(cxt->wthreads[idx], method, context, argv, vfree);
    }
    else
    {
        LOG(5)("add task fail, %d\n", idx);
    }
}

void sis_mul_worker_cxt_wait(s_sis_mul_worker_cxt *cxt)
{
    int isstop = 0;
    while(!isstop)
    {
        isstop = 1;
        for (int i = 0; i < cxt->wnums; i++)
        {
            if (cxt->wthreads[i])
            {
                if (cxt->wthreads[i]->tasks->count > 0)
                {
                    isstop = 0;
                    break;
                }
            }
        }
        sis_sleep(300);
    }
}

#if 0
int working(void *context, void *argv)
{
    printf("working start %x %s\n", sis_thread_handle(sis_thread_self()), (char *)argv);
    sis_sleep(3000);
    printf("working stop %s\n", (char *)argv);
    return 0;
}
int main()
{
    char info[128];
    s_sis_mul_worker_cxt *cxt = sis_mul_worker_cxt_create(32);
    for (int i = 0; i < 100; i++)
    {   
        sis_sprintf(info, 128, "%d%d%d", i, (i+1)*16, (i+1)*32);
        char *mess = sis_malloc(256);
        sis_sprintf(mess, 255, "%s is %d", info, i);
        sis_mul_worker_cxt_add_task(cxt, info, working, NULL, mess, sis_free_call);
    }
    while(1)
    {
        sis_sleep(1000);
    }

    return 0;
}

#endif