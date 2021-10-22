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

#include "Hwc2Layer.h"
#include "Hwc2Base.h"
#include "VideoTunnelDev.h"
#include "UvmDev.h"

Hwc2Layer::Hwc2Layer() : DrmFramebuffer(){
    mDataSpace     = HAL_DATASPACE_UNKNOWN;
    mUpdateZorder  = false;
    mVtBufferFd    = -1;
    mPreVtBufferFd = -1;
    mSolidColorBufferfd = -1;
    mVtUpdate      =  false;
    mNeedReleaseVtResource = false;
    mTimestamp     = -1;
    mExpectedPresentTime = 0;
    mQueuedFrames = 0;
    mTunnelId = -1;
    mGameMode = false;
    mNeedClearLastFrame = false;
    mQueueItems.clear();

    mPreUvmBufferFd = -1;
    mUvmBufferQueue.clear();
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
    if (isVtBufferUnlock()) {
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
    if (preType == DRM_FB_VIDEO_UVM_DMA && mPreUvmBufferFd >= 0)
        collectUvmBuffer(mPreUvmBufferFd, getPrevReleaseFence());
    dettachUvmBuffer();

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

    // for the fist vum buffer
    if (mFbType == DRM_FB_VIDEO_UVM_DMA) {
        if (mPreUvmBufferFd < 0) {
            int fd = dup(am_gralloc_get_buffer_fd(buffer));
            attachUvmBuffer(fd);
            mPreUvmBufferFd = fd;
        }
    }

    // changed from UVM to other type
    if (preType != mFbType && preType == DRM_FB_VIDEO_UVM_DMA) {
        releaseUvmResourceLock();
    }

    mSecure = am_gralloc_is_secure_buffer(mBufferHandle);
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Layer::setSidebandStream(const native_handle_t* stream) {
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
                doReleaseVtResource();
            }

            int ret = VideoTunnelDev::getInstance().connect(channel_id);
            if (ret >= 0) {
                MESON_LOGD("%s [%" PRId64 "] connect to videotunnel %d successed", __func__, mId, channel_id);
                mQueuedFrames = 0;
                mQueueItems.clear();
                mTunnelId = channel_id;
                getSolidColorBuffer();
                mVtUpdate = true;
            } else {
                MESON_LOGE("%s [%" PRId64 "] connect to videotunnel %d failed, error %d",
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

bool Hwc2Layer::isVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND;
}

bool Hwc2Layer::isVtBufferUnlock() {
    return mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND;
}

bool Hwc2Layer::isFbUpdated(){
    std::lock_guard<std::mutex> lock(mMutex);
    if (isVtBufferUnlock()) {
        return (shouldPresentNow(mTimestamp) && mVtUpdate);
    } else {
        return (mUpdated || mFbHandleUpdated);
    }
}

int32_t Hwc2Layer::getVtBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferUnlock())
        return -EINVAL;

    /* should present the current buffer now? */
    int ret = -1;
    if (shouldPresentNow(mTimestamp)) {
        ret = mVtUpdate ? mVtBufferFd : mPreVtBufferFd;
    } else {
        ret = mPreVtBufferFd;
    }

    MESON_LOGV("[%s] [%" PRIu64 "] vtBufferfd(%d)", __func__, mId, ret);

    return ret;
}

int32_t Hwc2Layer::acquireVtBuffer() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferUnlock()) {
        MESON_LOGE("[%s] [%" PRIu64 "] not videotunnel type", __func__, mId);
        return -EINVAL;
    }

    if (mTunnelId < 0) {
        MESON_LOGE("[%s] [%" PRIu64 "] mTunelId  %d error", __func__, mId, mTunnelId);
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
            attachUvmBuffer(vtBufferFd);
        } else {
            // has no buffer available
            break;
        }
    }

    if (mQueueItems.empty())
        return -EAGAIN;

    dettachUvmBuffer();

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
    ATRACE_INT64("Hwc2Layer:DropCount", dropCount);

    while (dropCount > 0) {
        auto item = mQueueItems.front();

        if (previousTimestamp == 0)
            previousTimestamp = item.timestamp;

        diffAdded = item.timestamp - previousTimestamp;
        previousTimestamp = item.timestamp;

        int releaseFence = -1;
        collectUvmBuffer(dup(item.bufferFd), releaseFence);
        VideoTunnelDev::getInstance().releaseBuffer(mTunnelId, item.bufferFd, releaseFence);
        mQueuedFrames--;

        MESON_LOGD("[%s] [%" PRIu64 "] vt buffer fd(%d) with timestamp(%" PRId64 ") expired droped "
                "expectedPresent time(%" PRId64 ") diffAdded(%" PRId64 ") queuedFrames(%d)",
                __func__, mId, item.bufferFd, item.timestamp,
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

    MESON_LOGV("[%s] [%" PRIu64 "] mVtBufferFd(%d) timestamp (%" PRId64 " us) expectedPresentTime(%"
            PRId64 " us) diffAdded(%" PRId64 " us) shouldPresent:%d, queueFrameSize:%zu",
            __func__, mId, mVtBufferFd, mTimestamp,
            mExpectedPresentTime, diffAdded,
            shouldPresentNow(mTimestamp), mQueueItems.size());

    return 0;
}

int32_t Hwc2Layer::releaseVtBuffer() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferUnlock()) {
        MESON_LOGD("layer:%" PRId64 " is not videotunnel layer", getUniqueId());
        return -EINVAL;
    }

    if (!mVtUpdate)
        return -EAGAIN;

    if (shouldPresentNow(mTimestamp) == false) {
        MESON_LOGV("[%s] [%" PRIu64 "] current vt buffer(%d) not present, timestamp not meet",
                __func__, mId, mVtBufferFd);
        return -EAGAIN;
    }

    if (mQueueItems.empty()) {
        MESON_LOGV("Queued vtbuffer is empty!!");
        return -EINVAL;
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

    int ret = VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
            mPreVtBufferFd, releaseFence);
    mQueuedFrames--;


    MESON_LOGV("[%s] [%" PRIu64 "] releaseFence:%d, mVtBufferfd:%d, mPreVtBufferFd(%d), queuedFrames(%d)",
            __func__, mId, releaseFence, mVtBufferFd, mPreVtBufferFd, mQueuedFrames);

    mPreVtBufferFd = mVtBufferFd;
    mVtUpdate = false;
    mVtBufferFd = -1;

    // CureRelease move to PrevRelase, it can be returned in next loop.
    if (mCurReleaseFence.get() && mCurReleaseFence != DrmFence::NO_FENCE)
        mPrevReleaseFence = mCurReleaseFence;
    mCurReleaseFence.reset();

    return ret;
}

