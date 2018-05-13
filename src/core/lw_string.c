
#include "lw_string.h"

int safestrcpy(char *out_, size_t outLen, const char *in_)
{
	if (!in_ || !out_) { 
		return 0; 
	}
	size_t len = strlen(in_);
	outLen--; //防止字符串最后一位可能是边界
	len = len > outLen ? outLen : len;
	memmove(out_, in_, len);
	memset(out_ + len, 0, outLen - len + 1);
	return (int)len;
}

int safestrncpy(char *out_, size_t outLen, const char *in_, size_t inLen)
{
	if (!in_ || !out_) { 
		return 0; 
	}

	size_t len = strlen(in_); 
	len = len > inLen ? inLen : len;
	outLen--; //防止字符串最后一位可能是边界
	len = len > outLen ? outLen : len;
	memmove(out_, in_, len);
	memset(out_ + len, 0, outLen - len + 1);
	return (int)len;
}

///////////////////////////////
// 关于拼音的定义
//////////////////////////////
#define OTHER_FIRSTCHAR  4  //定义股票中文字的多音字
const static char* other_Text[] = {
	"昇", "昉", "重庆", "银行" };
const static char* other_Char[] = {
	"S", "F", "CQ", "YH" };

static int li_SecPosValue[] = {
	1601, 1637, 1833, 2078, 2274, 2302, 2433, 2594, 2787, 3106, 3212,
	3472, 3635, 3722, 3730, 3858, 4027, 4086, 4390, 4558, 4684, 4925, 5249
};
const static char* lc_FirstLetter[] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "W", "X", "Y", "Z"
};
const static char* ls_SecondSecTable =
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

