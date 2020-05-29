#include "sis_malloc.h"

void sis_free_call(void *p)
{
	sis_free(p);
}

void sis_sdsfree_call(void *p)
{
	sis_sdsfree((s_sis_sds)p);
}