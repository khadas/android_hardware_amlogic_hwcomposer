/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_ENCODER_H
#define DRM_ENCODER_H

#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <DrmTypes.h>
#include <BasicTypes.h>

class DrmEncoder {
public:
    DrmEncoder(int drmFd, drmModeEncoderPtr p);
    ~DrmEncoder();

    uint32_t getId();

    uint32_t getCrtcId();
    int32_t setCrtcId(uint32_t crtc);
    uint32_t getPossibleCrtcs();

protected:
    int mDrmFd;
    uint32_t mId;
    uint32_t mCrtcId;
    uint32_t mCrtcMask;
};

#endif/*DRM_ENCODER_H*/
