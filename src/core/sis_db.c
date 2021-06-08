
#include "sis_db.h"

int sis_db_get_format(s_sis_sds argv_)
{
    int iformat = SISDB_FORMAT_JSON;
	// SISDB_FORMAT_CHARS; 这里必须明确返回数据类型 不能是CHARS或者是BYTES
    s_sis_json_handle *handle = argv_ ? sis_json_load(argv_, sis_sdslen(argv_)) : NULL;
    if (handle)
    {
        iformat = sis_db_get_format_from_node(handle->node, SISDB_FORMAT_JSON);
        sis_json_close(handle);
    }
    return iformat;
}

int sis_db_get_format_from_node(s_sis_json_node *node_, int default_)
{
	int o = default_;
	s_sis_json_node *node = sis_json_cmp_child_node(node_, "format");
	if (node && sis_strlen(node->value) > 0)
	{
        char ch = node->value[0];
        switch (ch)
        {
        case 'z':  // bitzip "zip"
        case 'Z':
            o = SISDB_FORMAT_BITZIP;
            break;
        case 's':  // struct 
        case 'S':
            o = SISDB_FORMAT_STRUCT;
            break;
        case 'b':  // bytes
        case 'B':
            // o = SISDB_FORMAT_BYTES;
            o = SISDB_FORMAT_BUFFER;
            break;
        case 'j':
        case 'J':
            o = SISDB_FORMAT_JSON;
            break;
        case 'a':
        case 'A':
            o = SISDB_FORMAT_ARRAY;
            break;
        case 'c':
        case 'C':
            o = SISDB_FORMAT_CSV;
            break;        
        default:
            o = SISDB_FORMAT_STRING;
            break;
        }    
	}
	return o;
}


s_sis_sds sis_db_struct_to_json_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, const char *key_,int isfields_)
{	
	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_json_node *jone = sis_json_create_object();
	if (isfields_)
	{
		s_sis_json_node *jfields = sis_json_create_object();
		int index = 0;
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			if (inunit->count > 1)
			{
				for (int k = 0; k < inunit->count; k++)
				{
					char fname[64];
					sis_sprintf(fname, 64, "%s%d", inunit->fname, k);
					sis_json_object_add_uint(jfields, fname, index++);
				}
			}
			else
			{
				sis_json_object_add_uint(jfields, inunit->fname, index++);
			}	
		}
		sis_json_object_add_node(jone, "fields", jfields);
	}

	s_sis_json_node *jtwo = sis_json_create_array();
	const char *val = in_;
	int count = (int)(ilen_ / db_->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jtwo, jval);
		val += db_->size;
	}
	sis_json_object_add_node(jone, key_? key_ : "values", jtwo);

	s_sis_sds o = sis_json_to_sds(jone, true);

	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sis_db_struct_to_array_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_)
{
	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_json_node *jone = sis_json_create_array();

	const char *val = in_;
	int count = (int)(ilen_ / db_->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jone, jval);
		val += db_->size;
	}
	s_sis_sds o = sis_json_to_sds(jone, true);
	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sis_db_struct_to_csv_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, int isfields_)
{
	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_sds o = sis_sdsempty();
	if (isfields_)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			if (inunit->count > 1)
			{
				for (int k = 0; k < inunit->count; k++)
				{
					char fname[64];
					sis_sprintf(fname, 64, "%s%d", inunit->fname, k);
					o = sis_csv_make_str(o, fname, sis_strlen(fname));
				}
			}
			else
			{
				o = sis_csv_make_str(o, inunit->fname, sis_strlen(inunit->fname));
			}
		}
		o = sis_csv_make_end(o);
	}

	const char *val = in_;
	int count = (int)(ilen_ / db_->size);

	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			o = sis_dynamic_field_to_csv(o, inunit, val);			
		}
		o = sis_csv_make_end(o);
		val += db_->size;
	}
	return o;
}

s_sis_sds sis_db_format_sds(s_sis_dynamic_db *db_, const char *key_, int iformat_, const char *in_, size_t ilen_, int isfields_)
{
	s_sis_sds other = NULL;
	switch (iformat_)
	{
	case SISDB_FORMAT_JSON:
		other = sis_db_struct_to_json_sds(db_, in_, ilen_, key_, isfields_);
		break;
	case SISDB_FORMAT_ARRAY:
		other = sis_db_struct_to_array_sds(db_, in_, ilen_);
		break;
	case SISDB_FORMAT_CSV:
		other = sis_db_struct_to_csv_sds(db_, in_, ilen_, isfields_);
		break;
	}
	return other;
}

