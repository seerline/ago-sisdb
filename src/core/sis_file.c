
#include <sis_file.h>
#include <sis_malloc.h>

s_sis_file *sis_file_create()
{
    s_sis_file * o =sis_malloc(sizeof(s_sis_file));
    memset(o, 0, sizeof(s_sis_file));
    return o;
}
void sis_file_destroy(s_sis_file *file_)
{
    sis_free(file_);
}
// 读取文件内容至字符串
s_sis_sds sis_file_read_to_sds(const char *fn_)
{
	s_sis_file_handle fp = sis_file_open(fn_, SIS_FILE_IO_READ, 0);
	if (!fp)
	{
		LOG(3)("cann't open file [%s].\n", fn_);
		return NULL;
	}
	size_t size = sis_file_size(fp);
	sis_file_seek(fp, 0, SEEK_SET);
	if (size == 0)
	{
		sis_file_close(fp);
		return NULL;
	}
	s_sis_sds buffer = sis_sdsnewlen(NULL, size + 1);
	sis_file_read(fp, buffer, size);
	sis_file_close(fp);
	return buffer;
}

bool sis_file_sds_write(const char *fn_, s_sis_sds buffer_)
{
	char path[SIS_PATH_LEN];
    sis_file_getpath(fn_, path, SIS_PATH_LEN);

    if (!sis_path_mkdir(path))
    {
        LOG(3)("cann't create dir [%s].\n", path);
        return false;
    }

    s_sis_file_handle fp = sis_file_open(fn_, SIS_FILE_IO_CREATE | SIS_FILE_IO_WRITE | SIS_FILE_IO_TRUNC, 0);
    if (!fp)
    {
        LOG(3)("cann't open file [%s].\n", fn_);
        return false;
    }
    sis_file_seek(fp, 0, SEEK_SET);
    sis_file_write(fp, buffer_, sis_sdslen(buffer_));
    sis_file_close(fp);

    return true;	
}

void sis_cat_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_)
{
    if (!inpath_) {
        sis_sprintf(outpath_,size_,"%s", srcpath_);
    } else {
        if (*inpath_== SIS_PATH_SEPARATOR ||!srcpath_) {
            // 如果为根目录，就直接使用
            sis_sprintf(outpath_,size_,"%s", inpath_);
        } else {
            // 如果为相对目录，就合并配置文件的目录
            sis_sprintf(outpath_,size_,"%s%s", srcpath_,inpath_);
        }
    }
    // 创建目录
    sis_path_complete(outpath_, SIS_PATH_LEN);
    if(!sis_path_mkdir(outpath_))
    {
		LOG(3)("cann't create dir [%s].\n", outpath_);   
    }
    // LOG(5)("outpath_:%s\n",outpath_);
}

void sis_check_path(const char *fn_)
{
	char outpath[SIS_PATH_LEN];
	sis_file_getpath(fn_, outpath, SIS_PATH_LEN);
    sis_path_complete(outpath, SIS_PATH_LEN);
    if(!sis_path_mkdir(outpath))
    {
		LOG(3)("cann't create dir [%s].\n", outpath);   
    }
    // LOG(5)("outpath_:%s\n",outpath);
}

