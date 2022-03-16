

#include "sis_dbf.h"

int _sis_file_dbf_parse(s_sis_file_dbf *dbf_)
{
	if (!dbf_ || !dbf_->fp) {return  0;}
	size_t size = sis_file_size(dbf_->fp);
	if (size == 0) {return  0;}

	dbf_->buffer = (char *)sis_malloc(size + 1);
	sis_file_seek(dbf_->fp, 0 ,SEEK_SET);
	size_t bytes = sis_file_read(dbf_->fp, dbf_->buffer, size);
	// printf("bytes=%d  %d\n", (int)bytes, (int)size);

    if (bytes < sizeof(s_sis_dbf_head)) 
	{
		sis_free(dbf_->buffer);
		dbf_->buffer = NULL;
        return 0;
    }
    char* ptr = dbf_->buffer;
 	memmove(&dbf_->head, ptr, sizeof(s_sis_dbf_head));
	// dbf_->current_record_i = -1;  //  设置当前记录号

	int fieldnums = (dbf_->head.head_len - sizeof(s_sis_dbf_head)) / sizeof(s_sis_dbf_field);
	
	// printf("field=%d  %d %d [%d, %d]\n", fieldnums, sizeof(s_sis_dbf_head), sizeof(s_sis_dbf_field),
	// 			dbf_->head.head_len,dbf_->head.record_len);

 	sis_struct_list_clear(dbf_->fields);
    ptr += sizeof(s_sis_dbf_head);
	for (int k = 0; k < fieldnums; k++)
	{
        s_sis_dbf_field *field = (s_sis_dbf_field *)sis_malloc(sizeof(s_sis_dbf_field));
        memmove(field,  ptr, sizeof(s_sis_dbf_field));
		ptr += sizeof(s_sis_dbf_field);
		if (field->type != 'N' && field->type != 'F')
		{
			field->dot = 0;
		}
		sis_map_pointer_set(dbf_->map_fields, field->name, field);
        sis_struct_list_push(dbf_->fields, field);
	}
	//有时候实际记录数没有那么多，会造成内存溢出
	//这里校验一下
	int count = (size - dbf_->head.head_len) / dbf_->head.record_len;
	count = count < 0 ? 0 : count;
	if (count < (int)dbf_->head.count)
	{
		dbf_->head.count = count;
	}
	return dbf_->head.count;
}

s_sis_file_dbf * sis_file_dbf_open(const char *name_,int mode_, int access_)
{
	mode_ = (!mode_) ? SIS_FILE_IO_READ : mode_;
    s_sis_file_handle fp = sis_file_open(name_, mode_, access_);
    if (!fp)
    {
		return NULL;
    }

    s_sis_file_dbf * o =sis_malloc(sizeof(s_sis_file_dbf));
    memset(o, 0, sizeof(s_sis_file_dbf));

	o->fp = fp;
	o->fields = sis_struct_list_create(sizeof(s_sis_dbf_field));
	o->map_fields = sis_map_pointer_create_v(sis_free_call);

    _sis_file_dbf_parse(o);
	// printf("field=%d  record=%d\n", o->fields->count, o->head.count);
    sis_file_close(o->fp);
    o->fp = NULL;
    return o;    
}

void sis_file_dbf_close(s_sis_file_dbf *dbf_)
{
	if(!dbf_) {return ;}
	sis_free(dbf_->buffer);   
    sis_struct_list_destroy(dbf_->fields);
	sis_map_pointer_destroy(dbf_->map_fields);
	sis_free(dbf_);    
}
char * _sis_file_dbf_find_record(s_sis_file_dbf *dbf_, int index)
{
	if (index <0 || index>(int)dbf_->head.count - 1)
	{
		return NULL;
	}
	return dbf_->buffer+dbf_->head.head_len+index*dbf_->head.record_len;
}
s_sis_dbf_field * _sis_file_dbf_find_field(s_sis_file_dbf *dbf_, const char *key_)
{
	s_sis_dbf_field * field = (s_sis_dbf_field *)sis_map_pointer_get(dbf_->map_fields, key_);
	return field;

	// s_sis_dbf_field * field ;
	// for (int i=0;i <dbf_->fields->count ; i++)
	// {
	// 	field = (s_sis_dbf_field *) sis_struct_list_get(dbf_->fields, i);
	// 	// printf("m_ %s -- %s \n", key_, field->name);

	// 	if (!sis_strncmp(key_,field->name,strlen(key_)))
	// 	{
	// 		return field;
	// 	}
	// }
	// return NULL;
}
int64 sis_file_dbf_get_int(s_sis_file_dbf *dbf_, int index, const char *key_, int64 defaultvalue_)
{
	char *buffer = (char *)_sis_file_dbf_find_record(dbf_, index);
	s_sis_dbf_field *field = _sis_file_dbf_find_field(dbf_, key_);
	if (!field||!buffer) 
	{
		return defaultvalue_;
	}
	char val[32];
	sis_strncpy(val, 32, buffer+field->offset, field->len);
	return sis_atoll(val);
}
double sis_file_dbf_get_double(s_sis_file_dbf *dbf_, int index, const char *key_, double defaultvalue_)
{
	char *buffer = (char *)_sis_file_dbf_find_record(dbf_, index);
	s_sis_dbf_field *field = _sis_file_dbf_find_field(dbf_, key_);
	if (!field||!buffer) 
	{
		return defaultvalue_;
	}
	char val[32];
	sis_strncpy(val, 32, buffer+field->offset, field->len);	
	// printf("key_=%s val=%s\n",key_, val);
	return atof(val);
}