int32_t Hwc2Layer::recieveVtCmds() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (!isVtBufferUnlock())
        return -EINVAL;

    enum vt_cmd cmd;
    struct vt_cmd_data cmdData;

    int ret = VideoTunnelDev::getInstance().recieveCmd(mTunnelId, cmd, cmdData);
    if (ret < 0)
        return ret;

    MESON_LOGD("recv videotunnel [%d] cmd=%d", mTunnelId, cmd);
    if (cmd == VT_CMD_SET_SOURCE_CROP)
        MESON_LOGD("recv source crop:(%d %d %d %d)",
                cmdData.crop.left, cmdData.crop.top, cmdData.crop.right, cmdData.crop.bottom);
    else
        MESON_LOGD("recv cmdData: %d", cmdData.data);

    ret = 0;
    // process the cmd
    switch (cmd) {
        case VT_CMD_SET_VIDEO_STATUS:
            // disable video cmd
            if (cmdData.data == 1) {
                // release all the vt release
                doReleaseVtResource(false);
                ret |= VT_CMD_DISABLE_VIDEO;
                mNeedClearLastFrame = true;
            }
            break;
        case VT_CMD_SET_GAME_MODE:
            if (cmdData.data == 1) {
                ret |= VT_CMD_GAME_MODE_ENABLE;
                mGameMode = true;
            } else if (cmdData.data == 0) {
                ret |= VT_CMD_GAME_MODE_DISABLE;
                mGameMode = false;
            }
            MESON_LOGD("recv cmds mGameMode:%d", mGameMode);
            break;
        case VT_CMD_SET_SOURCE_CROP:
            mVtSourceCrop.left = cmdData.crop.left;
            mVtSourceCrop.top = cmdData.crop.top;
            mVtSourceCrop.right = cmdData.crop.right;
            mVtSourceCrop.bottom = cmdData.crop.bottom;
            break;
        default:
            MESON_LOGE("Not supported videoTunnel [%d] cmd:%d", mTunnelId, cmd);
    }

    return ret;
}

