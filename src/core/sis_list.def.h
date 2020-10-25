

#define MAKE_UNLOCK_LIST

#ifdef MAKE_UNLOCK_LIST
#define _SIS_LIST_LOCK_H
#include "sis_list.unlock.h"
#else
#define _SIS_LIST_UNLOCK_H
#include "sis_list.lock.h"
#endif

