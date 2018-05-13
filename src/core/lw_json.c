

#include "lw_json.h"
#include "zmalloc.h"

#include <math.h>
#include <float.h>

#ifdef _MSC_VER
#pragma warning( disable :  4996 )
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
static const char *jerror;

//////////////////////////////////////////////
//   base function define
///////////////////////////////////////////////
static int dp_json_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;	if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

static char* dp_json_strdup(const char* str, size_t len_)
{
	size_t len = len_;
	if (!len_) len = strlen(str) ;
	char* copy = (char*)zmalloc(len + 1);
	memcpy(copy, str, len);
	copy[len] = 0;
	return copy;
}
int  dp_json_array_get_size(s_json_node *array_)
{
	s_json_node *c = array_->child; 
	int i = 0; 
	while (c) { i++, c = c->next; } 
	return i;
}
s_json_node *dp_json_array_get_item(s_json_node *array_, int index_)
{
	s_json_node *c = array_->child;  
	while (c && index_ > 0) { index_--, c = c->next; }
	return c;
}
s_json_node *dp_json_object_get_item(s_json_node *object, const char *key_)
{
	s_json_node *c = object->child;
	while (c && dp_json_strcasecmp(c->key_r, key_))  { c = c->next; }
	return c;
}
s_json_node *dp_json_next_item(s_json_node *now_)
{
	s_json_node *c = now_->child;
	if (c) { c = c->next; }
	return c;
}
int str_indexof(const char *s, char c)
{
	int len = (int)strlen(s);
	for (int i = 0; i < len; i++)
	{
		if (s[i] == c) return i;
	}
	return -1;
}
s_json_node *dp_json_find_item(s_json_node *node,const char *path_)
{
	if (node == NULL) return NULL;
	int pos = str_indexof(path_, '.');
	if (pos >= 0) 
	{
		char key[128], val[128];
		int len = pos;
		len = len > 127 ? 127 : len;
		strncpy(key, path_, len);  key[len] = 0;
		node = dp_json_object_get_item(node, key);
		len = (int)strlen(path_) - pos - 1;
		len = len > 127 ? 127 : len;
		strncpy(val, path_ + pos + 1, len);  val[len] = 0;
		return dp_json_find_item(node, val);
	}
	return dp_json_object_get_item(node, path_);
}
static s_json_node *dp_json_create_item(void)
{
	s_json_node* node = (s_json_node*)zmalloc(sizeof(s_json_node));
	memset(node, 0, sizeof(s_json_node));
	return node;
}

/* jump whitespace and cr/lf */
static const char *skip(const char *in) { while (in && *in && (unsigned char)*in <= 32) in++; return in; }

static const char *parse_value(s_json_node *item, const char *value, dp_json_read_handle *handle);
bool dp_json_parse(dp_json_read_handle *handle, const char *content_);
//////////////////////////////////////////////
//   output main function define
///////////////////////////////////////////////

dp_json_read_handle *dp_json_open(const char *filename_)
{
	FILE* fp = 0;
#if( _MSC_VER >= 1400 )
	fopen_s(&fp, filename_, "rb");
#else
	fp = fopen(filename_, "rb");
#endif
	printf("file=%s,fp=%p\n", filename_, fp);
	if (!fp) { return 0; }
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	//if (len == 0) return 0;
	char* copy = (char*)zmalloc(len + 1);
	memset(copy, 0, len + 1);
	size_t nbyte = fread(copy, 1, len, fp);
	fclose(fp);

	if (nbyte == 0) {
		zfree(copy);
		return 0;
	}
	dp_json_read_handle* handle = dp_json_load(copy); 
	zfree(copy);
	return handle;
}
dp_json_read_handle *dp_json_load(const char *content_)
{
	if (!content_) { return NULL; }
	struct dp_json_read_handle* handle = (dp_json_read_handle *)zmalloc(sizeof(dp_json_read_handle));
	memset(handle, 0, sizeof(dp_json_read_handle));
	handle->content = (char*)zmalloc(strlen(content_) + 1); //dp_json_strdup(content_, strlen(content_));
	if (!dp_json_parse(handle, content_)){
		dp_json_close(handle);
		return NULL;
	};
	//printf("node=%p\n", handle->node);
	return handle;
}

