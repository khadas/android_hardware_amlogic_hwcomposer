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
#include <hwcomposer3.h>

#include "VtInstanceMgr.h"
#include "Hwc2Layer.h"
#include "Hwc2Base.h"
#include "VideoTunnelDev.h"
#include "HwcConfig.h"

Hwc2Layer::Hwc2Layer(uint32_t dispId) : DrmFramebuffer(){
    mDataSpace     = HAL_DATASPACE_UNKNOWN;
    mUpdateZorder  = false;
    mVtBufferFd    = -1;
    mPreVtBufferFd = -1;
    mSolidColorBufferfd = -1;
    mDifd = -1;
    mFirstVtBuffer = false;
    mVtUpdate      =  false;
    mNeedReleaseVtResource = false;
    mTimestamp     = -1;
    mExpectedPresentTime = 0;
    mPreviousTimestamp = 0;
    mQueuedFrames = 0;
    mTunnelId = -1;
    mTunnelIdUpdate = false;
    mGameMode = false;
    mVideoDisplayStatus = VT_VIDEO_STATUS_SHOW;
    mAMVideoType = -1;
    mVtRefreshed = false;
    mQueueItems.clear();

    mPreUvmBufferFd = -1;

    mDisplayObserver = nullptr;
    mContentListener = nullptr;
    mAllocSolidColorBufferHandle = nullptr;
    mDisplayId = dispId;
    mHwcCompositionType = HWC2_COMPOSITION_INVALID;
    mBrightness = -1;
    mVideoDecTimestamp = -1;
    memset(&mVisibleRegion, 0, sizeof(mVisibleRegion));
    memset(&mDamageRegion, 0, sizeof(mDamageRegion));
    memset(&mBackupDisplayFrame, 0, sizeof(mBackupDisplayFrame));
    memset(&mCalibrateInfo, 0, sizeof(mCalibrateInfo));
    mNeedAskRefresh = false;
}

