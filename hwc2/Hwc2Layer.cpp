/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1

#include <MesonLog.h>
#include <math.h>
#include <sys/mman.h>
#include <cutils/properties.h>
#include <utils/Timers.h>
#include <inttypes.h>

#include "Hwc2Layer.h"
#include "Hwc2Base.h"
#include "VideoTunnelDev.h"

Hwc2Layer::Hwc2Layer() : DrmFramebuffer(){
    mDataSpace     = HAL_DATASPACE_UNKNOWN;
    mUpdateZorder  = false;
    mVtDeviceConnection = false;
    mVtBufferFd    = -1;
    mPreVtBufferFd = -1;
    mVtUpdate      =  false;
    mNeedReleaseVtResource = false;
    mTimeStamp     = -1;
}

Hwc2Layer::~Hwc2Layer() {
    // release last video tunnel buffer
    releaseVtResource();
}

hwc2_error_t Hwc2Layer::handleDimLayer(buffer_handle_t buffer) {
    int bufFd = am_gralloc_get_buffer_fd(buffer);
    if (bufFd < 0) {
        MESON_LOGE("[%s]: get invalid buffer fd %d", __func__, bufFd);
        return HWC2_ERROR_NONE;
    }

    int bufFormat = am_gralloc_get_format(buffer);

    /* Number of pixel components in memory
     * (i.e. R.G.B.A | R.G.B)
     */
    int components = 4;

    switch (bufFormat)
    {
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
            components = 4;
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
        case HAL_PIXEL_FORMAT_RGB_565:
            components = 3;
            break;
        default:
            MESON_LOGE("Need to expand the format(%d), check it out!", bufFormat);
            break;
    }
    char *base = (char *)mmap(NULL, components, PROT_READ, MAP_SHARED, bufFd, 0);
    if (base != MAP_FAILED) {
        int color[components];
        memcpy(color, base, components);

        switch (bufFormat)
        {
            case HAL_PIXEL_FORMAT_BGRA_8888:
                mColor.b = color[0];
                mColor.g = color[1];
                mColor.r = color[2];
                mColor.a = color[3];
                break;
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
                mColor.r = color[0];
                mColor.g = color[1];
                mColor.b = color[2];
                mColor.a = color[3];
                break;
            case HAL_PIXEL_FORMAT_RGB_565:
                mColor.r = color[0];
                mColor.g = color[1];
                mColor.b = color[2];
                mColor.a = 255;
                break;
            case HAL_PIXEL_FORMAT_RGB_888:
                mColor.b = color[0];
                mColor.g = color[1];
                mColor.r = color[2];
                mColor.a = 255;
                break;
            default:
                MESON_LOGE("Need to expand the format(%d), check it out!", bufFormat);
                break;
        }

        mFbType = DRM_FB_COLOR;
        munmap(base, components);
    } else {
        MESON_LOGE("[%s]: dim layer buffer mmap fail!", __func__);
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setBuffer(buffer_handle_t buffer, int32_t acquireFence) {
    /*record for check fbType change status */
    drm_fb_type_t preType = mFbType;

    /*
     * If video tunnel sideband recieve blank frame,
     * need release video tunnel resouce
     */
    if (isVtBuffer())
        mNeedReleaseVtResource = true;

    /*
    * SurfaceFlinger will call setCompostionType() first,then setBuffer().
    * So it is safe to calc drm_fb_type_t mFbType here.
    */
    clearBufferInfo();
    setBufferInfo(buffer, acquireFence);

    if (buffer == NULL) {
        MESON_LOGE("Receive null buffer, it is impossible.");
        mFbType = DRM_FB_UNDEFINED;
        return HWC2_ERROR_NONE;
    }

    /*set mFbType by usage of GraphicBuffer.*/
    if (mHwcCompositionType == HWC2_COMPOSITION_CURSOR) {
        mFbType = DRM_FB_CURSOR;
    } else if (am_gralloc_is_omx_v4l_buffer(buffer)) {
        mFbType = DRM_FB_VIDEO_OMX_V4L;
    } else if (am_gralloc_is_uvm_dma_buffer(buffer)) {
        mFbType = DRM_FB_VIDEO_UVM_DMA;
    } else if (am_gralloc_is_omx_metadata_buffer(buffer)) {
        int tunnel = 0;
        int ret = am_gralloc_get_omx_metadata_tunnel(buffer, &tunnel);
        if (ret != 0)
            return HWC2_ERROR_BAD_LAYER;
        if (tunnel == 0)
            mFbType = DRM_FB_VIDEO_OMX_PTS;
        else
            mFbType = DRM_FB_VIDEO_OMX_PTS_SECOND;
    } else if (am_gralloc_is_overlay_buffer(buffer)) {
        mFbType = DRM_FB_VIDEO_OVERLAY;
    } else if (am_gralloc_get_width(buffer) <= 1 && am_gralloc_get_height(buffer) <= 1) {
        //For the buffer which size is 1x1, we treat it as a dim layer.
        handleDimLayer(buffer);
    } else if (am_gralloc_is_coherent_buffer(buffer)) {
        if (am_gralloc_get_format(buffer) == HAL_PIXEL_FORMAT_YCrCb_420_SP)
            mFbType = DRM_FB_VIDEO_DMABUF;
        else
            mFbType = DRM_FB_SCANOUT;
    } else {
        mFbType = DRM_FB_RENDER;
    }

    if (preType != mFbType)
        mUpdated = true;

    mSecure = am_gralloc_is_secure_buffer(mBufferHandle);
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSidebandStream(const native_handle_t* stream) {
    clearBufferInfo();
    setBufferInfo(stream, -1);

    int type = AM_INVALID_SIDEBAND;
    int channel_id = 0;
    am_gralloc_get_sideband_type(stream, &type);
    am_gralloc_get_sideband_channel(stream, &channel_id);
    if (type == AM_TV_SIDEBAND) {
        mFbType = DRM_FB_VIDEO_SIDEBAND_TV;
    } else if (type == AM_FIXED_TUNNEL) {
        mFbType = DRM_FB_VIDEO_TUNNEL_SIDEBAND;
        mTunnelId = channel_id;
        if (!mVtDeviceConnection) {
            MESON_LOGD("%s connect to videotunnel %d", __func__, mTunnelId);
            VideoTunnelDev::getInstance().connect(mTunnelId);
            mVtDeviceConnection = true;
        }
    } else {
        if (channel_id == AM_VIDEO_EXTERNAL) {
            mFbType = DRM_FB_VIDEO_SIDEBAND_SECOND;
        } else {
            mFbType = DRM_FB_VIDEO_SIDEBAND;
        }
    }

    mSecure = false;
    mUpdated = true;
    return HWC2_ERROR_NONE;

}

hwc2_error_t Hwc2Layer::setColor(hwc_color_t color) {
    clearBufferInfo();

    mColor.r = color.r;
    mColor.g = color.g;
    mColor.b = color.b;
    mColor.a = color.a;

    mFbType = DRM_FB_COLOR;

    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSourceCrop(hwc_frect_t crop) {
    mSourceCrop.left = (int) ceilf(crop.left);
    mSourceCrop.top = (int) ceilf(crop.top);
    mSourceCrop.right = (int) floorf(crop.right);
    mSourceCrop.bottom = (int) floorf(crop.bottom);
    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setDisplayFrame(hwc_rect_t frame) {
    /*Used for display frame scale*/
    mBackupDisplayFrame.left = frame.left;
    mBackupDisplayFrame.top = frame.top;
    mBackupDisplayFrame.right = frame.right;
    mBackupDisplayFrame.bottom = frame.bottom;

    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setBlendMode(hwc2_blend_mode_t mode) {
    mBlendMode = (drm_blend_mode_t)mode;
    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setPlaneAlpha(float alpha) {
    mPlaneAlpha = alpha;
    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setTransform(hwc_transform_t transform) {
    mTransform = (int32_t)transform;
    mUpdated = true;
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
    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setDataspace(android_dataspace_t dataspace) {
    mDataSpace = dataspace;
    mUpdated = true;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setZorder(uint32_t z) {
    mZorder = z;
    mUpdateZorder = mUpdated = true;
    return HWC2_ERROR_NONE;
}

#ifdef HWC_HDR_METADATA_SUPPORT
int32_t Hwc2Layer::setPerFrameMetadata(
    uint32_t numElements, const int32_t* /*hw2_per_frame_metadata_key_t*/ keys,
    const float* metadata) {
    mHdrMetaData.clear();
    for (uint32_t i = 0; i < numElements; i++) {
        mHdrMetaData.insert({static_cast<drm_hdr_meatadata_t>(keys[i]),metadata[i]});
    }
    return HWC2_ERROR_NONE;
}
#endif


int32_t Hwc2Layer::commitCompType(
    hwc2_composition_t hwcComp) {
    if (mHwcCompositionType != hwcComp) {
        mHwcCompositionType = hwcComp;
    }
    return 0;
}

void Hwc2Layer::setUniqueId(hwc2_layer_t id) {
    mId = id;
}

hwc2_layer_t Hwc2Layer::getUniqueId() {
    return mId;
}

void Hwc2Layer::clearUpdateFlag() {
    mUpdateZorder = mUpdated = false;
    if (mNeedReleaseVtResource)
        releaseVtResource();
}

bool Hwc2Layer::isVtBuffer() {
    return mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND;
}

int32_t Hwc2Layer::getVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBuffer())
        return -EINVAL;

    int ret = -1;
    /* should present the current buffer now? */
    if (shouldPresentNow()) {
        ret = mVtUpdate ? mVtBufferFd : mPreVtBufferFd;
    } else {
        ret = mPreVtBufferFd;
    }

    return ret;
}

int32_t Hwc2Layer::acquireVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBuffer()) {
        MESON_LOGE("[%s] not videotunnel type", __func__);
        return -EINVAL;
    }

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] mTunelId  %d error", __func__, mTunnelId);
        return -EINVAL;
    }

    if (mVtUpdate)
        return -EAGAIN;

    int ret = VideoTunnelDev::getInstance().acquireBuffer(mTunnelId,
            mVtBufferFd, mTimeStamp);
    if (ret < 0)
        return ret;

    // acquire video tunnel buffer success
    mVtUpdate = true;
    mFbHandleUpdated = true;

    static nsecs_t previousTimeStamp = 0;
    if (previousTimeStamp == 0)
        previousTimeStamp = mTimeStamp;

    nsecs_t diffExpected = mTimeStamp - previousTimeStamp;
    previousTimeStamp = mTimeStamp;

    MESON_LOGV("[%s] mVtBufferFd(%d) timestamp (%" PRId64 ") timeDiff(%" PRId64 ")",
            __func__, mVtBufferFd, mTimeStamp, diffExpected);

    diffExpected = 0;

    return 0;
}

int32_t Hwc2Layer::releaseVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBuffer()) {
        MESON_LOGD("layer:%" PRId64 " is not videotunnel layer", getUniqueId());
        return -EINVAL;
    }

    if (!mVtUpdate)
        return -EAGAIN;

    if (shouldPresentNow() == false) {
        MESON_LOGV("[%s] current vt buffer not present, timestamp not meet", __func__);
        return -EAGAIN;
    }

    // First release vt buffer, save to the previous
    if (mPreVtBufferFd < 0) {
        mPreVtBufferFd = mVtBufferFd;
        mFbHandleUpdated = false;
        mVtUpdate = false;
        mTimeStamp = -1;
        return 0;
    }

    // set fence to the previous vt buffer
    mReleaseFence = getPrevReleaseFence();
    int ret = VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
            mPreVtBufferFd, mReleaseFence);

    MESON_LOGV("[%s] releaseFence:%d, bufferfd:%d, mPreVtBufferFd(%d)",
            __func__, mReleaseFence, mVtBufferFd, mPreVtBufferFd);

    mPreVtBufferFd = mVtBufferFd;
    mFbHandleUpdated = false;
    mVtUpdate = false;
    mTimeStamp = -1;

    return ret;
}

int32_t Hwc2Layer::releaseVtResource() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (isVtBuffer() || mNeedReleaseVtResource) {
        if (mVtUpdate && mPreVtBufferFd >= 0)
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    mPreVtBufferFd, getPrevReleaseFence());

        // todo: for the last vt buffer, there is no fence got from videocomposer
        // when videocomposer is disabled. Now set it to -1. And releaseVtResource
        // delay to clearUpdateFlag() when receive blank frame.
        if (mVtBufferFd >= 0)
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    mVtBufferFd, -1);

        mVtBufferFd = -1;
        mPreVtBufferFd = -1;
        mVtUpdate = false;

        if (mVtDeviceConnection) {
            MESON_LOGD("Hwc2Layer release disconnect %d", mTunnelId);
            VideoTunnelDev::getInstance().disconnect(mTunnelId);
        }

        mVtDeviceConnection = false;
        mNeedReleaseVtResource = false;
    }
    return 0;
}

void Hwc2Layer::setPresentTime(nsecs_t expectedPresentTime) {
    mExpectedPresentTime = ns2us(expectedPresentTime);

    static nsecs_t previousExpectedTime = 0;
    if (previousExpectedTime == 0)
        previousExpectedTime = mExpectedPresentTime;

    [[maybe_unused]] nsecs_t diffExpected = mExpectedPresentTime - previousExpectedTime;
    previousExpectedTime = mExpectedPresentTime;

    MESON_LOGV("[%s] vsyncTimeStamp:%lld, diffExpected:%lld",
            __func__, mExpectedPresentTime, diffExpected);
}

bool Hwc2Layer::shouldPresentNow() {
    if (mTimeStamp == -1)
        return true;

    const bool isDue =  mTimeStamp < mExpectedPresentTime;

    // Ignore timestamps more than a second in the future
    const bool isPlausible = mTimeStamp < (mExpectedPresentTime + s2ns(1));

    return  isDue ||  !isPlausible;
}
