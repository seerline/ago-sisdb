#include <sis_str.h>
#include <sis_map.h>

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

char *sis_strdup(const char *str_, size_t len_) SIS_MALLOC
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
	memcpy(buffer, str_, len);
	buffer[len] = 0;
	return buffer;
}

char *sis_str_sprintf(size_t mlen_, const char *fmt_, ...) SIS_MALLOC
{
	char *str = (char *)sis_malloc(mlen_ + 1);
	va_list args;
	va_start(args, fmt_);
	//snprintf(str, mlen_, fmt_, args);
	vsnprintf(str, mlen_, fmt_, args);
	va_end(args);
	str[mlen_] = 0;
	return str;
}

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
int sis_str_substr_nums(const char *s, char c)
{
	if (!s)
	{
		return 0;
	}
	int i, len, count;
	len = (int)strlen(s);
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
int sis_str_subcmp_head(const char *sub, const char *s, char c) //-1没有匹配的
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
			if (!sis_strncasecmp(sub, &s[pos], i - pos))
			{
				return count;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i > pos)
	{ //strncmp
		if (!sis_strncasecmp(sub, &s[pos], i - pos))
		{
			return count;
		}
	}
	return -1;
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
			printf("%s,  %s\n", str, sub);
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
		}
		start--;
		chars--;
		*len_ = *len_ + 1;
	}
	// start++; // 把回车跳过
	// printf("%.30s\n",start);
	// *len_ = 0;
	// lines = 2;
	// int slen = size_- (e_ - s_);
	// const char *stop = e_;
	// while(*stop&&*len_<slen&&lines > 0) {
	// 	if((unsigned char)*stop == '\n') { lines--;}
	// 	stop++;
	// 	*len_=*len_ + 1;
	// }
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

bool sis_str_get_id(char *out_, size_t olen_)
{
	// static char *sign = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
	static char *sign = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // 36
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
	out_[8] = 0;
	return true;
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
    port = atoi(url);
	printf("auth: %s  %s  %d\n", s1, s2, port);

    const char *auth = substr_; 
    auth = sis_str_parse(auth, "@", s1, 64);
	printf("auth: %s  %s\n", s1, auth);

    return 1;
}
#endif