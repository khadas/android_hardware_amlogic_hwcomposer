/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_FRAMEBUFFER_H
#define DRM_FRAMEBUFFER_H

#include <stdlib.h>
#include <cutils/native_handle.h>

#include <BasicTypes.h>
#include <Composition.h>
#include <DrmSync.h>
#include <DrmTypes.h>

#include <am_gralloc_ext.h>

/*buffer for display or render.*/
class DrmFramebuffer {
public:
    DrmFramebuffer();
    DrmFramebuffer(const native_handle_t * bufferhnd, int32_t acquireFence);
    virtual ~DrmFramebuffer();

    std::shared_ptr<DrmFence> getAcquireFence();

    int32_t setReleaseFence(int32_t fenceFd);
    /*dup current release fence.*/
    int32_t getReleaseFence();

protected:
    void setBufferInfo(const native_handle_t * bufferhnd, int32_t acquireFence);
    void clearBufferInfo();

    void reset();

public:
    native_handle_t * mBufferHandle;
    drm_color_t mColor;
    drm_fb_type_t mFbType;

    drm_rect_t mSourceCrop;
    drm_rect_t mDisplayFrame;
    drm_blend_mode_t mBlendMode;
    float mPlaneAlpha;
    int32_t mTransform;
    uint32_t mZorder;
    int32_t mDataspace;
    bool mSecure;

    meson_compositon_t mCompositionType;

    int32_t mComposeToType;

protected:
    std::shared_ptr<DrmFence> mAcquireFence;
    std::shared_ptr<DrmFence> mReleaseFence;
};

#endif/*DRM_FRAMEBUFFER_H*/
