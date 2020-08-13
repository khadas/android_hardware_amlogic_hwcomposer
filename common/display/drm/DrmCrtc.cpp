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

DrmCrtc::DrmCrtc()
: HwDisplayCrtc() {

}

DrmCrtc::~DrmCrtc() {

}

int32_t DrmCrtc::bind(
    std::shared_ptr<HwDisplayConnector>  connector,
    std::vector<std::shared_ptr<HwDisplayPlane>> planes) {
    UNUSED(connector);
    UNUSED(planes);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::unbind() {
    return 0;
}

int32_t DrmCrtc::loadProperities() {
    return 0;
}

int32_t DrmCrtc::update() {
    return 0;
}

int32_t DrmCrtc::getId() {
    return 0;
}

int32_t DrmCrtc::getMode(drm_mode_info_t & mode) {
    UNUSED(mode);
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

