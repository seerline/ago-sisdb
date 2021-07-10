
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
        head.zip = SIS_DISK_ZIP_NOZIP;
        size = sis_disk_files_write(cls_->work_fps, &head, in_, ilen_;)
    }
    return size;
}

int cb_sis_disk_io_read_log(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    s_sis_disk_reader_cb *callback = ctrl->reader->callback; 
    // 根据hid不同写入不同的数据到obj
    if(head_->hid != SIS_DISK_HID_MSG_LOG)
    {
        LOG(5)("other hid : %d at log.\n", head_->hid);
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

int sis_disk_io_sub_log(s_sis_disk_ctrl *cls_, s_sis_disk_reader *reader_)
{
    if (!reader_ || !reader_->callback)
    {
        return -1;
    }
    cls_->reader = reader_;
    cls_->isstop = false;  // 用户可以随时中断

    s_sis_disk_reader_cb *callback = reader_->callback;   
    if(callback->cb_start)
    {
        callback->cb_start(callback->cb_source, cls_->open_date);
    }
    // 每次都从头读起
    sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_log);

    // 不是因为中断而结束 就发送stop标志
    if(callback->cb_stop && !cls_->isstop)
    {
        callback->cb_stop(callback->cb_source, cls_->stop_date);
    }
    cls_->reader = NULL;
    return 0;
}
