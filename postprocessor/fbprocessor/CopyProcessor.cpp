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
    int infmt = am_gralloc_get_format (inputfb->mBufferHandle);
    int outfmt = am_gralloc_get_format (outfb->mBufferHandle);
    int w = am_gralloc_get_width(inputfb->mBufferHandle);
    int h = am_gralloc_get_height(inputfb->mBufferHandle);
    int srcFd = am_gralloc_get_buffer_fd(inputfb->mBufferHandle);
    int dstFd = am_gralloc_get_buffer_fd(outfb->mBufferHandle);
    MESON_LOGV("CopyProcessor %dx%d, fmt %d, %d", w, h, infmt, outfmt);
    {
        ATRACE_BEGIN("CopyProcessor::copy");
        mGe2dHelper->ge2DFmtConvert(dstFd, outfmt, w, h, srcFd, infmt, w, h);
        ATRACE_END();
    }
    return 0;
}

int32_t CopyProcessor::teardown() {
    return 0;
}

