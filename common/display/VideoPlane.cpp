/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VideoPlane.h"
#include <MesonLog.h>

VideoPlane::VideoPlane(int32_t drvFd, uint32_t id) :
    HwDisplayPlane(drvFd, id) {
    mPlaneType = VIDEO_PLANE;
}

VideoPlane::~VideoPlane() {

}

int VideoPlane::setPlane(std::shared_ptr<DrmFramebuffer> & fb) {
    buffer_handle_t buf      = fb->mBufferHandle;
#if 0
    drm_rect_t srcCrop       = fb->mSourceCrop;
    drm_rect_t disFrame      = fb->mDisplayFrame;

    mPlaneInfo.xoffset       = srcCrop.left;
    mPlaneInfo.yoffset       = srcCrop.top;
    mPlaneInfo.width         = srcCrop.right  - srcCrop.left;
    mPlaneInfo.height        = srcCrop.bottom - srcCrop.top;

    mPlaneInfo.dst_x         = disFrame.left;
    mPlaneInfo.dst_y         = disFrame.top;
    mPlaneInfo.dst_w         = disFrame.right  - disFrame.left;
    mPlaneInfo.dst_h         = disFrame.bottom - disFrame.top;
#endif
    MESON_LOGD("videoPlane [%p]", (void*)buf);

    // dumpPlaneInfo();
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t VideoPlane::blank() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void VideoPlane::dump(String8 & dumpstr) {
    MESON_LOG_EMPTY_FUN();
}

