#include <sis_str.h>
#include <sis_map.h>
#include <sis_sds.h>
#include <sis_math.h>

bool sis_str_isnumber(const char *in_, size_t ilen_)
{
    for (int i = 0; i < ilen_; i++)
    {
        if (in_[i] < '0' || in_[i] > '9')
        {
            return false;
        }
    }
    return true;
}
// 以第一个字符串为长度，进行比较
int sis_strcase_match(const char *son_, const char *source_)
{
	if (!son_)
	{
		return (son_ == source_) ? 0 : 1;
	}
	if (!source_)
	{
		return 1;
	}
	for (; tolower(*son_) == tolower(*source_); ++son_, ++source_)
	{
		if (*son_ == 0 || *source_ == 0)
		{
			return 0;
		}
	}
	if (*son_ == 0)
	{
		return 0;
	}
	return 1;
}

/**
 * @brief 比较两个字符串，比较过程忽略大小写，注意，如果两个字符串长度不一致，则比较长度按二者最小长度考虑，超过比较长度的不做比较
 * @param s1_ 
 * @param s2_ 
 * @return int 0 相等，1 不等
 */
int sis_strcasecmp(const char *s1_, const char *s2_)
{
	if (!s1_)
	{
		return (s1_ == s2_) ? 0 : 1;
	}
	if (!s2_)
	{
		return 1;
	}
	for (; tolower(*s1_) == tolower(*s2_); ++s1_, ++s2_)
	{
		if (*s1_ == 0 || *s2_ == 0)
		{
			return 0;
		}
	}
	return 1;
	//tolower(*(const unsigned char *)s1_) - tolower(*(const unsigned char *)s2_);
}
int sis_strncasecmp(const char *s1_, const char *s2_, size_t len_)
{
	if (!s1_)
	{
		return (s1_ == s2_) ? 0 : 1;
	}
	if (!s2_)
	{
		return 1;
	}
	for (size_t i = 1; tolower(*s1_) == tolower(*s2_); ++s1_, ++s2_, i++)
	{
		// printf("%d|%d|%ld|%d\n",*s1_,*s2_,len_,i);
		if (*s1_ == 0 || *s2_ == 0 || i >= len_)
		{
			return 0;
		}
	}
	return 1;
	//tolower(*(const unsigned char *)s1_) - tolower(*(const unsigned char *)s2_);
}
int sis_strncmp(const char *s1_, const char *s2_, size_t len_)
{
	if (!s1_)
	{
		return (s1_ == s2_) ? 0 : 1;
	}
	if (!s2_)
	{
		return 1;
	}
	for (size_t i = 1; (*s1_) == (*s2_); ++s1_, ++s2_, i++)
	{
		// printf("%d|%d|%ld|%d\n",*s1_,*s2_,len_,i);
		if (*s1_ == 0 || *s2_ == 0 || i >= len_)
		{
			return 0;
		}
	}
	return 1;
	//tolower(*(const unsigned char *)s1_) - tolower(*(const unsigned char *)s2_);
}

char *sis_strdup(const char *str_, size_t len_) SIS_NEW
{
	if (!str_)
	{
		return NULL;
	}
	size_t len = len_;
	if (len_ == 0)
	{
		len = strlen(str_);
	}
	char *buffer = (char *)sis_malloc(len + 1);
	memmove(buffer, str_, len);
	buffer[len] = 0;
	return buffer;
}

// char *sis_str_sprintf(size_t mlen_, const char *fmt_, ...) SIS_NEW
// {
// 	char *str = (char *)sis_malloc(mlen_ + 1);
// 	va_list args;
// 	va_start(args, fmt_);
// 	//snprintf(str, mlen_, fmt_, args);
// 	vsnprintf(str, mlen_, fmt_, args);
// 	va_end(args);
// 	str[mlen_] = 0;
// 	return str;
// }

