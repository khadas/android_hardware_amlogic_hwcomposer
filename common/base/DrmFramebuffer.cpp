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

DrmFramebuffer::DrmFramebuffer() {
    reset();
}

DrmFramebuffer::DrmFramebuffer(
    const native_handle_t * bufferhnd, int32_t acquireFence) {
    reset();
    mBufferHandle = bufferhnd;
    if (acquireFence >= 0)
        mAcquireFence = std::make_shared<DrmFence>(acquireFence);
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
    resetBufferInfo();
    mBlendMode = DRM_BLEND_MODE_INVALID;
    mPlaneAlpha = 1.0;
    mTransform = 0;
    mZorder = 0;
    mDataspace = 0;

    mAcquireFence = mReleaseFence = DrmFence::NO_FENCE;

    mDisplayFrame.left   = mSourceCrop.left   = 0;
    mDisplayFrame.top    = mSourceCrop.top    = 0;
    mDisplayFrame.right  = mSourceCrop.right  = 1920;
    mDisplayFrame.bottom = mSourceCrop.bottom = 1080;
}

void DrmFramebuffer::resetBufferInfo() {
    mAcquireFence.reset();
    mBufferHandle = NULL;
    mFbType = DRM_FB_RENDER;
}