int32_t Hwc2Layer::releaseVtResource() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    return doReleaseVtResource();
}

int32_t Hwc2Layer::doReleaseVtResource(bool needDisconnect) {
    if (isVtBufferUnlock() || mNeedReleaseVtResource) {
        MESON_LOGV("[%s] [%" PRIu64 "]", __func__, mId);
        if (mPreVtBufferFd >= 0) {
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    mPreVtBufferFd, getPrevReleaseFence());
            mQueuedFrames--;
            MESON_LOGV("[%s] [%" PRIu64 "] release(%d) queuedFrames(%d)",
                    __func__, mId, mPreVtBufferFd, mQueuedFrames);
        }

        // todo: for the last vt buffer, there is no fence got from videocomposer
        // when videocomposer is disabled. Now set it to -1. And releaseVtResource
        // delay to clearUpdateFlag() when receive blank frame.
        if (mVtBufferFd >= 0) {
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    mVtBufferFd, getPrevReleaseFence());
            mQueuedFrames--;
            MESON_LOGV("[%s] [%" PRIu64 "] release(%d) queudFrames(%d)",
                    __func__, mId, mVtBufferFd, mQueuedFrames);
            mQueueItems.pop_front();
        }

        // release the buffers in mQueueItems
        for (auto it = mQueueItems.begin(); it != mQueueItems.end(); it++) {
            VideoTunnelDev::getInstance().releaseBuffer(mTunnelId,
                    it->bufferFd, -1);
            mQueuedFrames--;
            MESON_LOGV("[%s] [%" PRIu64 "] release(%d) queuedFrames(%d)",
                    __func__, mId, it->bufferFd, mQueuedFrames);
        }

        mQueueItems.clear();
        mVtBufferFd = -1;
        mPreVtBufferFd = -1;
        mVtUpdate = false;
        mTimestamp = -1;
        freeSolidColorBuffer();

        if (mTunnelId >= 0 && needDisconnect) {
            MESON_LOGD("[%s] [%" PRIu64 "] Hwc2Layer release disconnect(%d) queuedFrames(%d)",
                    __func__, mId, mTunnelId, mQueuedFrames);
            if (mGameMode)
                VideoTunnelDev::getInstance().setNonBlockMode();

            memset(&mVtSourceCrop, 0, sizeof(mVtSourceCrop));
            VideoTunnelDev::getInstance().disconnect(mTunnelId);
            mTunnelId = -1;
        }

        mGameMode = false;
        mQueuedFrames = 0;
        mNeedReleaseVtResource = false;
    }
    return 0;
}

