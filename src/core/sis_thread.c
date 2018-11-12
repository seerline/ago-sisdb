
#include "sis_thread.h"

bool sis_plan_task_working(s_sis_plan_task *task_)
{
	return task_->working;
}

bool sis_plan_task_execute(s_sis_plan_task *task_)
{
	// ??
	if (task_->work_mode == SIS_WORK_MODE_GAPS)
	{
		if (sis_thread_wait_sleep(&task_->wait, task_->work_gap.delay) == SIS_ETIMEDOUT)
		{
			if (task_->work_gap.start == task_->work_gap.stop)
			{
				return true;
			}
			else
			{
				int min = sis_time_get_iminute(0);
				if ((task_->work_gap.start < task_->work_gap.stop && min > task_->work_gap.start && min < task_->work_gap.stop) ||
					(task_->work_gap.start > task_->work_gap.stop && (min > task_->work_gap.start || min < task_->work_gap.stop)))
				{
					return true;
				}
			}
		}
	}
	else
	{
		if (sis_thread_wait_sleep(&task_->wait, 30) == SIS_ETIMEDOUT) // 30?????
		{
			int min = sis_time_get_iminute(0);
			// printf("save plan ... -- -- -- %d \n", min);
			for (int k = 0; k < task_->work_plans->count; k++)
			{
				uint16 *lm = sis_struct_list_get(task_->work_plans, k);
				if (min == *lm)
				{
					return true;
				}
			}
		}
	}
	return false;
}
s_sis_plan_task *sis_plan_task_create()
{
	s_sis_plan_task *task = sis_malloc(sizeof(s_sis_plan_task));
	memset(task, 0, sizeof(s_sis_plan_task));

	task->work_plans = sis_struct_list_create(sizeof(uint16), NULL, 0);

	sis_thread_wait_create(&task->wait);

	sis_mutex_create(&task->mutex);

	sis_thread_wait_start(&task->wait);

	return task;
}
bool sis_plan_task_start(s_sis_plan_task *task_, SIS_THREAD_START_ROUTINE func_, void *val_)
{
	if (!sis_thread_create(func_, val_, &task_->work_pid))
	{
		// sis_out_log(1)("can't link task thread.\n");
		return false;
	}
	task_->working = true;
	return true;
}
void sis_plan_task_destroy(s_sis_plan_task *task_)
{
	task_->working = false;

	sis_thread_wait_kill(&task_->wait);
	sis_sleep(300); // ?????????
	if (task_->work_pid)
	{
		sis_thread_join(task_->work_pid);
		sis_out_log(5)("plan_task end.\n");
	}
	sis_thread_wait_stop(&task_->wait);

	sis_mutex_destroy(&task_->mutex);

	sis_thread_wait_destroy(&task_->wait);

	sis_struct_list_destroy(task_->work_plans);

	sis_free(task_);
}

#if 0
#include <signal.h>
#include <stdio.h>

void *_plan_task_example(void *argv_)
{
	s_sis_plan_task *task = (s_sis_plan_task *)argv_;

	while (sis_plan_task_working(task))
	{
		if (sis_plan_task_execute(task))
		{
			printf(" run ... \n");
		}
	}
	printf("end . \n");

	return NULL;
}

int __exit = 0;
s_sis_plan_task *__plan_task;

void exithandle(int sig)
{
	printf("exit .1. \n");
	sis_plan_task_destroy(__plan_task);
	printf("exit .ok . \n");
	__exit = 1;
}

int main()
{

	__plan_task = sis_plan_task_create();
	__plan_task->work_mode = SIS_WORK_MODE_GAPS;
	__plan_task->work_gap.start = 0;
	__plan_task->work_gap.stop = 2359;
	__plan_task->work_gap.delay = 5;

	sis_plan_task_start(__plan_task, _plan_task_example, __plan_task);

	signal(SIGINT, exithandle);

	printf("working ... \n");
	while (!__exit)
	{
		sis_sleep(300);
	}
}
#endif
