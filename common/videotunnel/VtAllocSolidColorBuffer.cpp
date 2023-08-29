/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include <errno.h>
#include <am_gralloc_ext.h>

#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>

#include "VtAllocSolidColorBuffer.h"

VtAllocSolidColorBuffer::VtAllocSolidColorBuffer() {
    mBufferFd = -1;
    mColorType = SOLID_COLOR_FOR_INVALID;
    mSolidColorHandle = NULL;
}

VtAllocSolidColorBuffer::~VtAllocSolidColorBuffer() {
    if (mBufferFd >= 0)
        close(mBufferFd);

    if (mSolidColorHandle)
        freeBuffer();
}

int VtAllocSolidColorBuffer::allocBuffer(vt_video_color_t colorType) {
    uint32_t stride;
    int format = 17; // always HAL_PIXEL_FORMAT_YCrCb_420_SP
    int len_y = VIDEO_BUFFER_H * VIDEO_BUFFER_W;
    int len_vu = len_y * 1 / 2;
    int i = 0;
    int ret;
    unsigned char *cpu_ptr = NULL;
    uint64_t usage = 0x408933;
    static GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (mBufferFd >= 0 && mColorType == colorType)
        return mBufferFd;

    mColorType = colorType;

    if (!mSolidColorHandle) {
        ret = allocService.allocate(VIDEO_BUFFER_W, VIDEO_BUFFER_H, format, 1,
                usage, &mSolidColorHandle, &stride, 0, "MesonHwcSolidColorBuffer");
        if (NO_ERROR != ret) {
            MESON_LOGE("%s alloc buffer failed", __func__);
            return -ret;
        }
    }

    mBufferFd =
        am_gralloc_get_buffer_fd((native_handle_t *)mSolidColorHandle);

    if (mBufferFd <= 0) {
        MESON_LOGE("%s, solid color fd is invalid", __func__);
        return -EINVAL;
    }

    cpu_ptr = (unsigned char *)mmap(NULL, len_y + len_vu,
            PROT_READ | PROT_WRITE, MAP_SHARED, mBufferFd, 0);
    switch (mColorType) {
        case SOLID_COLOR_FOR_BLACK:
            /* BLACK: set Y to 10 and UV to 0x80 */
            memset(cpu_ptr, 0x10, len_y);
            memset(cpu_ptr + len_y, 0x80, len_vu);
            break;
        case SOLID_COLOR_FOR_BLUE:
            /* BLUE: Y:0x29, V:0x6e, U:0xf0*/
            memset(cpu_ptr, 0x29, len_y);
            for (i = 0; i < len_vu; i += 2) {
                memset(cpu_ptr + len_y + i, 0x6e, 1);
                memset(cpu_ptr + len_y + i + 1, 0xf0, 1);
            }
            break;
        case SOLID_COLOR_FOR_GREEN:
            /* GREEN: set Y:0x90 V:0x36, U:0x23*/
            memset(cpu_ptr, 0x90, len_y);
            for (i = 0; i < len_vu; i += 2) {
                memset(cpu_ptr + len_y + i, 0x36, 1);
                memset(cpu_ptr + len_y + i + 1, 0x23, 1);
                i++;
            }
            break;
        default:
            memset(cpu_ptr, 0x0, len_y + len_vu);
    }
    munmap(cpu_ptr, len_y + len_vu);

    return mBufferFd;
}

int VtAllocSolidColorBuffer::getPreBuffer() {
    return mBufferFd;
}

void VtAllocSolidColorBuffer::freeBuffer() {
    if (mSolidColorHandle) {
        static GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();
        allocService.free(mSolidColorHandle);
        mSolidColorHandle = NULL;
    }
}
