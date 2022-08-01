/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <MesonLog.h>
#include <math.h>
#include <sys/mman.h>
#include <cutils/properties.h>
#include <utils/Timers.h>
#include <inttypes.h>
#include <misc.h>

#include "VtInstanceMgr.h"
#include "Hwc2Layer.h"
#include "Hwc2Base.h"
#include "VideoTunnelDev.h"

Hwc2Layer::Hwc2Layer(uint32_t dispId) : DrmFramebuffer(){
    mDataSpace     = HAL_DATASPACE_UNKNOWN;
    mUpdateZorder  = false;
    mVtBufferFd    = -1;
    mPreVtBufferFd = -1;
    mSolidColorBufferfd = -1;
    mVtUpdate      =  false;
    mNeedReleaseVtResource = false;
    mTimestamp     = -1;
    mExpectedPresentTime = 0;
    mPreviousTimestamp = 0;
    mQueuedFrames = 0;
    mTunnelId = -1;
    mGameMode = false;
    mVideoDisplayStatus = VT_VIDEO_STATUS_SHOW;
    mAMVideoType = -1;
    mQueueItems.clear();

    mPreUvmBufferFd = -1;

    mDisplayObserver = nullptr;
    mContentListener = nullptr;
    mDisplayId = dispId;
}

Hwc2Layer::~Hwc2Layer() {
    // release last video tunnel buffer
    releaseVtResource();
    releaseUvmResource();
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
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    /*record for check fbType change status */
    drm_fb_type_t preType = mFbType;

    /*
     * If video tunnel sideband recieve blank frame,
     * need release video tunnel resouce
     */
    if (isVtBufferLocked()) {
        mNeedReleaseVtResource = true;
        releaseUvmResourceLock();
    }

    /*
    * SurfaceFlinger will call setCompostionType() first,then setBuffer().
    * So it is safe to calc drm_fb_type_t mFbType here.
    */
    clearBufferInfo();
    setBufferInfo(buffer, acquireFence);

    /*
    * For UVM video buffer, set UVM flags.
    * As the buffer will was update already
    */
    dettachUvmBuffer();
    if (preType == DRM_FB_VIDEO_UVM_DMA && mPreUvmBufferFd >= 0)
        collectUvmBuffer(mPreUvmBufferFd, getPrevReleaseFence());

    if (buffer == NULL) {
        MESON_LOGE("Receive null buffer, it is impossible.");
        mFbType = DRM_FB_UNDEFINED;
        return HWC2_ERROR_NONE;
    }

    /*set mFbType by usage of GraphicBuffer.*/
    if (mHwcCompositionType == HWC2_COMPOSITION_CURSOR) {
        mFbType = DRM_FB_CURSOR;
    } else if (am_gralloc_is_uvm_dma_buffer(buffer)) {
        mFbType = DRM_FB_VIDEO_UVM_DMA;
        mPreUvmBufferFd = dup(am_gralloc_get_buffer_fd(buffer));
        attachUvmBuffer(mPreUvmBufferFd);
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

    // changed from UVM to other type
    if (preType != mFbType && preType == DRM_FB_VIDEO_UVM_DMA) {
        releaseUvmResourceLock();
    }

    mSecure = am_gralloc_is_secure_buffer(mBufferHandle);
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSidebandStream(const native_handle_t* stream,
        std::shared_ptr<VtDisplayObserver> observer) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGV("[%s] [%" PRIu64 "]", __func__, mId);
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
        if (channel_id < 0)
            return HWC2_ERROR_BAD_PARAMETER;

        if (mTunnelId != channel_id) {
            if (mTunnelId >= 0) {
                releaseVtResourceLocked();
            }
            mTunnelId = channel_id;

            if (!mDisplayObserver)
                mDisplayObserver = observer;

            int ret = registerConsumer();
            if (ret >= 0) {
                MESON_LOGD("%s [%" PRId64 "] register consumer for videotunnel %d succeeded",
                        __func__, mId, channel_id);
                mQueuedFrames = 0;
                mQueueItems.clear();
            } else {
                MESON_LOGE("%s [%" PRId64 "] register consumer for videotunnel %d failed, error %d",
                        __func__, mId, channel_id, ret);
                mTunnelId = -1;
                return HWC2_ERROR_BAD_PARAMETER;
            }
        }
    } else {
        if (channel_id == AM_VIDEO_EXTERNAL) {
            mFbType = DRM_FB_VIDEO_SIDEBAND_SECOND;
        } else {
            mFbType = DRM_FB_VIDEO_SIDEBAND;
        }
    }

    /* fbtype is not tunnel sideband */
    if (mFbType != DRM_FB_VIDEO_TUNNEL_SIDEBAND)
        releaseUvmResourceLock();

    mSecure = false;
    mUpdated = true;
    return HWC2_ERROR_NONE;

}

