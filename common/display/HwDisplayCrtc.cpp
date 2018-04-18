/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <HwDisplayManager.h>
#include <HwDisplayCrtc.h>
#include <MesonLog.h>
#include <DebugHelper.h>

#include "AmVinfo.h"

/* FBIO */
#define FBIOPUT_OSD_SYNC_FLIP 0x451b

HwDisplayCrtc::HwDisplayCrtc(int drvFd, int32_t id) {
    mId = id;
    mDrvFd = drvFd;
    mDisplayInfo.out_fen_fd = -1;
}

HwDisplayCrtc::~HwDisplayCrtc() {
}

#ifdef HWC_MANAGE_DISPLAY_MODE
int32_t HwDisplayCrtc::setMode(drm_mode_info_t &mode) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}
#else
int32_t HwDisplayCrtc::updateMode(std::string & displayMode) {
    MESON_LOGI("hw crtc update mode: %s", displayMode.c_str());

    mCurMode = displayMode;
    //update software vsync.
    vmode_e vmode = vmode_name_to_mode(mCurMode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        MESON_LOGE("Invalid display mode %s", displayMode.c_str());
        return -ENOENT ;
    }

    float refreshRate = vinfo->sync_duration_num / vinfo->sync_duration_den;
    HwDisplayManager::getInstance().updateRefreshPeriod(1e9 / refreshRate);
    return 0;
}
#endif

int32_t HwDisplayCrtc::getModeId() {
    vmode_e vmode = vmode_name_to_mode(mCurMode.c_str());
    return (int32_t)vmode;
}

int32_t HwDisplayCrtc::pageFlip(int32_t &out_fence) {
    // TODO: get real active config's width/height.
    mDisplayInfo.background_w = 1920;
    mDisplayInfo.background_h = 1080;

    ioctl(mDrvFd, FBIOPUT_OSD_SYNC_FLIP, &mDisplayInfo);

    if (DebugHelper::getInstance().discardOutFence()) {
        std::shared_ptr<DrmFence> outfence =
            std::make_shared<DrmFence>(mDisplayInfo.out_fen_fd);
        outfence->waitForever("crtc-output");
        out_fence = -1;
    } else {
        out_fence = (mDisplayInfo.out_fen_fd >= 0) ? mDisplayInfo.out_fen_fd : -1;
    }

    return 0;
}