void __first_letter(const char* strChs, char *out, size_t outLen)
{
	int H = 0;
	int L = 0;
	int W = 0;

	if (!out) {
		return;
	}

	unsigned int stringlen = (unsigned int)strlen(strChs);
	size_t len = 0;
	for (unsigned int i = 0; i < stringlen; i++)
	{
		if (len >= (outLen - 1))
		{
			break;
		}
		H = (unsigned char)(strChs[i + 0]);
		L = (unsigned char)(strChs[i + 1]);

		if (H == 0x20 ) continue;

		if (H == 0xA3 && (L >= 0xC1 && L <= 0xDA))  //Ａ Ｂ - A3C1 A3DA- A
		{
			*out++ = (char)(0x41 + (L - 0xC1));
			len++;
			i++;
			continue;
		}
		if (H < 0xA1 || L < 0xA1) {
			*out++ = strChs[i];
			len++;
			continue;
		}
		else 
		{
			W = (H - 160) * 100 + L - 160;
		}
		if (W > 1600 && W < 5590) {
			for (int j = 22; j >= 0; j--) {
				if (W >= li_SecPosValue[j]) {
					*out++ = lc_FirstLetter[j][0];
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
				*out++ = ls_SecondSecTable[W];
				len++;
			}
			else
			{
				//*out++ = (char)H;
				//*out++ = (char)L;
				//len++; len++;
			}
		}
	}
	*out = 0;
}
void get_first_letter(const char* strChs, char *out, size_t outLen)
{
	char str[255];
	int i, j, k, len1, len2;

	safestrcpy(str, sizeof(str), strChs);
	for (i = 0; i < (int)strlen(str); i++)
	{
		for (j = 0; j < OTHER_FIRSTCHAR; j++)
		{
			len1 = (int)strlen(other_Text[j]);
			if (strncmp(&str[i], other_Text[j], len1) == 0)
			{
				len2 = (int)strlen(other_Char[j]);
				safestrncpy(&str[i], sizeof(str), other_Char[j], len2);
				for (k = len2; k < len1; k++) {
					str[i + k] = 0x20;
				}
				i = i + len1 - 1;
			}
		}
	}
	 __first_letter(str, out, outLen);
}

#ifndef _MSC_VER
#include <errno.h>
//gb2312表示的汉字很少，试试 GBK 或 gb18030
void gbk_to_utf8(const char*  in, char*  out, size_t  outLen){
	iconv_t  cd;
	char *pin, *pout;
	size_t inLen_, outLen_;

	cd = iconv_open("utf-8", "gbk");
	if (cd == (iconv_t)-1)
	{
		LOG(3)("iconv_open false!..[%d]\n",errno);
		return ;
	}
	memset(out, 0, outLen);
	pin = (char *)in;
	pout = out;
	inLen_ = strlen(in) + 1;
	outLen_ = outLen;

	if (-1 == (int)iconv(cd, &pin, &inLen_, &pout, &outLen_))
	{
		LOG(3)("iconv false!..[%d]\n",errno);
	}
	iconv_close(cd);
	outLen = outLen_;
}

void utf8_to_gbk(const char*  in, size_t  inLen, char*  out, size_t  outLen){

	if(inLen<1||outLen<1){
		return ;
	}

	iconv_t  cd;
	char *pin, *pout;
	size_t inLen_, outLen_;
	
	cd = iconv_open("gbk", "utf-8");

	if (cd == (iconv_t)-1)
	{
		LOG(3)("iconv_open false!..[%d]\n", errno);
	}
	memset(out, 0, outLen);
	char *srcstr;
	srcstr=(char *)zmalloc(inLen+1);
	safestrncpy(srcstr, inLen+1, in, inLen);
	trim(srcstr);
	pin = srcstr;
	inLen_ = strlen(srcstr) + 1;

	pout = out;
	outLen_ = outLen;

	if (-1 == (int)iconv(cd, &pin, &inLen_, &pout, &outLen_))
	{
		LOG(3)("iconv false!..[%d]%s\n", errno, in);
	}
	iconv_close(cd);
	outLen = outLen_;
	zfree(srcstr);
}
#else
void gbk_to_utf8(const char*  in, char*  out, size_t  outLen){
	WCHAR *strSrc;
	LPSTR szRes;

	//获得临时变量的大小
	int i = MultiByteToWideChar(CP_ACP, 0, in, -1, NULL, 0);
	strSrc = (WCHAR *)zmalloc((i + 1)*sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, in, -1, strSrc, i);

	//获得临时变量的大小
	i = WideCharToMultiByte(CP_UTF8, 0, strSrc, -1, NULL, 0, NULL, NULL);
	szRes = (CHAR *)zmalloc((i + 1)*sizeof(CHAR));
	int j = WideCharToMultiByte(CP_UTF8, 0, strSrc, -1, szRes, i, NULL, NULL);

	zfree(strSrc);
	memset(out, 0, outLen);
	if (i > outLen)
	{
		memmove(out, szRes, outLen);
	}
	else
	{
		memmove(out, szRes, i);
	}
	zfree(szRes);
}
void utf8_to_gbk(const char*  in, size_t  inLen, char*  out, size_t  outLen)
{
	int i = MultiByteToWideChar(CP_UTF8, 0, in, (int)inLen, NULL, 0);
	wchar_t* strSrc = (wchar_t *)zmalloc((i + 1)*sizeof(wchar_t));   
	memset(strSrc, 0, i + 1);
	MultiByteToWideChar(CP_UTF8, 0, in, (int)inLen, strSrc, i);
	int ii = WideCharToMultiByte(CP_ACP, 0, strSrc, i, NULL, 0, NULL, NULL);
	char* szRes = (char *)zmalloc(ii + 1);   
	memset(szRes, 0, ii + 1);
	int j = WideCharToMultiByte(CP_ACP, 0, strSrc, i, szRes, ii, NULL, NULL);
	if (strSrc) {
		zfree(strSrc);
	}
	memset(out, 0, outLen);
	if (j > outLen)//ii
	{
		memmove(out, szRes, outLen);
	}
	else
	{
		memmove(out, szRes, j);
	}
	zfree(szRes);
}
#endif
void getpy_of_gbk(const char *src, char *out, size_t outLen)
{
	get_first_letter(src, out, outLen);
}
void getpy_of_utf8(const char *src, char *out, size_t outLen)
{
	char gbk[32];
	utf8_to_gbk(src, strlen(src), gbk, 31);
	get_first_letter(gbk, out, outLen);
}

void str_to_upper(char *string)
{
	int len, i = -1;
	len = (int)strlen(string);
	while (++i < len)
	{
		if (isalpha(string[i]) && islower(string[i]))
		{
			string[i] = toupper((int)string[i]);
		}
	}
}

void str_to_lower(char *string)
{
	int len, i = -1;
	len = (int)strlen(string);
	while (++i < len)
	{
		if (isalpha(string[i]) && isupper(string[i]))
		{
			string[i] = tolower((int)string[i]);
		}
	}
}

void trim(char *s)
{
	int i, len;
	len = (int)strlen(s);
	for (i = len - 1; i >= 0; i--)
	{
		if (s[i] != ' ' && s[i] != 0x0d && s[i] != 0x0a)
		{
			break;
		}
		else
		{
			s[i] = 0;
		}
	}
	for (i = 0; i < len; i++)
	{
		if (s[i] != ' ')
		{
			break;
		}
	}
	if (i != 0)
	{
		memmove(s, s + i, len - i);
		s[len - i] = 0;
	}
}
void delreturn(char *s)
{
	for (int i = 0; i<(int)strlen(s); i++)
	{
		if (s[i] == 0x0D || s[i] == 0x0A || s[i] == 0)
		{
			s[i] = 0;
			break;
		}
	}
}

int getchnum(const char *s, char c)
{
	int i, len, count;
	len = (int)strlen(s);
	for (i = 0, count = 0; i < len; i++)
	{
		if (s[i] == c) {
			count++;
		}
	}
	return count;
}

int getsubnum(const char *s, char c)
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
void getsubstr(char *sub, size_t subLen, const char *s, char c, int index)
{
	int i, pos, len, count;
	len = (int)strlen(s);
	pos = 0;
	for (i = 0, count = 0; i<len; i++)
	{
		if (s[i] == c)
		{
			if (index == count)
			{
				safestrncpy(sub, subLen, &s[pos], i - pos);
				return;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i>pos)
	{
		safestrncpy(sub, subLen, &s[pos], i - pos);
		return;
	}
	safestrcpy(sub, subLen, s);
}
int substrcmp(const char *sub, const char *s, char c)  //-1没有匹配的
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
			if (!strncmp(sub, &s[pos], i - pos))
			{
				return count;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i > pos)
	{
		if (!strncmp(sub, &s[pos], i - pos))
		{
			return count;
		}
	}
	return -1;
}
int headsubstrcmp(const char *sub, const char *s, char c)  //-1没有匹配的
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
			if (!strncmp(sub, &s[pos], i - pos))
			{
				return count;
			}
			pos = i + 1;
			count++;
		}
	}
	if (i>pos)
	{
		if (!strncmp(sub, &s[pos], i - pos)){
			return count;
		}
	}
	return -1;
}