/////////////////////////////////////////////////////////
// 多数据结构 统一排序输出的定义 
/////////////////////////////////////////////////////////
s_sis_subdb_unit *sis_subdb_unit_create(char *kname_, s_sis_dynamic_db *db_)
{
	s_sis_subdb_unit *o = SIS_MALLOC(s_sis_subdb_unit, o);
	o->db = db_;
	o->kname = sis_sdsnew(kname_);
	o->vlist = sis_struct_list_create(db_->size);
	o->vmsec = sis_struct_list_create(sizeof(msec_t));
	return o;	
}
void sis_subdb_unit_destroy(void *unit_)
{
	s_sis_subdb_unit *unit = (s_sis_subdb_unit *)unit_;
	sis_struct_list_destroy(unit->vlist);
	sis_struct_list_destroy(unit->vmsec);
	sis_sdsfree(unit->kname);
	sis_free(unit);
}
void sis_subdb_unit_clear(s_sis_subdb_unit *unit_)
{
	sis_struct_list_clear(unit_->vlist);
	sis_struct_list_clear(unit_->vmsec);
	unit_->next = NULL;
	unit_->prev = NULL;
}

/////////////////////////////////////////////////////////

s_sis_subdb_cxt *sis_subdb_cxt_create()
{
	s_sis_subdb_cxt *o = SIS_MALLOC(s_sis_subdb_cxt, o);
	o->work_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
	o->work_units = sis_map_pointer_create_v(sis_subdb_unit_destroy);
	return o;
}
void sis_subdb_cxt_destroy(s_sis_subdb_cxt *cxt_)
{
	if (cxt_->work_status == SIS_SUBDB_START || cxt_->work_status == SIS_SUBDB_STOP)
	{
		sis_subdb_cxt_sub_stop(cxt_);
		while (cxt_->work_status != SIS_SUBDB_STOPED)
		{
			sis_sleep(30); //等待结束
		}	
	}
	sis_map_list_destroy(cxt_->work_sdbs);
	sis_map_pointer_destroy(cxt_->work_units);
	sis_free(cxt_);
}

void sis_subdb_cxt_clear(s_sis_subdb_cxt *cxt_)
{
	if (cxt_->work_status == SIS_SUBDB_STOPED)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(cxt_->work_units);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_subdb_unit *unit = (s_sis_subdb_unit *)sis_dict_getval(de);
			sis_subdb_unit_clear(unit);
		}
		sis_dict_iter_free(di);
	}
}

// void sis_subdb_cxt_set_keys(s_sis_subdb_cxt *, s_sis_sds );
void sis_subdb_cxt_init_sdbs(s_sis_subdb_cxt *cxt_, const char *in_, size_t ilen_)
{
	s_sis_json_handle *injson = sis_json_load(in_, ilen_);
	if (!injson)
	{
		return ;
	}
	int scale = 0;
	s_sis_json_node *innode = sis_json_first_node(injson->node);
	while (innode)
	{
		s_sis_dynamic_db *sdb = sis_dynamic_db_create(innode);
		if (sdb)
		{
			sis_map_list_set(cxt_->work_sdbs, sdb->name, sdb);
			if (sdb->field_time)
			{
				scale = sis_max(sdb->field_time->style, scale);
			}
		}
		innode = sis_json_next_node(innode);
	}
	sis_json_close(injson);
	// calc min scale
	cxt_->cur_scale = sis_max(cxt_->cur_scale, scale);
}

