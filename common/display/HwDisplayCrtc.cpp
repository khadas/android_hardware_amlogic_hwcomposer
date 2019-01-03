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

static vframe_master_display_colour_s_t nullHdr;

HwDisplayCrtc::HwDisplayCrtc(int drvFd, int32_t id) {
    mId = id;
    mDrvFd = drvFd;
    mFirstPresent = true;
    /*for old vpu, always one channel.
    *for new vpu, it can be 1 or 2.
    */
    mOsdChannels = 1;
    memset(&hdrVideoInfo, 0, sizeof(hdrVideoInfo));
    memset(&nullHdr, 0, sizeof(nullHdr));
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
    if (!mConnected || mCurModeInfo.name[0] == 0)
        return -1;

    mode = mCurModeInfo;
    return 0;
}

int32_t HwDisplayCrtc::update() {
    /*update dynamic information which may change.*/
    std::lock_guard<std::mutex> lock(mMutex);
    memset(&mCurModeInfo, 0, sizeof(drm_mode_info_t));
    if (mConnected) {
        /*1. update current displayMode.*/
        mConnector->update();
        std::string displayMode;
        if (0 != sc_get_display_mode(displayMode)) {
            MESON_ASSERT(0," %s GetDisplayMode by sc failed.", __func__);
        }

        if (displayMode.empty()) {
             MESON_LOGE("displaymode should not null when connected.");
        } else {
            MESON_LOGI("hw crtc update to mode: (%s)", displayMode.c_str());
            for (auto it = mModes.begin(); it != mModes.end(); it ++) {
                if (strcmp(it->second.name, displayMode.c_str()) == 0) {
                    memcpy(&mCurModeInfo, &it->second, sizeof(drm_mode_info_t));
                    HwDisplayManager::getInstance().updateRefreshPeriod(1e9 / mCurModeInfo.refreshRate);
                    break;
                }
            }
        }
    }

    return 0;
}

int32_t HwDisplayCrtc::setDisplayFrame(display_zoom_info_t & info) {
    mScaleInfo = info;
    /*not used now, clear to 0.*/
    mScaleInfo.crtc_w = 0;
    mScaleInfo.crtc_h = 0;
    return 0;
}

int32_t HwDisplayCrtc::setOsdChannels(int32_t channels) {
    mOsdChannels = channels;
    return 0;
}

