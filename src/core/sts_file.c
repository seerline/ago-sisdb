
#include <sts_file.h>
#include <sts_malloc.h>

s_sts_sds sts_file_read_to_sds(const char *fn_)
{
	sts_file_handle fp = sts_file_open(fn_, STS_FILE_IO_READ, 0);
	if (!fp)
	{
		sts_out_error(3)("cann't open file [%s].\n", fn_);
		return NULL;
	}
	size_t size = sts_file_size(fp);
	sts_file_seek(fp, 0, SEEK_SET);
	if (size == 0)
	{
		sts_file_close(fp);
		return NULL;
	}
	s_sts_sds buffer = sts_sdsnewlen(NULL, size + 1);
	sts_file_read(fp, buffer, 1, size);
	sts_file_close(fp);
	return buffer;
}

void sts_get_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_)
{
    if (!inpath_) {
        sts_sprintf(outpath_,size_,srcpath_);
    } else {
        if (*inpath_=='/') {
            // 如果为根目录，就直接使用
            sts_sprintf(outpath_,size_,inpath_);
        } else {
            // 如果为相对目录，就合并配置文件的目录
            sts_sprintf(outpath_,size_,"%s%s", srcpath_,inpath_);
        }
    }
    // 创建目录
    sts_path_complete(outpath_,STS_PATH_LEN);
    if(!sts_path_mkdir(outpath_))
    {
		sts_out_error(3)("cann't create dir [%s].\n", outpath_);   
    }
    sts_out_error(5)("outpath_:%s\n",outpath_);
}