hwc2_error_t Hwc2Layer::setColor(hwc_color_t color) {
    std::lock_guard<std::mutex> lock(mMutex);
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

/* ========================== Uvm Attach =================================== */
/* just for non-tunnel type video */
int32_t Hwc2Layer::attachUvmBuffer(const int bufferFd) {
    if (!mUvmDettach)
        mUvmDettach = std::make_shared<UvmDettach>(mId);

    return mUvmDettach->attachUvmBuffer(bufferFd);
}

int32_t Hwc2Layer::dettachUvmBuffer() {
    if (mUvmDettach)
        return mUvmDettach->dettachUvmBuffer();

    return 0;
}

int32_t Hwc2Layer::collectUvmBuffer(const int fd, const int fence) {
    if (mUvmDettach)
        return mUvmDettach->collectUvmBuffer(fd, fence);

    return 0;
}

int32_t Hwc2Layer::releaseUvmResourceLock() {
    if (mPreUvmBufferFd >= 0)
        close(mPreUvmBufferFd);
    mPreUvmBufferFd = -1;

    if (mUvmDettach)
        return mUvmDettach->releaseUvmResource();

    return 0;
}

int32_t Hwc2Layer::releaseUvmResource() {
    std::lock_guard<std::mutex> lock(mMutex);

    return releaseUvmResourceLock();
}
/* ========================= End Uvm Attach ================================= */

/* ======================== videotunnel api ================================= */
bool Hwc2Layer::isVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND;
}

bool Hwc2Layer::isVtBufferLocked() {
    return mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND;
}

bool Hwc2Layer::isFbUpdated(){
    std::lock_guard<std::mutex> lock(mMutex);
    if (isVtBufferLocked()) {
        return (shouldPresentNow(mTimestamp) && mVtUpdate);
    } else {
        return (mUpdated || mFbHandleUpdated);
    }
}

int32_t Hwc2Layer::getVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked())
        return -EINVAL;

    /* should present the current buffer now? */
    int ret = -1;
    if (shouldPresentNow(mTimestamp)) {
        ret = mVtUpdate ? mVtBufferFd : mPreVtBufferFd;
    } else {
        ret = mPreVtBufferFd;
    }

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] vtBufferfd(%d)", __func__, mDisplayId, mId, ret);

    return ret;
}

void Hwc2Layer::freeSolidColorBuffer() {
    if (mSolidColorBufferfd >= 0) {
        close(mSolidColorBufferfd);
        mSolidColorBufferfd = -1;
    }
}

int32_t Hwc2Layer::getSolidColorBuffer(bool used) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked())
        return -EINVAL;

    if (used)
        mVtUpdate = false;

    return mSolidColorBufferfd;
}