void dp_json_close(dp_json_read_handle *handle_)
{
	if (!handle_) { return; }
	if (handle_->content) { zfree(handle_->content); }
	dp_json_free(handle_->node);
	zfree(handle_);
}

void dp_json_free(s_json_node *c)
{
	s_json_node *next;
	while (c)
	{
		next = c->next;
		if (c->child) { dp_json_free(c->child); }
		if (c->key_w) { zfree(c->key_w); }
		if (c->value_w) { zfree(c->value_w); }
		zfree(c);
		c = next;
	}
}
///////////////////////////////////////////////////
//  read  option
///////////////////////////////////////////////////

int   dp_json_get_int(s_json_node *root, const char *key_, int defaultvalue)
{
	s_json_node *c = dp_json_object_get_item(root, key_);
	if (c){
		return atoi(c->value_r);
	}
	return defaultvalue;
}
double dp_json_get_double(s_json_node *root, const char *key_, double defaultvalue)
{
	s_json_node *c = dp_json_object_get_item(root, key_);
	if (c){
		return atof(c->value_r);
	}
	return defaultvalue;
}
const char  *dp_json_get_str(s_json_node *root, const char *key_)
{
	s_json_node *c = dp_json_object_get_item(root, key_);
	if (c){
		return c->value_r;
	}
	return NULL;
}
bool  dp_json_get_bool(s_json_node *root, const char *key_, bool defaultvalue)
{
	s_json_node *c = dp_json_object_get_item(root, key_);
	if (c){
		if (!dp_json_strcasecmp(c->value_r, "1") || !dp_json_strcasecmp(c->value_r, "true"))
		{
			return true;
		}
		return false;
	}
	return defaultvalue;
}
int   dp_json_get_array_int(s_json_node *root, int index_, int defaultvalue)
{
	s_json_node *c = dp_json_array_get_item(root, index_);
	if (c){
		return atoi(c->value_r);
	}
	return defaultvalue;	
}
double dp_json_get_array_double(s_json_node *root, int index_, double defaultvalue)
{
	s_json_node *c = dp_json_array_get_item(root, index_);
	if (c){
		return atof(c->value_r);
	}
	return defaultvalue;	
}
const char  *dp_json_get_array_str(s_json_node *root, int index_)
{
	s_json_node *c = dp_json_array_get_item(root, index_);
	if (c){
		return c->value_r;
	}
	return NULL;	
}
bool dp_json_get_array_bool(s_json_node *root, int index_, bool defaultvalue)
{
	s_json_node *c = dp_json_array_get_item(root, index_);
	if (c){
		if (!dp_json_strcasecmp(c->value_r, "1") || !dp_json_strcasecmp(c->value_r, "true"))
		{
			return true;
		}
		return false;
	}
	return defaultvalue;	
}
#define __FAST__

