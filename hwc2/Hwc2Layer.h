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
#include <UvmDettach.h>
#include <VtConsumer.h>

#define VT_CMD_DISABLE_VIDEO     0x01
#define VT_CMD_GAME_MODE_ENABLE  0x02
#define VT_CMD_GAME_MODE_DISABLE 0x04

typedef enum {
    VT_VIDEO_STATUS_HIDE,
    VT_VIDEO_STATUS_CLEAR_LAST_FRAME,
    VT_VIDEO_STATUS_SHOW,
} vt_video_display_status;

class VtDisplayObserver {
public:
    VtDisplayObserver() {};
    virtual ~VtDisplayObserver() {};
    virtual void onFrameAvailable() = 0;
    virtual void onVtVideoGameMode(bool enable) = 0;
};

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
    Hwc2Layer(uint32_t dispId);
    virtual ~Hwc2Layer();

    bool isSecure() { return mSecure;}

    int32_t commitCompType(hwc2_composition_t hwcComp);

    bool isUpdateZorder() { return mUpdateZorder;}
    void clearUpdateFlag();

    void setLayerUpdate(bool update);
    int getVideoType();

    /* video tunnel api */
    bool isVtBuffer() override;
    bool isFbUpdated() override;
    int32_t getVtBuffer() override;
    void updateVtBuffer();
    int32_t releaseVtBuffer();
    int32_t releaseVtResource();
    void setPresentTime(nsecs_t expectedPresentTime);
    bool shouldPresentNow(nsecs_t timestamp);
    bool newGameBuffer();
    int32_t getSolidColorBuffer();

    int32_t registerConsumer();
    int32_t unregisterConsumer();
    bool isVtNeedClearLastFrame();
    bool isVtNeedHideVideo();
    int32_t onVtFrameAvailable(std::vector<std::shared_ptr<VtBufferItem>> & items);
    int32_t onVtFrameDisplayed(int bufferFd, int fenceFd);
    void onVtVideoHide();
    void onVtVideoBlank();
    void onVtVideoShow();
    void onVtVideoGameMode(int data);
    int32_t getVtVideoStatus();
    void setVtSourceCrop(drm_rect_t & rect);
    void onNeedShowTempBuffer(int colorType);
    void setVideoType(int videoType);

    void setDisplayObserver(std::shared_ptr<VtDisplayObserver> observer);
    void handleDisplayDisconnet(bool connect);

public:
    android_dataspace_t mDataSpace;
    hwc2_composition_t mHwcCompositionType;
    hwc_region_t mVisibleRegion;
    hwc_region_t mDamageRegion;
    drm_rect_t mBackupDisplayFrame;

public:
    // for videotunnel
    class VtContentChangeListener : public VtConsumer::VtContentListener {
    public:
        VtContentChangeListener(Hwc2Layer* layer) : mLayer(layer) {};
        ~VtContentChangeListener() {};

        int32_t onFrameAvailable(std::vector<std::shared_ptr<VtBufferItem>> & items);
        void onVideoHide();
        void onVideoBlank();
        void onVideoShow();
        void onVideoGameMode(int data);
        int32_t getVideoStatus();
        void onSourceCropChange(vt_rect_t & crop);
        void onNeedShowTempBuffer(int colorType);
        void setVideoType(int videoType);
    private:
        Hwc2Layer* mLayer;
    };

protected:
    hwc2_error_t handleDimLayer(buffer_handle_t buffer);
    int32_t releaseVtResourceLocked(bool needDisconnect = true);
    bool isVtBufferLocked() override;

    /* for NR */
    int32_t attachUvmBuffer(const int bufferFd);
    int32_t dettachUvmBuffer();
    int32_t collectUvmBuffer(const int bufferFd, const int fence);
    int32_t releaseUvmResource();
    int32_t releaseUvmResourceLock();

    /* solid color buffer for video tunnel layer */
    void freeSolidColorBuffer();

protected:
    bool mUpdateZorder;

    bool mGameMode;
    bool mVtUpdate;
    bool mNeedReleaseVtResource;
    int mTunnelId;
    int mVtBufferFd;
    int mSolidColorBufferfd;
    int mPreVtBufferFd;
    int mAMVideoType;
    uint32_t mDisplayId;
    int32_t mQueuedFrames;
    int64_t mTimestamp;
    nsecs_t mExpectedPresentTime;
    nsecs_t mPreviousTimestamp;
    std::deque<VtBufferItem> mQueueItems;
    std::shared_ptr<VtConsumer::VtContentListener> mContentListener;
    std::shared_ptr<VtConsumer> mVtConsumer;
    vt_video_display_status mVideoDisplayStatus;
    std::shared_ptr<VtDisplayObserver> mDisplayObserver;

    std::shared_ptr<UvmDettach> mUvmDettach;
    int mPreUvmBufferFd;
};


#endif/*HWC2_LAYER_H*/
