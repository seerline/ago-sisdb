
#include "sis_disk.h"

// 这里是关于log的读写函数

size_t sis_disk_io_write_log(s_sis_disk_ctrl *cls_, void *in_, size_t ilen_)
{
    size_t size = 0; 
    if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_LOG)
    {
        s_sis_disk_head head;
        // if (cls_->work_fps->max_page_size > 0 && 
        //     ilen_ > cls_->work_fps->max_page_size)
        // {
        //     /* 这里可以分块存储 */
        //     head.fin = 0;
        //     ....
        // }
        
        head.fin = 1;
        head.hid = SIS_DISK_HID_MSG_LOG;
        head.zip = SIS_DISK_ZIP_NOZIP; // LOG不压缩
        size = sis_disk_files_write(cls_->work_fps, &head, in_, ilen_);
        
    }
    return size;
}

int cb_sis_disk_io_read_log(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 
    // 根据hid不同写入不同的数据到obj
    // LOG(5)("other hid : %d at log. %zu\n", head_->hid, isize_);
    // sis_out_binary("mem:", imem_, isize_);
    if(head_->hid != SIS_DISK_HID_MSG_LOG)
    {
        // 这应该是严重错误
        LOG(5)("other hid : %d at log.\n", head_->hid);
        // sis_out_binary("mem:", imem_, isize_);
        return -2;
    }
    else
    {
        if(callback->cb_original)
        {
            callback->cb_original(callback->cb_source, head_, imem_, isize_);
        }
    }
    if (ctrl->isstop)
    {
        return -1;
    }
    return 0;
}

int sis_disk_io_sub_log(s_sis_disk_ctrl *cls_, void *cb_)
{
    if (!cb_)
    {
        return -1;
    }
    sis_disk_rcatch_init_of_sub(cls_->rcatch, NULL, NULL, NULL, cb_);
    cls_->isstop = false;  // 用户可以随时中断

    s_sis_disk_reader_cb *callback = cls_->rcatch->callback;   
    if(callback->cb_start)
    {
        callback->cb_start(callback->cb_source, cls_->open_date);
    }
    // 每次都从头读起
    sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_log);

    // 不是因为中断而结束 就发送stop标志
   if (cls_->isstop)
    {
        if(callback->cb_break)
        {
            callback->cb_break(callback->cb_source, cls_->stop_date);
        }
    }
    else
    {
        if(callback->cb_stop)
        {
            callback->cb_stop(callback->cb_source, cls_->stop_date);
        }
    }
    return 0;
}

#if 0
#include "sis_disk.h"
// 测试LOG文件读写
int    __read_nums = 0;
size_t __read_size = 0;
msec_t __read_msec = 0;
int    __write_nums = 1*1000*1000;
size_t __write_size = 0;
msec_t __write_msec = 0;

static void cb_start(void *src, int tt)
{
    printf("%s : %d\n", __func__, tt);
    __read_msec = sis_time_get_now_msec();
}
static void cb_stop(void *src, int tt)
{
    printf("%s : %d cost: %lld\n", __func__, tt, sis_time_get_now_msec() - __read_msec);
}
static void cb_break(void *src, int tt)
{
    printf("%s : %d\n", __func__, tt);
}
static void cb_original(void *src, s_sis_disk_head *head_, void *out_, size_t olen_)
{
    __read_nums++;
    if (__read_nums % sis_max((__write_nums / 10), 1) == 0 || __read_nums < 10)
    {
        printf("%s : %zu %d\n", __func__, olen_, __read_nums);
    }
}
void read_log(s_sis_disk_reader *cxt)
{
    sis_disk_reader_sub_log(cxt, 0);
}
void write_log(s_sis_disk_writer *cxt)
{
    sis_disk_writer_open(cxt, 0);
    int count = __write_nums;
    __write_msec = sis_time_get_now_msec();

    int sendsize = 1024;
    char *buffer = sis_malloc(sendsize);
    memset(buffer, '1', sendsize);
    for (int i = 0; i < count; i++)
    {
        sis_disk_writer_log(cxt, buffer, sendsize);
        __write_size += sendsize;
    }
    sis_free(buffer);
    sis_disk_writer_close(cxt);
    printf("write end %d %zu | cost: %lld.\n", __write_nums, __write_size, sis_time_get_now_msec() - __write_msec);
}

int main(int argc, char **argv)
{
    safe_memory_start();

    s_sis_disk_writer *wcxt = sis_disk_writer_create(".", "wlog", SIS_DISK_TYPE_LOG);
    write_log(wcxt);
    sis_disk_writer_destroy(wcxt);

    s_sis_disk_reader_cb cb = {0};
    cb.cb_source = NULL;
    cb.cb_start = cb_start;
    cb.cb_stop = cb_stop;
    cb.cb_break = cb_break;
    cb.cb_original = cb_original;
    s_sis_disk_reader *rcxt = sis_disk_reader_create(".", "wlog", SIS_DISK_TYPE_LOG, &cb);
    read_log(rcxt);
    sis_disk_reader_destroy(rcxt);

    safe_memory_stop();
    return 0;
}
#endif