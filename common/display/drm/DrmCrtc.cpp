/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>

 #include "DrmCrtc.h"

DrmCrtc::DrmCrtc(drmModeCrtcPtr p, uint32_t pipe)
    : HwDisplayCrtc() {
    mId = p->crtc_id;
    mPipe = pipe;
    mModeValid = p->mode_valid;
    if (mModeValid) {
        mMode = p->mode;
    } else {
        memset(&mMode, 0, sizeof(drmModeModeInfo));
    }

    mConnector.reset();
}

DrmCrtc::~DrmCrtc() {
    mConnector.reset();

}

int32_t DrmCrtc::getId() {
    return mId;
}

uint32_t DrmCrtc::getPipe() {
    return mPipe;
}

int32_t DrmCrtc::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_ASSERT(mConnector, "Crtc need setuped before load Properities.");


    MESON_LOGE("DrmCrtc::update nothing to do.");
    return 0;
}

int32_t DrmCrtc::getMode(drm_mode_info_t & mode) {
    if (!mModeValid) {
        return -EFAULT;
    }

    strncpy(mode.name, mMode.name, DRM_DISPLAY_MODE_LEN);
    mode.refreshRate = mMode.vrefresh;
    mode.pixelW = mMode.vdisplay;
    mode.pixelH = mMode.hdisplay;

    mode.dpiX = mode.dpiY = 160;
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::setMode(drm_mode_info_t & mode) {
    UNUSED(mode);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::waitVBlank(nsecs_t & timestamp) {
    UNUSED(timestamp);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::pageFlip(int32_t & out_fence) {
    UNUSED(out_fence);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void DrmCrtc::setViewPort(const drm_rect_wh_t viewPort) {
    MESON_LOGE("setViewPort should move out.");
    std::lock_guard<std::mutex> lock(mMutex);
    mViewPort = viewPort;
}

void DrmCrtc::getViewPort(drm_rect_wh_t & viewPort) {
    MESON_LOGE("getViewPort should move out.");
    std::lock_guard<std::mutex> lock(mMutex);
    viewPort = mViewPort;
}