static inline msec_t _subdb_cxt_get_vmsec(s_sis_subdb_unit *unit)
{
	msec_t *vmsec = (msec_t *)sis_struct_list_first(unit->vmsec);
	return vmsec ? *vmsec : 0;
}
void _show_link(s_sis_subdb_cxt *cxt)
{
	s_sis_subdb_unit *next = cxt->head;
	int index = 0;
	while(next)
	{
		printf("-%3d-[%4d] %s.%s : %lld | %p %p | %p %p %p\n", index++, next->vlist->count, next->kname, next->db->name, 
			_subdb_cxt_get_vmsec(next), cxt->head, cxt->tail, next->prev, next, next->next);
		next = next->next;
	}	
}
void _subdb_cxt_link_move(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *unit)
{
	// 抹掉unit
	if (unit->prev)
	{
		unit->prev->next = unit->next;
	}
	if (unit->next)
	{
		unit->next->prev = unit->prev;
	}
	if (unit == cxt->head)
	{
		cxt->head = unit->next;
		if (unit->next)
		{
			unit->next->prev = NULL;
		}
	}
	if (unit == cxt->tail)
	{
		cxt->tail = unit->prev;
		if (unit->prev)
		{
			unit->prev->next = NULL;
		}
	}
}
void _subdb_cxt_link_tail(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *unit)
{
	_subdb_cxt_link_move(cxt, unit);
	// 到尾部
	if (cxt->tail)
	{
		cxt->tail->next = unit;
	}
	unit->prev = cxt->tail;
	unit->next = NULL;
	cxt->tail = unit;
	if (!cxt->head)
	{
		cxt->head = unit;
	}
}
void _subdb_cxt_link_insert(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *nearunit, s_sis_subdb_unit *unit)
{
	_subdb_cxt_link_move(cxt, unit);
	// 插入到 nearunit 前
	if (nearunit == cxt->head)
	{
		nearunit->prev = unit;
		unit->next = nearunit;
		unit->prev = NULL;
		cxt->head = unit;
	}
	else
	{
		// if (!nearunit->prev)
		// {
		// 	LOG(0)("--------------\n");
		// }
		// _show_link(cxt);
		nearunit->prev->next = unit;
		unit->prev = nearunit->prev;
		nearunit->prev = unit;
		unit->next = nearunit;

	}
}
void _subdb_cxt_next_link(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *unit)
{
	// 从下一记录开始查找
	int mode = SIS_SUBDB_NONE; 
	msec_t agov = _subdb_cxt_get_vmsec(unit);
	s_sis_subdb_unit *nearunit = unit->next;
	while (nearunit)
	{
		msec_t curv = _subdb_cxt_get_vmsec(nearunit);
		if (agov <= curv)
		{
			break;
		}
		else
		{
			mode = nearunit->next ? SIS_SUBDB_INSERT : SIS_SUBDB_TAIL; 
		}
		nearunit = nearunit->next;
	};
	if (mode == SIS_SUBDB_TAIL)
	{ // 到尾部
		_subdb_cxt_link_tail(cxt, unit);
	}
	else if (mode == SIS_SUBDB_INSERT)
	{ 
		_subdb_cxt_link_insert(cxt, nearunit, unit);
	}	
}
void _subdb_cxt_push_link(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *unit)
{
	// 从下一记录开始查找
	int mode = SIS_SUBDB_NONE; 
	s_sis_subdb_unit *nearunit = NULL;
	if (cxt->head && cxt->tail)
	{
		msec_t agov = _subdb_cxt_get_vmsec(unit);
		nearunit = cxt->head;
		while (nearunit)
		{
			msec_t curv = _subdb_cxt_get_vmsec(nearunit);
			if (agov <= curv)
			{
				mode = SIS_SUBDB_INSERT;
				break;
			}
			nearunit = nearunit->next;
		};
	}
	if (mode == SIS_SUBDB_INSERT)
	{ 
		_subdb_cxt_link_insert(cxt, nearunit, unit);
	}
	else
	{  // 到尾部
		_subdb_cxt_link_tail(cxt, unit);
	}	
}

