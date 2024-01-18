/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_FRAMEBUFFER_H
#define DRM_FRAMEBUFFER_H

#include <stdlib.h>
#include <cutils/native_handle.h>
#include <math.h>

#include <BasicTypes.h>
#include <DrmSync.h>
#include <DrmTypes.h>
#include <DrmFrameBufferBase.h>
#include <hardware/hwcomposer2.h>

#include <am_gralloc_ext.h>

#define FORCE_CLIENT_REQ (1)

/*buffer for display or render.*/
class DrmFramebuffer : public DrmFramebufferBase {
public:
    DrmFramebuffer();
    DrmFramebuffer(const native_handle_t * bufferhnd, int32_t acquireFence);
    virtual ~DrmFramebuffer();

    virtual drm_rect_t getSourceCrop() override;
    virtual int32_t setAcquireFence(int32_t fenceFd) override;

    /* overloaded virtual function setBufferInfo */
    using DrmFramebufferBase::setBufferInfo;
    void setBufferInfo(const native_handle_t * bufferhnd, int32_t acquireFence, bool isSidebandBuffer=false);
    void clearBufferInfo();

    virtual bool isAfbcBuffer() override;

    std::shared_ptr<DrmFence> getAcquireFence();

    virtual void setUniqueId(hwc2_layer_t id);
    virtual hwc2_layer_t getUniqueId();

    /*set release fence for last present loop.*/
    int32_t setPrevReleaseFence(int32_t fenceFd);
    int32_t setCurReleaseFence(int32_t fenceFd);
    /*dup current release fence.*/
    int32_t getPrevReleaseFence();
    int32_t getCurReleaseFence();

    /* need merge fence */
    int32_t onLayerDisplayed(int32_t releaseFence, int32_t processFence);

    int32_t lock(void ** addr);
    int32_t unlock();

    bool isRotated();
    bool isSidebandBuffer() {return mIsSidebandBuffer;}
    virtual uint32_t getVideoTimestamp() {return 0;}

    virtual bool isFbUpdated() {return (mUpdated || mFbHandleUpdated);}
    bool isUpdated() {return mUpdated;}
    void clearFbHandleFlag();

    // Virtuals for video tunnel
    virtual bool haveValidBuffer() {return true;};
    virtual int32_t getBufferFd() { return -EINVAL; }
    virtual bool isVtBuffer() { return false;}
    virtual bool interruptVtProcess(drm_plane_blank_t & flag __unused) {return false;}
    virtual int32_t getSolidColorBuffer() { return -EINVAL; }
    virtual void freeSolidColorBuffer() {};

    // for video processor
    virtual void setDiProcessorFd(int32_t fd __unused) {};
    virtual void setDiProcessorFence(int32_t fenceFd __unused) {};
    virtual int32_t getDiProcessorFence() {return -1;};
    int32_t setProcessFence(int32_t fenceFd);
    int32_t getProcessFence();
    virtual bool isVirtualLayer() { return false;}

    void setReqFlag(uint32_t flag) { mReqFlag = flag; };
    int32_t getReqFlag() { return mReqFlag; };

protected:
    virtual bool isVtBufferLocked() {return false;}

    void reset();

public:
    native_handle_t * mBufferHandle;
    drm_color_t mColor;

    drm_rect_t mVtSourceCrop;
    drm_rect_t mVtDisplayFrame;
    bool mFbHandleUpdated;
    bool mIsSidebandBuffer;

    hwc2_layer_t mId;

    std::map<drm_hdr_metadata_t, float> mHdrMetaData;

    std::shared_ptr<DrmFence> mPrevReleaseFence;
    std::shared_ptr<DrmFence> mCurReleaseFence;
    bool mIsVirtualLayer = false;
    int32_t mBufferState = 0;
    enum {
        MODE_BACK = 0,
        MODE_FRONT = 1,
    };
protected:
    std::shared_ptr<DrmFence> mAcquireFence;
    std::shared_ptr<DrmFence> mProcessFence;

    void * mMapBase;
    std::mutex mMutex;
    std::mutex mPFenceMutex;
    uint32_t mReqFlag = 0;
};

#endif/*DRM_FRAMEBUFFER_H*/