Hwc2Layer::~Hwc2Layer() {
    // release last video tunnel buffer
    releaseVtResource();
    releaseUvmResource();
    freeSolidColorBuffer();
    if (mDifd >= 0)
        close(mDifd);
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
     * If video tunnel sideband receive blank frame,
     * need release video tunnel resource
     */
    if (isVtBufferLocked()) {
        mNeedReleaseVtResource = true;
        releaseUvmResourceLock();
    }

    /*
    * SurfaceFlinger will call setCompositionType() first,then setBuffer().
    * So it is safe to calc drm_fb_type_t mFbType here.
    */
    clearBufferInfo();
    setBufferInfo(buffer, acquireFence);

    /*
    * For UVM video buffer, set UVM flags.
    * As the buffer will was update already
    */
    detachUvmBuffer();
    if (preType == DRM_FB_VIDEO_UVM_DMA && mPreUvmBufferFd >= 0) {
        collectUvmBuffer(mPreUvmBufferFd, getPrevReleaseFence());
        mPreUvmBufferFd = -1;
    }

    if (buffer == NULL || mBufferHandle == NULL) {
        MESON_LOGE("Receive null buffer, it is impossible.");
        mFbType = DRM_FB_UNDEFINED;
        return HWC2_ERROR_NONE;
    }

    /*set mFbType by usage of GraphicBuffer.*/
    if (mHwcCompositionType == HWC2_COMPOSITION_CURSOR) {
        mFbType = DRM_FB_CURSOR;
    } else if (am_gralloc_is_uvm_dma_buffer(mBufferHandle)) {
        mFbType = DRM_FB_VIDEO_UVM_DMA;
        int bufFd = am_gralloc_get_buffer_fd(mBufferHandle);
        if (bufFd >= 0) {
            mPreUvmBufferFd = dup(bufFd);
            attachUvmBuffer(mPreUvmBufferFd);
            getVideoInfoFromUVM(bufFd);
        }
    } else if (am_gralloc_is_omx_metadata_buffer(mBufferHandle)) {
        int tunnel = 0;
        int ret = am_gralloc_get_omx_metadata_tunnel(mBufferHandle, &tunnel);
        if (ret != 0)
            return HWC2_ERROR_BAD_LAYER;
        if (tunnel == 0)
            mFbType = DRM_FB_VIDEO_OMX_PTS;
        else
            mFbType = DRM_FB_VIDEO_OMX_PTS_SECOND;
    } else if (am_gralloc_is_overlay_buffer(mBufferHandle)) {
        mFbType = DRM_FB_VIDEO_OVERLAY;
    } else if (am_gralloc_get_width(mBufferHandle) <= 1 && am_gralloc_get_height(mBufferHandle) <= 1) {
        //For the buffer which size is 1x1, we treat it as a dim layer.
        handleDimLayer(mBufferHandle);
    } else if (am_gralloc_is_coherent_buffer(mBufferHandle)) {
        if (am_gralloc_get_format(mBufferHandle) == HAL_PIXEL_FORMAT_YCrCb_420_SP)
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
    VtInstanceMgr::getInstance().lockInstancesMutex();
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
                mTunnelIdUpdate = true;
                mAMVideoType = -1;
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
    VtInstanceMgr::getInstance().unlockInstancesMutex();

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
    vtRefresh();
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setDisplayFrame(hwc_rect_t frame) {
    /*Used for display frame scale*/
    mBackupDisplayFrame.left = frame.left;
    mBackupDisplayFrame.top = frame.top;
    mBackupDisplayFrame.right = frame.right;
    mBackupDisplayFrame.bottom = frame.bottom;

    mUpdated = true;
    vtRefresh();
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

hwc2_error_t Hwc2Layer::setCompositionType(int32_t type){
    mHwcCompositionType = type;
    mUpdated = true;
    if (type == HWC3_COMPOSITION_DECORATION)
        mFbType = DRM_FB_DECORATION;

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
    vtRefresh();
    return HWC2_ERROR_NONE;
}

int32_t Hwc2Layer::setPerFrameMetadata(
    uint32_t numElements, const int32_t* /*hw2_per_frame_metadata_key_t*/ keys,
    const float* metadata) {
    mHdrMetaData.clear();
    for (uint32_t i = 0; i < numElements; i++) {
        mHdrMetaData.insert({static_cast<drm_hdr_metadata_t>(keys[i]),metadata[i]});
    }
    return HWC2_ERROR_NONE;
}

//TODO: set bright to composition
int32_t Hwc2Layer::setBrightness(float brightness) {
    if (brightness < 0)
        return HWC2_ERROR_BAD_PARAMETER;

    mBrightness = brightness;
    return HWC2_ERROR_NONE;
}

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
int32_t Hwc2Layer::uvmDetachEnable(bool enable) {
    std::lock_guard<std::mutex> lock(mMutex);
    releaseUvmResourceLock();
    if (mUvmDetach)
        return mUvmDetach->setEnable(enable);

    return 0;
}

int32_t Hwc2Layer::attachUvmBuffer(const int bufferFd) {
    if (!mUvmDetach)
        mUvmDetach = std::make_shared<UvmDetach>(mId);

    return mUvmDetach->attachUvmBuffer(bufferFd);
}

int32_t Hwc2Layer::detachUvmBuffer() {
    if (mUvmDetach)
        return mUvmDetach->detachUvmBuffer();

    return 0;
}

int32_t Hwc2Layer::collectUvmBuffer(const int fd, const int fence) {
    if (mUvmDetach)
        return mUvmDetach->collectUvmBuffer(fd, fence);

    return 0;
}

int32_t Hwc2Layer::releaseUvmResourceLock() {
    if (mPreUvmBufferFd >= 0)
        close(mPreUvmBufferFd);
    mPreUvmBufferFd = -1;

    if (mUvmDetach)
        return mUvmDetach->releaseUvmResource();

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

bool Hwc2Layer::isFbUpdated() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mSolidColorBufferfd >= 0)
        return true;

    if (isVtBufferLocked()) {
        return (shouldPresentNow(mTimestamp) && mVtUpdate) || mVtRefreshed;
    } else {
        return (mUpdated || mFbHandleUpdated);
    }
}

void Hwc2Layer::setDiProcessorFd(int32_t fd) {
    // do not dup fd
    if (mDifd >= 0)
        close(mDifd);

    mDifd = fd;
}

void Hwc2Layer::setDiProcessorFence(int32_t fenceFd) {
    mDiProcessorFence.reset(new DrmFence(fenceFd));
}

int32_t Hwc2Layer::getDiProcessorFence() {
    if (mDiProcessorFence.get())
        return mDiProcessorFence->dup();
    return -1;
}

bool Hwc2Layer::haveValidBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (isVtBufferLocked())
        return mFirstVtBuffer;
    else
        return true;
}

int32_t Hwc2Layer::getBufferFd() {
    std::lock_guard<std::mutex> lock(mMutex);
    return getBufferFdLocked();
}

int32_t Hwc2Layer::getBufferFdLocked() {
    int32_t fd = -1;

    if (mDifd >= 0 ) {
        fd = mDifd;
    } else if (mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
        /* should present the current buffer now? */
        if (shouldPresentNow(mTimestamp))
            fd = mVtUpdate ? mVtBufferFd : mPreVtBufferFd;
        else
            fd = mPreVtBufferFd;
    } else if (!isSidebandBuffer()){
        fd = am_gralloc_get_buffer_fd(mBufferHandle);
    }

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] return fd:%d",
            __func__, mDisplayId, mId, fd);

    return fd;
}

