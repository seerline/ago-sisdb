
#include <sis_log.h>

char __log_error[255];

void sis_log_close()
{

}
bool sis_log_open(const char *log_, int level_, int limit_)
{
	// 如果指定了 log_ 名就输出到文件，
	// 如果等于 console 就打印到屏幕上
	// 配置中应该有一个log文件名和最高级别
	// 建立一个全局变量，专门用于输出log
	// error直接输出到屏幕
	return true;
}
char *sis_log(int level_, const char *fmt_, ...)
{
	return NULL;
}

void sis_out_binary(const char *key_, const char *val_, size_t len_) 
{
    printf("%s : %d ==> \n", key_, (int)len_);
	size_t i = 0;
    size_t j;
	while(i < len_)
	{
		uint8_t val = val_[i] & 0xff;
		if(i%8==0 && i > 0) {
			printf(" ");
		}
		if(i%16==0) {
			if(i>0) {
				printf(" ");
				for(j=i-16;j<i;j++) {
					int vv = val_[j];
					if(vv > 0x20 && vv < 0x7e) {
						printf("%c", vv);
					}
					else {
						printf(".");
					}
				}
				printf("\n");
			}
			printf("%.8lu: ", i);
		}
		printf("%.2x ", val);
		i++;
	}
	if(len_%16!=0) {
		for(j=0; j<(16-(len_%16)); j++) {
			printf("   ");
		}
		printf("  ");
		if(len_%16<=8) printf(" ");
		for(j=i-(len_%16);j<i;j++) {
			int vv = val_[j];
			if(vv > 0x20 && vv < 0x7e) {
				printf("%c", vv);
			}
			else {
				printf(".");
			}
		}
	}
	printf("\n");
}

#if 0

// inline void why_me()
// {
//     printf( "This function is %s\n", __func__ );
//     printf( "The file is %s.\n", __FILE__ );
//     printf( "This is line %d.\n", __LINE__ );

// }
#define why_me() do { \
	printf( "This function is %s\n", __func__ ); \
    printf( "The file is %s.\n", __FILE__ ); \
    printf( "This is line %d.\n", __LINE__ ); \
} while(0)
int main()
{
    printf( "The file is %s.\n", __FILE__ );
    printf( "The date is %s.\n", __DATE__ );
    printf( "The time is %s.\n", __TIME__ );
    printf( "This is line %d.\n", __LINE__ );
    printf( "This function is %s.\n", __func__ );

    why_me();

     return 0;
}


#endif