int32_t HwDisplayCrtc::pageFlip(int32_t &out_fence) {
    if (mFirstPresent) {
        mFirstPresent = false;
        closeLogoDisplay();
    }

    osd_page_flip_info_t flipInfo;
    flipInfo.background_w = mScaleInfo.framebuffer_w;
    flipInfo.background_h = mScaleInfo.framebuffer_h;
    flipInfo.fullScreen_w = mScaleInfo.framebuffer_w;
    flipInfo.fullScreen_h = mScaleInfo.framebuffer_h;
    flipInfo.curPosition_x = mScaleInfo.crtc_display_x;
    flipInfo.curPosition_y = mScaleInfo.crtc_display_y;
    flipInfo.curPosition_w = mScaleInfo.crtc_display_w;
    flipInfo.curPosition_h = mScaleInfo.crtc_display_h;
    flipInfo.hdr_mode = (mOsdChannels == 1) ? 1 : 0;

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

int32_t HwDisplayCrtc::getHdrMetadataKeys(
    std::vector<drm_hdr_meatadata_t> & keys) {
    static drm_hdr_meatadata_t supportedKeys[] = {
        DRM_DISPLAY_RED_PRIMARY_X,
        DRM_DISPLAY_RED_PRIMARY_Y,
        DRM_DISPLAY_GREEN_PRIMARY_X,
        DRM_DISPLAY_GREEN_PRIMARY_Y,
        DRM_DISPLAY_BLUE_PRIMARY_X,
        DRM_DISPLAY_BLUE_PRIMARY_Y,
        DRM_WHITE_POINT_X,
        DRM_WHITE_POINT_Y,
        DRM_MAX_LUMINANCE,
        DRM_MIN_LUMINANCE,
        DRM_MAX_CONTENT_LIGHT_LEVEL,
        DRM_MAX_FRAME_AVERAGE_LIGHT_LEVEL,
    };

    for (uint32_t i = 0;i < sizeof(supportedKeys)/sizeof(drm_hdr_meatadata_t); i++) {
        keys.push_back(supportedKeys[i]);
    }

    return 0;
}

int32_t HwDisplayCrtc::setHdrMetadata(
    std::map<drm_hdr_meatadata_t, float> & hdrmedata) {
    if (updateHdrMetadata(hdrmedata) == true)
        return set_hdr_info(hdrVideoInfo);

    return 0;
}

bool HwDisplayCrtc::updateHdrMetadata(
    std::map<drm_hdr_meatadata_t, float> & hdrmedata) {
    vframe_master_display_colour_s_t newHdr;
    memset(&newHdr,0,sizeof(vframe_master_display_colour_s_t));
    if (!hdrmedata.empty()) {
        for (auto iter = hdrmedata.begin(); iter != hdrmedata.end(); ++iter) {
            switch (iter->first) {
                case DRM_DISPLAY_RED_PRIMARY_X:
                    newHdr.primaries[2][0] = (u32)(iter->second * 50000); //mR.x
                    break;
                case DRM_DISPLAY_RED_PRIMARY_Y:
                    newHdr.primaries[2][1] = (u32)(iter->second * 50000); //mR.Y
                    break;
                case DRM_DISPLAY_GREEN_PRIMARY_X:
                    newHdr.primaries[0][0] = (u32)(iter->second * 50000);//mG.x
                    break;
                case DRM_DISPLAY_GREEN_PRIMARY_Y:
                    newHdr.primaries[0][1] = (u32)(iter->second * 50000);//mG.y
                    break;
                case DRM_DISPLAY_BLUE_PRIMARY_X:
                    newHdr.primaries[1][0] = (u32)(iter->second * 50000);//mB.x
                    break;
                case DRM_DISPLAY_BLUE_PRIMARY_Y:
                    newHdr.primaries[1][1] = (u32)(iter->second * 50000);//mB.Y
                    break;
                case DRM_WHITE_POINT_X:
                    newHdr.white_point[0] = (u32)(iter->second * 50000);//mW.x
                    break;
                case DRM_WHITE_POINT_Y:
                    newHdr.white_point[1] = (u32)(iter->second * 50000);//mW.Y
                    break;
                case DRM_MAX_LUMINANCE:
                    newHdr.luminance[0] = (u32)(iter->second * 1000); //mMaxDL
                    break;
                case DRM_MIN_LUMINANCE:
                    newHdr.luminance[1] = (u32)(iter->second * 10000);//mMinDL
                    break;
                case DRM_MAX_CONTENT_LIGHT_LEVEL:
                    newHdr.content_light_level.max_content = (u32)(iter->second); //mMaxCLL
                    newHdr.content_light_level.present_flag = 1;
                    break;
                case DRM_MAX_FRAME_AVERAGE_LIGHT_LEVEL:
                    newHdr.content_light_level.max_pic_average = (u32)(iter->second);//mMaxFALL
                    newHdr.content_light_level.present_flag = 1;
                    break;
                default:
                    MESON_LOGE("unkown key %d",iter->first);
                    break;
            }
        }
    }

    if (memcmp(&hdrVideoInfo, &nullHdr, sizeof(vframe_master_display_colour_s_t)) == 0)
        return false;
    newHdr.present_flag = 1;

    if (memcmp(&hdrVideoInfo, &newHdr, sizeof(vframe_master_display_colour_s_t)) == 0)
        return false;

    hdrVideoInfo = newHdr;
    return true;
}

void HwDisplayCrtc::closeLogoDisplay() {
    sysfs_set_string(DISPLAY_LOGO_INDEX, "-1");
    sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
}


