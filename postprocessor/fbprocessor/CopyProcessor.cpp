/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#include "CopyProcessor.h"
#include <MesonLog.h>

#include <ui/PixelFormat.h>

CopyProcessor::CopyProcessor() {
}

CopyProcessor::~CopyProcessor() {
}

int32_t CopyProcessor::setup() {
    return 0;
}

int32_t CopyProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb __unused,
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int & processFence __unused) {
    return 0;
}

int32_t CopyProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence __unused) {
    return 0;
}

int32_t CopyProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb,
    std::shared_ptr<DrmFramebuffer> & outfb) {
    void * inmem = NULL, * outmem = NULL;
    int infmt = am_gralloc_get_format (inputfb->mBufferHandle);
    int outfmt = am_gralloc_get_format (outfb->mBufferHandle);
    int w = am_gralloc_get_width(inputfb->mBufferHandle);
    int h = am_gralloc_get_height(inputfb->mBufferHandle);
    int instride = am_gralloc_get_stride_in_pixel(inputfb->mBufferHandle);
    int outstride = am_gralloc_get_stride_in_pixel(outfb->mBufferHandle);

    MESON_LOGV("CopyProcessor %dx%d stride (%d,%d), fmt %d, %d",
        w, h, instride, outstride, infmt, outfmt);

    if (inputfb->lock(&inmem) == 0 && outfb->lock(&outmem) == 0) {
        char * src =  (char *)inmem;
        char * dst =  (char *)outmem;
        ATRACE_BEGIN("CopyProcessor::copy");

        if (infmt == outfmt) {
            int32_t bytes = bytesPerPixel(infmt);
            for (int ir = 0; ir < h; ir++) {
                memcpy(dst, src, w * bytes);
                src += instride * bytes;
                dst += outstride * bytes;
            }
        } else if (infmt == HAL_PIXEL_FORMAT_RGB_888 && outfmt == HAL_PIXEL_FORMAT_RGBA_8888) {
            int32_t bytes = bytesPerPixel(outfmt);
            for (int ir = 0; ir < h; ir++) {
                for (int k = 0; k < w; k++) {
                    dst[k * 4] = src[k * 3];
                    dst[k * 4 + 1] = src[k * 3 + 1];
                    dst[k * 4 + 2] = src[k * 3 + 2];
                    dst[k * 4 + 3] = 0xFF;
                }
                src += instride * 3;
                dst += outstride * bytes;
            }
        } else {
            MESON_LOGE("infmt doesn't match outfmt, don't copy infmt = %d, outfmt = %d", infmt, outfmt);
            return -1;
        }

        ATRACE_END();
        inputfb->unlock();
        outfb->unlock();
    }

    return 0;
}

int32_t CopyProcessor::teardown() {
    return 0;
}