void Hwc2Layer::updateVtBuffer() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    int expiredItemCount = 0;
    int dropCount = 0;
    nsecs_t diffAdded = 0;

    if (!isVtBufferLocked())
        return;

     /*
     * getVtBuffer might drop the some buffers at the head of the queue
     * if there is a buffer behind them which is timely to be presented.
     */
    if (mQueueItems.empty())
        return;

    for (auto it = mQueueItems.begin(); it != mQueueItems.end(); it++) {
        if (it->mTimeStamp > 0 &&  shouldPresentNow(it->mTimeStamp)) {
            expiredItemCount++;
        }
    }

    // drop expiredItemCount-1 buffers
    dropCount = expiredItemCount - 1;
    ATRACE_INT64("Hwc2Layer:DropCount", dropCount);

    while (dropCount > 0) {
        auto item = mQueueItems.front();

        diffAdded = item.mTimeStamp - mPreviousTimestamp;
        mPreviousTimestamp = item.mTimeStamp;

        int releaseFence = -1;
        collectUvmBuffer(dup(item.mVtBufferFd), releaseFence);
        onVtFrameDisplayed(item.mVtBufferFd, releaseFence);
        mQueuedFrames--;

        MESON_LOGD("[%s] [%d] [%" PRIu64 "] vt buffer fd(%d) with timestamp(%" PRId64 ") expired "
                "droped expectedPresent time(%" PRId64 ") diffAdded(%" PRId64 ") queuedFrames(%d)",
                __func__, mDisplayId,mId, item.mVtBufferFd, item.mTimeStamp,
                mExpectedPresentTime, diffAdded, mQueuedFrames);
        mQueueItems.pop_front();
        dropCount--;
    }

    // update current mVtBuffer
    mVtUpdate = true;
    mVtBufferFd = mQueueItems[0].mVtBufferFd;
    mTimestamp = mQueueItems[0].mTimeStamp;

    diffAdded = mTimestamp - mPreviousTimestamp;
    mPreviousTimestamp = mTimestamp;

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] mVtBufferFd(%d) timestamp (%" PRId64 " us) expectedPresentTime(%"
            PRId64 " us) diffAdded(%" PRId64 " us) shouldPresent:%d, queueFrameSize:%zu",
            __func__, mDisplayId, mId, mVtBufferFd, mTimestamp, mExpectedPresentTime, diffAdded,
            shouldPresentNow(mTimestamp), mQueueItems.size());
}

int32_t Hwc2Layer::releaseVtBuffer() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked()) {
        MESON_LOGD("layer:%" PRId64 " is not videotunnel layer", getUniqueId());
        return -EINVAL;
    }

    dettachUvmBuffer();

    if (!mVtUpdate) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] vt buffer not update", __func__, mDisplayId, mId);
        return -EAGAIN;
    }

    if (shouldPresentNow(mTimestamp) == false) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] current vt buffer(%d) not present, timestamp not meet",
                __func__, mDisplayId, mId, mVtBufferFd);
        return -EAGAIN;
    }

    if (mQueueItems.empty()) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] Queued vtbuffer is empty!!",
                __func__, mDisplayId, mId);
        return -EAGAIN;
    }

    // remove it from the queueItems
    mQueueItems.pop_front();

    // First release vt buffer, save to the previous
    if (mPreVtBufferFd < 0) {
        mPreVtBufferFd = mVtBufferFd;
        mVtUpdate = false;
        mVtBufferFd = -1;

        // CureRelease move to PrevRelase, it can be returned in next loop.
        if (mCurReleaseFence.get() && mCurReleaseFence != DrmFence::NO_FENCE)
            mPrevReleaseFence = mCurReleaseFence;
        mCurReleaseFence.reset();

        return 0;
    }

    // set fence to the previous vt buffer
    int releaseFence = getPrevReleaseFence();
    collectUvmBuffer(dup(mPreVtBufferFd), dup(releaseFence));

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] releaseFence:%d, mVtBufferfd:%d, mPreVtBufferFd(%d), queuedFrames(%d)",
            __func__, mDisplayId, mId, releaseFence, mVtBufferFd, mPreVtBufferFd, mQueuedFrames);

    int32_t ret = onVtFrameDisplayed(mPreVtBufferFd, releaseFence);
    mQueuedFrames--;

    mPreVtBufferFd = mVtBufferFd;
    mVtUpdate = false;
    mVtBufferFd = -1;

    // CureRelease move to PrevRelase, it can be returned in next loop.
    if (mCurReleaseFence.get() && mCurReleaseFence != DrmFence::NO_FENCE)
        mPrevReleaseFence = mCurReleaseFence;
    mCurReleaseFence.reset();

    return ret;
}

int32_t Hwc2Layer::releaseVtResource() {
    /* layer will destroy or layer FbType changed to non-videotunnel */
    std::lock_guard<std::mutex> lock(mMutex);
    return releaseVtResourceLocked();
}