#ifdef _MSC_VER
char *strsep(char **stringp, const char *delim)
{
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if ((s = *stringp) == 0)
	{
		return 0;
	}

	tok = s;
	while (1)
	{
		c = *s++;
		spanp = delim;
		do
		{
			if ((sc = *spanp++) == c)
			{
				if (c == 0)
					s = 0;
				else
					s[-1] = 0;
				*stringp = s;
				return tok;
			}
		} while (sc != 0);
	}
}

#endif
int  parse_uri(const char *uri_, char *protocol_, size_t  pLen, char * ip_, size_t  iLen, int *port_)
{
	if (!uri_) {
		return -1;
	}
	char * pos = strstr(uri_, "://");
	if (!pos) {
		return -2;
	}
	safestrncpy(protocol_, pLen, uri_, pos - uri_);
	size_t len = strlen(uri_);
	char *str = zmalloc(len);
	safestrncpy(str, len, pos + 3, len);

	pos = strstr(str, ":");
	if (!pos) {
		safestrcpy(ip_, iLen, str);
		zfree(str);
		return -3;
	}
	safestrncpy(ip_, iLen, str, pos - str);
	safestrncpy(str, len, pos + 1, len);
	*port_ = atoi(str);

	if (!protocol_[0] || !ip_[0] || *port_ == 0) {
		zfree(str);
		return -4;
	}
	zfree(str);
	return 0;
}
bool parse_auth(const char *auth_, char * username_, size_t  uLen, char * password_, size_t  pLen)
{
	if (!auth_) { 
		return false; 
	}

	char * pos = strstr(auth_, "@");
	if (!pos) {
		return false;
	}
	safestrncpy(username_, uLen, auth_, pos - auth_);
	safestrncpy(password_, pLen, pos + 1, strlen(auth_));

	if (!username_[0] || !password_[0]){
		return false;
	}
	return true;
}

