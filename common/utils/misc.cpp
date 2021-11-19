/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <BasicTypes.h>
#include <MesonLog.h>

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <cutils/properties.h>
#include <am_gralloc_ext.h>

#if PLATFORM_SDK_VERSION >= 28
#include <ui/Rect.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#endif

#include <misc.h>
#include <ion/ion.h>
#include <linux/ion_4.12.h>

#define ION_FLAG_EXTEND_MESON_HEAP (1 << 30)
#define ION_HEAP_TYPE_CUSTOM 16

static int32_t solid_color_fd = -1;

bool sys_get_bool_prop(const char *prop, bool defVal) {
    return property_get_bool(prop, defVal);
}

int32_t sys_get_string_prop(const char *prop, char *val) {
    return property_get(prop, val, NULL);
}

int32_t sys_set_prop(const char *prop, const char *val) {
    return property_set(prop, val);
}

int32_t sysfs_get_string_ex(const char* path, char *str, int32_t size,
    bool needOriginalData) {

    int32_t fd, len;

    if ( NULL == str ) {
        MESON_LOGE("buf is NULL");
        return -1;
    }

    if ((fd = open(path, O_RDONLY)) < 0) {
        MESON_LOGE("readSysFs, open %s fail.", path);
        return -1;
    }

    len = read(fd, str, size);
    if (len < 0) {
        MESON_LOGE("read error: %s, %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (!needOriginalData) {
        int32_t i , j;
        for (i = 0, j = 0; i <= len -1; i++) {
            /*change '\0' to 0x20(spacing), otherwise the string buffer will be cut off
             * if the last char is '\0' should not replace it
             */
            if (0x0 == str[i] && i < len - 1) {
                str[i] = 0x20;
            }

            /* delete all the character of '\n' */
            if (0x0a != str[i]) {
                str[j++] = str[i];
            }
        }

        str[j] = 0x0;
    }

    close(fd);
    return 0;
}

int32_t sysfs_get_string(const char* path, char *str, int32_t len) {
    char * buf = new char[len];
    int32_t ret = sysfs_get_string_ex(path, (char*)buf, len, false);
    strcpy(str, buf);
    delete[] buf;

    return ret;
}

int32_t sysfs_get_string_original(const char *path, char *str, int32_t len) {
    char * buf = new char[len];
    int32_t ret = sysfs_get_string_ex(path, (char*)buf, len, true);
    strncpy(str, buf, len);
    str[len-1] = '\0';
    delete[] buf;
    return ret;
}

int32_t sysfs_set_string(const char *path, const char *val) {
    int32_t bytes;
    int32_t fd = open(path, O_RDWR);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        //MESON_LOGI("setSysfsStr %s= %s\n", path,val);
        close(fd);
        return 0;
    } else {
        MESON_LOGE(" open file [%s] error:%s", path, strerror(errno));
        return -1;
    }
}

int32_t sysfs_get_int(const char* path, int32_t def) {
    int32_t val = def;
    char str[64];
    if (sysfs_get_string(path, str, 64) == 0) {
        val = atoi(str);
        MESON_LOGD("sysfs(%s) read int32_t (%d)", path, val);
    }
    return val;
}

#if PLATFORM_SDK_VERSION >= 28
native_handle_t * gralloc_alloc_dma_buf(
    int w, int h, int format, bool bScanout, bool afbc) {
    static GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();
    uint32_t stride;
    uint64_t usage = 0;

    if (bScanout)
        usage |= GRALLOC_USAGE_HW_FB;
    if (!afbc)
        usage |= GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;

    buffer_handle_t handle;
    if (NO_ERROR != allocService.allocate(
        w, h, format, 1, usage, &handle, &stride, 0, "MesonHwc")) {
        MESON_LOGE("gralloc_alloc_dma_buf alloc (%dx%d-%d) failed.", w, h, format);
        return NULL;
    }

    return (native_handle_t *)handle;
}

int32_t gralloc_free_dma_buf(native_handle_t * hnd) {
    static GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();
    allocService.free(hnd);
    return 0;
}

native_handle_t * gralloc_ref_dma_buf(const native_handle_t * hnd, bool isSidebandBuffer) {
    /*must use GraphicBufferMapper, for we need do lock&unlock by it later.*/
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();
    buffer_handle_t rethnd;
    if (!isSidebandBuffer && am_gralloc_is_valid_graphic_buffer(hnd)) {
        int w = am_gralloc_get_width(hnd);
        int h = am_gralloc_get_height(hnd);
        int stridew = am_gralloc_get_stride_in_pixel(hnd);
        int format = am_gralloc_get_format(hnd);
        uint64_t usage = am_gralloc_get_consumer_usage(hnd);

        if (NO_ERROR != maper.importBuffer(hnd, w, h, 1, format, usage, stridew, &rethnd)) {
            /*may be we got handle not alloc by gralloc*/
            MESON_LOGE("gralloc_ref_dma_buf import buffer (%dx%d) failed.", w, h);
            rethnd = native_handle_clone(hnd);
        }
    } else {
        rethnd = native_handle_clone(hnd);
    }

    return (native_handle_t *)rethnd;
}

int32_t gralloc_unref_dma_buf(native_handle_t * hnd, bool isSidebandBuffer) {
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();

    bool bfreed = false;
    if (!isSidebandBuffer && am_gralloc_is_valid_graphic_buffer(hnd)) {
        if (NO_ERROR == maper.freeBuffer(hnd)) {
            bfreed = true;
        }
    }

    if (bfreed == false) {
        /*may be we got handle not alloc by gralloc*/
        native_handle_close(hnd);
        native_handle_delete(hnd);
    }

    return 0;
}

