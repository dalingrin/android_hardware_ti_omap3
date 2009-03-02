/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define OVERLAY_DEBUG 1
#define LOG_TAG "Overlay"

#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <hardware/overlay.h>
#include <linux/videodev.h>
#include <omap24xxvout.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "v4l2_utils.h"

#define LOG_FUNCTION_NAME    LOGD(" %s ###### Calling %s() ######",  __FILE__, __FUNCTION__);

#ifndef LOGE
#define LOGE(fmt,args...) \
        do { printf(fmt, ##args); } \
        while (0)
#endif

#ifndef LOGI
#define LOGI(fmt,args...) \
        do { LOGE(fmt, ##args); } \
        while (0)
#endif

int v4l2_overlay_get(int name) {
    int result = -1;
    switch (name) {
        case OVERLAY_MINIFICATION_LIMIT:
            result = 4; // 0 = no limit
            break;
        case OVERLAY_MAGNIFICATION_LIMIT:
            result = 2; // 0 = no limit
            break;
        case OVERLAY_SCALING_FRAC_BITS:
            result = 0; // 0 = infinite
            break;
        case OVERLAY_ROTATION_STEP_DEG:
            result = 90; // 90 rotation steps (for instance)
            break;
        case OVERLAY_HORIZONTAL_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_VERTICAL_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_WIDTH_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_HEIGHT_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
    }
    return result;
}

int v4l2_overlay_open(int id)
{
    LOG_FUNCTION_NAME

    if (id == V4L2_OVERLAY_PLANE_VIDEO1)
        return open("/dev/video1", O_RDWR);
    else if (id == V4L2_OVERLAY_PLANE_VIDEO2)
        return open("/dev/video2", O_RDWR);
    return -EINVAL;
}

void dump_pixfmt(struct v4l2_pix_format *pix)
{
    LOGI("w: %d\n", pix->width);
    LOGI("h: %d\n", pix->height);
    LOGI("color: %x\n", pix->colorspace);
    switch (pix->pixelformat) {
        case V4L2_PIX_FMT_YUYV:
            LOGI ("YUYV\n");
            break;
        case V4L2_PIX_FMT_UYVY:
            LOGI ("UYVY\n");
            break;
        case V4L2_PIX_FMT_RGB565:
            LOGI ("RGB565\n");
            break;
        case V4L2_PIX_FMT_RGB565X:
            LOGI ("RGB565X\n");
            break;
        default:
            LOGI("not supported\n");
    }
}

void dump_crop(struct v4l2_crop *crop)
{
    LOGI("crop l: %d ", crop->c.left);
    LOGI("crop t: %d ", crop->c.top);
    LOGI("crop w: %d ", crop->c.width);
    LOGI("crop h: %d\n", crop->c.height);
}

void dump_window(struct v4l2_window *win)
{
    LOGI("window l: %d ", win->w.left);
    LOGI("window t: %d ", win->w.top);
    LOGI("window w: %d ", win->w.width);
    LOGI("window h: %d\n", win->w.height);
}
void v4l2_overlay_dump_state(int fd)
{
    struct v4l2_format format;
    struct v4l2_crop crop;
    int ret;

    LOGI("dumping driver state:");
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    if (ret < 0)
        return;
    LOGI("output pixfmt:\n");
    dump_pixfmt(&format.fmt.pix);

    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    if (ret < 0)
        return;
    LOGI("v4l2_overlay window:\n");
    dump_window(&format.fmt.win);

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_G_CROP, &crop);
    if (ret < 0)
        return;
    LOGI("output crop:\n");
    dump_crop(&crop);
/*
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_G_CROP, &crop);
    if (ret < 0)
        return;
    LOGI("ovelay crop:\n");
    dump_crop(&crop);
*/
}

static void error(int fd, const char *msg)
{
  LOGE("Error = %s from %s", strerror(errno), msg);
#ifdef OVERLAY_DEBUG
  v4l2_overlay_dump_state(fd);
#endif
}

static int v4l2_overlay_ioctl(int fd, int req, void *arg, const char* msg)
{
    int ret;
    ret = ioctl(fd, req, arg);
    if (ret < 0) {
        error(fd, msg);
        return -1;
    }
    return 0;
}

int configure_pixfmt(struct v4l2_pix_format *pix, int32_t fmt,
                     uint32_t w, uint32_t h)
{
    LOG_FUNCTION_NAME

    int fd;

    switch (fmt) {
        case OVERLAY_FORMAT_RGBA_8888:
            return -1;
        case OVERLAY_FORMAT_RGB_565:
            pix->pixelformat = V4L2_PIX_FMT_RGB565;
            break;
        case OVERLAY_FORMAT_BGRA_8888:
            return -1;
        case OVERLAY_FORMAT_YCbCr_422_SP:
            break;
        case OVERLAY_FORMAT_YCbCr_420_SP:
            return -1;
        case OVERLAY_FORMAT_YCbCr_422_I:
            //pix->pixelformat = V4L2_PIX_FMT_YUYV;
            pix->pixelformat = V4L2_PIX_FMT_UYVY;
            break;
        case OVERLAY_FORMAT_YCbCr_420_I:
            return -1;
        default:
            return -1;
    }
    pix->width = w;
    pix->height = h;
    return 0;
}

static void configure_window(struct v4l2_window *win, int32_t w,
                             int32_t h, int32_t x, int32_t y)
{
    LOG_FUNCTION_NAME

    win->w.left = x;
    win->w.top = y;
    win->w.width = w;
    win->w.height = h;
}

void get_window(struct v4l2_format *format, int32_t *x,
                int32_t *y, int32_t *w, int32_t *h)
{
    LOG_FUNCTION_NAME

    *x = format->fmt.win.w.left;
    *y = format->fmt.win.w.top;
    *w = format->fmt.win.w.width;
    *h = format->fmt.win.w.height;
}

static int set_crop(int fd, int32_t left, int32_t top, int32_t width,
                    int32_t height)
{
    LOG_FUNCTION_NAME

    struct v4l2_crop crop;
    int ret;

    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_CROP, &crop, "set crop");
    crop.c.left = left;
    crop.c.top = top;
    crop.c.width = width;
    crop.c.height = height;
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    return v4l2_overlay_ioctl(fd, VIDIOC_S_CROP, &crop, "set crop");
}


int v4l2_overlay_init(int fd, uint32_t w, uint32_t h, uint32_t fmt)
{
    LOG_FUNCTION_NAME

    struct v4l2_format format;
    int ret;

    /* configure the v4l2_overlay framebuffer */
    /*
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FBUF, &fbuf, "get fbuf");
    if (ret)
        return ret;
    if (fbuf.fmt.pixelformat != dst_format) {
        fbuf.fmt.pixelformat = dst_format;
        ret = v4l2_overlay_ioctl(fd, VIDIOC_S_FBUF, &fbuf, "set fbuf");
        if (ret)
            return ret;
    }
    */

    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get format");
    if (ret)
        return ret;
    LOGI("v4l2_overlay_init:: w=%d h=%d", format.fmt.pix.width, format.fmt.pix.height);

    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    configure_pixfmt(&format.fmt.pix, fmt, w, h);
    LOGI("v4l2_overlay_init:: w=%d h=%d", format.fmt.pix.width, format.fmt.pix.height);
    ret = v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &format, "set output format");

    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get output format");
    LOGI("v4l2_overlay_init:: w=%d h=%d", format.fmt.pix.width, format.fmt.pix.height);
    return ret;
}

int v4l2_overlay_set_position(int fd, int32_t x, int32_t y, int32_t w, int32_t h)
{
    LOG_FUNCTION_NAME

    struct v4l2_format format;
    int ret;

     /* configure the src format pix */
    /* configure the dst v4l2_overlay window */
    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format,
                             "get v4l2_overlay format");
    if (ret)
       return ret;
    LOGI("v4l2_overlay_set_position:: w=%d h=%d", format.fmt.win.w.width, format.fmt.win.w.height);

    configure_window(&format.fmt.win, w, h, x, y);
    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &format,
                             "set v4l2_overlay format");
    LOGI("v4l2_overlay_set_position:: w=%d h=%d", format.fmt.win.w.width, format.fmt.win.w.height);

    if (ret)
       return ret;
    v4l2_overlay_dump_state(fd);

    return 0;
}