void Hwc2Layer::updateVtBuffer() {
    ATRACE_CALL();
    mMutex.lock();
    bool isVideoTypeChange = false;
    int expiredItemCount = 0;
    int dropCount = 0;
    nsecs_t diffAdded = 0;

    if (!isVtBufferLocked()) {
        mMutex.unlock();
        return;
    }

     /*
     * getBufferFd might drop the some buffers at the head of the queue
     * if there is a buffer behind them which is timely to be presented.
     */
    if (mQueueItems.empty()) {
        mMutex.unlock();
        return;
    }

    for (auto it = mQueueItems.begin(); it != mQueueItems.end(); it++) {
        if (it->mTimeStamp > 0 &&  shouldPresentNow(it->mTimeStamp)) {
            // do not drop frame in game mode
            if (!mGameMode)
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

    if (shouldPresentNow(mTimestamp)) {
        isVideoTypeChange = getVideoInfoFromUVM(mVtBufferFd);
        mFirstVtBuffer = true;
        mTunnelIdUpdate = false;
    }

    diffAdded = mTimestamp - mPreviousTimestamp;
    mPreviousTimestamp = mTimestamp;

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] mVtBufferFd(%d) timestamp (%" PRId64 " us) expectedPresentTime(%"
            PRId64 " us) diffAdded(%" PRId64 " us) shouldPresent:%d, queueFrameSize:%zu, isVideoTypeChange:%d",
            __func__, mDisplayId, mId, mVtBufferFd, mTimestamp, mExpectedPresentTime, diffAdded,
            shouldPresentNow(mTimestamp), mQueueItems.size(), isVideoTypeChange);

    mMutex.unlock();

    if (isVideoTypeChange)
        mDisplayObserver->askSurfaceFlingerRefresh();
}

int32_t Hwc2Layer::releaseVtBuffer() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked()) {
        MESON_LOGD("layer:%" PRId64 " is not videotunnel layer", getUniqueId());
        return -EINVAL;
    }

    detachUvmBuffer();

    if (mVtRefreshed) {
        mVtRefreshed = false;

        if (!shouldPresentNow(mTimestamp) ||
            (shouldPresentNow(mTimestamp) && !mVtUpdate)) {
            MESON_LOGV("[%s] [%" PRIu64 "] mVtRefreshed", __func__, mId);
            /* set release fence */
            setVtPrevReleaseFence();
            return 0;
        }
    }

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
    if (mVtBufferFd >= 0)
        mQueueItems.pop_front();

    // First release vt buffer, save to the previous
    if (mPreVtBufferFd < 0) {
        if (mVtBufferFd >= 0) {
            mPreVtBufferFd = mVtBufferFd;
            mVtUpdate = false;
            mVtBufferFd = -1;
            setVtPrevReleaseFence();
        }
        return 0;
    }

    // set fence to the previous vt buffer
    int releaseFence = getPrevReleaseFence();
    if (mPreVtBufferFd >= 0) {
        if (releaseFence >= 0) {
            collectUvmBuffer(dup(mPreVtBufferFd), dup(releaseFence));
        } else {
            collectUvmBuffer(dup(mPreVtBufferFd), releaseFence);
        }
    }

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] releaseFence:%d, mVtBufferfd:%d, mPreVtBufferFd(%d), queuedFrames(%d)",
            __func__, mDisplayId, mId, releaseFence, mVtBufferFd, mPreVtBufferFd, mQueuedFrames);

    int32_t ret = onVtFrameDisplayed(mPreVtBufferFd, releaseFence);
    mQueuedFrames--;

    mPreVtBufferFd = mVtBufferFd;
    mVtUpdate = false;
    mVtBufferFd = -1;
    setVtPrevReleaseFence();

    return ret;
}

