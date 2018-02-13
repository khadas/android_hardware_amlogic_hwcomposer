/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef FIXED_SIZE_MODE_MGR_H
#define FIXED_SIZE_MODE_MGR_H

#include "HwcModeMgr.h"

/*
 * FixedSizeModeMgr:
 * This class designed for TV or reference box.
 * The display size is fixed (the framebuffer size).
 * But the refresh rate, dpi will update when display changed.
 * For example:user choosed a new display mode in setting.
 */
class FixedSizeModeMgr : public HwcModeMgr {
public:
    FixedSizeModeMgr();
    ~FixedSizeModeMgr();

    const char * getName();

    hwc2_error_t  getDisplayConfigs(
        uint32_t* outNumConfigs, hwc2_config_t* outConfigs);
    hwc2_error_t  getDisplayAttribute(
        hwc2_config_t config, int32_t attribute, int32_t* outValue);
    hwc2_error_t getActiveConfig(hwc2_config_t* outConfig);
    hwc2_error_t setActiveConfig(hwc2_config_t config);

protected:
    uint32_t mDisplayWidth;
    uint32_t mDisplayHeight;
    uint32_t mRefreshRate;
    uint32_t mDpiX;
    uint32_t mDpiY;
};

#endif/*FIXED_SIZE_MODE_MGR_H*/