static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(s_json_node *item, const char *str, int iskey, dp_json_read_handle *handle)
{
#ifdef __FAST__
	if (*str != '\"') { jerror = str; return 0; }	/* not a string! */

	const char *ptr = str + 1;  int len = 0;
	//while (*ptr != '\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;	/* Skip escaped quotes. */
	ptr = str + 1;
	while (*ptr != '\"' && *ptr)
	{
		ptr++;	len++;
	}
	char *out = handle->content + handle->position;
	memcpy(out,str + 1,len);
	*(handle->content + handle->position + len) = 0;
	if (*ptr == '\"') { ptr++; }
#else
	const char *ptr = str + 1; char *ptr2; char *out; int len = 0; unsigned uc, uc2;
	if (*str != '\"') { jerror = str; return 0; }	/* not a string! */

	while (*ptr != '\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;	/* Skip escaped quotes. */

	out = handle->content + handle->position;	/* This is how long we need for the string, roughly. */

	ptr = str + 1; ptr2 = out;
	while (*ptr != '\"' && *ptr)
	{
		if (*ptr != '\\') *ptr2++ = *ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
			case 'b': *ptr2++ = '\b';	break;
			case 'f': *ptr2++ = '\f';	break;
			case 'n': *ptr2++ = '\n';	break;
			case 'r': *ptr2++ = '\r';	break;
			case 't': *ptr2++ = '\t';	break;
			case 'u':	 /* transcode utf16 to utf8. */
				sscanf(ptr + 1, "%4x", &uc); ptr += 4;	/* get the unicode char. */

				if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)	break;	/* check for invalid.	*/

				if (uc >= 0xD800 && uc <= 0xDBFF)	/* UTF16 surrogate pairs.	*/
				{
					if (ptr[1] != '\\' || ptr[2] != 'u')	break;	/* missing second-half of surrogate.	*/
					sscanf(ptr + 3, "%4x", &uc2); ptr += 6;
					if (uc2<0xDC00 || uc2>0xDFFF)		break;	/* invalid second-half of surrogate.	*/
					uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
				}

				len = 4; if (uc < 0x80) len = 1; else if (uc < 0x800) len = 2; else if (uc < 0x10000) len = 3; ptr2 += len;

				switch (len) {
				case 4: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
				case 3: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
				case 2: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
				case 1: *--ptr2 = (uc | firstByteMark[len]);
				}
				ptr2 += len;
				break;
			default:  *ptr2++ = *ptr; break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
#endif
	if (*ptr == '\"') { ptr++; }
	if (iskey){
		item->key_r = out;
		handle->position += len + 1;
		//printf("item->key =%s.%d\n", item->key_r, len);
	}
	else{
		item->value_r = out;
		handle->position += len + 1;
		//printf("item->value =%s.%d  ptr=[%c]\n", item->value_r, len, *ptr);
	}
	item->type = DP_JSON_STRING;
	return ptr;
}

static const char *parse_number(s_json_node *item, const char *num, dp_json_read_handle *handle)
{

#ifdef __FAST__
	const char *ptr = num ;  size_t len = 0;

	int isfloat = 0;
	/* Could use sscanf for this? */
	if (*ptr == '-') { ptr++; }/* Has sign? */
	if (*ptr == '0') { ptr++; }			/* is zero */
	if (*ptr >= '1' && *ptr <= '9')	{ do	ptr++;	while (*ptr >= '0' && *ptr <= '9'); }/* Number? */
	if (*ptr == '.' && ptr[1] >= '0' && ptr[1] <= '9') { isfloat = 1;  ptr++;	do	ptr++;	while (*ptr >= '0' && *ptr <= '9'); }	/* Fractional part? */
	if (*ptr == 'e' || *ptr == 'E')		/* Exponent? */
	{
		ptr++; if (*ptr == '+' || *ptr == '-') ptr++;		/* With sign? */
		while (*ptr >= '0' && *ptr <= '9') {
			ptr++;	/* Number? */
		}
	}
	len = ptr - num;

	memcpy(handle->content + handle->position, num, len);
	*(handle->content + handle->position + len) = 0;
	item->value_r = handle->content + handle->position;
	handle->position += (int)len + 1;

	if (!isfloat)
	{
		item->type = DP_JSON_INT;
	}
	else
	{
		item->type = DP_JSON_DOUBLE;
	}
	return ptr;
#else
double n = 0, sign = 1, scale = 0; int subscale = 0, signsubscale = 1;
	int isfloat = 0;
	/* Could use sscanf for this? */
	if (*num == '-') sign = -1, num++;	/* Has sign? */
	if (*num == '0') num++;			/* is zero */
	if (*num >= '1' && *num <= '9')	do	n = (n*10.0) + (*num++ - '0');	while (*num >= '0' && *num <= '9');	/* Number? */
	if (*num == '.' && num[1] >= '0' && num[1] <= '9') { isfloat = 1;  num++;		do	n = (n*10.0) + (*num++ - '0'), scale--; while (*num >= '0' && *num <= '9'); }	/* Fractional part? */
	if (*num == 'e' || *num == 'E')		/* Exponent? */
	{
		num++; if (*num == '+') num++;	else if (*num == '-') signsubscale = -1, num++;		/* With sign? */
		while (*num >= '0' && *num <= '9') subscale = (subscale * 10) + (*num++ - '0');	/* Number? */
	}

	n = sign*n*pow(10.0, (scale + subscale*signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */

	static char value[64];
	if (!isfloat)
	{
		sprintf(value, "%d", (int)n);
		int len = strlen(value);
		memcpy(handle->content + handle->position, value, len);
		*(handle->content + handle->position + len) = 0;
		item->value_r = handle->content + handle->position;
		handle->position += len + 1;
		item->type = DP_JSON_INT;
	}
	else
	{
		if (fabs(floor(n) - n) <= DBL_EPSILON && fabs(n)<1.0e60)sprintf(value, "%.0f", n);
		else if (fabs(n)<1.0e-6 || fabs(n)>1.0e9)			sprintf(value, "%e", n);
		else												sprintf(value, "%f", n);
		int len = strlen(value);
		memcpy(handle->content + handle->position, value, len);
		*(handle->content + handle->position + len) = 0;
		item->value_r = handle->content + handle->position;
		handle->position += len + 1;
		item->type = DP_JSON_DOUBLE;
	}
	return num;
#endif
}

/* Build an array from input text. */
static const char *parse_array(s_json_node *item, const char *value, dp_json_read_handle *handle)
{
	if (*value != '[')	{ jerror = value; return 0; }	/* not an array! */

	item->type = DP_JSON_ARRAY;
	value = skip(value + 1);
	if (*value == ']') { return value + 1; }	/* empty array. */

	struct s_json_node *child;
	item->child = child = dp_json_create_item();
	value = skip(parse_value(child, skip(value), handle));	/* skip any spacing, get the value. */
	if (!value) { return 0; }

	while (*value == ',')
	{
		s_json_node *new_item = dp_json_create_item();
		child->next = new_item; new_item->prev = child; child = new_item;
		value = skip(parse_value(child, skip(value + 1), handle));
		if (!value) { return 0; }/* memory fail */
	}

	if (*value == ']') { return value + 1; }	/* end of array */
	jerror = value; return 0;	/* malformed. */
}
/* Build an object from the text. */
static const char *parse_object(s_json_node *item, const char *value, dp_json_read_handle *handle)
{
	if (*value != '{')	{ jerror = value; return 0; }	/* not an object! */

	item->type = DP_JSON_OBJECT;
	value = skip(value + 1);
	if (*value == '}') { return value + 1; }/* empty array. */

	struct s_json_node *child;
	item->child = child = dp_json_create_item();
	value = skip(parse_string(child, skip(value),1 ,handle));
	if (!value) { return 0; }
	if (*value != ':') { jerror = value; return 0; }	/* fail! */
	value = skip(parse_value(child, skip(value + 1), handle));	/* skip any spacing, get the value. */
	if (!value) { return 0; }

	while (*value == ',')
	{
		struct s_json_node *new_item = dp_json_create_item();
		child->next = new_item; new_item->prev = child; child = new_item;
		value = skip(parse_string(child, skip(value + 1), 1, handle));
		//printf("1.5++%c++%p\n", *value, value);
		if (!value) { return 0; }
		if (*value != ':') { jerror = value; return 0; }	/* fail! */
		value = skip(parse_value(child, skip(value + 1), handle));	/* skip any spacing, get the value. */
		//printf("2++%c++%p\n", *value, value);
		if (!value) { return 0; }
	}

	if (*value == '}') { return value + 1; }/* end of array */
	jerror = value; 
	return 0;	/* malformed. */
}
static const char *parse_value(s_json_node *item, const char *value, dp_json_read_handle *handle)
{
	if (!handle) { return 0; }
	//printf("value=%c, \n", *value);
	if (!value)						 { return 0; }	/* Fail on null. */
	if (*value == '\"')				 { return parse_string(item, value, 0, handle); }
	if (*value == '-' || (*value >= '0' && *value <= '9'))	{ return parse_number(item, value, handle); }
	if (*value == '[')				 { return parse_array(item, value, handle); }
	if (*value == '{')				 { return parse_object(item, value, handle); }

	/*	if ((*value >= 'a' && *value <= 'z') || (*value >= 'A' && *value <= 'Z')){return parse_string(item, value);}*/
	jerror = value;
	return 0;	/* failure. */
}

bool dp_json_parse(dp_json_read_handle *handle,const char *content_)
{
	const char *end = 0;
	struct s_json_node *c = dp_json_create_item();
	jerror = 0;
	if (!c) { return false; }     /* memory fail */

	end = parse_value(c, skip(content_), handle);
	//printf("end=%p, c=%p\n", end, c);

	if (!end)	{ dp_json_free(c); return false; }	/* parse failure. jerror is set. */
	handle->node = c;
	return true;
}

//////////////////////////////////////////////
//   write function define
///////////////////////////////////////////////

void  dp_json_save(s_json_node *node_, const char *outfilename_)
{
	FILE* fp = 0;
#if( _MSC_VER >= 1400 )
	fopen_s(&fp, outfilename_, "wb");
#else
	fp = fopen(outfilename_, "wb");
#endif
	char* copy = dp_json_print(node_);
	fwrite(copy, 1, strlen(copy), fp);
	fclose(fp);
	zfree(copy);
}

s_json_node *dp_json_create_array(void)
{
	s_json_node *item = dp_json_create_item(); 
	if (item) {
		item->type = DP_JSON_ARRAY;
	}
	return item;
}
s_json_node *dp_json_create_object(void)
{
	s_json_node *item = dp_json_create_item(); 
	if (item){
		item->type = DP_JSON_OBJECT;
	}
	return item;
}

/* Utility for array list handling. */
static void suffix_object(s_json_node *prev, s_json_node *item) { prev->next = item; item->prev = prev; }

/* Add item to array/object. */
void   dp_json_array_add_item(s_json_node *array_, s_json_node *item)
{ 
	if (!item) { return; }
	struct s_json_node *c = array_->child;  
	if (!c) { array_->child = item; }
	else 
	{ 
		while (c && c->next) {
			c = c->next;
		}
		suffix_object(c, item); 
	} 
}
void   dp_json_object_add_item(s_json_node *object, const char *key_, s_json_node *item)
{ 
	if (!item) { return; }
	if (item->key_w) { zfree(item->key_w); }
	item->key_w = dp_json_strdup(key_, 0);
	item->key_r = item->key_w;
	dp_json_array_add_item(object, item);
}
//剥离数组中的某条记录，
s_json_node *dp_json_array_detach_item(s_json_node *array_, int which)			
{
	s_json_node *c = array_->child; 
	while (c && which > 0) {
		c = c->next;
		which--;
	}
	if (!c) { return 0; }
	if (c->prev) { c->prev->next = c->next; }
	if (c->next) { c->next->prev = c->prev; }
	if (c == array_->child) { array_->child = c->next; }
	c->prev = c->next = 0; 
	return c;
}
void   dp_json_array_delete_item(s_json_node *array_, int which) //删除数组中某一项			
{ 
	dp_json_free(dp_json_array_detach_item(array_, which));
}
s_json_node *dp_json_object_detach_item(s_json_node *object, const char *key_) 
{
	int i = 0; 
	s_json_node *c = object->child; 
	while (c && dp_json_strcasecmp(c->key_r, key_)) {
		i++;
		c = c->next;
	}
	if (c) { return dp_json_array_detach_item(object, i); }
	return 0;
}
void   dp_json_object_delete_item(s_json_node *object, const char *key_) 
{ 
	dp_json_free(dp_json_object_detach_item(object, key_));
}

void   dp_json_array_set_item(s_json_node *array_, int which, s_json_node *newitem)
{
	s_json_node *c = array_->child;
	while (c && which > 0) {
		c = c->next;
		which--;
	}
	if (!c) { return; }
	newitem->next = c->next;
	newitem->prev = c->prev;
	if (newitem->next) { newitem->next->prev = newitem; }
	if (c == array_->child) { 
		array_->child = newitem; 
	}
	else { 
		newitem->prev->next = newitem; 
	}
	c->next = c->prev = 0;
	dp_json_free(c);
}
void dp_json_object_set_item(s_json_node *object, const char *key_, s_json_node *newitem)
{
	int i = 0; s_json_node *c = object->child; 
	while (c && dp_json_strcasecmp(c->key_r, key_)) i++, c = c->next; 
	if (c)
	{ 
		newitem->key_r = dp_json_strdup(key_, 0); 
		dp_json_array_set_item(object, i, newitem); 
	}
	else {
		dp_json_object_add_item(object, key_, newitem);
	}
}
void dp_json_array_add_int(s_json_node *node_, long value_)
{
	s_json_node *item = dp_json_create_item();
	if (item)
	{
		item->type = DP_JSON_INT;
		item->value_w = (char*)zmalloc(21);	/* 2^64+1 can be represented in 21 chars. */
		sprintf(item->value_w, "%ld", value_);
		item->value_r = item->value_w;
	}
	dp_json_array_add_item(node_, item);
}
void dp_json_array_add_double(s_json_node *node_, double value_)
{
	s_json_node *item = dp_json_create_item();
	if (item)
	{
		item->value_w = (char*)zmalloc(64);	/* This is a nice tradeoff. */
		if (fabs(floor(value_) - value_) <= DBL_EPSILON && fabs(value_)<1.0e60)	sprintf(item->value_w, "%.0f", value_);
		else if (fabs(value_)<1.0e-6 || fabs(value_)>1.0e9)			sprintf(item->value_w, "%e", value_);
		else												sprintf(item->value_w, "%.4f", value_);
		item->value_r = item->value_w;
		item->type = DP_JSON_DOUBLE;
	}
	dp_json_array_add_item(node_, item);
}
void dp_json_array_add_string(s_json_node *node_, const char *value_)
{
	s_json_node *item = dp_json_create_item();
	if (item)
	{
		item->type = DP_JSON_STRING;
		item->value_w = dp_json_strdup(value_, 0);
		item->value_r = item->value_w;
	}
	dp_json_array_add_item(node_, item);
}

void dp_json_set_int(s_json_node *node_, const char *key_, long value_)
{
	s_json_node *item = dp_json_create_item();
	if (item)
	{
		item->type = DP_JSON_INT;
		item->value_w = (char*)zmalloc(21);	/* 2^64+1 can be represented in 21 chars. */
		sprintf(item->value_w, "%ld", value_);
		item->value_r = item->value_w;
	}
	dp_json_object_set_item(node_, key_, item);
}
void dp_json_set_double(s_json_node *node_, const char *key_, double value_, int demical)
{
	s_json_node *item = dp_json_create_item();
	if (item)
	{
		item->value_w = (char*)zmalloc(64);	/* This is a nice tradeoff. */
		switch (demical)
		{
		case 0:
			sprintf(item->value_w, "%.0f", value_);
			break;
		case 1:
			sprintf(item->value_w, "%.1f", value_);
			break;
		case 2:
			sprintf(item->value_w, "%.2f", value_);
			break;
		case 3:
			sprintf(item->value_w, "%.3f", value_);
			break;
		case 4:
			sprintf(item->value_w, "%.4f", value_);
			break;
		default:
			sprintf(item->value_w, "%.f", value_);
			break;
		}

		//if (fabs(floor(value_) - value_) <= DBL_EPSILON && fabs(value_)<1.0e60)	sprintf(item->value_w, "%.0f", value_);
		//else if (fabs(value_)<1.0e-6 || fabs(value_)>1.0e9)			sprintf(item->value_w, "%e", value_);
		//else												sprintf(item->value_w, "%.f", value_);
		item->value_r = item->value_w;
		item->type = DP_JSON_DOUBLE;
	}
	dp_json_object_set_item(node_, key_, item);
}
void dp_json_set_string(s_json_node *node_, const char *key_, const char *value_)
{
	s_json_node *item = dp_json_create_item();
	if (item)
	{
		item->type = DP_JSON_STRING;
		item->value_w = dp_json_strdup(value_,0);
		item->value_r = item->value_w;
	}
	dp_json_object_set_item(node_, key_, item);
}

/* clone */
s_json_node *dp_json_clone(s_json_node *item, int recurse)
{
	s_json_node *newitem, *cptr, *nptr = 0, *newchild;
	/* Bail on bad ptr */
	if (!item) { return 0; }
	/* Create new item */
	newitem = dp_json_create_item();
	if (!newitem)  { return 0; }
	/* Copy over all vars */
	newitem->type = item->type;
	if (item->key_r)	
	{ 
		newitem->key_w = dp_json_strdup(item->key_r, 0);
		newitem->key_r = newitem->key_w;
	}
	if (item->value_r)
	{
		newitem->value_w = dp_json_strdup(item->value_r, 0);
		newitem->value_r = newitem->value_w;
	}
	/* If non-recursive, then we're done! */
	if (!recurse) { return newitem; }
	/* Walk the ->next chain for the child. */
	cptr = item->child;
	while (cptr)
	{
		newchild = dp_json_clone(cptr, 1);		/* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) { dp_json_free(newitem); return 0; }
		if (nptr)	{ nptr->next = newchild, newchild->prev = nptr; nptr = newchild; }	/* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else		{ newitem->child = newchild; nptr = newchild; }					/* Set newitem->child and move to it */
		cptr = cptr->next;
	}
	return newitem;
}

//////////////////////////////////////////////////////////////////////////
///   output info
/////////////////////////////////////////////////////////////////////////
static char *print_value(s_json_node *item, int depth, int fmt);

/* Render the number nicely from the given item into a string. */
static char *print_number(s_json_node *item)
{
	return dp_json_strdup(item->value_r, 0);
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(const char *str)
{
	const char *ptr; char *ptr2, *out; int len = 0; unsigned char token;

	if (!str) {
		return dp_json_strdup("", 0);
	}
	ptr = str; 
	while ((token = *ptr) && ++len) { 
		if (strchr("\"\\\b\f\n\r\t", token)) {
			len++;
		}
		else if (token < 32) {
			len += 5;
		}
		ptr++;
	}

	out = (char*)zmalloc(len + 3);
	if (!out) {
		return 0;
	}

	ptr2 = out; ptr = str;
	*ptr2++ = '\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr != '\"' && *ptr != '\\') {
			*ptr2++ = *ptr++;
		}
		else
		{
			*ptr2++ = '\\';
			switch (token = *ptr++)
			{
			case '\\':	*ptr2++ = '\\';	break;
			case '\"':	*ptr2++ = '\"';	break;
			case '\b':	*ptr2++ = 'b';	break;
			case '\f':	*ptr2++ = 'f';	break;
			case '\n':	*ptr2++ = 'n';	break;
			case '\r':	*ptr2++ = 'r';	break;
			case '\t':	*ptr2++ = 't';	break;
			default: sprintf(ptr2, "u%04x", token); ptr2 += 5;	break;	/* escape and print */
			}
		}
	}
	*ptr2++ = '\"'; *ptr2++ = 0;
	return out;
}

/* Render an array to text */
static char *print_array(s_json_node *item,int depth,int fmt)
{
	char **entries;
	char *out=0,*ptr,*ret;
	size_t len=5;
	s_json_node *child=item->child;
	int numentries=0,i=0,fail=0;
	
	/* How many entries in the array? */
	while (child) {
		numentries++;
		child = child->next;
	}
	/* Explicitly handle numentries==0 */
	if (!numentries)
	{
		out=(char*)zmalloc(3);
		if (out){ memmove(out, "[]", 2); out[2] = 0; }
		return out;
	}
	/* Allocate an array to hold the values for each */
	entries=(char**)zmalloc(numentries*sizeof(char*));
	if (!entries) return 0;
	memset(entries,0,numentries*sizeof(char*));
	/* Retrieve all the results: */
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) { len += strlen(ret) + 2 + (fmt ? 1 : 0); } 
		else { fail = 1; }
		child=child->next;
	}
	
	/* If we didn't fail, try to zmalloc the output string */
	if (!fail)  { out = (char*)zmalloc(len); }
	/* If that fails, we fail. */
	if (!out) { fail = 1; }

	/* Handle failure. */
	if (fail)
	{
		for (i = 0; i < numentries; i++) { 
			if (entries[i])  { zfree(entries[i]); } 
		}
		zfree(entries);
		return 0;
	}
	
	/* Compose the output array. */
	*out='[';
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		int el = (int)strlen(entries[i]);
		memmove(ptr, entries[i], el); 
		ptr += el;
		if (i != numentries - 1) { *ptr++ = ','; if (fmt) { *ptr++ = ' '; } *ptr = 0; }
		zfree(entries[i]);
	}
	zfree(entries);
	*ptr++=']';*ptr++=0;
	return out;
}



