

// #define MAKE_LOCK_LIST
#define MAKE_UNLOCK_LIST
// #define MAKE_OK_LIST

#ifdef MAKE_UNLOCK_LIST
#define _SIS_LIST_LOCK_H
#include "sis_list.unlock.h"
#endif

#ifdef MAKE_LOCK_LIST
#define _SIS_LIST_UNLOCK_H
#include "sis_list.lock.h"
#endif

#ifdef MAKE_OK_LIST
#define _SIS_LIST_OK_H
#include "sis_list.lock.h"
#endif