void _subdb_cxt_add_link(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *unit)
{
	if (!unit->db->field_time)
	{
		if (cxt->head)
		{
			cxt->head->prev = unit;
			unit->next = cxt->head;
		}
		if (!cxt->tail)
		{
			cxt->tail = unit;
		}
		cxt->head = unit;
	}
	else
	{
		if (!cxt->head || !cxt->tail)
		{
			cxt->head = unit;
			cxt->tail = unit;
		}
		else
		{
			_subdb_cxt_push_link(cxt, unit);
		}
	}
}
void _subdb_cxt_del_link(s_sis_subdb_cxt *cxt, s_sis_subdb_unit *unit)
{
	if (unit->prev)
	{
		unit->prev->next = unit->next;
	}
	if (unit->next)
	{
		unit->next->prev = unit->prev;
	}
	if (unit == cxt->head)
	{
		cxt->head = unit->next;
	}
	if (unit == cxt->tail)
	{
		cxt->tail = unit->prev;
	}
	// printf("unit= %p\n", unit);
	sis_subdb_unit_clear(unit);
	// sis_subdb_unit_destroy(unit);
}
// 增加的数据一定是从小到大排序的数据 新增的数据一定比以前的数据时间更晚
// 没有时间字段的最先发布 有时间的字段找到小于或等于就插入
void sis_subdb_cxt_add_data(s_sis_subdb_cxt *cxt_, const char *key_, void *in_, size_t ilen_)
{
	int isnew = 0;
	int ischange = 0;
	s_sis_subdb_unit *unit = sis_map_pointer_get(cxt_->work_units, key_);
	if (!unit)
	{
		char kname[128];
		char sname[128];
		sis_str_divide(key_, '.', kname, sname);
		s_sis_dynamic_db *db = sis_map_list_get(cxt_->work_sdbs, sname);
		if (db)
		{
			unit = sis_subdb_unit_create(kname, db);
			sis_map_pointer_set(cxt_->work_units, key_, unit);
			isnew = 1;
		}
	}
	if (unit)
	{
		// add data
		ischange = (unit->vlist->count < 1);
		int count = ilen_ / unit->db->size;
		if (count > 0)
		{
			for (int i = 0; i < count; i++)
			{
				msec_t vmsec = 0;
				if (unit->db->field_time)
				{
					vmsec = sis_dynamic_db_get_time(unit->db, i, in_, ilen_);
					vmsec = sis_time_unit_convert(unit->db->field_time->style, cxt_->cur_scale, vmsec);
				}
				// if (i==0) printf("%s = %lld\n", key_, vmsec);
				sis_struct_list_push(unit->vmsec, &vmsec);
			}
			sis_struct_list_pushs(unit->vlist, in_, count);
			if (isnew)
			{
				_subdb_cxt_add_link(cxt_, unit);
			}
			else if (ischange) // 数据没有必定不在链中
			{
				_subdb_cxt_push_link(cxt_, unit);
			}
		}
	}
	// _show_link(cxt_);
}
int _subdb_cxt_get_data(s_sis_subdb_cxt *cxt, s_sis_db_chars *out)
{
	if (!cxt->head||!cxt->tail)
	{
		return 2;
	}
	s_sis_subdb_unit *unit = cxt->head;
	out->sname = unit->db->name;
	out->kname = unit->kname;
	if (unit->vlist->count < 1)
	{
		_subdb_cxt_del_link(cxt, unit);
		return 1;
	}
	out->size = unit->db->size;
	out->data = sis_struct_list_pop(unit->vlist);
	msec_t agov = _subdb_cxt_get_vmsec(unit); 
	sis_struct_list_pop(unit->vmsec);
	msec_t curv = _subdb_cxt_get_vmsec(unit); 
	if (agov != curv) // 这里修改指针 时间相同下次还传该key数据
	{
		_subdb_cxt_next_link(cxt, unit);
	}
	return 0;
}

static void *_thread_subdb_cxt_sub(void *argv_)
{
    s_sis_subdb_cxt *context = (s_sis_subdb_cxt *)argv_;

	context->work_status = SIS_SUBDB_START;

    if (context->cb_sub_start)
    {
        context->cb_sub_start(context->cb_source, NULL);
    } 
	bool isstop = false;
	while (context->work_status == SIS_SUBDB_START)
	{
		s_sis_db_chars data = {0};
		int o = _subdb_cxt_get_data(context, &data);
		if (o == 0)
		{
			if (context->cb_key_bytes)
			{
				context->cb_key_bytes(context->cb_source, &data);
			} 	
			// _show_link(context);		
		}
		else if (o == 1)  // 某个key数据读完
		{
			// _show_link(context);		
			if (context->cb_key_stop)
			{
				context->cb_key_stop(context->cb_source, &data);
			} 
			// _show_link(context);		
		}
		else // 数据全部读完 或数据出错
		{
			isstop = (o == 2);
			break;
		}
	}
    if (isstop && context->cb_sub_stop)
    {
        context->cb_sub_stop(context->cb_source, NULL);
    } 
	context->work_status = SIS_SUBDB_STOPED;
    return NULL;
}
// 开始订阅
void sis_subdb_cxt_sub_start(s_sis_subdb_cxt *cxt_)
{
	if (cxt_->work_status == SIS_SUBDB_START || cxt_->work_status == SIS_SUBDB_STOP)
	{
		return ;
	}
	// 启动一个线程 处理数据
	// sis_thread_create(_thread_subdb_cxt_sub, cxt_, &cxt_->work_thread);
	_thread_subdb_cxt_sub(cxt_);
	// while (cxt_->work_status != SIS_SUBDB_STOPED)
	// {
	// 	sis_sleep(10);
	// }
	
}
// 结束或中断订阅
void sis_subdb_cxt_sub_stop(s_sis_subdb_cxt *cxt_)
{
	if (cxt_->work_status == SIS_SUBDB_STOPED || cxt_->work_status == SIS_SUBDB_STOP)
	{
		return ;
	}
	cxt_->work_status = SIS_SUBDB_STOP;
}

