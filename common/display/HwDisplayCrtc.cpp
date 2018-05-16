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
#include <cutils/properties.h>

#include "AmVinfo.h"
#include "AmFramebuffer.h"

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

int32_t HwDisplayCrtc::parseDftFbSize(uint32_t & width, uint32_t & height) {
    char uiMode[PROPERTY_VALUE_MAX] = {0};
    if (property_get("ro.ui_mode", uiMode, NULL) > 0) {
        if (!strncmp(uiMode, "720", 3)) {
            width  = 1280;
            height = 720;
        } else if (!strncmp(uiMode, "1080", 4)) {
            width  = 1920;
            height = 1080;
        } else if (!strncmp(uiMode, "4k2k", 4)) {
            width  = 3840;
            height = 2160;
        } else {
            MESON_LOGE("parseDftFbSize: get not support mode [%s]", uiMode);
        }
    } else {
        width  = WIDTH_PRIMARY_FRAMEBUFFER;
        height = HEIGHT_PRIMARY_FRAMEBUFFER;
    }
    MESON_LOGI("default frame buffer size (%d x %d)", width, height);

    return 0;
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

