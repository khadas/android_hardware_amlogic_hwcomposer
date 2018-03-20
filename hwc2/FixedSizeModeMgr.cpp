/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "FixedSizeModeMgr.h"
#include <MesonLog.h>
#include <misc.h>

#define DEFUALT_DPI (159)
#define DEFAULT_REFRESH_RATE (60.0f)
#define MAX_STR_LEN         512
//#define FREE_SCALE_ENABLE               "0x10001"
#define DISPLAY_FB0_FREESCALE           "/sys/class/graphics/fb0/free_scale"
#define DISPLAY_FB0_FREESCALE_AXIS      "/sys/class/graphics/fb0/free_scale_axis"

FixedSizeModeMgr::FixedSizeModeMgr() {
#if defined(WIDTH_PRIMARY_FRAMEBUFFER) && \
    defined(HEIGHT_PRIMARY_FRAMEBUFFER)
    mCurMode.pixelW = WIDTH_PRIMARY_FRAMEBUFFER;
    mCurMode.pixelH = HEIGHT_PRIMARY_FRAMEBUFFER;
#else
    MESON_LOGE("FixedSizeModeMgr need define the"
        "WIDTH_PRIMARY_FRAMEBUFFER and WIDTH_PRIMARY_FRAMEBUFFER.");
#endif
}

FixedSizeModeMgr::~FixedSizeModeMgr() {

}

HwcModeMgr::ModesPolicy FixedSizeModeMgr::getPolicyType() {
    return FIXED_SIZE_POLICY;
}

const char * FixedSizeModeMgr::getName() {
    return "FixedSizeMode";
}

void FixedSizeModeMgr::setDisplayResources(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector> & connector) {
    mConnector = connector;
    mCrtc = crtc;

    updateDisplayResources();
}

int32_t FixedSizeModeMgr::updateDisplayResources() {
    bool useFakeMode = false;

    if (mConnector->isConnected()) {
        int modeId = mCrtc->getModeId();
        if (modeId < 0) {
            MESON_LOGE("Get current display mode failed.\n");
            useFakeMode = true;
        } else {
            mModes.clear();
            mConnector->getModes(mModes);

            std::map<uint32_t, drm_mode_info_t>::iterator it = mModes.find(modeId);
            if (it != mModes.end()) {
                mCurMode.refreshRate = it->second.refreshRate;
                mCurMode.dpiX = it->second.dpiX;
                mCurMode.dpiY = it->second.dpiY;
                strncpy(mCurMode.name, it->second.name , DRM_DISPLAY_MODE_LEN);
                MESON_LOGI("ModeMgr update to (%s)", mCurMode.name);
                updateFreescaleAxis();
            } else {
                MESON_LOGE("ModeMgr cant find modeid (%d) in connector (%d)",
                    modeId, mConnector->getType());
                useFakeMode = true;
            }
        }
    } else {
        useFakeMode = true;
    }

    if (useFakeMode) {
        mCurMode.refreshRate = DEFAULT_REFRESH_RATE;
        mCurMode.dpiX = mCurMode.dpiY = DEFUALT_DPI;
        strncpy(mCurMode.name, "DefaultMode", DRM_DISPLAY_MODE_LEN);
    }

    return 0;
}

void FixedSizeModeMgr::updateFreescaleAxis()
{
    char axis[MAX_STR_LEN] = {0};
    sprintf(axis, "%d %d %d %d",
            0, 0, mCurMode.pixelW - 1, mCurMode.pixelH - 1);
    sysfs_set_string(DISPLAY_FB0_FREESCALE_AXIS, axis);
    MESON_LOGD("update free scale axis: %s", axis);
    sysfs_set_string(DISPLAY_FB0_FREESCALE, "0x10001");
}

hwc2_error_t  FixedSizeModeMgr::getDisplayConfigs(
    uint32_t * outNumConfigs, hwc2_config_t * outConfigs) {
    *outNumConfigs = 1;
    if (outConfigs) {
        *outConfigs = 0;
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t  FixedSizeModeMgr::getDisplayAttribute(
    hwc2_config_t config, int32_t attribute, int32_t * outValue) {
    switch (attribute) {
        case HWC2_ATTRIBUTE_WIDTH:
            *outValue = mCurMode.pixelW;
            break;
        case HWC2_ATTRIBUTE_HEIGHT:
            *outValue = mCurMode.pixelH;
            break;
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            *outValue = 1e9 / mCurMode.refreshRate;
            break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = mCurMode.dpiX;
            break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue = mCurMode.dpiY;
            break;
        default:
            MESON_LOGE("Unkown display attribute(%d)", attribute);
            break;
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t FixedSizeModeMgr::getActiveConfig(
    hwc2_config_t * outConfig) {
    *outConfig = 0;
    return HWC2_ERROR_NONE;
}

hwc2_error_t FixedSizeModeMgr::setActiveConfig(
    hwc2_config_t config) {
    if (config > 0) {
        MESON_LOGE("FixedSizeModeMgr dont support config (%d)", config);
    }
    return HWC2_ERROR_NONE;
}

void FixedSizeModeMgr::dump(String8 & dumpstr) {
    dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   \n");
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");
    dumpstr.appendFormat("     %2d     |      %.3f      |   %5d   |   %5d    |"
        "    %3d    |    %3d    \n",
         0,
         mCurMode.refreshRate,
         mCurMode.pixelW,
         mCurMode.pixelH,
         mCurMode.dpiX,
         mCurMode.dpiY);
}