/* Render an object to text. */
static char *print_object(s_json_node *item,int depth,int fmt)
{
	char **entries=0,**names=0;
	char *out = 0, *ptr, *ret, *str; size_t len = 7;
	int i = 0, j;
	s_json_node *child=item->child;
	int numentries=0,fail=0;
	/* Count the number of entries. */
	while (child) {
		numentries++;
		child = child->next;
	}
	/* Explicitly handle empty object case */
	if (!numentries)
	{
		out=(char*)zmalloc(fmt?depth+3:3);
		if (!out)	{ return 0; }
		ptr=out;*ptr++='{';
		if (fmt) { *ptr++ = '\n'; for (i = 0; i < depth - 1; i++) { *ptr++ = '\t'; } }
		*ptr++='}';*ptr++=0;
		return out;
	}
	/* Allocate space for the names and the objects */
	entries=(char**)zmalloc(numentries*sizeof(char*));
	if (!entries) { return 0; }
	names=(char**)zmalloc(numentries*sizeof(char*));
	if (!names) { zfree(entries); return 0; }
	memset(entries,0,sizeof(char*)*numentries);
	memset(names,0,sizeof(char*)*numentries);

	/* Collect all the results into our arrays: */
	child=item->child;depth++;
	if (fmt) { len += depth; }
	while (child)
	{
		names[i]=str=print_string_ptr(child->key_r);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) { 
			len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0); 
		}
		else {
			fail = 1;
		}
		child=child->next;
	}
	
	/* Try to allocate the output string */
	if (!fail) { out = (char*)zmalloc(len); }
	if (!out) { fail = 1; }

	/* Handle failure */
	if (fail)
	{
		for (i = 0; i < numentries; i++) {
			if (names[i]) { zfree(names[i]); }
			if (entries[i]){ zfree(entries[i]); };
		}
		zfree(names);zfree(entries);
		return 0;
	}
	
	/* Compose the output: */
	*out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
	for (i=0;i<numentries;i++)
	{
		if (fmt) {
			for (j = 0; j < depth; j++) {
				*ptr++ = '\t';
			}
		}
		size_t ll = strlen(names[i]);
		memmove(ptr, names[i], ll); 
		ptr += ll;
		*ptr++ = ':'; 
		if (fmt) { *ptr++ = '\t'; }
		ll = strlen(entries[i]);
		memmove(ptr, entries[i], ll); 
		ptr += ll;
		if (i != numentries - 1) { *ptr++ = ','; }
		if (fmt){ *ptr++ = '\n'; }
		*ptr = 0;
		zfree(names[i]);zfree(entries[i]);
	}
	
	zfree(names);zfree(entries);
	if (fmt) {
		for (i = 0; i < depth - 1; i++) {
			*ptr++ = '\t';
		}
	}
	*ptr++='}';	*ptr++=0;
	return out;	
}


/* Render a value to text. */
static char *print_value(s_json_node *item, int depth, int fmt)
{
	char *out = 0; 
	if (!item) {
		return 0;
	}
	switch ((item->type) & 255)
	{
	case DP_JSON_INT:		out = print_number(item); break;
	case DP_JSON_DOUBLE:	out = print_number(item); break;
	case DP_JSON_STRING:	out = print_string_ptr(item->value_r); break;
	case DP_JSON_ARRAY:		out = print_array(item, depth, fmt); break;
	case DP_JSON_OBJECT:	out = print_object(item, depth, fmt); break;
	}
	return out;
}

/* Render a s_json_node item/entity/structure to text. */
char *dp_json_print(s_json_node *item)				{ return print_value(item, 0, 1); }
char *dp_json_print_unformatted(s_json_node *item)	{ return print_value(item, 0, 0); }


