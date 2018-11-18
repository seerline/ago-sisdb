#include <sis_str.h>
#include <sis_map.h>

static struct s_sis_kv_pair _sis_map_letter_define[] = {
	{"昇", "S"},
	{"昉", "F"},
	{"重庆", "CQ"},
	{"银行", "YH"}
};

void _get_first_letter(const char* in_, char *out_, int olen_)
{
	static int letter_region[] = {
		1601, 1637, 1833, 2078, 2274, 2302, 2433, 2594, 2787, 3106, 3212,
		3472, 3635, 3722, 3730, 3858, 4027, 4086, 4390, 4558, 4684, 4925, 5249
	};
	const static char* letter_first[] = {
		"A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M", "N", "O",
		"P", "Q", "R", "S", "T", "W", "X", "Y", "Z"
	};
	const static char* letter_second =
		"CJWGNSPGCGNE[Y[BTYYZDXYKYGT[JNNJQMBSGZSCYJSYY[PGKBZGY[YWJKGKLJYWKPJQHY[W[DZLSGMRYPYWWCCKZNKYYGTTNJJNYKKZYTCJNMCYLQLYPYQFQRPZSLWBTGKJFYXJWZLTBNCXJJJJTXDTTSQZYCDXXHGCK[PHFFSS[YBGXLPPBYLL[HLXS[ZM[JHSOJNGHDZQYKLGJHSGQZHXQGKEZZWYSCSCJXYEYXADZPMDSSMZJZQJYZC[J[WQJBYZPXGZNZCPWHKXHQKMWFBPBYDTJZZKQHY"
		"LYGXFPTYJYYZPSZLFCHMQSHGMXXSXJ[[DCSBBQBEFSJYHXWGZKPYLQBGLDLCCTNMAYDDKSSNGYCSGXLYZAYBNPTSDKDYLHGYMYLCXPY[JNDQJWXQXFYYFJLEJPZRXCCQWQQSBNKYMGPLBMJRQCFLNYMYQMSQYRBCJTHZTQFRXQHXMJJCJLXQGJMSHZKBSWYEMYLTXFSYDSWLYCJQXSJNQBSCTYHBFTDCYZDJWYGHQFRXWCKQKXEBPTLPXJZSRMEBWHJLBJSLYYSMDXLCLQKXLHXJRZJMFQHXHWY"
		"WSBHTRXXGLHQHFNM[YKLDYXZPYLGG[MTCFPAJJZYLJTYANJGBJPLQGDZYQYAXBKYSECJSZNSLYZHSXLZCGHPXZHZNYTDSBCJKDLZAYFMYDLEBBGQYZKXGLDNDNYSKJSHDLYXBCGHXYPKDJMMZNGMMCLGWZSZXZJFZNMLZZTHCSYDBDLLSCDDNLKJYKJSYCJLKWHQASDKNHCSGANHDAASHTCPLCPQYBSDMPJLPZJOQLCDHJJYSPRCHN[NNLHLYYQYHWZPTCZGWWMZFFJQQQQYXACLBHKDJXDGMMY"
		"DJXZLLSYGXGKJRYWZWYCLZMSSJZLDBYD[FCXYHLXCHYZJQ[[QAGMNYXPFRKSSBJLYXYSYGLNSCMHZWWMNZJJLXXHCHSY[[TTXRYCYXBYHCSMXJSZNPWGPXXTAYBGAJCXLY[DCCWZOCWKCCSBNHCPDYZNFCYYTYCKXKYBSQKKYTQQXFCWCHCYKELZQBSQYJQCCLMTHSYWHMKTLKJLYCXWHEQQHTQH[PQ[QSCFYMNDMGBWHWLGSLLYSDLMLXPTHMJHWLJZYHZJXHTXJLHXRSWLWZJCBXMHZQXSDZP"
		"MGFCSGLSXYMJSHXPJXWMYQKSMYPLRTHBXFTPMHYXLCHLHLZYLXGSSSSTCLSLDCLRPBHZHXYYFHB[GDMYCNQQWLQHJJ[YWJZYEJJDHPBLQXTQKWHLCHQXAGTLXLJXMSL[HTZKZJECXJCJNMFBY[SFYWYBJZGNYSDZSQYRSLJPCLPWXSDWEJBJCBCNAYTWGMPAPCLYQPCLZXSBNMSGGFNZJJBZSFZYNDXHPLQKZCZWALSBCCJX[YZGWKYPSGXFZFCDKHJGXDLQFSGDSLQWZKXTMHSBGZMJZRGLYJB"
		"PMLMSXLZJQQHZYJCZYDJWBMYKLDDPMJEGXYHYLXHLQYQHKYCWCJMYYXNATJHYCCXZPCQLBZWWYTWBQCMLPMYRJCCCXFPZNZZLJPLXXYZTZLGDLDCKLYRZZGQTGJHHGJLJAXFGFJZSLCFDQZLCLGJDJCSNZLLJPJQDCCLCJXMYZFTSXGCGSBRZXJQQCTZHGYQTJQQLZXJYLYLBCYAMCSTYLPDJBYREGKLZYZHLYSZQLZNWCZCLLWJQJJJKDGJZOLBBZPPGLGHTGZXYGHZMYCNQSYCYHBHGXKAMTX"
		"YXNBSKYZZGJZLQJDFCJXDYGJQJJPMGWGJJJPKQSBGBMMCJSSCLPQPDXCDYYKY[CJDDYYGYWRHJRTGZNYQLDKLJSZZGZQZJGDYKSHPZMTLCPWNJAFYZDJCNMWESCYGLBTZCGMSSLLYXQSXSBSJSBBSGGHFJLYPMZJNLYYWDQSHZXTYYWHMZYHYWDBXBTLMSYYYFSXJC[DXXLHJHF[SXZQHFZMZCZTQCXZXRTTDJHNNYZQQMNQDMMG[YDXMJGDHCDYZBFFALLZTDLTFXMXQZDNGWQDBDCZJDXBZGS"
		"QQDDJCMBKZFFXMKDMDSYYSZCMLJDSYNSBRSKMKMPCKLGDBQTFZSWTFGGLYPLLJZHGJ[GYPZLTCSMCNBTJBQFKTHBYZGKPBBYMTDSSXTBNPDKLEYCJNYDDYKZDDHQHSDZSCTARLLTKZLGECLLKJLQJAQNBDKKGHPJTZQKSECSHALQFMMGJNLYJBBTMLYZXDCJPLDLPCQDHZYCBZSCZBZMSLJFLKRZJSNFRGJHXPDHYJYBZGDLQCSEZGXLBLGYXTWMABCHECMWYJYZLLJJYHLG[DJLSLYGKDZPZXJ"
		"YYZLWCXSZFGWYYDLYHCLJSCMBJHBLYZLYCBLYDPDQYSXQZBYTDKYXJY[CNRJMPDJGKLCLJBCTBJDDBBLBLCZQRPPXJCJLZCSHLTOLJNMDDDLNGKAQHQHJGYKHEZNMSHRP[QQJCHGMFPRXHJGDYCHGHLYRZQLCYQJNZSQTKQJYMSZSWLCFQQQXYFGGYPTQWLMCRNFKKFSYYLQBMQAMMMYXCTPSHCPTXXZZSMPHPSHMCLMLDQFYQXSZYYDYJZZHQPDSZGLSTJBCKBXYQZJSGPSXQZQZRQTBDKYXZK"
		"HHGFLBCSMDLDGDZDBLZYYCXNNCSYBZBFGLZZXSWMSCCMQNJQSBDQSJTXXMBLTXZCLZSHZCXRQJGJYLXZFJPHYMZQQYDFQJJLZZNZJCDGZYGCTXMZYSCTLKPHTXHTLBJXJLXSCDQXCBBTJFQZFSLTJBTKQBXXJJLJCHCZDBZJDCZJDCPRNPQCJPFCZLCLZXZDMXMPHJSGZGSZZQLYLWTJPFSYASMCJBTZKYCWMYTCSJJLJCQLWZMALBXYFBPNLSFHTGJWEJJXXGLLJSTGSHJQLZFKCGNNNSZFDEQ"
		"FHBSAQTGYLBXMMYGSZLDYDQMJJRGBJTKGDHGKBLQKBDMBYLXWCXYTTYBKMRTJZXQJBHLMHMJJZMQASLDCYXYQDLQCAFYWYXQHZ";

	int H = 0;
	int L = 0;
	int W = 0;
	size_t ilen = strlen(in_);

	int len = 0;
	for (int i = 0; i < (int)ilen; i++)
	{
		if (len >= (olen_ - 1)) 
			break;
		H = (uint8)(in_[i + 0]);
		L = (uint8)(in_[i + 1]);

		if (H == 0x20 ) continue;

		if (H == 0xA3 && (L >= 0xC1 && L <= 0xDA))  //Ａ Ｂ - A3C1 A3DA- A
		{
			out_[len] = (char)(0x41 + (L - 0xC1));
			len++;
			i++;
			continue;
		}
		if (H < 0xA1 || L < 0xA1) {
			out_[len] = in_[i];
			len++;
			continue;
		}
		else 
		{
			W = (H - 160) * 100 + L - 160;
		}
		if (W > 1600 && W < 5590) {
			for (int j = 22; j >= 0; j--) 
			{
				if (W >= letter_region[j]) 
				{
					// out_[len] = (char)*letter_first[j];
					out_[len] = (char)letter_first[j][0];
					len++;
					i++;
					break;
				}
			}
			continue;
		}
		else 
		{
			i++;
			W = (H - 160 - 56) * 94 + L - 161;
			if (W >= 0 && W <= 3007)
			{
				out_[len] = letter_second[W];
				len++;
			}
			else
			{
				//out_[len] = (char)H;  len++;
				//out_[len] = (char)L;  len++;
			}
		}
	}
	out_[len] = 0; 
}
void _get_first_letter_proc(const char* in_, char *out_, int olen_)
{
	int  len, len1, len2;

	char str[255];
	len = (int)strlen(in_) + 1; 
	len = len > 254 ? 254 : len ; 
	sis_strcpy(str, len, in_);  
	int nums = sizeof(_sis_map_letter_define) / sizeof(struct s_sis_kv_pair);
	for (int i = 0; i < len; i++)
	{
		for (int j = 0; j < nums; j++)
		{
			len1 = (int)strlen(_sis_map_letter_define[j].key);
			if (!sis_strncmp(&str[i], _sis_map_letter_define[j].key, len1))
			{
				len2 = (int)strlen(_sis_map_letter_define[j].val);
				sis_strncpy(&str[i], len, _sis_map_letter_define[j].val, len2);
				for (int k = len2; k < len1; k++) 
				{
					str[i + k] = 0x20;
				}
				i = i + len1 - 1;
			}
		}
	}
	_get_first_letter(str, out_, olen_);
}

