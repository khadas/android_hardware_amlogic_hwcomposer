/* Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <HwDisplayConnector.h>
#include <MesonLog.h>
#include "AmVinfo.h"

HwDisplayConnector::HwDisplayConnector(int32_t drvFd, uint32_t id) {
    mDrvFd = drvFd;
    mId = id;
}

HwDisplayConnector::~HwDisplayConnector() {
}

int32_t HwDisplayConnector::getModes(
    std::map<uint32_t, drm_mode_info_t> & modes) {
    modes = mDisplayModes;
    return 0;
}

void HwDisplayConnector::loadPhysicalSize() {
    struct vinfo_base_s info;
    if (read_vout_info(&info) == 0) {
        mPhyWidth = info.screen_real_width;
        mPhyHeight = info.screen_real_height;
    } else {
        mPhyWidth = mPhyHeight = 0;
    }
    MESON_LOGD("readDisplayPhySize physical size (%d x %d)", mPhyWidth, mPhyHeight);
}