int32_t Hwc2Layer::releaseVtResourceLocked(bool needDisconnect) {
    ATRACE_CALL();
    if (isVtBufferLocked() || mNeedReleaseVtResource) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "]", __func__, mDisplayId, mId);
        if (!mVtConsumer)
            return 0;

        if (mPreVtBufferFd >= 0) {
            onVtFrameDisplayed(mPreVtBufferFd, getPrevReleaseFence());
            mQueuedFrames--;
            MESON_LOGV("[%s] [%d] [%" PRIu64 "] release(%d) queuedFrames(%d)",
                    __func__, mDisplayId, mId, mPreVtBufferFd, mQueuedFrames);
        }

        // todo: for the last vt buffer, there is no fence got from videocomposer
        // when videocomposer is disabled. Now set it to -1. And releaseVtResource
        // delay to clearUpdateFlag() when receive blank frame.
        if (mVtBufferFd >= 0) {
            onVtFrameDisplayed(mVtBufferFd, -1);
            mQueuedFrames--;
            MESON_LOGV("[%s] [%d] [%" PRIu64 "] release(%d) queudFrames(%d)",
                    __func__, mDisplayId, mId, mVtBufferFd, mQueuedFrames);
            mQueueItems.pop_front();
        }

        // release the buffers in mQueueItems
        for (auto it = mQueueItems.begin(); it != mQueueItems.end(); it++) {
            onVtFrameDisplayed(it->mVtBufferFd, -1);
            mQueuedFrames--;
            MESON_LOGV("[%s] [%d] [%" PRIu64 "] release(%d) queuedFrames(%d)",
                    __func__, mDisplayId, mId, it->mVtBufferFd, mQueuedFrames);
        }

        mQueueItems.clear();
        mVtBufferFd = -1;
        mPreVtBufferFd = -1;
        mVtUpdate = false;
        mTimestamp = -1;
        freeSolidColorBuffer();

        if (mTunnelId >= 0 && needDisconnect) {
            MESON_LOGD("[%s] [%d] [%" PRIu64 "] Hwc2Layer release disconnect(%d) queuedFrames(%d)",
                    __func__, mDisplayId, mId, mTunnelId, mQueuedFrames);
            unregisterConsumer();
        }

        mQueuedFrames = 0;
        mNeedReleaseVtResource = false;
    }
    return 0;
}

void Hwc2Layer::handleDisplayDisconnet(bool connect) {
    MESON_LOGV("[%s] [%d] [%" PRIu64 "] connect:%d", __func__, mDisplayId, mId, connect);
    if (connect) {
        registerConsumer();
    } else {
        std::lock_guard<std::mutex> lock(mMutex);
        releaseVtResourceLocked(false);
    }
}

int Hwc2Layer::getVideoTunnelId() {
    return mTunnelId;
}

void Hwc2Layer::setPresentTime(nsecs_t expectedPresentTime) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked())
        return;

    nsecs_t previousExpectedTime;
    previousExpectedTime = ((mExpectedPresentTime == 0) ?
        ns2us(expectedPresentTime) : mExpectedPresentTime);

    mExpectedPresentTime = ns2us(expectedPresentTime);

    [[maybe_unused]] nsecs_t diffExpected = mExpectedPresentTime - previousExpectedTime;
    ATRACE_INT64("Hwc2Layer:DiffExpected", diffExpected);

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] vsyncTimeStamp:%" PRId64 ", diffExpected:%" PRId64 "",
            __func__, mDisplayId, mId, mExpectedPresentTime, diffExpected);
}

bool Hwc2Layer::shouldPresentNow(nsecs_t timestamp) {
    if (timestamp == -1)
        return true;

    // game mode ignore timestamps
    if (mGameMode)
        return true;

    const bool isDue =  timestamp < mExpectedPresentTime;

    // Ignore timestamps more than a second in the future, timestamp is us
    const bool isPlausible = timestamp < (mExpectedPresentTime + s2ns(1)/1000);

    return  isDue ||  !isPlausible;
}

bool Hwc2Layer::newGameBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool ret = false;
    if (isVtBufferLocked() && mGameMode) {
        ret =  (mQueueItems.size() > 0) ? true : false;
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] return %d",
            __func__, mDisplayId, mId, ret);
    }

    if (ret) {
        // update current mVtBuffer
        mVtUpdate = true;
        mVtBufferFd = mQueueItems[0].mVtBufferFd;
        mTimestamp = mQueueItems[0].mTimeStamp;

        [[maybe_unused]] nsecs_t diffAdded = mTimestamp - mPreviousTimestamp;
        mPreviousTimestamp = mTimestamp;

        MESON_LOGV("[%s] [%d] [%" PRIu64 "] mVtBufferFd(%d) timestamp (%" PRId64 " us) expectedPresentTime(%"
                PRId64 " us) diffAdded(%" PRId64 " us) shouldPresent:%d, queueFrameSize:%zu",
                __func__, mDisplayId, mId, mVtBufferFd, mTimestamp, mExpectedPresentTime, diffAdded,
                shouldPresentNow(mTimestamp), mQueueItems.size());
    }
    /* not game mode */
    return ret;
}

