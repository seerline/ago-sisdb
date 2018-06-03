
#include <sts_log.h>

char __log_error[255];

void sts_log_close()
{

}
bool sts_log_open(const char *log_)
{
	// 如果指定了 log_ 名就输出到文件，
	// 如果等于 console 就打印到屏幕上
	// 配置中应该有一个log文件名和最高级别
	// 建立一个全局变量，专门用于输出log
	// error直接输出到屏幕
	return true;
}
char *sts_log(int level_, const char *fmt_, ...)
{
	return NULL;
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