void sis_file_dbf_get_str(s_sis_file_dbf *dbf_, int index, const char *key_,char *out_, size_t olen_)
{
	char *buffer = (char *)_sis_file_dbf_find_record(dbf_, index);
	s_sis_dbf_field *field = _sis_file_dbf_find_field(dbf_, key_);
	if (!field||!buffer) 
	{
		out_[0] = 0;
		return ;
	}
	// sis_out_binary("str",buffer+field->offset, field->len);
	sis_strncpy(out_, olen_, buffer + field->offset, field->len);
}
void sis_file_dbf_get_str_of_fid(s_sis_file_dbf *dbf_, int index, int fid, char *out_, size_t olen_)
{
	char *buffer = (char *)_sis_file_dbf_find_record(dbf_, index);
	s_sis_dbf_field *field = (s_sis_dbf_field *)sis_struct_list_get(dbf_->fields, fid);
	if (!field||!buffer) 
	{
		out_[0] = 0;
		return ;
	}
	sis_strncpy(out_, olen_, buffer+field->offset, field->len);	
}
void sis_file_dbf_set_int(s_sis_file_dbf *dbf_, int index, char *key_, uint64 in_)
{

}

void sis_file_dbf_set_double(s_sis_file_dbf *dbf_, int index, char *key_, double in_, int dot_)
{

}
void sis_file_dbf_set_str(s_sis_file_dbf *dbf_, int index, char *key_, const char *in_, size_t ilen_)
{

}

#if 0

int main()
{

	s_sis_file_dbf *dbf = sis_file_dbf_open("order.dbf", 0, 0);
    if (!dbf) 
    {
        printf("error");    
        return 0;
    }
	
	char value[164];
	int start =sis_time_get_now();
/*	for(uint32 i = 0; i < dbf->head.count; i++)
	{
		// printf("\n---- i= %d -------\n", i);
		for(int k = 0; k < 200; k++)
		{
			sis_file_dbf_get_str_of_fid(dbf,i,k%20,value,64);
		}
		if(i%10000==0)
		printf("--%d-- sec = %d -------\n", i, (int)sis_time_get_now() - start);
	}*/
	start =sis_time_get_now();
	printf("\n %d * %d + %d = [%d] \n", dbf->head.record_len, dbf->head.count, dbf->head.head_len, (dbf->head.head_len - sizeof(s_sis_dbf_head)) / sizeof(s_sis_dbf_field));
	for(uint32 i = 0; i < dbf->head.count ; i++)
	{
		// 
		for(int k = 0; k < 200; k++)
		{
			char *key = sis_struct_list_get(dbf->fields,k%20);
			// char *key = sis_struct_list_get(dbf->fields,19);
			sis_file_dbf_get_str(dbf,i,key,value,64);
		}
		if(i%10000==0)
			printf("--%d-- sec = %d ----%s---\n", i, (int)sis_time_get_now() - start, value);
	}
	sis_file_dbf_close(dbf);
	system("pause");
	return 0;
}
#endif
// int c_dbf_option::get_fields_count()
// {
//     if (m_dbf_info_ps == NULL) return 0;
//     return (m_dbf_info_ps->m_fields_i);
// }

// int c_dbf_option::get_record_count()
// {
//     if (m_dbf_info_ps == NULL) return 0;
//     return (m_dbf_info_ps->records);
// }

// DBF_FIELD_TYPE c_dbf_option::get_field_info( int field_, char * fieldname_, int * width_, int * decimals_ )
// {
//     if (m_dbf_info_ps == NULL) return SIS_DBF_FIELD_TYPE_INVALID;

//     if( field_ < 0 || field_ >= m_dbf_info_ps->m_fields_i )
//         return (SIS_DBF_FIELD_TYPE_INVALID);

//     if( width_ != NULL )
//         *width_ = m_dbf_info_ps->m_field_size_pi[field_];

