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

FixedSizeModeMgr::FixedSizeModeMgr() {
#if defined(WIDTH_PRIMARY_FRAMEBUFFER) && \
    defined(HEIGHT_PRIMARY_FRAMEBUFFER)
    mDisplayWidth = WIDTH_PRIMARY_FRAMEBUFFER;
    mDisplayHeight = HEIGHT_PRIMARY_FRAMEBUFFER ;
#else
    MESON_LOGE("FixedSizeModeMgr need define the"
        "WIDTH_PRIMARY_FRAMEBUFFER and WIDTH_PRIMARY_FRAMEBUFFER.");
#endif

    mRefreshRate = 60;
    mDpiX = 160;
    mDpiY = 160;
}

FixedSizeModeMgr::~FixedSizeModeMgr() {

}

const char * FixedSizeModeMgr::getName() {
    return "FixedSizeMode";
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
            *outValue = mDisplayWidth;
            break;
        case HWC2_ATTRIBUTE_HEIGHT:
            *outValue = mDisplayHeight;
            break;
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            *outValue = 1e9 / mRefreshRate;
            break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = 160;
            break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue = 160;
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

