/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <HwDisplayCrtc.h>
#include <MesonLog.h>

HwDisplayCrtc::HwDisplayCrtc(int drvFd, int32_t id) {
    mId = id;
    mDrvFd = drvFd;
}

HwDisplayCrtc::~HwDisplayCrtc() {
}

int32_t HwDisplayCrtc::setMode(drm_mode_info_t & mode) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtc::pageFlip(int32_t & out_fence) {
    MESON_LOG_EMPTY_FUN();
    out_fence = -1;
    return 0;
}

