
#include "sisdb_call.h"
#include "sisdb_fields.h"
#include "sisdb_map.h"
#include "sisdb_io.h"
#include "sis_json.h"
#include "sisdb_collect.h"
#include "sisdb_method.h"

static struct s_sis_method _sis_method_table[] = {
	{"incr",    SISDB_METHOD_STYLE_WRITE, sisdb_method_write_incr},     // 数值必须增长且不为零
	{"nonzero", SISDB_METHOD_STYLE_WRITE, sisdb_method_write_nonzero},  // 仅仅非零

	{"once",    SISDB_METHOD_STYLE_SUBSCRIBE, sisdb_method_subscribe_once},  // 只对 0 值赋值一次，后面不再赋值
	{"min",     SISDB_METHOD_STYLE_SUBSCRIBE, sisdb_method_subscribe_min},   // 求最小值
	{"max",     SISDB_METHOD_STYLE_SUBSCRIBE, sisdb_method_subscribe_max},   // 求最大值
	{"gap",     SISDB_METHOD_STYLE_SUBSCRIBE, sisdb_method_subscribe_gap},  // 和上一笔的差值
};

s_sis_map_pointer *sisdb_method_define_create()
{
	int nums = sizeof(_sis_method_table) / sizeof(struct s_sis_method);
	s_sis_map_pointer *map = sis_method_map_create(&_sis_method_table[0], nums);
	return map;
}
void sisdb_method_define_destroy(s_sis_map_pointer *map_)
{
	sis_method_map_destroy(map_);
}

//////////////////////////////////////////////////////////////////////
// class->obj = in;
// sis_method_class_execute(class);
//////////////////////////////////////////////////////////////////////

void *sisdb_method_write_incr(void *obj_, s_sis_json_node *node_)
{	
	s_sis_collect_method_buffer *obj = (s_sis_collect_method_buffer *)obj_;

	s_sisdb_table *tb = obj->collect->db;
	const char *field = sis_json_get_str(node_,"argv");
	s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);

	if (!fu) return SIS_METHOD_VOID_TRUE;
	if (!sisdb_field_is_integer(fu)) return SIS_METHOD_VOID_TRUE;

	uint64 u64 = sisdb_field_get_uint(fu, obj->in);
	if (u64 == 0)
	{
		return  SIS_METHOD_VOID_FALSE;
	} 
	if (tb->control.issubs)
	{
		uint64 u64_last = sisdb_field_get_uint(fu, obj->collect->lasted);
		if (u64 <= u64_last)
		{
			return  SIS_METHOD_VOID_FALSE;
		}
	} else 
	{
		const char *last = sis_struct_list_get(obj->collect->value, obj->collect->value->count - 1);
		uint64 u64_last = sisdb_field_get_uint(fu, last);
		if (u64 <= u64_last)
		{
			return  SIS_METHOD_VOID_FALSE;
		}		
	}
	return SIS_METHOD_VOID_TRUE;
}
void *sisdb_method_write_nonzero(void *obj_, s_sis_json_node *node_)
{	
	s_sis_collect_method_buffer *obj = (s_sis_collect_method_buffer *)obj_;

	s_sisdb_table *tb = obj->collect->db;
	const char *field = sis_json_get_str(node_,"argv");
	s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);

	// printf("---%d--\n", ok);
	if (!fu) return SIS_METHOD_VOID_TRUE;
	// printf("---%d--\n", ok);
	if (!sisdb_field_is_integer(fu)) return SIS_METHOD_VOID_TRUE;
	// printf("---%d--\n", ok);

	uint64 u64 = sisdb_field_get_uint(fu, obj->in);
	// printf("--u64= %ld--\n", u64);
	if (u64 == 0)
	{
		return  SIS_METHOD_VOID_FALSE;
	} 
	// printf("---%d--\n", ok);
	return  SIS_METHOD_VOID_TRUE;
}

//////////////////////////////////////////////////////////////////////
// class->obj = in;
// sis_method_class_execute(class);
//////////////////////////////////////////////////////////////////////