//     if( decimals_ != NULL )
//         *decimals_ = m_dbf_info_ps->m_field_decimals_pi[field_];

// 	if (fieldname_ != NULL)
//     {
//         int i;
//         strncpy( fieldname_, (char *)m_dbf_info_ps->m_header_ptr + field_*32, 11 );
//         fieldname_[11] = '\0';

//         for(i = 10; i > 0 && fieldname_[i] == ' '; i--)
//             fieldname_[i] = '\0';
//     }
        
//     if ( m_dbf_info_ps->m_field_type_ptr[field_] == 'N'
//       || m_dbf_info_ps->m_field_type_ptr[field_] == 'F'
//       || m_dbf_info_ps->m_field_type_ptr[field_] == 'D' )
//     {
//         if ( m_dbf_info_ps->m_field_decimals_pi[field_] > 0 )
//             return ( SIS_DBF_FIELD_TYPE_DOUBLE );
//         else
//             return ( SIS_DBF_FIELD_TYPE_UINT );
//     }else
//     {
//         return ( SIS_DBF_FIELD_TYPE_STRING );
//     }
// }

// int c_dbf_option::get_field_index( const char *fieldname_)
// {
//     if (m_dbf_info_ps == NULL) return -1;

//     char      name[12],name1[12],name2[12];
//     int       i;

//     strncpy(name1, fieldname_, 11);
//     name1[11]='\0';

//     str_to_upper(name1);

//     for( i = 0; i < get_fields_count(); i++ )
//     {
//         get_field_info( i, name, NULL, NULL);
//         strncpy(name2, name, 11);
//         name2[11]='\0';
//         str_to_upper(name2);

//         if( !strncmp(name1, name2, 10))
//             return (i);
//     }
//     return (-1);
// }

// bool c_dbf_option::read_field_attribute( int entity_,  int field_, char * buffer_)
// {
//     if (m_dbf_info_ps == NULL) return FALSE;
//     int       record_offset;
//     uint8    *record_pby;
    
//     if ( entity_ < 0 || entity_ >= m_dbf_info_ps->records )
//         return  FALSE;
//     if ( field_ < 0  || field_  >= m_dbf_info_ps->m_fields_i )
//         return FALSE;
//     if ( m_dbf_info_ps->current_record_i != entity_ )
//     {
//         record_offset = m_dbf_info_ps->record_len * entity_ + m_dbf_info_ps->header_len;
// 		if(m_dbf_info_ps->m_fp)
// 		{//文件
// 			if ( fseek( m_dbf_info_ps->m_fp, record_offset, 0 ) != 0 )
// 			{
// 				return FALSE;
// 			}
// 			if ( fread( m_dbf_info_ps->m_current_record_ptr, m_dbf_info_ps->record_len, 1, m_dbf_info_ps->m_fp ) != 1 )
// 			{
// 				return FALSE;
// 			}
// 		}else
// 		{//内存
// 			if (record_offset > m_dbf_info_ps->m_dbf_buffer_size_i)
// 			{//超出内存区域 
// 				return FALSE;
// 			}
// 			memmove(m_dbf_info_ps->m_current_record_ptr, m_dbf_info_ps->m_dbf_buffer_ptr+record_offset, m_dbf_info_ps->record_len);
// 		}
//         m_dbf_info_ps->current_record_i = entity_;
//     }
//     record_pby = (uint8 *) m_dbf_info_ps->m_current_record_ptr;
    
//     strncpy( buffer_,
//         ((const char *)record_pby)+m_dbf_info_ps->m_field_offset_pi[field_],
//         m_dbf_info_ps->m_field_size_pi[field_] );
//     buffer_[m_dbf_info_ps->m_field_size_pi[field_]] = '\0';
//     return TRUE;
// }

// void * c_dbf_option::read_attribute( int entity_, int field_, char reqtype_)
// {
//     if (m_dbf_info_ps == NULL) return NULL;

//     int       record_offset;
//     uint8     *record_pby;
//     void     *return_field = NULL;

//     if ( entity_ < 0 || entity_ >= m_dbf_info_ps->records )
//         return NULL;
//     if ( field_ < 0  || field_  >= m_dbf_info_ps->m_fields_i )
//         return NULL;
//     if ( m_dbf_info_ps->current_record_i != entity_ )
//     {
//         record_offset = m_dbf_info_ps->record_len * entity_ + m_dbf_info_ps->header_len;
// 		if (m_dbf_info_ps->m_fp)
// 		{//文件
// 			if ( fseek( m_dbf_info_ps->m_fp, record_offset, 0 ) != 0 )
// 			{
// 				return NULL;
// 			}
// 			if ( fread( m_dbf_info_ps->m_current_record_ptr, m_dbf_info_ps->record_len, 1, m_dbf_info_ps->m_fp ) != 1 )
// 			{
// 				return NULL;
// 			}
// 		}else
// 		{//内存
// 			if (record_offset > m_dbf_info_ps->m_dbf_buffer_size_i)
// 			{//超出内存区域 
// 				return FALSE;
// 			}
// 			memmove(m_dbf_info_ps->m_current_record_ptr, m_dbf_info_ps->m_dbf_buffer_ptr+record_offset, m_dbf_info_ps->record_len);