int32_t Hwc2Layer::registerConsumer() {
    ATRACE_CALL();
    int32_t ret = -1;

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] tunneld is less than 0, cannot register consumer",
            __func__, mDisplayId, mId);
        return ret;
    }

    if (mVtConsumer)
        return 0;

    mVtConsumer = std::make_shared<VtConsumer> (mTunnelId,
                                                mDisplayId,
                                                mId);

    if (!mContentListener)
        mContentListener = std::make_shared<VtContentChangeListener>(this);

    ret = mVtConsumer->setVtContentListener(mContentListener);
    if (ret < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] set %d content listener failed ",
            __func__, mDisplayId, mId, mTunnelId);
        return ret;
    }

    ret = VtInstanceMgr::getInstance().connectInstance(mTunnelId, mVtConsumer);
    if (ret >= 0)
        MESON_LOGD("[%s] [%d] [%" PRIu64 "] connect to instance %d seccussed",
            __func__, mDisplayId, mId, mTunnelId);
    else
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] connect to instance %d failed",
            __func__, mDisplayId, mId, mTunnelId);

    return ret;
}

int32_t Hwc2Layer::unregisterConsumer() {
    ATRACE_CALL();
    int32_t ret = -1;

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] tunneld is less than 0, cannot unregister consumer",
            __func__, mDisplayId, mId);
        return ret;
    }

    if (!mVtConsumer) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] consumer %d not create",
            __func__, mDisplayId, mId, mTunnelId);
        return ret;
    }

    memset(&mVtSourceCrop, 0, sizeof(mVtSourceCrop));

    ret = VtInstanceMgr::getInstance().disconnectInstance(mTunnelId, mVtConsumer);
    mVtConsumer.reset();
    mVtConsumer = nullptr;
    mTunnelId = -1;

    return ret;
}

bool Hwc2Layer::isVtNeedClearFrame() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool ret = false;

    if (mVideoDisplayStatus == VT_VIDEO_STATUS_BLANK) {
        /* need do disable video composer once */
        mVideoDisplayStatus = VT_VIDEO_STATUS_SHOW;
        ret = true;
    } else if (mVideoDisplayStatus == VT_VIDEO_STATUS_HIDE) {
        setPrevReleaseFence(-1);
        ret = true;
    }

    if (ret)
        releaseVtResourceLocked(false);

    return ret;
}

int32_t Hwc2Layer::onVtFrameDisplayed(int bufferFd, int fenceFd) {
    ATRACE_CALL();

    int32_t ret = -1;
    if (!mVtConsumer)
        return ret;
    MESON_LOGV("[%s] [%d] [%" PRIu64 "] mTunelId %d, release vtBuffer(%d), fenceFd(%d)",
            __func__, mDisplayId, mId, mTunnelId, bufferFd, fenceFd);
    ret = mVtConsumer->onVtFrameDisplayed(bufferFd, fenceFd);
    if (ret) {
        MESON_LOGD("[%s] [%d] [%" PRIu64 "] release vt buffer error, "
                "bufferFd=%d, fenceFd:%d", __func__, mDisplayId, mId, bufferFd, fenceFd);
    }

    return ret;
}

int32_t Hwc2Layer::onVtFrameAvailable(
        std::vector<std::shared_ptr<VtBufferItem>> & items) {
    ATRACE_CALL();
    mMutex.lock();

    if (!isVtBufferLocked()) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] not videotunnel type", __func__, mDisplayId, mId);
        mMutex.unlock();
        return -EINVAL;
    }

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] mTunelId %d error", __func__, mDisplayId, mId, mTunnelId);
        mMutex.unlock();
        return -EINVAL;
    }

    for (auto it=items.begin(); it != items.end(); it ++) {
        VtBufferItem item = {(*it)->mVtBufferFd, (*it)->mTimeStamp};
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] mTunelId %d, "
                "get vtBuffer(%d), timeStamp(%" PRId64 ")", __func__,
                mDisplayId, mId, mTunnelId, (*it)->mVtBufferFd, (*it)->mTimeStamp);
        mQueueItems.push_back(item);
        mQueuedFrames++;
        attachUvmBuffer(item.mVtBufferFd);
    }

    if (mQueueItems.empty()) {
        mMutex.unlock();
        return -EAGAIN;
    }
    mMutex.unlock();

    if (mGameMode)
        mDisplayObserver->onFrameAvailable();
    return 0;
}

