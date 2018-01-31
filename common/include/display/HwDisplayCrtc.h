/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CRTC_H
#define HW_DISPLAY_CRTC_H

#include <stdlib.h>

#include <DrmTypes.h>

class HwDisplayCrtc {
public:
    HwDisplayCrtc(int drvFd, int32_t id);
    ~HwDisplayCrtc();

    int32_t setMode(drm_mode_info_t &mode);
    int32_t pageFlip(int32_t &out_fence);


protected:
    int32_t mId;
    int mDrvFd;
};

#endif/*HW_DISPLAY_CRTC_H*/