void Hwc2Layer::setPresentTime(nsecs_t expectedPresentTime) {
    ATRACE_CALL();
    nsecs_t previousExpectedTime;
    previousExpectedTime = ((mExpectedPresentTime == 0) ?
        ns2us(expectedPresentTime) : mExpectedPresentTime);

    mExpectedPresentTime = ns2us(expectedPresentTime);

    [[maybe_unused]] nsecs_t diffExpected = mExpectedPresentTime - previousExpectedTime;
    ATRACE_INT64("Hwc2Layer:DiffExpected", diffExpected);

    MESON_LOGV("[%s] [%" PRIu64 "] vsyncTimeStamp:%" PRId64 ", diffExpected:%" PRId64 "",
            __func__, mId, mExpectedPresentTime, diffExpected);
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
    if (mGameMode) {
        return mQueueItems.size();
    }

    /* not game mode */
    return false;
}

int32_t Hwc2Layer::attachUvmBuffer(const int bufferFd) {
    return UvmDev::getInstance().attachBuffer(bufferFd);
}

int32_t Hwc2Layer::dettachUvmBuffer() {
    if (mUvmBufferQueue.size() <= 1)
        return -EAGAIN;

    int signalItemCount = 0;
    for (auto it = mUvmBufferQueue.begin(); it != mUvmBufferQueue.end(); it++) {
        auto currentStatus = it->releaseFence->getStatus();
        // fence was signaled
        if (currentStatus == DrmFence::Status::Signaled ||
                currentStatus == DrmFence::Status::Invalid)
            signalItemCount++;
    }

    MESON_LOGV("%s, UvmBufferQueue size:%zu, signalItemCount:%d",
            __func__, mUvmBufferQueue.size(), signalItemCount);

    while (signalItemCount > 0) {
        auto item = mUvmBufferQueue.front();
        signalItemCount--;

        MESON_LOGV("%s dettachBuffer:%d, fenceStatus:%d",
                __func__, item.bufferFd, item.releaseFence->getStatus());
        //dettach it
        UvmDev::getInstance().dettachBuffer(item.bufferFd);
        if (item.bufferFd >= 0)
            close(item.bufferFd);

        mUvmBufferQueue.pop_front();
    }
    return 0;
}

int32_t Hwc2Layer::collectUvmBuffer(const int fd, const int fence) {
    if (fd < 0) {
        MESON_LOGV("%s: get invalid fd", __func__);
        if (fence >= 0)
            close(fence);
        return -EINVAL;
    }
    if (fence < 0)
        MESON_LOGV("%s: get invalid fence", __func__);

    UvmBuffer item = {fd, std::move(std::make_shared<DrmFence>(fence))};
    mUvmBufferQueue.push_back(item);

    return 0;
}

int32_t Hwc2Layer::releaseUvmResourceLock() {
    for (auto it = mUvmBufferQueue.begin(); it != mUvmBufferQueue.end(); it++) {
        if (it->bufferFd >= 0)
            close(it->bufferFd);
    }

    mUvmBufferQueue.clear();
    if (mPreUvmBufferFd >= 0)
        close(mPreUvmBufferFd);

    mPreUvmBufferFd = -1;

    return 0;
}

int32_t Hwc2Layer::releaseUvmResource() {
    std::lock_guard<std::mutex> lock(mMutex);

    return releaseUvmResourceLock();
}

bool Hwc2Layer::isNeedClearLastFrame() {
    if (mNeedClearLastFrame) {
        /* need do disable video composer once */
        mNeedClearLastFrame = false;
        return true;
    }

    return false;
}

void Hwc2Layer::freeSolidColorBuffer() {
    if (mSolidColorBufferfd >= 0) {
        close(mSolidColorBufferfd);
        mSolidColorBufferfd = -1;
    }
}

int32_t Hwc2Layer::getSolidColorBuffer() {
    /* will send a colorFrame to VC when get a null VT buffer
     * at the beginning */
    if (mSolidColorBufferfd < 0) {
        int fd = gralloc_get_solid_color_buf_fd(SET_VIDEO_TO_BLACK);

        if (fd >= 0)
            mSolidColorBufferfd = dup(fd);
    }

    mVtUpdate = false;
    return mSolidColorBufferfd;
}