void *sisdb_method_subscribe_once(void *obj_, s_sis_json_node *node_)
{	
	s_sis_collect_method_buffer *obj = (s_sis_collect_method_buffer *)obj_;

	if (!sisdb_field_is_integer(obj->field)) return NULL;

	uint64 u64 = 0;
	s_sisdb_table *tb = obj->collect->db;
	const char *field = sis_json_get_str(node_,"argv");
	if (obj->init)
	{
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);
		if (fu)
		{
			u64 = sisdb_field_get_uint(fu, obj->in);
		}
	} 
	else
	{
		u64 = sisdb_field_get_uint(obj->field, obj->last);
		if (u64 == 0) // 如果是开始第一笔，就直接取对应字段值
		{
			u64 = sisdb_field_get_uint(obj->field, obj->in);
		}
	}
	sisdb_field_set_uint(obj->field, obj->out, u64);
	return NULL; 
}
void *sisdb_method_subscribe_min(void *obj_, s_sis_json_node *node_)
{	
	s_sis_collect_method_buffer *obj = (s_sis_collect_method_buffer *)obj_;

	if (!sisdb_field_is_integer(obj->field)) return NULL;

	uint64 u64 = 0;
	s_sisdb_table *tb = obj->collect->db;
	const char *field = sis_json_get_str(node_,"argv");
	if (obj->init)
	{
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);
		if (fu)
		{
			u64 = sisdb_field_get_uint(fu, obj->in);
		}
	} 
	else
	{
		// in_.low < front.low || !front.low => in.low = in_.low
		//		else in_.close < moved.low => in.low = in_.close
		//		       else in.low = moved.low
		uint64 u64_front, u64_last, u64_inited;
		u64 = sisdb_field_get_uint(obj->field, obj->in);
		u64_front = sisdb_field_get_uint(obj->field, obj->collect->front);
		u64_last = sisdb_field_get_uint(obj->field, obj->last);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);

		u64_inited = sisdb_field_get_uint(fu, obj->in);
		if (u64 < u64_front || !u64_front)
		{
		}
		else
		{
			if (u64_inited < u64_last)
			{
				u64 = u64_inited;
			}
			else
			{
				u64 = u64_last;
			}
		}
	}
	sisdb_field_set_uint(obj->field, obj->out, u64);
	
	return NULL; 
}
void *sisdb_method_subscribe_max(void *obj_, s_sis_json_node *node_)
{	
	s_sis_collect_method_buffer *obj = (s_sis_collect_method_buffer *)obj_;

	if (!sisdb_field_is_integer(obj->field)) return NULL;

	uint64 u64 = 0;
	s_sisdb_table *tb = obj->collect->db;
	const char *field = sis_json_get_str(node_,"argv");
	if (obj->init)
	{
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);
		if (fu)
		{
			u64 = sisdb_field_get_uint(fu, obj->in);
		}
	} 
	else
	{
		uint64 u64_front, u64_last, u64_inited;
		u64 = sisdb_field_get_uint(obj->field, obj->in);
		u64_front = sisdb_field_get_uint(obj->field, obj->collect->front);
		u64_last = sisdb_field_get_uint(obj->field, obj->last);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, field);

		u64_inited = sisdb_field_get_uint(fu, obj->in);
		if (u64 > u64_front || !u64_front)
		{
		}
		else
		{
			if (u64_inited > u64_last)
			{
				u64 = u64_inited;
			}
			else
			{
				u64 = u64_last;
			}
		}
	}
	sisdb_field_set_uint(obj->field, obj->out, u64);

	return NULL; 
}
void *sisdb_method_subscribe_gap(void *obj_, s_sis_json_node *node_)
{	
	s_sis_collect_method_buffer *obj = (s_sis_collect_method_buffer *)obj_;

	uint64 u64 = sisdb_field_get_uint(obj->field, obj->in) - sisdb_field_get_uint(obj->field, obj->collect->front);
	sisdb_field_set_uint(obj->field, obj->out, u64);

	return NULL; 
}