void Hwc2Layer::onVtVideoStatus(vt_video_status_t status) {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("[%s] [%d] [%" PRIu64 "] status: %d", __func__, mDisplayId, mId, status);
    mVideoDisplayStatus = status;
}

void Hwc2Layer::onVtVideoGameMode(int data) {
    if (data != 1 && data != 0) {
        MESON_LOGW("[%s] [%d] [%" PRIu64 "] get an invalid param for gameMode",
                __func__, mDisplayId, mId);
    } else {
        mGameMode = (data == 1 ? true : false);
        MESON_LOGD("[%s] [%d] [%" PRIu64 "] %s video game mode",
                __func__, mDisplayId, mId, mGameMode ? "enable" : "disable");
    }
}
int32_t Hwc2Layer::getVtVideoStatus() {
    // no implement
    return 0;
}

void Hwc2Layer::setVtSourceCrop(drm_rect_t & rect) {
    std::lock_guard<std::mutex> lock(mMutex);
    mVtSourceCrop.left   = rect.left;
    mVtSourceCrop.top    = rect.top;
    mVtSourceCrop.right  = rect.right;
    mVtSourceCrop.bottom = rect.bottom;
}

void Hwc2Layer::onNeedShowTempBuffer(int colorType) {
    // set default to black
    colorType = SET_VIDEO_TO_BLACK;

    mSolidColorBufferfd =
        dup(gralloc_get_solid_color_buf_fd((video_color_t)colorType));

    if (mSolidColorBufferfd >= 0)
        mVtUpdate = true;
}

void Hwc2Layer::setVideoType(int videoType) {
    mAMVideoType = videoType;
}

int Hwc2Layer::getVideoType() {
    int video_type = -1;
    if (isVtBuffer())
        video_type = mAMVideoType;
    else {
        if (mFbType != DRM_FB_VIDEO_SIDEBAND_TV &&
            mFbType != DRM_FB_VIDEO_TUNNEL_SIDEBAND &&
            mFbType != DRM_FB_VIDEO_SIDEBAND_SECOND &&
            mFbType != DRM_FB_VIDEO_SIDEBAND)
            am_gralloc_get_omx_video_type(mBufferHandle, &video_type);
    }

    return video_type;
}
/* ========================================================================= */

/* ================ content change listener for videotunnel ================ */
int32_t Hwc2Layer::VtContentChangeListener::onFrameAvailable(
        std::vector<std::shared_ptr<VtBufferItem>> & items) {
    int32_t ret = -1;
    if (mLayer)
        ret = mLayer->onVtFrameAvailable(items);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
    return ret;
}

void Hwc2Layer::VtContentChangeListener::onVideoStatus(vt_video_status_t status) {
    if (mLayer)
        mLayer->onVtVideoStatus(status);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}

void Hwc2Layer::VtContentChangeListener::onVideoGameMode(int data) {
    if (mLayer)
        mLayer->onVtVideoGameMode(data);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}

int32_t Hwc2Layer::VtContentChangeListener::getVideoStatus() {
    int32_t ret = -1;
    if (mLayer)
        ret = mLayer->getVtVideoStatus();
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);

    return ret;
}

void Hwc2Layer::VtContentChangeListener::onSourceCropChange(vt_rect & crop) {
    drm_rect_t rect;
    rect.left   = crop.left;
    rect.top    = crop.top;
    rect.right  = crop.right;
    rect.bottom = crop.bottom;

    if (mLayer)
        mLayer->setVtSourceCrop(rect);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}

void Hwc2Layer::VtContentChangeListener::onNeedShowTempBuffer(int colorType) {
    if (mLayer)
        mLayer->onNeedShowTempBuffer(colorType);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}

void Hwc2Layer::VtContentChangeListener::setVideoType(int videoType) {
    if (mLayer)
        mLayer->setVideoType(videoType);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}
/* ========================================================================= */
