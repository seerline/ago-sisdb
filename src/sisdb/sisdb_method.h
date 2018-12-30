
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_METHOD_H
#define _SISDB_METHOD_H

#include "sis_core.h"
#include "sis_map.h"
#include "sis_method.h"

#include "sisdb_collect.h" 

#define SISDB_METHOD_STYLE_WRITE      "write"
#define SISDB_METHOD_STYLE_SUBSCRIBE  "subscribe"

#define SISDB_CALL_STYLE_SYSTEM      "system"  // 系统自带的调用
#define SISDB_CALL_STYLE_LOCAL       "local"   // 本地装载的调用
#define SISDB_CALL_STYLE_REMOTE      "remote"  // 远程过程调用


s_sis_map_pointer *sisdb_method_define_create();
void sisdb_method_define_destroy(s_sis_map_pointer *map_);  // sis_method_map_destroy

////////////////////////////////
//  下面是功能调用
////////////////////////////////

void *sisdb_method_write_incr(void *obj_, void *node_);

void *sisdb_method_write_nonzero(void *obj_, void *node_);

void *sisdb_method_subscribe_once(void *obj_, void *node_);

void *sisdb_method_subscribe_min(void *obj_, void *node_);

void *sisdb_method_subscribe_max(void *obj_, void *node_);

void *sisdb_method_subscribe_gap(void *obj_, void *node_);

#endif  /* _SIS_CALL_H */