void Hwc2Layer::askVtRefresh(bool flag) {
    VideoTunnelDev::getInstance().askRefresh(mTunnelId, flag);
}

void Hwc2Layer::setNeedAskRefresh(bool needRefresh) {
    mNeedAskRefresh = needRefresh;
}

/* layer will destroy or layer FbType changed to non-videotunnel */
int32_t Hwc2Layer::releaseVtResource() {
    int32_t ret = 0;

    VtInstanceMgr::getInstance().lockInstancesMutex();
    std::lock_guard<std::mutex> lock(mMutex);
    ret = releaseVtResourceLocked();
    VtInstanceMgr::getInstance().unlockInstancesMutex();

    return ret;
}

/* need hold VtInstanceMgr::mInstanceMutex and Hwc2Layer::mMutex before call */
int32_t Hwc2Layer::releaseVtResourceLocked() {
    return releaseVtResourceLocked(true);
}

int32_t Hwc2Layer::releaseVtResourceLocked(bool needDisconnect) {
    return releaseVtResourceLocked(needDisconnect, false);
}

int32_t Hwc2Layer::releaseVtResourceLocked(bool needDisconnect,
        bool needKeepLastFrame) {
    ATRACE_CALL();
    if (isVtBufferLocked() || mNeedReleaseVtResource) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "]", __func__, mDisplayId, mId);
        if (!mVtConsumer)
            return 0;

        if (needDisconnect && mNeedAskRefresh ) {
            askVtRefresh(true);
            mNeedAskRefresh = false;
        }

        if (!needKeepLastFrame || needDisconnect) {
            if (mPreVtBufferFd >= 0) {
                onVtFrameDisplayed(mPreVtBufferFd, getPrevReleaseFence());
                mQueuedFrames--;
                MESON_LOGV("[%s] [%d] [%" PRIu64 "] release(%d) queuedFrames(%d)",
                        __func__, mDisplayId, mId, mPreVtBufferFd, mQueuedFrames);
            }
        }

        if (mVtBufferFd >= 0) {
            onVtFrameDisplayed(mVtBufferFd, getCurReleaseFence());
            mQueuedFrames--;
            MESON_LOGV("[%s] [%d] [%" PRIu64 "] release(%d) queuedFrames(%d)",
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

        if (!needKeepLastFrame || needDisconnect)
            mPreVtBufferFd = -1;
        mVtBufferFd = -1;
        mQueueItems.clear();
        mVtUpdate = false;
        mTimestamp = -1;

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

void Hwc2Layer::handleDisplayDisconnect(bool connect) {
    if (connect) {
        VtInstanceMgr::getInstance().lockInstancesMutex();
        registerConsumer();
        VtInstanceMgr::getInstance().unlockInstancesMutex();
    }
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

    /* not game mode */
    return ret;
}

/* refresh vt layer */
void Hwc2Layer::vtRefresh() {
    mVtRefreshed = true;
}

int Hwc2Layer::getVideoTunnelId() {
    return mTunnelId;
}

void Hwc2Layer::setVtPrevReleaseFence() {
    // CureRelease move to PrevRelease, it can be returned in next loop.
    if (mCurReleaseFence.get() && mCurReleaseFence != DrmFence::NO_FENCE)
        mPrevReleaseFence = mCurReleaseFence;
    mCurReleaseFence.reset();
}

int32_t Hwc2Layer::registerConsumer() {
    ATRACE_CALL();
    int32_t ret = -1;

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] tunnelId is less than 0, cannot register consumer",
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

    ret = VtInstanceMgr::getInstance().connectInstanceLocked(mTunnelId, mVtConsumer);
    if (ret >= 0)
        MESON_LOGD("[%s] [%d] [%" PRIu64 "] connect to instance %d seccussed",
            __func__, mDisplayId, mId, mTunnelId);
    else
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] connect to instance %d failed",
            __func__, mDisplayId, mId, mTunnelId);

    askVtRefresh(false);
    return ret;
}