int32_t gralloc_lock_dma_buf(
    native_handle_t * handle, void** vaddr) {
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();
    uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
    int w = am_gralloc_get_width(handle);
    int h = am_gralloc_get_height(handle);

    Rect r(w, h);
    if (NO_ERROR == maper.lock(handle, usage, r, vaddr))
        return 0;

    MESON_LOGE("lock buffer failed ");
    return -EINVAL;
}

int32_t gralloc_unlock_dma_buf(native_handle_t * handle) {
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();
    if (NO_ERROR == maper.unlock(handle))
        return 0;
    return -EINVAL;
}

int32_t gralloc_alloc_solid_color_buf() {
    int32_t ion_fd = -1;
    int32_t len = VIDEO_BUFFER_W * VIDEO_BUFFER_H * 3 / 2;
    int ret;
    unsigned int heap_mask = 1 << ION_HEAP_TYPE_CUSTOM;

    ion_fd = ion_open();
    if (ion_fd < 0) {
        MESON_LOGE("%s ion_open fail", __func__);
        return -EINVAL;
    }

    ret = ion_alloc_fd(ion_fd, len, 0, heap_mask,
            ION_FLAG_EXTEND_MESON_HEAP, &solid_color_fd);
    if (ret < 0) {
        MESON_LOGE("%s ion_alloc_fd fail, ret:%d",
                __func__, ret);
    }
    close(ion_fd);

    return ret;
}

int32_t gralloc_free_solid_color_buf() {
    if (solid_color_fd >= 0) {
        close(solid_color_fd);
        solid_color_fd = -1;
    }

    return 0;
}

int32_t gralloc_get_solid_color_buf_fd(video_color_t color) {
    int buffer_size = VIDEO_BUFFER_H * VIDEO_BUFFER_W;
    int len = buffer_size * 3 / 2;
    unsigned char *cpu_ptr = NULL;
    static video_color_t pre_color = SET_VIDEO_INVALID;

    if (solid_color_fd >= 0) {
        if (pre_color == color)
            return solid_color_fd;

        cpu_ptr = (unsigned char *)mmap(NULL, len,
                PROT_READ | PROT_WRITE, MAP_SHARED, solid_color_fd, 0);
        switch (color) {
            case SET_VIDEO_TO_BLACK :
                /* set Y to 0 and UV to 128*/
                memset(cpu_ptr, 0x0, buffer_size);
                memset(cpu_ptr + buffer_size, 0x80, buffer_size / 2);
                pre_color = SET_VIDEO_TO_BLACK;
                break;
            default:
                pre_color = SET_VIDEO_INVALID;
                memset(cpu_ptr, 0x00, len);
        }
        munmap(cpu_ptr, len);

        return solid_color_fd;
    }

    MESON_LOGE("%s, solid color fd is invalid", __func__);
    return -EINVAL;
}
#else
native_handle_t* native_handle_clone(const native_handle_t* handle) {

    if (!handle) return NULL;

    native_handle_t* clone = native_handle_create(handle->numFds, handle->numInts);
    if (!clone) return NULL;

    for (int i = 0; i < handle->numFds; i++) {
        clone->data[i] = ::dup(handle->data[i]);
        if (clone->data[i] < 0) {
            ALOGE("native_handle clone fail, delete clone handle");
            clone->numFds = i;
            native_handle_close(clone);
            native_handle_delete(clone);
            return NULL;
        }
    }

    memcpy(&clone->data[handle->numFds], &handle->data[handle->numFds],
            sizeof(int) * handle->numInts);

    return clone;
}

native_handle_t * gralloc_alloc_dma_buf(
    int w, int h, int format, bool bScanout, bool afbc) {
    UNUSED(w);
    UNUSED(h);
    UNUSED(format);
    UNUSED(bScanout);
    UNUSED(afbc);
    MESON_ASSERT(0, "NO IMPLEMENT.");
    return NULL;
}

int32_t gralloc_free_dma_buf(native_handle_t * hnd) {
    MESON_ASSERT(0, "NO IMPLEMENT.");
    return 0;
}

native_handle_t * gralloc_ref_dma_buf(const native_handle_t * hnd) {
    ./*in fact we need the retain function in gralloc1 hal, but
     it is not exposed to other moduels,
     we use clone instead now.*/
    return native_handle_clone(hnd);
}

int32_t gralloc_unref_dma_buf(native_handle_t * hnd) {
    native_handle_close(hnd);
    native_handle_delete(hnd);
}

int32_t gralloc_lock_dma_buf(
    native_handle_t * handle, uint32_t usage, void** vaddr) {
    UNUSED(handle);
    UNUSED(usage);
    UNUSED(vaddr);
    MESON_ASSERT(0, "gralloc_lock_dma_buf NO IMPLEMENT.");
    return -EINVAL;
}

int32_t gralloc_unlock_dma_buf(native_handle_t * handle) {
    UNUSED(handle);
    MESON_ASSERT(0, "gralloc_unlock_dma_buf NO IMPLEMENT.");
    return -EINVAL;
}

int32_t gralloc_alloc_solid_color_buf() {
    MESON_ASSERT(0, "%s NO IMPLEMENT.", __func__);
    return -EINVAL;
}

int32_t gralloc_free_solid_color_buf() {
    MESON_ASSERT(0, "%s NO IMPLEMENT.", __func__);
    return -EINVAL;
}

int32_t gralloc_get_solid_color_buf_fd(video_color_t color) {
    UNUSED(color);
    MESON_ASSERT(0, "%s NO IMPLEMENT.", __func__);
}
#endif