const char *sis_str_split(const char *s_, size_t *len_, char c_)
{
	*len_ = 0;
	const char *ptr = s_;
	while (ptr && *ptr && (unsigned char)*ptr != c_)
	{
		ptr++;
		*len_ = *len_ + 1;
	}
	return ptr;
}
void sis_str_merge(char *in_, size_t ilen_, char ch_, const char *one_, const char *two_)
{
	int off = 0;
	if (one_)
	{
		size_t s1 = sis_strlen(one_);
		s1 = sis_min(s1, ilen_ - 2);
		memmove(in_, one_, s1);
		in_ += s1;  
		off += s1;
	}
	if (ch_)
	{
		*in_ = ch_; in_++;
		off++;
	}
	if (two_)
	{
		size_t s2 = sis_strlen(two_);
		s2 = sis_min(s2, ilen_ - off - 1);
		memmove(in_, two_, s2);
		in_ += s2;
	}
	*in_ = 0;
}

int sis_str_divide(const char *in_, char ch_, char *one_, char *two_)
{
	one_[0] = 0;
	two_[0] = 0;
	int index = 0;
	const char *ptr = in_;
	while (ptr && *ptr)
	{
		if (*ptr == ch_)
		{
			one_[index] = 0;
			index = 0;
			ptr++;
			while (*ptr)
			{
				two_[index] = *ptr;
				index++;
				ptr++;
			}
			two_[index] = 0;
			return 2;
		}
		else
		{
			one_[index] = *ptr;
		}
		index++;
		ptr++;
	}
	one_[index] = 0;
	return 1;
}
int sis_str_divide_sds(const char *in_, char ch_, s_sis_sds *one_,  s_sis_sds *two_)
{
    const char *start = in_;
	const char *ptr = in_;
    int count = 0;
	while (ptr && *ptr)
	{
		if (*ptr == ch_)
		{
            *one_ = count > 0 ? sis_sdsnewlen(start, count) : NULL;
			ptr++;
			start = ptr;
            count = 0;
			while (*ptr)
			{
				ptr++;
                count++;
                
			}
            *two_ = count > 0 ? sis_sdsnewlen(start, count) : NULL;
			return 2;
		}
		ptr++;
        count++;
	}
    *one_ = count > 0 ? sis_sdsnewlen(start, count) : NULL;
	return 1;
}
void sis_str_swap(const char *in_, char *out, char ic_,char oc_)
{
	char *ptr = (char *)in_;
	while (ptr && *ptr)
	{
		if ((unsigned char)*ptr == ic_)
		{
			*out = oc_;
		}
		else
		{
			*out = (unsigned char)*ptr;
		}
		ptr++;
		out++;
	}
	// out[sis_strlen(in_)] = 0;
	*out = 0;
}

const char *sis_str_replace(const char *in_, char ic_, char oc_)
{
	char *ptr = (char *)in_;
	while (ptr && *ptr)
	{
		if ((unsigned char)*ptr == ic_)
		{
			*ptr = oc_;
		}
		ptr++;
	}
	return in_;
}
void sis_str_to_upper(char *in_)
{
	int len, i = -1;
	len = (int)strlen(in_);
	while (++i < len)
	{
		if (isalpha(in_[i]) && islower(in_[i]))
		{
			in_[i] = toupper((int)in_[i]);
		}
	}
}

void sis_str_to_lower(char *in_)
{
	int len, i = -1;
	len = (int)strlen(in_);
	while (++i < len)
	{
		if (isalpha(in_[i]) && isupper(in_[i]))
		{
			in_[i] = tolower((int)in_[i]);
		}
	}
}
int sis_strsub(char *big_, char *small_)
{
	char *str = strstr(big_, small_);
	if (!str)
	{
		return -1;
	}	
	return (int)(str - big_);
}
bool sis_str_method(const char *minfo_, char *mname_, size_t mlen_, char *param_, size_t plen_)
{
	int o = 0;
	const char *ptr = minfo_;
	const char *start = minfo_;
	while (ptr && *ptr)
	{
		if (*ptr == '(')
		{
			if (ptr == start)
			{
				return false;
			}
			sis_strncpy(mname_, mlen_, start, ptr - start);
			ptr++;
			start = ptr;
			o++;
		}
		else if (*ptr == ')')
		{
			if (ptr == start)
			{
				param_[0] = 0;
			}
			else
			{
				sis_strncpy(param_, plen_, start, ptr - start);
			}
			o++; 
			break;
		}
		else
		{
			ptr++;
		}
	}
	return o == 2;
}
bool sis_str_exist_ch(const char *in_, size_t ilen_, const char *ic_, size_t clen_)
{
    for (size_t i = 0; i < ilen_; i++)
    {
		for (size_t j = 0; j < clen_; j++)
        {
			if (in_[i] == ic_[j])
			{
				return true;
			}
		}
    }
    return false;	
}