#ifndef _MSC_VER
#include<termio.h>
char wait(const char *str)
{
	if(str) {
		printf(" %s.",str);
	}
	struct termios tmtemp, tm;
	int fd = 0;

	if (tcgetattr(fd, &tm) != 0){      //获取当前的终端属性设置，并保存到tm结构体中
		return 0;
	}
	tmtemp = tm;
	cfmakeraw(&tmtemp);     //将tetemp初始化为终端原始模式的属性设置
	if (tcsetattr(fd, TCSANOW, &tmtemp) != 0){     //将终端设置为原始模式的设置
		return 0;
	}
	char c = getchar();
	if (tcsetattr(fd, TCSANOW, &tm) != 0){      //接收字符完毕后将终端设置回原来的属性
		return 0;
	}
	return c;
}
#else
char wait(const char *str){
	if (str) {
		printf(" %s.", str);
	}
	system("pause");
	return 0;
}
#endif
///////////////////////////
//数字和字符串的转换
////////////////////////////
int char_to_int(const char ** str)
{
	int code = **((uint8 **)str);
	if (!str) { return 0; }
	if (!*str) { return 0; }
	if (!code) { return 0; }

	(*str)++;
	if (code > 0x80)
	{
		code *= 256;
		code += **((uint8 **)str);
		(*str)++;
	}
	return code;
}

int int_to_char(char **str, int chr)
{
	int bt = 1;

	if (!str)  { return 0; }
	if (!*str) { return 0; }

	if (chr < 256)
	{
		**((uint8 **)str) = (chr % 256);
		(*str)++;
	}
	else
	{
		**((uint8 **)str) = (chr / 256);
		(*str)++;
		**((uint8 **)str) = (chr % 256);
		(*str)++;
		bt++;
	}
	return bt;
}

char *next_token(char *buf, char *token, const char *stopchars)
{
	if (!stopchars)
	{
		stopchars = " ";
	}

	int cc;
	char *p = token;
	while ((cc = char_to_int(&buf)) && (!strchr(stopchars, cc)))
	{
		int_to_char(&token, cc);
	}
	int_to_char(&token, 0);
	trim(p);
	return buf;
}

void str_to_hex(char *chr_, char *src_, int len_) // "3048" -->
{
	char ch;
	while (len_--)
	{
		ch = (*src_++);
		*chr_++ = (char_to_hex(ch) << 4) + char_to_hex(ch); // "0H"
	}
}

unsigned char char_to_hex(char ch) // 3 -> 3   F -> 15 
{

	if (ch >= '0' && ch <= '9')
	{
		return (ch - '0');
	}
	if (ch >= 'A' && ch <= 'F')
	{
		return (ch - 'A' + 0xA);
	}
	if (ch >= 'a' && ch <= 'f')
	{
		return (ch - 'a' + 0xA);
	}
	return 0;
}

unsigned long long  strcode_to_int64(char* str) // SH600600 -- 0xSH600600
{
	if (!str) { return 0; }
	unsigned long long  value = 0;
	int len = (int)strlen(str);
	len = min(len, 8);
	for (int i = 0; i<len; i++)
	{
		value = value << 8 | str[i];
	}
	return value; //!!!需要测试
}

unsigned long long  strtoint64(const char* str, int len)
{
	if (!str) { return 0; }
	unsigned long long  value = 0;
	if (len < 0 || len >(int)strlen(str)) {
		len = (int)strlen(str);
	}
	for (int i = 0; i<len; i++)
	{
		if (str[i]<'0' || str[i]>'9') continue;
		value = value * 10 + (str[i] - '0');
	}
	return value;
}
double strtodouble(const char* str_, double default_)
{
	if (!str_)  { 
		return default_; 
	}
	return atof(str_);
}
int strtoint(const char* str_, int default_)
{
	if (!str_) { 
		return default_; 
	}
	return atoi(str_);
}
