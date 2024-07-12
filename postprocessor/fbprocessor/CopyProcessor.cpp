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
    mGe2dHelper = std::make_shared<Ge2dHelper>();
    mRotation = (Rotation) HwcConfig::getVirtualDisplayRotation();
    MESON_LOGV("CopyProcessor: mRotation = %d", mRotation);
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
    Ge2dBufferInfo srcInfo, dstInfo;

    srcInfo.fmt = am_gralloc_get_format (inputfb->mBufferHandle);
    srcInfo.w = am_gralloc_get_width(inputfb->mBufferHandle);
    srcInfo.h = am_gralloc_get_height(inputfb->mBufferHandle);
    srcInfo.fd = am_gralloc_get_buffer_fd(inputfb->mBufferHandle);
    srcInfo.stride = am_gralloc_get_stride_in_pixel(inputfb->mBufferHandle);
    dstInfo.fmt = am_gralloc_get_format (outfb->mBufferHandle);
    dstInfo.fd = am_gralloc_get_buffer_fd(outfb->mBufferHandle);
    dstInfo.w = am_gralloc_get_width(outfb->mBufferHandle);
    dstInfo.h = am_gralloc_get_height(outfb->mBufferHandle);
    dstInfo.stride = am_gralloc_get_stride_in_pixel(outfb->mBufferHandle);

    MESON_LOGV("CopyProcessor %dx%d -> %dx%d, fmt %d -> %d, Stride in pixel %d -> %d",
            srcInfo.w, srcInfo.h, dstInfo.w, dstInfo.h, srcInfo.fmt,
            dstInfo.fmt, srcInfo.stride, dstInfo.stride);

    {
        ATRACE_BEGIN("CopyProcessor::copy");
        mGe2dHelper->ge2DFmtConvert(srcInfo, dstInfo, mRotation);
        ATRACE_END();
    }
    return 0;
}

int32_t CopyProcessor::teardown() {
    return 0;
}

