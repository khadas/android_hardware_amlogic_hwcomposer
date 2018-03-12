/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include <math.h>

#include "Hwc2Layer.h"
#include "Hwc2Base.h"


Hwc2Layer::Hwc2Layer() : DrmFramebuffer(){
    mDataSpace = HAL_DATASPACE_UNKNOWN;
}

Hwc2Layer::~Hwc2Layer() {
}

hwc2_error_t Hwc2Layer::setBuffer(buffer_handle_t buffer, int32_t acquireFence) {
    /*
    * SurfaceFlinger will call setCompostionType() first,then setBuffer().
    * So it is safe to calc drm_fb_type_t mFbType here.
    */
    resetBufferInfo();

    mBufferHandle = buffer;
    mAcquireFence = std::make_shared<DrmFence>(acquireFence);

    /*set mFbType by usage of GraphicBuffer.*/
    if (mHwcCompositionType == HWC2_COMPOSITION_CURSOR) {
        mFbType = DRM_FB_CURSOR;
    } else if (isOmxVideo()) {
        mFbType = DRM_FB_VIDEO_OMX;
    } else if (isOverlayVideo()) {
        mFbType = DRM_FB_VIDEO_OVERLAY;
    } else if (isContiguousBuf()) {
        mFbType = DRM_FB_SCANOUT;
    } else {
        mFbType = DRM_FB_RENDER;
    }

    MESON_LOGD("layer setBuffer [%p, %d]", (void*)buffer, acquireFence);
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSidebandStream(const native_handle_t* stream) {
    resetBufferInfo();

    mBufferHandle = stream;
    mFbType = DRM_FB_VIDEO_SIDEBAND;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setColor(hwc_color_t color) {
    resetBufferInfo();

    mColor.r = color.r;
    mColor.g = color.g;
    mColor.b = color.b;
    mColor.a = color.a;

    mFbType = DRM_FB_COLOR;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSourceCrop(hwc_frect_t crop) {

    mSourceCrop.left = (int) ceilf(crop.left);
    mSourceCrop.top = (int) ceilf(crop.top);
    mSourceCrop.right = (int) floorf(crop.right);
    mSourceCrop.bottom = (int) floorf(crop.bottom);

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setDisplayFrame(hwc_rect_t frame) {

    mDisplayFrame.left = frame.left;
    mDisplayFrame.top = frame.top;
    mDisplayFrame.right = frame.right;
    mDisplayFrame.bottom = frame.bottom;

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setBlendMode(hwc2_blend_mode_t mode) {
    mBlendMode = (drm_blend_mode_t)mode;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setPlaneAlpha(float alpha) {
    mPlaneAlpha = alpha;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setTransform(hwc_transform_t transform) {
    mTransform = (int32_t)transform;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setVisibleRegion(hwc_region_t visible) {
    mVisibleRegion = visible;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSurfaceDamage(hwc_region_t damage) {
    mDamageRegion = damage;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setCompositionType(hwc2_composition_t type){
    mHwcCompositionType = type;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setDataspace(android_dataspace_t dataspace) {
    mDataSpace = dataspace;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setZorder(uint32_t z) {
    mZorder = z;
    return HWC2_ERROR_NONE;
}

int32_t Hwc2Layer::commitCompositionType() {
    hwc2_composition_t hwcCompostion = translateCompositionType(mCompositionType);
    if (mHwcCompositionType != hwcCompostion) {
        mHwcCompositionType = hwcCompostion;
    }
    return 0;
}

bool Hwc2Layer::isSecure() {
    MESON_LOG_EMPTY_FUN();
    return false;
}

void Hwc2Layer::setUniqueId(hwc2_layer_t id) {
    mId = id;
}

hwc2_layer_t Hwc2Layer::getUniqueId() {
    return mId;
}

void Hwc2Layer::dump(String8 & dumpstr) {
    MESON_LOG_EMPTY_FUN();
}


