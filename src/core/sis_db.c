
#include "sis_db.h"

int sis_is_multiple_sub(const char *key, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (key[i] == '*' || key[i] == ',')
        // if (key[i] == '*')
        {
            return true;
        }
    }
    return false;
}

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
s_sis_subdb_unit *sis_subdb_unit_create(char *kname_, s_sis_dynamic_db *db_, int pagesize_)
{
	s_sis_subdb_unit *o = SIS_MALLOC(s_sis_subdb_unit, o);
	o->db = db_;
	o->kname = sis_sdsnew(kname_);
	o->vlist = sis_node_list_create(pagesize_, db_->size);
	o->vmsec = sis_node_list_create(pagesize_, sizeof(msec_t));
	return o;	
}
void sis_subdb_unit_destroy(void *unit_)
{
	s_sis_subdb_unit *unit = (s_sis_subdb_unit *)unit_;
	sis_node_list_destroy(unit->vlist);
	sis_node_list_destroy(unit->vmsec);
	sis_sdsfree(unit->kname);
	sis_free(unit);
}
void sis_subdb_unit_clear(s_sis_subdb_unit *unit_)
{
	sis_node_list_clear(unit_->vlist);
	sis_node_list_clear(unit_->vmsec);
}

/////////////////////////////////////////////////////////

s_sis_subdb_cxt *sis_subdb_cxt_create(int pagesize_)
{
	s_sis_subdb_cxt *o = SIS_MALLOC(s_sis_subdb_cxt, o);
	o->work_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
	o->work_units = sis_map_list_create(sis_subdb_unit_destroy);
	o->pagesize = pagesize_ ? pagesize_ : 1024;
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
	sis_map_list_destroy(cxt_->work_units);
	sis_free(cxt_);
}

void sis_subdb_cxt_clear(s_sis_subdb_cxt *cxt_)
{
	int count = sis_map_list_getsize(cxt_->work_units);
	for (int i = 0; i < count; i++)
	{
		s_sis_subdb_unit *unit = sis_map_list_geti(cxt_->work_units, i);
		sis_subdb_unit_clear(unit);
	}
	cxt_->new_msec = 0;
	cxt_->new_sums = 0;
}

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
	// printf("scale: %d\n", cxt_->cur_scale);
}
void sis_subdb_cxt_set_sdbs(s_sis_subdb_cxt *cxt_, s_sis_dynamic_db *sdb_)
{
	if (!sis_map_list_get(cxt_->work_sdbs, sdb_->name))
	{
		sis_dynamic_db_incr(sdb_);
		sis_map_list_set(cxt_->work_sdbs, sdb_->name, sdb_);
		if (sdb_->field_time)
		{
			cxt_->cur_scale = sis_max(sdb_->field_time->style, cxt_->cur_scale);
		}
	}
}
static inline msec_t _subdb_cxt_get_vmsec(s_sis_subdb_unit *unit)
{
	msec_t *vmsec = (msec_t *)sis_node_list_get(unit->vmsec, 0);
	// if (!vmsec)
	// {
	// 	LOG(0)("vmsec : %d %d  %d %d\n", unit->vmsec->count, unit->vlist->count, unit->vmsec->nouse, unit->vlist->nouse);
	// }
	return vmsec ? *vmsec : 0;
}

static inline void  _subdb_cxt_set_msec(s_sis_subdb_cxt *cxt, msec_t vmsec)
{
	if (vmsec < cxt->new_msec || cxt->new_sums == 0)
	{
		cxt->new_msec = vmsec;
	}
}

// 增加的数据一定是从小到大排序的数据 新增的数据一定比以前的数据时间更晚
// 没有时间字段的最先发布 有时间的字段找到小于或等于就插入
void sis_subdb_cxt_push_sdbs(s_sis_subdb_cxt *cxt_, const char *key_, void *in_, size_t ilen_)
{
	s_sis_subdb_unit *unit = NULL;
	int uidx = sis_map_list_get_index(cxt_->work_units, key_);
	if (uidx < 0)
	{
		char kname[128];
		char sname[128];
		sis_str_divide(key_, '.', kname, sname);
		s_sis_dynamic_db *db = sis_map_list_get(cxt_->work_sdbs, sname);
		if (db)
		{
			unit = sis_subdb_unit_create(kname, db, cxt_->pagesize);
			uidx = sis_map_list_set(cxt_->work_units, key_, unit);
		}
	}
	else
	{
		unit = sis_map_list_geti(cxt_->work_units, uidx);
	}
	if (unit)
	{
		// add data
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
				if (unit->vmsec->count == 0)
				{
					_subdb_cxt_set_msec(cxt_, vmsec);
				}
				// if (i==0) printf("%s = %lld\n", key_, vmsec);
				sis_node_list_push(unit->vmsec, &vmsec);
				sis_node_list_push(unit->vlist, (void *)((const char*)in_ + i * unit->db->size));
			}
			cxt_->new_sums += count;
		}
	}
	// _show_link(cxt_);
}

