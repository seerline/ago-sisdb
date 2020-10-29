

// #define MAKE_LIST_LOCK
// #define MAKE_LIST_UNLOCK
#define MAKE_LIST_FAST

#ifdef MAKE_LIST_UNLOCK
#include "sis_list.unlock.h"
#endif

#ifdef MAKE_LIST_LOCK
#include "sis_list.lock.h"
#endif

#ifdef MAKE_LIST_FAST
#include "sis_list.fast.h"
#endif