int v4l2_overlay_get_position(int fd, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    struct v4l2_format format;
    int ret;

    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get v4l2_overlay format");
    if (ret)
       return ret;
    get_window(&format, x, y, w, h);
    return 0;
}


int v4l2_overlay_set_rotation(int fd, int degree, int step)
{
    LOG_FUNCTION_NAME

    int ret;

    ret = ioctl (fd, VIDIOC_S_OMAP2_ROTATION, &degree);
    if (ret < 0) {
        error (fd, "VIDIOC_S_OMAP2_ROTATION ioctl");
        return ret;
    }

    return 0;
}

int v4l2_overlay_set_colorkey(int fd,  int enable, int colorkey)
{
    LOG_FUNCTION_NAME

    int ret;
    struct omap24xxvout_colorkey sColorkey;
    sColorkey.output_dev = OMAP24XX_OUTPUT_LCD;
    sColorkey.key_type = OMAP24XX_GFX_DESTINATION;
    sColorkey.key_val = colorkey;

    if (enable){
        ret = ioctl (fd, VIDIOC_OMAP2_COLORKEY_ENABLE, &sColorkey.output_dev);
        if (ret < 0) {
            error (fd, "VIDIOC_OMAP2_COLORKEY_ENABLE ioctl");
            return ret;
        }

        ret = ioctl (fd, VIDIOC_S_OMAP2_COLORKEY, &sColorkey);
        if (ret < 0) {
            error (fd, "VIDIOC_S_OMAP2_COLORKEY ioctl");
            return ret;
        }
    }
    else{
        ret = ioctl (fd, VIDIOC_OMAP2_COLORKEY_DISABLE, &sColorkey.output_dev);
        if (ret < 0) {
            error (fd, "VIDIOC_OMAP2_COLORKEY_DISABLE ioctl");
            return ret;
        }
    }
    return 0;
}