// 初始化数据名 和数据记录大小
void sis_subdb_cxt_init_data(s_sis_subdb_cxt *cxt_, const char *sname_, size_t size_)
{
	s_sis_dynamic_db *sdb = sis_dynamic_db_create_none(sname_, size_);
	sis_map_list_set(cxt_->work_sdbs, sdb->name, sdb);
	cxt_->cur_scale = SIS_DYNAMIC_TYPE_WSEC;
}
// 传入数据 必须设置数据微秒时间 仅仅跟一条数据
void sis_subdb_cxt_push_data(s_sis_subdb_cxt *cxt_, const char *key_, msec_t msec_, void *in_, size_t ilen_)
{
	s_sis_subdb_unit *unit = NULL;
	int uidx = sis_map_list_get_index(cxt_->work_units, key_);
	if (uidx < 0)
	{
		char kname[128];
		char sname[128];
		sis_str_divide(key_, '.', kname, sname);
		s_sis_dynamic_db *db = sis_map_list_get(cxt_->work_sdbs, sname);
		if (db)
		{
			unit = sis_subdb_unit_create(kname, db, cxt_->pagesize);
			sis_map_list_set(cxt_->work_units, key_, unit);
		}
	}
	else
	{
		unit = sis_map_list_geti(cxt_->work_units, uidx);
	}
	if (unit)
	{
		// add data
		if (ilen_ == unit->db->size)
		{
			if (unit->vmsec->count == 0)
			{
				_subdb_cxt_set_msec(cxt_, msec_);
			}
			sis_node_list_push(unit->vmsec, &msec_);
			sis_node_list_push(unit->vlist, in_);
			cxt_->new_sums ++;
		}
	}
}

int _subdb_cxt_sub_data(s_sis_subdb_cxt *cxt)
{
	msec_t  curr_msec = cxt->new_msec; 
	int     next_nums = 0;
	msec_t  next_msec = 0; 
	int count = sis_map_list_getsize(cxt->work_units);
	for (int i = 0; i < count; i++)
	{
		s_sis_subdb_unit *unit = sis_map_list_geti(cxt->work_units, i);
		int nums = sis_node_list_getsize(unit->vlist);
		for (int k = 0; k < nums; k++)
		{
			msec_t curv = _subdb_cxt_get_vmsec(unit); 
			if (curv <= curr_msec)
			{
				s_sis_db_chars chars = {0};
				chars.sname = unit->db->name;
				chars.kname = unit->kname;
				chars.size = unit->db->size;
				chars.data = sis_node_list_pop(unit->vlist);
				sis_node_list_pop(unit->vmsec);
				cxt->new_sums --;
				if (cxt->cb_key_chars)
				{
					cxt->cb_key_chars(cxt->cb_source, &chars);
				} 				
			}
			else
			{
				if (curv < next_msec || next_nums == 0)
				{
					next_msec = curv;
					next_nums = 1;
				}
				break;
			}
		}
		if (nums > 0 && sis_node_list_getsize(unit->vlist) < 1)
		{
			if (cxt->cb_key_stop)
			{
				s_sis_db_chars chars = {0};
				chars.sname = unit->db->name;
				chars.kname = unit->kname;
				chars.data = NULL;
				chars.size = 0;
				cxt->cb_key_stop(cxt->cb_source, &chars);
			} 
		}
	}
	if (cxt->new_sums == 0)
	{
		return 1;
	}
	// 还未结束
	// next_msec 必定大于 curr_msec  curr_msec >= cxt->new_msec
	if (curr_msec == cxt->new_msec)
	{
		 cxt->new_msec = next_msec;
	}
	else if (curr_msec > cxt->new_msec)
	{
		// 如果时序提前 就用提前的时间再过一遍
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
		int o = _subdb_cxt_sub_data(context);
		// printf("==== stop 1 %p %d %p %p\n", context->cb_key_stop, o, context->head , context->tail);
		if (o == 1)
		{
			isstop = (o == 1);
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
		sis_subdb_cxt_push_sdbs(cxt, "600600.sminu", &vdate[0], 2*sizeof(_date_info));
		_minutes++;
	}
	return 0;
}
static int cb_key_chars(void *cxt_, void *avgv_)
{
	// printf("%s\n", __func__);
	s_sis_subdb_cxt *cxt = (s_sis_subdb_cxt *)cxt_;
	s_sis_db_chars *chars = (s_sis_db_chars *)avgv_;

	s_sis_dynamic_db *db = sis_map_list_get(cxt->work_sdbs, chars->sname);
	if (db)
	{
		s_sis_sds in = sis_sdb_to_array_sds(db, NULL, chars->data, chars->size);
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

	s_sis_subdb_cxt *cxt = sis_subdb_cxt_create(10);
	
	sis_subdb_cxt_init_sdbs(cxt, indbstr, strlen(indbstr));
	cxt->cb_source = cxt;
	cxt->cb_sub_start = cb_sub_start;
	cxt->cb_sub_stop = cb_sub_stop;
	// cxt->cb_key_stop = cb_key_stop;
	cxt->cb_key_chars = cb_key_chars;

	
	_date_info vdate = {20210512, 1000};
	sis_subdb_cxt_push_sdbs(cxt, "600600.sdate", &vdate, sizeof(_date_info));

	msec_t nowsec = sis_time_get_now();

	vdate.time = nowsec/60;  vdate.value = 2000;
	sis_subdb_cxt_push_sdbs(cxt, "600600.sminu", &vdate, sizeof(_date_info));
	vdate.time++;  vdate.value++;
	sis_subdb_cxt_push_sdbs(cxt, "600600.sminu", &vdate, sizeof(_date_info));
	_curminute = vdate.time;

	vdate.time = nowsec;  vdate.value = 3000;
	for (int i = 0; i < 20; i++)
	{
		sis_subdb_cxt_push_sdbs(cxt, "600600.sssec", &vdate, sizeof(_date_info));
		vdate.time+=10;  vdate.value++;
	}
	_msec_info vmsec = {0};
	vmsec.time = nowsec * 1000;  vmsec.value = 10000;
	for (int i = 0; i < 400; i++)
	{
		sis_subdb_cxt_push_sdbs(cxt, "600600.smsec", &vmsec, sizeof(_msec_info));
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