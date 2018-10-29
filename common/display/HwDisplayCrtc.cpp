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
#include <HwcConfig.h>
#include <MesonLog.h>
#include <DebugHelper.h>
#include <cutils/properties.h>
#include <systemcontrol.h>
#include <misc.h>

#include "AmVinfo.h"
#include "AmFramebuffer.h"

HwDisplayCrtc::HwDisplayCrtc(int drvFd, int32_t id) {
    mId = id;
    mDrvFd = drvFd;

    mFirstPresent = true;
    memset(&mBackupZoomInfo,0,sizeof(mBackupZoomInfo));
    parseDftFbSize(mFbWidth, mFbHeight);
}

HwDisplayCrtc::~HwDisplayCrtc() {
}

int32_t HwDisplayCrtc::setUp(
    std::shared_ptr<HwDisplayConnector>  connector,
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> planes) {
    mConnector = connector;
    mPlanes = planes;
    return 0;
}

int32_t HwDisplayCrtc::loadProperities() {
    return updateMode();
}

int32_t HwDisplayCrtc::setMode(drm_mode_info_t & mode __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtc::updateMode() {
    std::string displayMode;
    if (0 != sc_get_display_mode(displayMode)) {
        MESON_ASSERT(0," %s GetDisplayMode by sc failed.", __func__);
        return -ENOENT ;
    }

    MESON_LOGI("hw crtc update to mode: %s", displayMode.c_str());

    mCurMode = displayMode;
    //update software vsync.
    vmode_e vmode = vmode_name_to_mode(mCurMode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        MESON_LOGD("updateMode to invalid display mode %s", displayMode.c_str());
        return -ENOENT ;
    }

    float refreshRate = vinfo->sync_duration_num / vinfo->sync_duration_den;
    HwDisplayManager::getInstance().updateRefreshPeriod(1e9 / refreshRate);
    return 0;
}

int32_t HwDisplayCrtc::updateActiveMode(std::string & displayMode, bool policy) {
    MESON_LOGI("Crtc active mode: %s", displayMode.c_str());

    mConnector->switchRatePolicy(policy);
    sc_set_display_mode(displayMode);
    return 0;
}

int32_t HwDisplayCrtc::getModeId() {
    std::map<uint32_t, drm_mode_info_t> displayModes;
    mConnector->getModes(displayModes);
    std::map<uint32_t, drm_mode_info_t>::iterator it = displayModes.begin();
    for (; it != displayModes.end(); ++it)
        if (strncmp(it->second.name, mCurMode.c_str(), DRM_DISPLAY_MODE_LEN) == 0)
            return it->first;

    MESON_ASSERT(0, "[%s]: Get not support display mode %s",
        __func__, mCurMode.c_str());
    return -ENOENT;
}

int32_t HwDisplayCrtc::parseDftFbSize(uint32_t & width, uint32_t & height) {
    HwcConfig::getFramebufferSize(0, width, height);
    return 0;
}

int32_t HwDisplayCrtc::prePageFlip() {
    if (mFirstPresent) {
        mFirstPresent = false;
        closeLogoDisplay();
    }

    display_zoom_info_t zoomInfo;
    getZoomInfo(zoomInfo);
    if (memcmp(&mBackupZoomInfo, &zoomInfo, sizeof(mBackupZoomInfo))) {
        mBackupZoomInfo = zoomInfo;
        for (auto it = mPlanes.begin(); it!=mPlanes.end(); ++it) {
            it->second->updateZoomInfo(zoomInfo);
        }
    }
    return 0;
}

int32_t HwDisplayCrtc::pageFlip(int32_t &out_fence) {
    osd_page_flip_info_t flipInfo;

    flipInfo.background_w = mBackupZoomInfo.framebuffer_w;
    flipInfo.background_h = mBackupZoomInfo.framebuffer_h;
    flipInfo.fullScreen_w = mBackupZoomInfo.crtc_w;
    flipInfo.fullScreen_h = mBackupZoomInfo.crtc_h;

    flipInfo.curPosition_x = mBackupZoomInfo.crtc_display_x;
    flipInfo.curPosition_y = mBackupZoomInfo.crtc_display_y;
    flipInfo.curPosition_w = mBackupZoomInfo.crtc_display_w;
    flipInfo.curPosition_h = mBackupZoomInfo.crtc_display_h;

    ioctl(mDrvFd, FBIOPUT_OSD_DO_HWC, &flipInfo);

    if (DebugHelper::getInstance().discardOutFence()) {
        std::shared_ptr<DrmFence> outfence =
            std::make_shared<DrmFence>(flipInfo.out_fen_fd);
        outfence->waitForever("crtc-output");
        out_fence = -1;
    } else {
        out_fence = (flipInfo.out_fen_fd >= 0) ? flipInfo.out_fen_fd : -1;
    }

    return 0;
}

void HwDisplayCrtc::closeLogoDisplay() {
    sysfs_set_string(DISPLAY_LOGO_INDEX, "-1");
    sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
}

int32_t HwDisplayCrtc::getZoomInfo(display_zoom_info_t & zoomInfo) {
    int crtc_display[4];
    vmode_e vmode = vmode_name_to_mode(mCurMode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
         MESON_LOGE("Invalid display mode %s", mCurMode.c_str());
         return -ENOENT;
    }

    zoomInfo.framebuffer_w = mFbWidth;
    zoomInfo.framebuffer_h = mFbHeight;
    zoomInfo.crtc_w = vinfo->width;
    zoomInfo.crtc_h = vinfo->field_height;

    if (0 == sc_get_osd_position(mCurMode, crtc_display)) {
        zoomInfo.crtc_display_x = crtc_display[0];
        zoomInfo.crtc_display_y = crtc_display[1];
        zoomInfo.crtc_display_w = crtc_display[2];
        zoomInfo.crtc_display_h = crtc_display[3];

        /*for interlaced.*/
        if (vinfo->field_height != vinfo->height) {
            zoomInfo.crtc_display_y = vinfo->field_height * zoomInfo.crtc_display_y / vinfo->height;
            zoomInfo.crtc_display_h = vinfo->field_height * zoomInfo.crtc_display_h / vinfo->height;
        }
        return 0;
    } else {
        MESON_LOGE("GetOsdPosition by sc failed, set to no scaled value.");
        zoomInfo.crtc_display_x = 0;
        zoomInfo.crtc_display_y = 0;
        zoomInfo.crtc_display_w = zoomInfo.crtc_w;
        zoomInfo.crtc_display_h = zoomInfo.crtc_h;
        return -ENOENT;
    }
}

