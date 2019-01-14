
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
	sis_file_handle fp = sis_file_open(fn_, SIS_FILE_IO_READ, 0);
	if (!fp)
	{
		sis_out_log(3)("cann't open file [%s].\n", fn_);
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
	sis_file_read(fp, buffer, 1, size);
	sis_file_close(fp);
	return buffer;
}

bool sis_file_sds_write(const char *fn_, s_sis_sds buffer_)
{
	char path[SIS_PATH_LEN];
    sis_file_getpath(fn_, path, SIS_PATH_LEN);

    if (!sis_path_mkdir(path))
    {
        sis_out_log(3)("cann't create dir [%s].\n", path);
        return false;
    }

    sis_file_handle fp = sis_file_open(fn_, SIS_FILE_IO_CREATE | SIS_FILE_IO_WRITE | SIS_FILE_IO_TRUCT, 0);
    if (!fp)
    {
        sis_out_log(3)("cann't open file [%s].\n", fn_);
        return false;
    }
    sis_file_seek(fp, 0, SEEK_SET);
    sis_file_write(fp, buffer_, 1, sis_sdslen(buffer_));
    sis_file_close(fp);

    return true;	
}

void sis_get_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_)
{
    if (!inpath_) {
        sis_sprintf(outpath_,size_,"%s", srcpath_);
    } else {
        if (*inpath_=='/') {
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
		sis_out_log(3)("cann't create dir [%s].\n", outpath_);   
    }
    sis_out_log(5)("outpath_:%s\n",outpath_);
}