/* need lock VtInstanceMgr.mInstanceMutex before */
int32_t Hwc2Layer::unregisterConsumer() {
    ATRACE_CALL();
    int32_t ret = -1;

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] tunnelId is less than 0, cannot unregister consumer",
            __func__, mDisplayId, mId);
        return ret;
    }

    if (!mVtConsumer) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] consumer %d not create",
            __func__, mDisplayId, mId, mTunnelId);
        return ret;
    }

    memset(&mVtSourceCrop, 0, sizeof(mVtSourceCrop));
    memset(&mVtDisplayFrame, 0, sizeof(mVtDisplayFrame));

    ret = VtInstanceMgr::getInstance().disconnectInstanceLocked(mTunnelId, mVtConsumer);
    mVtConsumer.reset();
    mVtConsumer = nullptr;

    mTunnelId = -1;

    return ret;
}


int32_t Hwc2Layer::onVtFrameDisplayed(int bufferFd, int fenceFd) {
    ATRACE_CALL();

    int32_t ret = -1;
    if (!mVtConsumer)
        return ret;
    ret = mVtConsumer->onVtFrameDisplayed(bufferFd, fenceFd);
    if (ret) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] release vt buffer error, "
                "bufferFd=%d, fenceFd:%d", __func__, mDisplayId, mId, bufferFd, fenceFd);
    }

    return ret;
}

int32_t Hwc2Layer::onVtFrameAvailable(
        std::vector<std::shared_ptr<VtBufferItem>> & items) {
    ATRACE_CALL();
    mMutex.lock();

    /* if mNeedReleaseVtResource is true that means
     * this sideband layer receive blank frame. and layer's
     * type change from sideband to color.
     * don't return error.
     * */
    if (!isVtBufferLocked() && !mNeedReleaseVtResource) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] not videotunnel type", __func__, mDisplayId, mId);
        mMutex.unlock();
        return -EINVAL;
    }

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%d] [%" PRIu64 "] mTunnelId %d error", __func__, mDisplayId, mId, mTunnelId);
        mMutex.unlock();
        return -EINVAL;
    }

    for (auto it=items.begin(); it != items.end(); it ++) {
        VtBufferItem item = {(*it)->mVtBufferFd, (*it)->mTimeStamp};
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] mTunnelId %d, "
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

    mUpdated = true;
    vtRefresh();
}

void Hwc2Layer::setVtDisplayFrame(drm_rect_t & rect) {
    std::lock_guard<std::mutex> lock(mMutex);
    float scaleW = 1.0f;
    float scaleH = 1.0f;

    if (mDisplayId == HWC_DISPLAY_EXTERNAL) {
        uint32_t externalDispW, externalDispH;
        uint32_t primaryDispW, primaryDispH;
        HwcConfig::getFramebufferSize(HWC_DISPLAY_EXTERNAL, externalDispW, externalDispH);
        HwcConfig::getFramebufferSize(HWC_DISPLAY_PRIMARY, primaryDispW, primaryDispH);

        scaleW = (float)externalDispW / primaryDispW;
        scaleH = (float)externalDispH / primaryDispH;
    }

    mVtDisplayFrame.left   = (int32_t)ceilf(rect.left * scaleW);
    mVtDisplayFrame.top    = (int32_t)ceilf(rect.top * scaleH);
    mVtDisplayFrame.right  = (int32_t)ceilf(rect.right * scaleW);
    mVtDisplayFrame.bottom = (int32_t)ceilf(rect.bottom * scaleH);

    if (mCalibrateInfo.framebuffer_w > 0)
        adjustDisplayFrameLocked();

    mUpdated = true;
    vtRefresh();
}

