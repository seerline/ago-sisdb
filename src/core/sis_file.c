
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

void sis_get_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_)
{
    if (!inpath_) {
        sis_sprintf(outpath_,size_,"%s", srcpath_);
    } else {
        if (*inpath_=='/'||!srcpath_) {
            // 如果为根目录，就直接使用
            sis_sprintf(outpath_,size_,"%s", inpath_);
        } else {
            // 如果为相对目录，就合并配置文件的目录
            sis_sprintf(outpath_,size_,"%s%s", srcpath_,inpath_);
        }
    }
    // 创建目录
    sis_path_complete(outpath_,SIS_PATH_LEN);
    if(!sis_path_mkdir(outpath_))
    {
		LOG(3)("cann't create dir [%s].\n", outpath_);   
    }
    LOG(5)("outpath_:%s\n",outpath_);
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
    LOG(5)("outpath_:%s\n",outpath);
}

#if 0
// test large file
int main()
{
    msec_t start = sis_time_get_now_msec();

#if 1
    int fp = sis_open("large.log", SIS_FILE_IO_DSYNC | SIS_FILE_IO_TRUNC | SIS_FILE_IO_CREATE | SIS_FILE_IO_RDWR, SIS_FILE_MODE_NORMAL);
    // 采用上面的写入方式，安全，写入速度 66秒/1G（固态硬盘）
    // int fp = sis_open("large.log", O_TRUNC | O_CREAT | O_RDWR | O_APPEND);
    // 采用上面的写入方式，安全，写入速度 70秒/1G（固态硬盘）
    // for (int i = 0; i < 15625 ; i++)
    // {
    //     char buffer[32*10000];
    //     size_t bytes = sis_write(fp, buffer, 32*10000);
    //     if (bytes!=32*10000||i%3125==0)
    for (int i = 0; i < 5*1000*1000; i++)
    {
        char buffer[1000];
        size_t bytes = sis_write(fp, buffer, 1000);
        if (bytes!=1000||i%1000000==0)
        {
            printf("[%4d] %llu size= %zu  %zu\n", i, sis_time_get_now_msec() - start, bytes, sis_seek(fp, 0, SEEK_CUR));
        }
    }
    size_t size = sis_size(fp);
    printf("msec %llu %x size= %zu\n", sis_time_get_now_msec() - start, O_RSYNC, size);
    sis_seek(fp, -100 ,SEEK_END);
    size_t pos;
    sis_getpos(fp, &pos);
    printf("pos = %zu\n", pos);
    pos = 4999990000;
    sis_setpos(fp, &pos);
    printf("pos = %zu\n", pos);
    sis_getpos(fp, &pos);
    printf("pos = %zu\n", pos);
    sis_close(fp);
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