#if 0
#pragma pack(push,1)
typedef struct _date_info {
	int time;
	int value;
} _date_info;

typedef struct _msec_info {
	msec_t time;
	int    value;
} _msec_info;

#pragma pack(pop)

int _isstoped = 0;
int _curminute = 0;
int _minutes = 0;
static int cb_sub_start(void *cxt_, void *avgv_)
{
	printf("%s\n", __func__);
	return 0;
}
static int cb_sub_stop(void *cxt, void *avgv_)
{
	printf("%s\n", __func__);
	return 0;
}
static int cb_key_stop(void *cxt_, void *avgv_)
{
	s_sis_subdb_cxt *cxt = (s_sis_subdb_cxt *)cxt_;
	s_sis_db_chars *chars = (s_sis_db_chars *)avgv_;
	LOG(0)("%s.%s stop.\n", chars->kname, chars->sname);
	if (!sis_strcasecmp(chars->sname, "sminu") && _minutes < 5)
	{
		LOG(1)("%s.%s new data.\n", chars->kname, chars->sname);
		_date_info vdate[2];
		vdate[0].time = ++_curminute;  vdate[0].value = 4000;
		vdate[1].time = ++_curminute;  vdate[1].value = 4001;
		sis_subdb_cxt_add_data(cxt, "600600.sminu", &vdate[0], 2*sizeof(_date_info));
		_minutes++;
	}
	return 0;
}
static int cb_key_bytes(void *cxt_, void *avgv_)
{
	// printf("%s\n", __func__);
	s_sis_subdb_cxt *cxt = (s_sis_subdb_cxt *)cxt_;
	s_sis_db_chars *chars = (s_sis_db_chars *)avgv_;

	s_sis_dynamic_db *db = sis_map_list_get(cxt->work_sdbs, chars->sname);
	if (db)
	{
		s_sis_sds in = sis_dynamic_db_to_array_sds(db, NULL, chars->data, chars->size);
		printf("%s %s = %s\n",chars->kname, chars->sname, in);
		sis_sdsfree(in);	
	}
	else
	{
		printf("no %s %s.\n", chars->kname, chars->sname);
	}
	return 0;
}
int main()
{
	const char *indbstr = "{\"sdate\":{\"fields\":{\"time\":[\"D\",4],\"value\":[\"I\",4]}},\
		\"sminu\":{\"fields\":{\"time\":[\"M\",4],\"value\":[\"I\",4]}},\
		\"sssec\":{\"fields\":{\"time\":[\"S\",4],\"value\":[\"I\",4]}},\
		\"smsec\":{\"fields\":{\"time\":[\"T\",8],\"value\":[\"I\",4]}}}";

	s_sis_subdb_cxt *cxt = sis_subdb_cxt_create();
	
	sis_subdb_cxt_init_sdbs(cxt, indbstr, strlen(indbstr));
	cxt->cb_source = cxt;
	cxt->cb_sub_start = cb_sub_start;
	cxt->cb_sub_stop = cb_sub_stop;
	cxt->cb_key_stop = cb_key_stop;
	cxt->cb_key_bytes = cb_key_bytes;

	
	_date_info vdate = {20210512, 1000};
	// sis_subdb_cxt_add_data(cxt, "600600.sdate", &vdate, sizeof(_date_info));

	msec_t nowsec = sis_time_get_now();

	vdate.time = nowsec/60;  vdate.value = 2000;
	sis_subdb_cxt_add_data(cxt, "600600.sminu", &vdate, sizeof(_date_info));
	vdate.time++;  vdate.value++;
	sis_subdb_cxt_add_data(cxt, "600600.sminu", &vdate, sizeof(_date_info));
	_curminute = vdate.time;

	vdate.time = nowsec;  vdate.value = 3000;
	for (int i = 0; i < 20; i++)
	{
		sis_subdb_cxt_add_data(cxt, "600600.sssec", &vdate, sizeof(_date_info));
		vdate.time+=10;  vdate.value++;
	}
	_msec_info vmsec = {0};
	vmsec.time = nowsec * 1000;  vmsec.value = 10000;
	for (int i = 0; i < 400; i++)
	{
		sis_subdb_cxt_add_data(cxt, "600600.smsec", &vmsec, sizeof(_msec_info));
		vmsec.time += 500;  vmsec.value++;
	}
	// return 0;
	_isstoped = 0;
	sis_subdb_cxt_sub_start(cxt);
	
	while(!_isstoped)
	{
		sis_sleep(1000);
	}

	sis_subdb_cxt_destroy(cxt);
	return 0;
}

#endif