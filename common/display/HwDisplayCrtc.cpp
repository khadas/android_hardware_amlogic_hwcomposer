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
    /*load static information when pipeline present.*/
    {
        std::lock_guard<std::mutex> lock(mMutex);
        MESON_ASSERT(mConnector, "Crtc need setuped before load Properities.");
        mConnector->loadProperities();
        mModes.clear();
        mConnected = mConnector->isConnected();
        if (mConnected) {
            mConnector->getModes(mModes);
            /*TODO: add display mode filter here
            * to remove unsupported displaymode.
            */
        }
    }

    return 0;
}

int32_t HwDisplayCrtc::setMode(drm_mode_info_t & mode) {
    MESON_LOGI("Crtc active mode: %s", mode.name);
    std::string dispmode(mode.name);
    sc_set_display_mode(dispmode);
    return 0;
}

int32_t HwDisplayCrtc::getMode(drm_mode_info_t & mode) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mConnected) {
        mode = mCurModeInfo;
        return 0;
    } else {
        return -1;
    }
}

int32_t HwDisplayCrtc::update() {
    /*update dynamic information which may change.*/
    std::lock_guard<std::mutex> lock(mMutex);
    memset(&mCurModeInfo, 0, sizeof(drm_mode_info_t));
    if (mConnected) {
        /*1. update current displayMode.*/
        std::string displayMode;
        if (0 != sc_get_display_mode(displayMode)) {
            MESON_ASSERT(0," %s GetDisplayMode by sc failed.", __func__);
        }

        if (displayMode.empty())
             MESON_LOGE("displaymode should not null when connected.");
        else {
            MESON_LOGI("hw crtc update to mode: (%s)", displayMode.c_str());
            for (auto it = mModes.begin(); it != mModes.end(); it ++) {
                if (strcmp(it->second.name, displayMode.c_str()) == 0) {
                    memcpy(&mCurModeInfo, &it->second, sizeof(drm_mode_info_t));
                    HwDisplayManager::getInstance().updateRefreshPeriod(1e9 / mCurModeInfo.refreshRate);
                    break;
                }
            }
        }
    } else {
        /*use a null name*/
        strcpy(mCurModeInfo.name, "null");
    }

    return 0;
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
    std::lock_guard<std::mutex> lock(mMutex);

    int crtc_display[4];
    vmode_e vmode = vmode_name_to_mode(mCurModeInfo.name);
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
         MESON_LOGD("Invalid display mode %s", mCurModeInfo.name);
         return -ENOENT;
    }

    zoomInfo.framebuffer_w = mFbWidth;
    zoomInfo.framebuffer_h = mFbHeight;
    zoomInfo.crtc_w = vinfo->width;
    zoomInfo.crtc_h = vinfo->field_height;

    std::string dispModeStr(mCurModeInfo.name);
    if (0 == sc_get_osd_position(dispModeStr, crtc_display)) {
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

