
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


s_sis_map_pointer *sisdb_method_define_create();
void sisdb_method_define_destroy(s_sis_map_pointer *map_);  // sis_method_map_destroy

////////////////////////////////
//  下面是功能调用
////////////////////////////////

void *sisdb_method_write_incr(void *obj_, s_sis_json_node *node_);

void *sisdb_method_write_nonzero(void *obj_, s_sis_json_node *node_);

void *sisdb_method_subscribe_once(void *obj_, s_sis_json_node *node_);

void *sisdb_method_subscribe_min(void *obj_, s_sis_json_node *node_);

void *sisdb_method_subscribe_max(void *obj_, s_sis_json_node *node_);

void *sisdb_method_subscribe_gap(void *obj_, s_sis_json_node *node_);

#endif  /* _SIS_CALL_H */
