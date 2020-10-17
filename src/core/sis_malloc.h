
#ifndef _SIS_MALLOC_H
#define _SIS_MALLOC_H

#include <sis_os.h>
#include <os_malloc.h>

#include <sis_sds.h>
#include <sis_dict.h>

#define SIS_NEW
#define SIS_NOTUSED(V) ((void) V)

typedef void sis_free_define(void *);

#ifdef __cplusplus
extern "C" {
#endif

void sis_free_call(void *);

void sis_sdsfree_call(void *);

#ifdef __cplusplus
}
#endif

#endif //_SIS_MALLOC_H
