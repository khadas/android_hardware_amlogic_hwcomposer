/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC2_LAYER_H
#define HWC2_LAYER_H

#include <deque>
#include <hardware/hwcomposer2.h>

#include <BasicTypes.h>
#include <DrmFramebuffer.h>

#define VT_CMD_DISABLE_VIDEO     0x01
#define VT_CMD_GAME_MODE_ENABLE  0x02
#define VT_CMD_GAME_MODE_DISABLE 0x04

class Hwc2Layer : public DrmFramebuffer {
/*Interfaces for hwc2.0 api.*/
public:
    /*set layer data info, only one of tree functions called.*/
    hwc2_error_t setBuffer(buffer_handle_t buffer, int32_t acquireFence);
    hwc2_error_t setSidebandStream(const native_handle_t* stream);
    hwc2_error_t setColor(hwc_color_t color);

    hwc2_error_t setSourceCrop(hwc_frect_t crop);
    hwc2_error_t setDisplayFrame(hwc_rect_t frame);
    hwc2_error_t setBlendMode(hwc2_blend_mode_t mode);
    hwc2_error_t setPlaneAlpha(float alpha);
    hwc2_error_t setTransform(hwc_transform_t transform);
    hwc2_error_t setVisibleRegion(hwc_region_t visible);
    hwc2_error_t setSurfaceDamage(hwc_region_t damage);
    hwc2_error_t setCompositionType(hwc2_composition_t type);
    hwc2_error_t setDataspace(android_dataspace_t dataspace);
    hwc2_error_t setZorder(uint32_t z);
#ifdef HWC_HDR_METADATA_SUPPORT
    int32_t setPerFrameMetadata(
            uint32_t numElements, const int32_t* /*hw2_per_frame_metadata_key_t*/ keys,
            const float* metadata);
#endif

/*Extend api.*/
public:
    Hwc2Layer();
    virtual ~Hwc2Layer();

    bool isSecure() { return mSecure;}

    int32_t commitCompType(hwc2_composition_t hwcComp);

    bool isUpdateZorder() { return mUpdateZorder;}
    void clearUpdateFlag();

    void setLayerUpdate(bool update);
    /* video tunnel api */
    bool isVtBuffer() override;
    bool isFbUpdated() override;
    int32_t getVtBuffer() override;
    int32_t acquireVtBuffer() override;
    int32_t releaseVtBuffer() override;
    int32_t recieveVtCmds() override;
    int32_t releaseVtResource();
    void setPresentTime(nsecs_t expectedPresentTime);
    bool shouldPresentNow(nsecs_t timestamp);
    bool newGameBuffer();

public:
    android_dataspace_t mDataSpace;
    hwc2_composition_t mHwcCompositionType;
    hwc_region_t mVisibleRegion;
    hwc_region_t mDamageRegion;
    drm_rect_t mBackupDisplayFrame;

protected:
    hwc2_error_t handleDimLayer(buffer_handle_t buffer);
    int32_t doReleaseVtResource();

    /* for NR */
    int32_t attachUvmBuffer(const int bufferFd);
    int32_t dettachUvmBuffer();
    int32_t collectUvmBuffer(const int bufferFd, const int fence);
    int32_t releaseUvmResource();
    int32_t releaseUvmResourceLock();

protected:
    bool mUpdateZorder;

    /* for videotunnel type layer */
    struct VtBufferItem {
        int bufferFd;
        int64_t timestamp;
    };

    int mTunnelId;
    std::mutex mMutex;
    int mVtBufferFd;
    int mPreVtBufferFd;
    int64_t mTimestamp;
    std::deque<VtBufferItem> mQueueItems;
    int32_t mQueuedFrames;
    bool mGameMode;

    nsecs_t mExpectedPresentTime;
    bool mVtUpdate;
    bool mNeedReleaseVtResource;

    /* for NR */
    struct UvmBuffer {
        int bufferFd;
        std::shared_ptr<DrmFence> releaseFence;
    };

    std::deque<UvmBuffer> mUvmBufferQueue;
    int mPreUvmBufferFd;
};


#endif/*HWC2_LAYER_H*/