#if 0
// test large file
int main()
{

#if 1
// 机械硬盘linux速度
// fopen write : cost msec 993
// fopen write : cost msec 19713
// fopen write : cost msec 19714
// sys catch write : cost msec 41682
// no catch write : cost msec 2696
// no catch write && no write info: cost msec 2041

// fopen write : cost msec 716
// fopen write : cost msec 38781
// fopen write : cost msec 38781
// sys catch write : cost msec 15829
// no catch write : cost msec 3171
// no catch write && no write info: cost msec 2439

// 固态硬盘linux虚拟机速度
// fopen write : cost msec 4266
// fopen write : cost msec 4266
// sys catch write : cost msec 2959
// no catch write : cost msec 87
// no catch write && no write info: cost msec 30

    size_t wnums = 1 * 1000 * 1000;
    size_t wsize = 32 * 1024;

    wnums = wnums/(wsize/1024);
    {
        msec_t start = sis_time_get_now_msec();
        s_sis_file_handle fp = fopen("large.log", "wab+");
        char *wcatch = sis_malloc(wsize);
        for (int i = 0; i < wnums; i++)
        {
            size_t bytes = fwrite((char *)wcatch, 1, wsize, fp); 
            fflush(fp);
            if (bytes != wsize || i % (wnums/10) == 0)
            {
                printf("[%4d] %llu size= %zu \n", i, sis_time_get_now_msec() - start, bytes);
            }
        }
        sis_free(wcatch);
        printf("fopen write : cost msec %llu\n", sis_time_get_now_msec() - start);
        fsync(fileno(fp));
        printf("fopen write : cost msec %llu\n", sis_time_get_now_msec() - start);
        fclose(fp);
        printf("fopen write : cost msec %llu\n", sis_time_get_now_msec() - start);
    }
    {
        msec_t start = sis_time_get_now_msec();
        int fp = sis_open("large.log", SIS_FILE_IO_TRUNC | SIS_FILE_IO_CREATE | SIS_FILE_IO_RDWR, SIS_FILE_MODE_NORMAL);
        char *wcatch = sis_malloc(wsize);
        for (int i = 0; i < wnums; i++)
        {
            size_t bytes = sis_write(fp, wcatch, wsize);
            if (bytes != wsize || i % (wnums/10) == 0)
            {
                printf("[%4d] %llu size= %zu %zu\n", i, sis_time_get_now_msec() - start, bytes, sis_seek(fp, 0, SEEK_CUR));
            }
        }
        sis_free(wcatch);
        fsync(fp);
        sis_close(fp);
        printf("sys catch write : cost msec %llu\n", sis_time_get_now_msec() - start);
    }
    wnums = wnums/100;
    {
        msec_t start = sis_time_get_now_msec();
        int fp = sis_open("large.log", SIS_FILE_IO_TRUNC | SIS_FILE_IO_CREATE | SIS_FILE_IO_RDWR, SIS_FILE_MODE_NORMAL);
        char *wcatch = sis_malloc(wsize);
        for (int i = 0; i < wnums; i++)
        {
            size_t bytes = sis_write(fp, wcatch, wsize);
            fsync(fp);
            if (bytes != wsize || i % (wnums/10) == 0)
            {
                printf("[%4d] %llu size= %zu  %zu\n", i, sis_time_get_now_msec() - start, bytes, sis_seek(fp, 0, SEEK_CUR));
            }
        }
        sis_free(wcatch);
        sis_close(fp);
        printf("no catch write : cost msec %llu\n", sis_time_get_now_msec() - start);
    }
    {
        msec_t start = sis_time_get_now_msec();
        int fp = sis_open("large.log", SIS_FILE_IO_TRUNC | SIS_FILE_IO_CREATE | SIS_FILE_IO_RDWR, SIS_FILE_MODE_NORMAL);
        char *wcatch = sis_malloc(wsize);
        for (int i = 0; i < wnums; i++)
        {
            size_t bytes = sis_write(fp, wcatch, wsize);
            fdatasync(fp);
            if (bytes != wsize || i % (wnums/10) == 0)
            {
                printf("[%4d] %llu size= %zu  %zu\n", i, sis_time_get_now_msec() - start, bytes, sis_seek(fp, 0, SEEK_CUR));
            }
        }
        fsync(fp);
        sis_free(wcatch);
        sis_close(fp);
        printf("no catch write && no write info: cost msec %llu\n", sis_time_get_now_msec() - start);
    }


    // size_t size = sis_size(fp);
    // printf("msec %llu %x size= %zu\n", sis_time_get_now_msec() - start, O_RSYNC, size);
    // sis_seek(fp, -100 ,SEEK_END);
    // size_t pos;
    // sis_getpos(fp, &pos);
    // printf("pos = %zu\n", pos);
    // pos = 4999990000;
    // sis_setpos(fp, &pos);
    // printf("pos = %zu\n", pos);
    // sis_getpos(fp, &pos);
    // printf("pos = %zu\n", pos);
    // sis_close(fp);

#else
    // 采用下面的写入方式，不安全，写入时如果有其他进程读 就会写失败 写入速度是 66秒/1G（固态硬盘）
    s_sis_file_handle fp =  sis_file_open("large.log", SIS_FILE_IO_CREATE | SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR, 0);
    //write
    for (int i = 0; i < 15625 ; i++)
    {
        char buffer[32*10000];
        size_t bytes = sis_file_write(fp, buffer, 32*10000);
        if (bytes!=32*10000||i%3125==0)
    // for (int i = 0; i < 5*1000*1000; i++)
    // {
    //     char buffer[1000];
    //     size_t bytes = sis_file_write(fp, buffer, 1000);
    //     if (bytes!=1000||i%1000000==0)
        {
            printf("[%4d] %llu size= %zu  %zu\n", i, sis_time_get_now_msec() - start, bytes, sis_file_seek(fp, 0, SEEK_CUR));
        }
    }
    size_t size = sis_file_size(fp);
    printf("msec %llu size= %zu\n", sis_time_get_now_msec() - start, size);
    sis_file_seek(fp, -100 ,SEEK_END);
    size_t pos;
    sis_file_getpos(fp, &pos);
    printf("pos = %zu\n", pos);
    pos = 4999990000;
    sis_file_setpos(fp, &pos);
    printf("pos = %zu\n", pos);
    sis_file_getpos(fp, &pos);
    printf("pos = %zu\n", pos);

    sis_file_close(fp);
#endif
    return 0;
}

#endif