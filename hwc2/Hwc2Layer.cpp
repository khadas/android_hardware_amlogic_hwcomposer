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
    mTimestamp     = -1;
    mQueuedFrames = 0;
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
    std::lock_guard<std::mutex> lock(mMutex);
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
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGV("[%s] (%lld)", __func__, mId);
    clearBufferInfo();
    setBufferInfo(stream, -1, true);

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
            mQueuedFrames = 0;
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

void Hwc2Layer::setLayerUpdate(bool update) {
    mUpdated = update;
}

void Hwc2Layer::clearUpdateFlag() {
    mUpdateZorder = mUpdated = false;
    if (mNeedReleaseVtResource)
        releaseVtResource();
}

bool Hwc2Layer::isVtBuffer() {
    return mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND;
}

bool Hwc2Layer::isFbUpdated(){
    std::lock_guard<std::mutex> lock(mMutex);
    if (isVtBuffer()) {
        return (shouldPresentNow(mTimestamp) && mVtUpdate);
    } else {
        return (mUpdated || mFbHandleUpdated);
    }
}

int32_t Hwc2Layer::getVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBuffer())
        return -EINVAL;

    /* should present the current buffer now? */
    int ret = -1;
    if (shouldPresentNow(mTimestamp)) {
        ret = mVtUpdate ? mVtBufferFd : mPreVtBufferFd;
    } else {
        ret = mPreVtBufferFd;
    }

    MESON_LOGV("[%s] vtBufferfd(%d)", __func__, ret);

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

    // acquire all the buffer that is available
    while (true) {
        int vtBufferFd = -1;
        int64_t vtTimestamp = -1;
        int ret = VideoTunnelDev::getInstance().acquireBuffer(mTunnelId,
                vtBufferFd, vtTimestamp);

        if (ret == 0) {
            VtBufferItem item = {vtBufferFd, vtTimestamp};
            mQueueItems.push_back(item);
            mQueuedFrames++;
        } else {
            // has no buffer available
            break;
        }
    }

    if (mQueueItems.empty())
        return -EAGAIN;

    static nsecs_t previousTimestamp = 0;
    [[maybe_unused]] nsecs_t diffAdded = 0;

    /*
     * getVtBuffer might drop the some buffers at the head of the queue
     * if there is a buffer behind them which is timely to be presented.
     */
    int expiredItemCount = 0;
    for (auto it = mQueueItems.begin(); it != mQueueItems.end(); it++) {
        if (it->timestamp > 0 &&  shouldPresentNow(it->timestamp)) {
            expiredItemCount++;
        }
    }

    // drop expiredItemCount-1 buffers
    int dropCount = expiredItemCount - 1;
    while (dropCount > 0) {
        auto item = mQueueItems.front();

        if (previousTimestamp == 0)
            previousTimestamp = item.timestamp;

        diffAdded = item.timestamp - previousTimestamp;
        previousTimestamp = item.timestamp;

        VideoTunnelDev::getInstance().releaseBuffer(mTunnelId, item.bufferFd, getPrevReleaseFence());
        mQueuedFrames--;

        MESON_LOGD("vt buffer fd(%d) with timestamp(%" PRId64 ") expired droped "
                "expectedPresent time(%" PRId64 ") diffAdded(%" PRId64 ") queuedFrames(%d)",
                item.bufferFd, item.timestamp,
                mExpectedPresentTime, diffAdded, mQueuedFrames);
        mQueueItems.pop_front();
        dropCount--;
    }

    // update current mVtBuffer
    mVtUpdate = true;
    mVtBufferFd = mQueueItems[0].bufferFd;
    mTimestamp = mQueueItems[0].timestamp;

    if (previousTimestamp == 0)
        previousTimestamp = mTimestamp;
    diffAdded = mTimestamp - previousTimestamp;
    previousTimestamp = mTimestamp;

    MESON_LOGV("[%s] mVtBufferFd(%d) timestamp (%" PRId64 " us) expectedPresentTime(%"
            PRId64 " us) diffAdded(%" PRId64 " us) shouldPresent:%d, queueFrameSize:%d",
            __func__, mVtBufferFd, mTimestamp,
            mExpectedPresentTime, diffAdded,
            shouldPresentNow(mTimestamp), mQueueItems.size());

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

    if (shouldPresentNow(mTimestamp) == false) {
        MESON_LOGV("[%s] current vt buffer(%d) not present, timestamp not meet",
                __func__, mVtBufferFd);
        return -EAGAIN;
    }

    if (mQueueItems.empty()) {
        MESON_LOGE("Queued vtbuffer is empty!!");
        return -EINVAL;
    }

    // remove it from the queueItems
    mQueueItems.pop_front();

    // First release vt buffer, save to the previous
    if (mPreVtBufferFd < 0) {
        mPreVtBufferFd = mVtBufferFd;
        mVtUpdate = false;
        mVtBufferFd = -1;
        return 0;
    }

    // set fence to the previous vt buffer
    int releaseFence = getPrevReleaseFence();
    int ret = VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
            mPreVtBufferFd, releaseFence);
    mQueuedFrames--;

    MESON_LOGV("[%s] releaseFence:%d, mVtBufferfd:%d, mPreVtBufferFd(%d), queuedFrames(%d)",
            __func__, releaseFence, mVtBufferFd, mPreVtBufferFd, mQueuedFrames);

    mPreVtBufferFd = mVtBufferFd;
    mVtUpdate = false;
    mVtBufferFd = -1;

    return ret;
}

int32_t Hwc2Layer::releaseVtResource() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (isVtBuffer() || mNeedReleaseVtResource) {
        MESON_LOGV("ReleaseVtResource");
        if (mPreVtBufferFd >= 0) {
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    mPreVtBufferFd, getPrevReleaseFence());
            mQueuedFrames--;
            MESON_LOGV("ReleaseVtResource release(%d) queuedFrames(%d)",
                    mPreVtBufferFd, mQueuedFrames);
        }

        // todo: for the last vt buffer, there is no fence got from videocomposer
        // when videocomposer is disabled. Now set it to -1. And releaseVtResource
        // delay to clearUpdateFlag() when receive blank frame.
        if (mVtBufferFd >= 0) {
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    mVtBufferFd, getPrevReleaseFence());
            mQueuedFrames--;
            MESON_LOGV("ReleaseVtResource release(%d) queudFrames(%d)", mVtBufferFd, mQueuedFrames);
            mQueueItems.pop_front();
        }

        // release the buffers in mQueueItems
        for (auto it = mQueueItems.begin(); it != mQueueItems.end(); it++) {
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    it->bufferFd, -1);
            mQueuedFrames--;
            MESON_LOGV("ReleaseVtResource release(%d) queuedFrames(%d)",
                    it->bufferFd, mQueuedFrames);
        }
        mQueueItems.clear();

        mVtBufferFd = -1;
        mPreVtBufferFd = -1;
        mVtUpdate = false;

        if (mVtDeviceConnection) {
            MESON_LOGD("Hwc2Layer release disconnect(%d) queuedFrames(%d)", mTunnelId, mQueuedFrames);
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

bool Hwc2Layer::shouldPresentNow(nsecs_t timestamp) {
    if (timestamp == -1)
        return true;

    const bool isDue =  timestamp < mExpectedPresentTime;

    // Ignore timestamps more than a second in the future
    const bool isPlausible = timestamp < (mExpectedPresentTime + s2ns(1));

    return  isDue ||  !isPlausible;
}
