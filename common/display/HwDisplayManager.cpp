/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "drm/DrmDevice.h"
#include "fbdev/HwDisplayManagerFbdev.h"
#include <MesonLog.h>

static std::shared_ptr<HwDisplayManager> gHwDisplayManager;

std::shared_ptr<HwDisplayManager> getHwDisplayManager() {
    if (gHwDisplayManager)
        return gHwDisplayManager;

    // check whether drm device exist
    if (access("/dev/dri/card0", R_OK | W_OK) == 0) {
        gHwDisplayManager = getDrmDevice();
        MESON_LOGD("load drm resources");
    } else {
        gHwDisplayManager = std::make_shared<HwDisplayManagerFbdev>();
        MESON_LOGD("load fbdev resources");
    }

    return gHwDisplayManager;
}

int32_t destroyHwDisplayManager() {
    gHwDisplayManager.reset();
    return 0;
}