void Hwc2Layer::adjustDisplayFrame(display_zoom_info_t & calibrateInfo) {
    std::lock_guard<std::mutex> lock(mMutex);

    mCalibrateInfo = calibrateInfo;
    adjustDisplayFrameLocked();
}
void Hwc2Layer::adjustDisplayFrameLocked() {
    bool bNoScale = false;
    drm_rect_t dispFrame;

    if (mCalibrateInfo.framebuffer_w == (mCalibrateInfo.crtc_display_w - mCalibrateInfo.crtc_display_x) &&
        mCalibrateInfo.framebuffer_h == (mCalibrateInfo.crtc_display_h - mCalibrateInfo.crtc_display_y))
        bNoScale = true;

    if (mVtDisplayFrame.left > 0 || mVtDisplayFrame.top > 0 ||
        mVtDisplayFrame.right > 0 || mVtDisplayFrame.bottom > 0)
        dispFrame = mVtDisplayFrame;
    else
        dispFrame = mBackupDisplayFrame;

    if (bNoScale || mIsVirtualLayer) {
        mDisplayFrame = dispFrame;
    } else {
       mDisplayFrame.left = (int32_t)ceilf((float)dispFrame.left *
           mCalibrateInfo.crtc_display_w / mCalibrateInfo.framebuffer_w) +
           mCalibrateInfo.crtc_display_x;
       mDisplayFrame.top = (int32_t)ceilf((float)dispFrame.top *
           mCalibrateInfo.crtc_display_h / mCalibrateInfo.framebuffer_h) +
           mCalibrateInfo.crtc_display_y;
       mDisplayFrame.right = (int32_t)ceilf((float)dispFrame.right *
           mCalibrateInfo.crtc_display_w / mCalibrateInfo.framebuffer_w) +
           mCalibrateInfo.crtc_display_x;
       mDisplayFrame.bottom = (int32_t)ceilf((float)dispFrame.bottom *
           mCalibrateInfo.crtc_display_h / mCalibrateInfo.framebuffer_h) +
           mCalibrateInfo.crtc_display_y;
    }
}

void Hwc2Layer::freeSolidColorBufferLocked() {
    if (mSolidColorBufferfd >= 0) {
        MESON_LOGV("[%s] [%d] [%" PRIu64 "] close solid color buffer:%d",
                __func__, mDisplayId, mId, mSolidColorBufferfd);
        close(mSolidColorBufferfd);
        mSolidColorBufferfd = -1;
        mVtUpdate = false;

        /* ignore temporary buffer's release fence */
        mVtRefreshed = false;
        setCurReleaseFence(-1);
    }
}

void Hwc2Layer::freeSolidColorBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    freeSolidColorBufferLocked();
}

/* this function is called after getBufferFd and
 * an invalid bufferFd was returned from getBufferFd */
int32_t Hwc2Layer::getSolidColorBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked())
        return -EINVAL;

    if (mSolidColorBufferfd < 0 &&
        (mAllocSolidColorBufferHandle.get() && mVtRefreshed) &&
        (mVideoDisplayStatus == VT_VIDEO_STATUS_COLOR_ONCE ||
         mVideoDisplayStatus == VT_VIDEO_STATUS_COLOR_ALWAYS)) {
        /* need refresh temporary buffer to VC */
        int fd = mAllocSolidColorBufferHandle->getPreBuffer();
        if (fd >= 0)
            mSolidColorBufferfd = dup(fd);
    }

    MESON_LOGV("[%s] [%d] [%" PRIu64 "] return fd:%d, mVtRefreshed:%d, mVideoDisplayStatus:%d",
            __func__, mDisplayId, mId, mSolidColorBufferfd, mVtRefreshed, mVideoDisplayStatus);
    return mSolidColorBufferfd;
}

bool Hwc2Layer::isVtNeedClearFrameOrShowColorBuffer() {
    bool ret = false;

    switch (mVideoDisplayStatus) {
        case VT_VIDEO_STATUS_BLANK:
            mVideoDisplayStatus = VT_VIDEO_STATUS_SHOW;
            /* need do disable video composer once */
            releaseVtResourceLocked(false);
            ret = true;
            break;
        case VT_VIDEO_STATUS_HIDE:
            releaseVtResourceLocked(false);
            setCurReleaseFence(-1);
            ret = true;
            break;
        case VT_VIDEO_STATUS_COLOR_ONCE:
            /* reset status flag to SHOW if get a valid,
             * or keep the status in order to support refresh
             * temp buffer */
            if (getBufferFdLocked() >= 0 || !mQueueItems.empty())
                mVideoDisplayStatus = VT_VIDEO_STATUS_SHOW;
            [[fallthrough]];
        case VT_VIDEO_STATUS_COLOR_ALWAYS:
            releaseVtResourceLocked(false);
            break;
        case VT_VIDEO_STATUS_COLOR_DISABLE:
            if (getBufferFdLocked() >= 0 || !mQueueItems.empty()) {
                mVideoDisplayStatus = VT_VIDEO_STATUS_SHOW;
                freeSolidColorBufferLocked();
            } else {
                mVideoDisplayStatus = VT_VIDEO_STATUS_COLOR_ONCE;
            }
            break;
        case VT_VIDEO_STATUS_HOLD_FRAME:
            releaseVtResourceLocked(false, true);
            break;
        default:
            // nothing to do;
            break;
    }

    return ret;
}

