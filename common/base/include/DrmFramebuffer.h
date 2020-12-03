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
#include <DrmSync.h>
#include <DrmTypes.h>

#include <am_gralloc_ext.h>

/*buffer for display or render.*/
class DrmFramebuffer {
public:
    DrmFramebuffer();
    DrmFramebuffer(const native_handle_t * bufferhnd, int32_t acquireFence);
    virtual ~DrmFramebuffer();

    int32_t setAcquireFence(int32_t fenceFd);
    std::shared_ptr<DrmFence> getAcquireFence();

    /*set release fence for last present loop.*/
    int32_t setPrevReleaseFence(int32_t fenceFd);
    int32_t setCurReleaseFence(int32_t fenceFd);
    /*dup current release fence.*/
    int32_t getPrevReleaseFence();

    int32_t lock(void ** addr);
    int32_t unlock();

    bool isRotated();

    // Virtuals for video tunnel
    virtual int32_t getVtBuffer() { return -EINVAL; }
    virtual int32_t acquireVtBuffer() { return 0; }
    virtual int32_t releaseVtBuffer(int releaseFence __unused) { return 0; }
    virtual bool isVtLayer() { return false;}

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

    int32_t mCompositionType;

    std::map<drm_hdr_meatadata_t, float> mHdrMetaData;
protected:
    std::shared_ptr<DrmFence> mAcquireFence;
    std::shared_ptr<DrmFence> mPrevReleaseFence;
    std::shared_ptr<DrmFence> mCurReleaseFence;

    void * mMapBase;
};

#endif/*DRM_FRAMEBUFFER_H*/