// 		}
//         m_dbf_info_ps->current_record_i = entity_;
//     }
//     record_pby = (uint8 *) m_dbf_info_ps->m_current_record_ptr;

//     if ( m_dbf_info_ps->m_field_size_pi[field_]+1 > m_string_field_len_i )
//     {
//         m_string_field_len_i = m_dbf_info_ps->m_field_size_pi[field_]*2 + 10;
// 		m_string_field_ptr = (char *)zrealloc(m_string_field_ptr, m_string_field_len_i);
//     }
//     strncpy( m_string_field_ptr,
//              ((const char *)record_pby)+m_dbf_info_ps->m_field_offset_pi[field_],
//              m_dbf_info_ps->m_field_size_pi[field_] );
//     m_string_field_ptr[m_dbf_info_ps->m_field_size_pi[field_]] = '\0';
//     return_field = m_string_field_ptr;
//     if ( reqtype_ == 'N' )
//     {
//         m_double_field_d = atof(m_string_field_ptr);
//         return_field = &m_double_field_d;
//     }
//     return (return_field);
// }
// int c_dbf_option::read_integer_attribute(int record_, int field_)
// {
//     if (m_dbf_info_ps == NULL) return 0;

//     double *value;
// 	value = (double *)read_attribute(record_, field_, 'N');
// 	if (value == NULL)
//         return 0;
//     else
// 		return((int)*value);
// }
// double c_dbf_option::read_double_attribute(int record_, int field_)
// {
//     if (m_dbf_info_ps == NULL) return 0;

//     double *pdValue;
// 	pdValue = (double *)read_attribute(record_, field_, 'N');
//     if ( pdValue == NULL )
//         return 0;
//     else
//         return(*pdValue);
// }
// const char * c_dbf_option::read_string_attribute(int record_, int field_)
// {
//     if (m_dbf_info_ps == NULL) return NULL;
// 	return((const char *)read_attribute(record_, field_, 'C'));
// }
// int c_dbf_option::is_null_attribute(int record_, int field_)
// {
//     if (m_dbf_info_ps == NULL) return FALSE;
//     const char *pszValue;
// 	pszValue = read_string_attribute(record_, field_);
//     switch(m_dbf_info_ps->m_field_type_ptr[field_])
//     {
//         case 'N':
//         case 'F':
//             return (pszValue[0] == '*');
//         case 'D':
//             return (strncmp(pszValue, "00000000", 8) == 0);
//         case 'L':
//             return (pszValue[0] == '?');
//         default:
//             return (strlen(pszValue) == 0);
            
//     }
//     return FALSE;
// }

// int c_dbf_option::is_delete_record(int record_)
// {
//     if (m_dbf_info_ps == NULL) return TRUE;
    
//     int       record_offset;
//     uint8        *record_pby;
    
// 	if (record_ < 0 || record_ >= m_dbf_info_ps->records)
//         return TRUE;
// 	if (m_dbf_info_ps->current_record_i != record_)
//     {
//         record_offset = m_dbf_info_ps->record_len * record_ + m_dbf_info_ps->header_len;
// 		if (m_dbf_info_ps->m_fp)
// 		{
// 			if ( fseek( m_dbf_info_ps->m_fp, record_offset, 0 ) != 0 )
// 			{
// 				return TRUE;
// 			}
// 			if ( fread( m_dbf_info_ps->m_current_record_ptr, m_dbf_info_ps->record_len, 1, m_dbf_info_ps->m_fp ) != 1 )
// 			{
// 				return TRUE;
// 			}
// 		}else
// 		{
// 			if (record_offset > m_dbf_info_ps->m_dbf_buffer_size_i)
// 			{//超出内存区域 
// 				return FALSE;
// 			}
// 			memmove(m_dbf_info_ps->m_current_record_ptr, m_dbf_info_ps->m_dbf_buffer_ptr+record_offset, m_dbf_info_ps->record_len);
// 		}
//         m_dbf_info_ps->current_record_i = record_;
//     }
//     record_pby = (uint8 *) m_dbf_info_ps->m_current_record_ptr;
    
//     if (record_pby[0] == '*') return TRUE;
//     else return FALSE;

// }


