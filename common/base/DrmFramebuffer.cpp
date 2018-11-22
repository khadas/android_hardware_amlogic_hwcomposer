/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <DrmFramebuffer.h>
#include <MesonLog.h>

#if PLATFORM_SDK_VERSION < 28
#include <misc.h>
#endif

DrmFramebuffer::DrmFramebuffer()
    : mBufferHandle(NULL) {
    reset();
}

DrmFramebuffer::DrmFramebuffer(
    const native_handle_t * bufferhnd, int32_t acquireFence)
    : mBufferHandle(NULL) {
    reset();
    setBufferInfo(bufferhnd, acquireFence);

    if (bufferhnd) {
        mDisplayFrame.left   = mSourceCrop.left   = 0;
        mDisplayFrame.top    = mSourceCrop.top    = 0;
        mDisplayFrame.right  = mSourceCrop.right  = am_gralloc_get_width(bufferhnd);
        mDisplayFrame.bottom = mSourceCrop.bottom = am_gralloc_get_height(bufferhnd);
    }
}

DrmFramebuffer::~DrmFramebuffer() {
    reset();
}

std::shared_ptr<DrmFence> DrmFramebuffer::getAcquireFence() {
    return mAcquireFence;
}

int32_t DrmFramebuffer::setReleaseFence(int32_t fenceFd) {
    mReleaseFence.reset(new DrmFence(fenceFd));
    return 0;
}

int32_t DrmFramebuffer::getReleaseFence() {
    return mReleaseFence->dup();
}

void DrmFramebuffer::reset() {
    clearBufferInfo();
    mHdrMetaData.clear();
    mBlendMode       = DRM_BLEND_MODE_INVALID;
    mPlaneAlpha      = 1.0;
    mTransform       = 0;
    mZorder          = 0xFFFFFFFF; //set to special value for debug.
    mDataspace       = 0;
    mCompositionType = 0;

    mAcquireFence = mReleaseFence = DrmFence::NO_FENCE;

    mDisplayFrame.left   = mSourceCrop.left   = 0;
    mDisplayFrame.top    = mSourceCrop.top    = 0;
    mDisplayFrame.right  = mSourceCrop.right  = 0;
    mDisplayFrame.bottom = mSourceCrop.bottom = 0;
}

void DrmFramebuffer::setBufferInfo(
    const native_handle_t * bufferhnd,
    int32_t acquireFence) {
    if (bufferhnd) {
        mBufferHandle = native_handle_clone(bufferhnd);
        if (acquireFence >= 0)
            mAcquireFence = std::make_shared<DrmFence>(acquireFence);
    }
}

void DrmFramebuffer::clearBufferInfo() {
    if (mBufferHandle) {
        native_handle_close(mBufferHandle);
        native_handle_delete(mBufferHandle);
        mBufferHandle  = NULL;
    }

    mAcquireFence.reset();
    mAcquireFence  = DrmFence::NO_FENCE;
    mFbType        = DRM_FB_RENDER;
    mSecure         = false;
}

bool DrmFramebuffer::isRotated() {
    return mTransform != 0;
}

