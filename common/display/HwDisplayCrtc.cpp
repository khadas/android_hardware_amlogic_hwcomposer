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

/* FBIO */
#define FBIOPUT_OSD_SYNC_FLIP 0x451b

HwDisplayCrtc::HwDisplayCrtc(int drvFd, int32_t id) {
    mId = id;
    mDrvFd = drvFd;
    mDisplayInfo.out_fen_fd = -1;
}

HwDisplayCrtc::~HwDisplayCrtc() {
}

int32_t HwDisplayCrtc::setMode(drm_mode_info_t &mode) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtc::pageFlip(int32_t &out_fence) {
    // TODO: get real active config's width/height.
    mDisplayInfo.background_w = 1920;
    mDisplayInfo.background_h = 1080;

    ioctl(mDrvFd, FBIOPUT_OSD_SYNC_FLIP, &mDisplayInfo);

    out_fence = mDisplayInfo.out_fen_fd;
    return 0;
}