int v4l2_overlay_req_buf(int fd, uint32_t *num_bufs)
{
    LOG_FUNCTION_NAME

    struct v4l2_requestbuffers reqbuf;
    int ret, i;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = *num_bufs;
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0) {
        error(fd, "reqbuf ioctl");
        return ret;
    }
    LOGI("%d buffers allocated %d requested", reqbuf.count, 4);
    if (reqbuf.count > *num_bufs) {
        error(fd, "Not enough buffer structs passed to get_buffers");
        return -ENOMEM;
    }
    *num_bufs = reqbuf.count;
    LOGI("buffer cookie is %d", reqbuf.type);
    return 0;
}

static int is_mmaped(struct v4l2_buffer *buf)
{
    return buf->flags == V4L2_BUF_FLAG_MAPPED;
}

static int is_queued(struct v4l2_buffer *buf)
{
    /* is either on the input or output queue in the kernel */
    return (buf->flags & V4L2_BUF_FLAG_QUEUED) ||
           (buf->flags & V4L2_BUF_FLAG_DONE);
}

static int is_dequeued(struct v4l2_buffer *buf)
{
    /* is on neither input or output queue in kernel */
    return (!(buf->flags & V4L2_BUF_FLAG_QUEUED) &&
            !(buf->flags & V4L2_BUF_FLAG_DONE));
}

static int query_buffer(int fd, int index, struct v4l2_buffer *buf)
{
    LOG_FUNCTION_NAME

    buf->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf->memory = V4L2_MEMORY_MMAP;
    buf->index = index;
    LOGI("query buffer, mem=%u type=%u index=%u\n", buf->memory, buf->type,
         buf->index);
    return v4l2_overlay_ioctl(fd, VIDIOC_QUERYBUF, buf, "querybuf ioctl");
}

int v4l2_overlay_map_buf(int fd, int index, void **start, size_t *len)
{
    LOG_FUNCTION_NAME

    struct v4l2_buffer buf;
    int ret;

    ret = query_buffer(fd, index, &buf);
    if (ret)
        return ret;

    if (is_mmaped(&buf)) {
        LOGE("Trying to mmap buffers that are already mapped!\n");
        return -EINVAL;
    }

    *len = buf.length;
    *start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                  fd, buf.m.offset);
    if (*start == MAP_FAILED) {
        LOGE("map failed, length=%u offset=%u\n", buf.length, buf.m.offset);
        return -EINVAL;
    }
    return 0;
}

int v4l2_overlay_unmap_buf(void *start, size_t len)
{
    LOG_FUNCTION_NAME

    return munmap(start, len);
}


int v4l2_overlay_get_caps(int fd, struct v4l2_capability *caps)
{
    return v4l2_overlay_ioctl(fd, VIDIOC_QUERYCAP, caps, "query cap");
}

int v4l2_overlay_stream_on(int fd)
{
    LOG_FUNCTION_NAME

    uint32_t type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    return v4l2_overlay_ioctl(fd, VIDIOC_STREAMON, &type, "stream on");
}

int v4l2_overlay_stream_off(int fd)
{
    LOG_FUNCTION_NAME

    uint32_t type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    return v4l2_overlay_ioctl(fd, VIDIOC_STREAMOFF, &type, "stream off");
}

int v4l2_overlay_q_buf(int fd, int index)
{
    struct v4l2_buffer buf;
    int ret;

    /*
    ret = query_buffer(fd, buffer_cookie, index, &buf);
    if (ret)
        return ret;
    if (is_queued(buf)) {
        LOGE("Trying to queue buffer to kernel that is already queued!\n");
        return -EINVAL
    }
    */
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.index = index;
    buf.memory = V4L2_MEMORY_MMAP;

    return v4l2_overlay_ioctl(fd, VIDIOC_QBUF, &buf, "qbuf");
}

int v4l2_overlay_dq_buf(int fd, int *index)
{
    struct v4l2_buffer buf;
    int ret;

    /*
    ret = query_buffer(fd, buffer_cookie, index, &buf);
    if (ret)
        return ret;

    if (is_dequeued(buf)) {
        LOGE("Trying to dequeue buffer that is not in kernel!\n");
        return -EINVAL
    }
    */
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory = V4L2_MEMORY_MMAP;

    ret = v4l2_overlay_ioctl(fd, VIDIOC_DQBUF, &buf, "dqbuf");
    if (ret)
      return ret;
    *index = buf.index;
    return 0;
}