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

/* FBIO */
#define FBIOPUT_OSD_SYNC_FLIP 0x451b

/*Used for zoom position*/
#define OFFSET_STEP          2

HwDisplayCrtc::HwDisplayCrtc(int drvFd, int32_t id) {
    mId = id;
    mDrvFd = drvFd;
    mFirstPresent = true;
    mDisplayInfo.out_fen_fd = -1;
    memset(mBackupZoomPos,0,sizeof(mBackupZoomPos));

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
    HwcConfig::getFramebufferSize(0, width, height);
    return 0;
}

int32_t HwDisplayCrtc::prePageFlip() {
    if (mFirstPresent) {
        mFirstPresent = false;
        closeLogoDisplay();
    }

    bool bUpdate = false;
    display_zoom_info_t zoomInfo;
    getZoomInfo(zoomInfo);

    for (int i = 0; i < 4; i++)
        if (mBackupZoomPos[i] != zoomInfo.position[i]) {
            bUpdate = true;
            break;
        }
    if (!bUpdate)
        return 0;
    memcpy(mBackupZoomPos,zoomInfo.position,sizeof(zoomInfo.position));
    zoomInfo.framebuffer_w = mFbWidth;
    zoomInfo.framebuffer_h = mFbHeight;

    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>>::iterator it = mPlanes.begin();
    for (; it!=mPlanes.end(); ++it) {
        it->second->updateZoomInfo(zoomInfo);
    }

    return 0;
}

int32_t HwDisplayCrtc::pageFlip(int32_t &out_fence) {
    // TODO: get real active config's width/height.
    mDisplayInfo.background_w = 1920;
    mDisplayInfo.background_h = 1080;

    updateDisplayInfo();

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

void HwDisplayCrtc::closeLogoDisplay() {
    sysfs_set_string(DISPLAY_LOGO_INDEX, "-1");
    sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
}

int32_t HwDisplayCrtc::updateDisplayInfo() {
    display_zoom_info_t zoomInfo;
    if (getZoomInfo(zoomInfo) == 0) {
        mDisplayInfo.fullScreen_w = zoomInfo.width;
        mDisplayInfo.fullScreen_h = zoomInfo.field_height;

        mDisplayInfo.curPosition_x = zoomInfo.position[0];
        mDisplayInfo.curPosition_y = zoomInfo.position[1];
        mDisplayInfo.curPosition_w = zoomInfo.position[2];
        mDisplayInfo.curPosition_h = zoomInfo.position[3];

        #if 0
        MESON_LOGV("updateOsdPosition display info:\n"
            "background          [w-%d h-%d]\n"
            "full screen         [w-%d h-%d]\n"
            "current position    [x-%d y-%d w-%d h-%d]",
            mDisplayInfo.background_w,  mDisplayInfo.background_h,
            mDisplayInfo.fullScreen_w,  mDisplayInfo.fullScreen_h,
            mDisplayInfo.curPosition_x, mDisplayInfo.curPosition_y,
            mDisplayInfo.curPosition_w, mDisplayInfo.curPosition_h);
        #endif
    }

    return 0;
}

int32_t HwDisplayCrtc::getZoomInfo(display_zoom_info_t & zoomInfo) {
    float temp;
    if (0 == sc_get_osd_position(mCurMode, zoomInfo.position)) {
        vmode_e vmode = vmode_name_to_mode(mCurMode.c_str());
        const struct vinfo_s* vinfo = get_tv_info(vmode);
        if (vmode == VMODE_MAX || vinfo == NULL) {
         MESON_LOGE("Invalid display mode %s", mCurMode.c_str());
         return -ENOENT;
        }
        zoomInfo.width          = vinfo->width;
        zoomInfo.height         = vinfo->field_height;
        zoomInfo.field_height   = vinfo->field_height;

        if (vmode == VMODE_480I        || vmode == VMODE_576I    ||
            vmode == VMODE_480CVBS     || vmode == VMODE_576CVBS ||
            vmode == VMODE_1080I_50HZ  || vmode == VMODE_1080I) {
            zoomInfo.position[1] /= 2;
            zoomInfo.position[3] /= 2;
        }

        /*
         * Cal by apk:
         * mCurrentLeft = (100-percent)*(mMaxRight)/(100*2*offsetStep);
         * offsetStep: because 20% is too large ,so divide the value to smooth the view
         * TODO: percent should not be calculated only by position_x
         */
        temp = (float)((float)(zoomInfo.position[0] * (100*2*OFFSET_STEP)) /
                        (float)(zoomInfo.width));
        zoomInfo.percent = 100 - (int)(temp + 0.5);
        //MESON_LOGD("getOsdPosition: percent = %d", percent);

    } else {
        MESON_LOGE("GetOsdPosition by sc failed.");
        return -ENOENT;
    }

    return 0;
}