void sis_get_spell_gbk(const char *in_, char *out_, size_t olen_)
{	
	int len = olen_ < 32 ? olen_: 32;
	_get_first_letter_proc(in_, out_, len);
	// sprintf(des, str.c_str());
}
int sis_get_spell_utf8(const char *in_, char *out_, size_t olen_)
{
	char out[32];
	int n = sis_utf8_to_gbk(in_,strlen(in_), out, 32);
	if(n) 
	{
		return n;
	}
	int len = olen_ < 32 ? olen_: 32;
	_get_first_letter_proc(out, out_, len);
	return 0;
	// int sis_utf8_to_gbk(const char *in, size_t ilen_, char *out_, size_t olen_)

	// char out[32];
	// utf8_to_gb2312(src, strlen(src), out, 32);
	// std::string str = _get_first_letter_proc(out,32);
	// sprintf(des, str.c_str());
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
	for (int i = 1; tolower(*s1_) == tolower(*s2_); ++s1_, ++s2_, i++)
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
	for (int i = 1; (*s1_) == (*s2_); ++s1_, ++s2_, i++)
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

char *sis_strdup(const char *str_, size_t len_) //SIS_MALLOC
{
	if (!str_) { return NULL; }
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

char *sis_str_sprintf(size_t mlen_, const char *fmt_, ...) 
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

const char *sis_str_replace(const char *in_, char ic_,char oc_)
{
	char *ptr = (char *)in_;
	while (ptr && *ptr)
	{
		if ((unsigned char)*ptr == ic_) {
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
    while(ptr&&*ptr)
    {
        if (*ptr == c) {           
            if(idx_ == count){
               	sis_strncpy(out_, olen_, start, ptr - start);
				return; 
            }
            ptr++;
			start = ptr;
			count++;            
        } else {
            ptr++;
        }
    }
    if (ptr > start) {
        sis_strncpy(out_, olen_, start, ptr - start);
    } else {
        sis_strcpy(out_, olen_, in_);
    }
}
int sis_str_substr_nums(const char *s, char c)
{
	if (!s) {
		return 0;
	}
	int i, len, count;
	len = (int)strlen(s);
	for (i = 0, count = 0; i<len; i++)
	{
		if (s[i] == c) count++;
	}
	if (len>0 && s[len - 1] != c) {
		count++;
	}
	return count;
}
int sis_str_subcmp(const char *sub, const char *s, char c)  //-1没有匹配的
{
	if (!sub || !s) {
		return -1;
	}
	int i, pos, len, count;
	len = (int)strlen(s);
	pos = 0;
	for (i = 0, count = 0; i<len; i++)
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
int sis_str_subcmp_head(const char *sub, const char *s, char c)  //-1没有匹配的
{
	if (!sub || !s) {
		return -1;
	}
	int i, pos, len, count;
	len = (int)strlen(s);
	pos = 0;
	for (i = 0, count = 0; i<len; i++)
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
	if (i>pos)
	{   //strncmp
		if (!sis_strncasecmp(sub, &s[pos], i - pos)){
			return count;
		}
	}
	return -1;
}

const char *sis_str_getline(const char *e_, int *len_, const char *s_, size_t size_)
{
	if (!e_||!s_) return NULL;

	*len_ = 0;
	int lines = 2;
	int chars = 50;

	const char *start = e_;
	while(*start&&start>s_&&(lines>0||chars>0))
	{
		if((unsigned char)*start == '\n')  lines--;
		start--;
		chars--;
		*len_=*len_ + 1;
	}
	// start++; // 把回车跳过
	// printf("%.30s\n",start);
	// *len_ = 0;
	// lines = 2;
	// int slen = size_- (e_ - s_);
	// const char *stop = e_;
	// while(*stop&&*len_<slen&&lines > 0) {
	// 	if((unsigned char)*stop == '\n')  lines--;
	// 	stop++;
	// 	*len_=*len_ + 1;
	// }
	// printf("%.30s   %d\n",start, *len_);
	return start;
}
// 判断 字符串是否在source中， 如果在，并且完全匹配就返回0 否则返回1 没有匹配就返回-1
// source = aaa,bbbbbb,0001 
int sis_str_match(const char* substr_, const char* source_, char c)
{
	char *s = strstr(source_,substr_);
	if (s) {
		int len =strlen(substr_);
		s +=len;
		// if(!strcmp(substr_,"603096")||!strcmp(substr_,"KDL")) {
		// 	printf("c == %x\n",*s);
		// }
		if(!*s||*s==c) {
			// printf("c == %x\n",*s);
			return 0;
		}
		return 1;
	}
	return -1;
}
const char *sis_str_parse(const char *src_, const char *sign_, char *out_, size_t olen_)
{
	char *s = strstr(src_,sign_);
	if (s) {
		sis_strncpy(out_, olen_, src_, s - src_);
		return s + strlen(sign_);
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