bool Hwc2Layer::interruptVtProcess(drm_plane_blank_t & flag) {
    /* 1, handle VtCmds
     * 2, show solid color buffer
     * 3, tunnelID changed, do not disable VideoComposer
     * */
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferLocked())
        return false;

    if (isVtNeedClearFrameOrShowColorBuffer()) {
        /* VT cmd need clear last frame, so disable VC */
        flag = BLANK_FOR_NO_CONTENT;
        return true;
    }

    if (getBufferFdLocked() < 0 &&
        !mAllocSolidColorBufferHandle &&
        !mTunnelIdUpdate ) {
        /* no buffer to VideoComposer and tunnel ID not changed,
         * need disable VC */
        flag = BLANK_FOR_NO_CONTENT;
        return true;
    }

    if (getBufferFdLocked() < 0 && mAllocSolidColorBufferHandle) {
        /* need show solid color buffer */
        return true;
    }

    return false;
}

void Hwc2Layer::onNeedShowTempBuffer(vt_video_color_t colorType) {
    bool needAskSFRefresh = false;
    mMutex.lock();
    if (mSolidColorBufferfd < 0) {
        if (!mAllocSolidColorBufferHandle)
            mAllocSolidColorBufferHandle = std::make_shared<VtAllocSolidColorBuffer>();

        int bufFd = mAllocSolidColorBufferHandle->allocBuffer(colorType);
        if (bufFd >= 0)
            mSolidColorBufferfd = dup(bufFd);
    }

    if (mSolidColorBufferfd >= 0) {
        mVtUpdate = true;
        if (!mFirstVtBuffer) {
            needAskSFRefresh = true;
            mFirstVtBuffer = true;
        }
    }

    mMutex.unlock();

    if (needAskSFRefresh)
        mDisplayObserver->askSurfaceFlingerRefresh();
}

void Hwc2Layer::onNeedShowTempBufferWithStatus(
        vt_video_color_t colorType, vt_video_status_t status) {
    if (status == VT_VIDEO_STATUS_COLOR_ONCE ||
        status == VT_VIDEO_STATUS_COLOR_ALWAYS)
        onNeedShowTempBuffer(colorType);

    onVtVideoStatus(status);
}

/*
 * get video info from uvm driver
 *
 * @param fd     [in] buffer fd
 *
 * return ture means video type change
 * */
bool Hwc2Layer::getVideoInfoFromUVM(int fd) {
    /* this function is called after attachUvmBuffer*/
    bool ret = false;
    int prvAMVideoType = mAMVideoType;
    struct uvm_fd_info videoInfo;
    String8 layerInfo;

    if (fd >= 0 && mUvmDetach) {
        memset(&videoInfo, 0, sizeof(videoInfo));
        videoInfo.fd = fd;

        mUvmDetach->getVideoInfo(videoInfo);

        mAMVideoType = videoInfo.type;
        mVideoDecTimestamp = (uint32_t)(videoInfo.timestamp / 1e9);

        if (prvAMVideoType != mAMVideoType)
            ret = true;
     }

    layerInfo.appendFormat("layerInfo(%" PRIu64 " %s %u)",
            mId, drmFbTypeToString(mFbType), mVideoDecTimestamp);
    ATRACE_NAME(layerInfo.c_str());
    return ret;
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

void Hwc2Layer::VtContentChangeListener::onDisplayFrameChange(vt_rect & frame) {
    drm_rect_t rect;
    rect.left   = frame .left;
    rect.top    = frame.top;
    rect.right  = frame.right;
    rect.bottom = frame.bottom;

    if (mLayer)
        mLayer->setVtDisplayFrame(rect);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}


void Hwc2Layer::VtContentChangeListener::onNeedShowTempBuffer(vt_video_color_t colorType) {
    if (mLayer)
        mLayer->onNeedShowTempBuffer(colorType);
    else
        MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                __func__);
}

void Hwc2Layer::VtContentChangeListener::onNeedShowTempBufferWithStatus(
        vt_video_color_t colorType, vt_video_status_t status) {
    if (mLayer)
        mLayer->onNeedShowTempBufferWithStatus(colorType, status);
    else
         MESON_LOGE("Hwc2Layer::VtContentChangeListener::%s mLayer is NULL",
                 __func__);
}
/* ========================================================================= */