int sis_str_pos(const char *in_, size_t ilen_, char c)
{
	for(size_t i = 0; i < ilen_; i++, in_++)
	{
		if(strchr("{}[]", *in_))
		{
			break;
		}
		if(*in_ == c)
		{
			return i;
		}
	}
	return -1;
}
void sis_str_substr(char *out_, size_t olen_, const char *in_, char c, int idx_)
{
	int count = 0;
	const char *ptr = in_;
	const char *start = ptr;
	while (ptr && *ptr)
	{
		if (*ptr == c)
		{
			if (idx_ == count)
			{
				sis_strncpy(out_, olen_, start, ptr - start);
				return;
			}
			ptr++;
			start = ptr;
			count++;
		}
		else
		{
			ptr++;
		}
	}
	if (ptr > start)
	{
		sis_strncpy(out_, olen_, start, ptr - start);
	}
	else
	{
		sis_strcpy(out_, olen_, in_);
	}
}
/*从字符串中找出指定字符的出现次数，最后部分的count++逻辑看不懂
参数列表：
    s，字符串
    ilen，字符串长度
    c,目标字符
返回值：
    目标字符的出现次数，当字符串非空时，返回值不小于1
*/
int sis_str_substr_nums(const char *s, size_t ilen_, char c)
{
	if (!s)
	{
		return 0;
	}
	int i, len, count;
	len = (int)ilen_;
	for (i = 0, count = 0; i < len; i++)
	{
		if (s[i] == c)
		{
			count++;
		}
	}
	if (len > 0 && s[len - 1] != c)
	{
		count++;
	}
	return count;
}
int sis_str_subcmp(const char *sub, const char *s, char c) //-1没有匹配的
{
	if (!sub || !s)
	{
		return -1;
	}
	int i, pos, len, count;
	len = (int)strlen(s);
	pos = 0;
	for (i = 0, count = 0; i < len; i++)
	{
		if (s[i] == c)
		{
			// printf("%s,%s,%d -- %d == %d\n",sub, &s[pos], i , pos, sis_strncasecmp(sub, &s[pos], i - pos));
			if (!sis_strncasecmp(sub, &s[pos], i - pos))
			{
				return count;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i > pos)
	{
		if (!sis_strncasecmp(sub, &s[pos], i - pos))
		{
			return count;
		}
	}
	return -1;
}
int sis_str_subcmp_strict(const char *sub, const char *s, char c) 
{
	if (!sub || !s)
	{
		return -1;
	}
	int i, pos, len, count;
	int sublen = (int)strlen(sub);
	len = (int)strlen(s);
	pos = 0;
	for (i = 0, count = 0; i < len; i++)
	{
		if (s[i] == c)
		{
			if (!sis_strncasecmp(sub, &s[pos], i - pos)&&sublen==i - pos)
			{
				return count;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i > pos)
	{ //strncmp
		if (!sis_strncasecmp(sub, &s[pos], i - pos)&&sublen ==i - pos)
		{
			return count;
		}
	}
	return -1;
}
int sis_strsubcmp_last(const char *sub, const char *s, char c) 
{
	if (!sub || !s)
	{
		return -1;
	}
	int i;
	int sublen = (int)strlen(sub);
	int len = (int)strlen(s);
	for (i = len - 2; i >= 0; i--)
	{
		if (s[i] == c)
		{
			if (!sis_strncasecmp(sub, &s[i + 1], len - i - 1) && sublen == len - i - 1)
			{
				return 0;
			}
		}
	}
	if (i < 0)
	{ //strncmp
		if (!sis_strncasecmp(sub, &s[0], len) && sublen == len)
		{
			return 0;
		}
	}
	return 1;
}
// s 串为 “01,02”, 检查sub中有没有这些字串，sub=“1111“ 返回-1， ”10200“ 返回 0 表示发现字串
int sis_str_subcmp_match(const char *sub, const char *s, char c) //-1没有匹配的
{
	if (!sub || !s)
	{
		return -1;
	}
	char str[16];
	int i, pos, len, count;
	len = (int)strlen(s);
	pos = 0;
	for (i = 0, count = 0; i < len; i++)
	{
		if (s[i] == c)
		{
			sis_strncpy(str, 16, &s[pos], i - pos);
			// printf("%s,  %s\n", str, sub);
			if (strstr(sub, str))
			{
				return count;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i > pos)
	{ //strncmp
		sis_strncpy(str, 16, &s[pos], i - pos);
		if (strstr(sub, str))
		{
			return count;
		}
	}
	return -1;
}
const char *sis_str_getline(const char *e_, int *len_, const char *s_, size_t size_)
{
	if (!e_ || !s_)
	{
		return NULL;
	}

	*len_ = 0;
	int lines = 2;
	int chars = 50;

	const char *start = e_;
	while (*start && start > s_ && (lines > 0 || chars > 0))
	{
		if ((unsigned char)*start == '\n')
		{
			lines--;
			if (lines == 0) 
			{
				break;
			}
		}
		start--;
		chars--;
		*len_ = *len_ + 1;
	}
	// start++; // 把回车跳过
	// printf("%.30s\n",start);
	// *len_ = 0;
	lines = 1;
	// int slen = size_- (e_ - s_);
	const char *stop = e_;
	while (*stop && (unsigned char)(stop - s_) < (size_ - 1) && lines > 0) {
		if((unsigned char)*stop == '\n') 
		{ 
			lines--;
			if (lines == 0) 
			{
				break;
			}
		}
		stop++;
		*len_=*len_ + 1;
	}
	// printf("%.30s   %d\n",start, *len_);
	return start;
}
// 判断 字符串是否在source中， 如果在，并且完全匹配就返回0 否则返回1 没有匹配就返回-1
// source = aaa,bbbbbb,0001
// strstr不确定是否会修改source
int sis_str_match(const char *substr_, const char *source_, char c)
{
	char *s = strstr(source_, substr_);
	if (s)
	{
		int len = strlen(substr_);
		s += len;
		// if(!strcmp(substr_,"603096")||!strcmp(substr_,"KDL")) {
		// 	printf("c == %x\n",*s);
		// }
		if (!*s || *s == c)
		{
			// printf("c == %x\n",*s);
			return 0;
		}
		return 1;
	}
	return -1;
}
const char *sis_str_parse(const char *src_, const char *sign_, char *out_, size_t olen_)
{
	char *s = strstr(src_, sign_);
	if (s)
	{
		sis_strncpy(out_, olen_, src_, s - src_);
		return s + strlen(sign_);
	}
	else
	{
		sis_strncpy(out_, olen_, src_, strlen(src_));
	}
	return NULL;
}

// char *sis_strsep(char **src_, const char *sign_)
// {
// 	char *s;
// 	const char *spanp;
// 	int c, sc;
// 	char *tok;

// 	if ((s = *src_) == 0)
// 	{
// 		return 0;
// 	}

// 	tok = s;
// 	while (1)
// 	{
// 		c = *s++;
// 		spanp = sign_;
// 		do
// 		{
// 			if ((sc = *spanp++) == c)
// 			{
// 				if (c == 0)
// 					s = 0;
// 				else
// 					s[-1] = 0;
// 				*src_ = s;
// 				return tok;
// 			}
// 		} while (sc != 0);
// 	}
// }
// 年月日时分秒 YYNNDDHHMMSS
bool sis_str_get_time_id(char *out_, size_t olen_)
{
	static char *sign = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	if (olen_ < 13)
	{
		return false;
	}
	struct tm ptm = {0};
	sis_time_check(0, &ptm);
	int len = (int)strlen(sign);
	sis_sprintf(out_, olen_, "%02d%02d%02d%02d%02d%02d", 
				(ptm.tm_year - 100) , (ptm.tm_mon + 1), ptm.tm_mday,
				 ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
	for(int i = 12; i < olen_ - 1; i++)
	{
		// out_[i] = 0x30 + (rand() % 10);
		out_[i] = sign[rand() % len];
	}
	out_[olen_ - 1] = 0;
	return true;
}

void sis_str_get_random(char *out_, size_t olen_)
{
	static char *sign = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
	int maxsize = (int)strlen(sign);
	for (int i = 0; i < olen_; i++)
	{
		out_[i] = sign[rand() % maxsize];
	}
}
bool sis_str_get_id(char *out_, size_t olen_)
{
	static char *sign = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
	// static char *sign = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // 36
	if (olen_ < 9)
	{
		return false;
	}

	struct tm ptm = {0};
	sis_time_check(0, &ptm);
	int len = (int)strlen(sign);
	out_[0] = sign[(ptm.tm_year - 118) % len];
	out_[1] = sign[(ptm.tm_mon + 1) % len];
	out_[2] = sign[ptm.tm_mday];
	out_[3] = sign[ptm.tm_hour];
	out_[4] = sign[ptm.tm_min % len];
	out_[5] = sign[ptm.tm_min / len];
	out_[6] = sign[ptm.tm_sec % len];
	out_[7] = sign[ptm.tm_sec / len];
	for(int i = 8; i < olen_ - 1; i++)
	{
		out_[i] = sign[rand() % len];
	}
	
	out_[olen_ - 1] = 0;
	return true;
}

int64 sis_str_read_long(char *s) 
{
    int64 v = 0;
    int dec, mult = 1;
    char c;

    if (*s == '-') {
        mult = -1;
        s++;
    } else if (*s == '+') {
        mult = 1;
        s++;
    }

    while ((c = *(s++)) != '\r') 
	{
        dec = c - '0';
        if (dec >= 0 && dec < 10) {
            v *= 10;
            v += dec;
        } else {
            break;
        }
    }

    return mult*v;
}

void sis_str_change(char *outs_, int olen_, const char *ins_, const char *cuts_, const char *news_)
{
	outs_[0] = 0;
	int isize  = sis_strlen(ins_);
	int cutsize = sis_strlen(cuts_);
	int newsize = sis_strlen(news_);
	char *ptr = strstr(ins_, cuts_);
	if (!ptr || cutsize == 0 || isize == 0)
	{
		return ;
	}
	if (isize + newsize - cutsize >= olen_)
	{
		return ;
	}
	if (ptr == ins_)
	{
		int size = isize - cutsize;
		if (newsize > 0)
		{
			memmove(outs_, news_, newsize);
		}
		memmove(outs_ + newsize, ptr + cutsize, size);
		outs_[size + newsize] = 0;
	}
	else
	{
		int osize = ptr - ins_;
		memmove(outs_, ins_, osize);
		if (newsize > 0)
		{
			memmove(outs_ + osize, news_, newsize);
		}
		osize += newsize;
		int size = isize - cutsize;
		memmove(outs_ + osize, ptr + cutsize, size);
		osize += size;
		outs_[osize] = 0;
	}
}
// 从 V1 --> V2 头尾标记符更换
// SH600600 SH --> .SSE ==> 600600.SSE
// 600600.SSE .SSE --> SH ==> SH600600
void sis_str_swap_ht(const char *v1_, int v1len_, const char *v1sign_, int v1slen_,
	char *v2_, int v2len_, const char *v2sign_, int v2slen_)
{
    v2_[0] = 0;
	char *ptr = strstr(v1_, v1sign_);
	if (!ptr || !v2_ || v2len_ < v1len_ - v1slen_ + v2slen_)
	{
		return ;
	}
	if (ptr == v1_)
	{
		// 原始标记在头 -- 换到尾部
		int size = v1len_ - v1slen_;
		memmove(v2_, ptr + v1slen_, size);
		memmove(v2_ + size, v2sign_, v2slen_);
		v2_[size + v2slen_] = 0;
	}
	else
	{
		// 原始标记在尾部 -- 换到头部
		int size = ptr - v1_;
		memmove(v2_, v2sign_, v2slen_);
		memmove(v2_ + v2slen_,  v1_, size);
		v2_[size + v2slen_] = 0;
	}
}

// 从 V1 --> V2 头尾标记符更换
// SH600600 SH SZ--> .SSE .SZE ==> 600600.SSE
// 600600.SSE .SSE .SZE --> SH SZ ==> SH600600
void sis_str_swap_ht2(const char *v1_, int v1len_, const char *v1sign1_, const char *v1sign2_,
	char *v2_, int v2len_, const char *v2sign1_, const char *v2sign2_)
{
	v2_[0] = 0;
	if (strstr(v1_, v1sign1_))
	{
		sis_str_swap_ht(v1_, v1len_, v1sign1_, sis_strlen(v1sign1_), v2_, v2len_, v2sign1_, sis_strlen(v2sign1_));
	}
	if (strstr(v1_, v1sign2_))
	{
		sis_str_swap_ht(v1_, v1len_, v1sign2_, sis_strlen(v1sign2_), v2_, v2len_, v2sign2_, sis_strlen(v2sign2_));
	}
}

#if 0
#include <sis_time.h>

int main1()
{
    char* source_ = "blog.csdn,blog,net";
    char* substr_ = "csdn,blog"    ;

	int start = sis_time_get_isec(0);
	int count = 1;
    for(int k=0;k<count;k++){
		if (k>0)
		{
			//sis_str_find(substr_,source_);
			sis_str_match(substr_,source_,',');
		}	else {
			//printf("The First Occurence at: %d\n",sis_str_find(substr_,source_));
			printf("The First Occurence at: %d\n",sis_str_match(substr_,source_,','));
		}
	}
	printf("cost sec: %d \n",sis_time_get_isec(0) - start);

    return 1;
}
int main()
{
// s 串为 “01,02”, 检查sub中有没有这些字串，sub=“1111“ 返回-1， ”10200“ 返回 0 表示发现字串
	int o = sis_str_subcmp_match("SH600600,SH600601", "600600", ',');  //-1没有匹配的
	printf("o=%d\n", o);
	return 0;
	char cn[20];
	sis_get_spell_gbk("上证指数",cn, 12);
	printf("|%s|\n", cn);
	return 0;

	char* test = "MD001|000001|上证指数|       249282447| 189607223525.60|  2630.5195|  2600.5004|  2666.4853|  2597.3477|  2654.8795|  2654.8795|        |15:01:03.000";

	char source[] = "hello,上海, world! welcome to china!";
	char delim[] = "|";
 
	char *s = strdup(test);
	char *token;
	// for(token = sis_strsep(&s, delim); token != NULL; token = strsep(&s, delim)) {
	while ((token = sis_strsep(&s, delim)) != NULL){
		printf(token);
		printf("+");
	}
	printf("\n");

	return 0;
    char* source_ = "sdsd://127.0.0.0:10000";
    char* substr_ = "@blog"    ;
	char s1[64],s2[64];
	int port;

	char *ss = sis_str_sprintf(64, "ii=%.*f ", 2, 0);
	printf("s1: %s\n", ss);
	sis_free(ss);

    const char *url = source_; 
    url = sis_str_parse(url, "://", s1, 64);
    url = sis_str_parse(url, ":", s2, 64);
    port = sis_atoll(url);
	printf("auth: %s  %s  %d\n", s1, s2, port);

    const char *auth = substr_; 
    auth = sis_str_parse(auth, "@", s1, 64);
	printf("auth: %s  %s\n", s1, auth);

    return 1;
}
#endif