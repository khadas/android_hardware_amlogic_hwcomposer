/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_FRAMEBUFFER_BASE_H
#define DRM_FRAMEBUFFER_BASE_H

#include <stdlib.h>
#include <DrmTypes.h>

/* buffer for display or render.*/

class DrmFramebufferBase {
public:
    DrmFramebufferBase();
    virtual ~DrmFramebufferBase();
    virtual int32_t setAcquireFence(int32_t fenceFd);
    virtual void setBufferInfo(int fd, int32_t acquireFence);
    virtual void reset();

public:
    int fd;
    drm_fb_type_t mFbType;
    drm_rect_t mDisplayFrame;
    drm_blend_mode_t mBlendMode;
    float mPlaneAlpha;
    int32_t mTransform;
    uint32_t mZorder;
    int32_t mDataspace;
    bool mSecure;
    int32_t mCompositionType;
};

#endif/*DRM_FRAMEBUFFER_BASE_H*/
