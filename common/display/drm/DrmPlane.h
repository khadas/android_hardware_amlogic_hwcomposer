/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef DRM_PLANE_H
#define DRM_PLANE_H

#include <stdlib.h>
#include <DrmFramebuffer.h>
#include <HwDisplayPlane.h>
#include <DrmBo.h>
#include "DrmCrtc.h"
#include "DrmProperty.h"
#include <queue>

class DrmPlane : public HwDisplayPlane {
public:
    DrmPlane(int drmFd, drmModePlanePtr p);
    ~DrmPlane();

    uint32_t getId();
    uint32_t getType();
    const char * getName();

    uint32_t getCapabilities();
    int32_t getFixedZorder();

    uint32_t getPossibleCrtcs();
    int32_t setCrtcId(uint32_t crtcid);

    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb,
        uint32_t zorder, int blankOp);

    void setDebugFlag(int dbgFlag);

    void dump(String8 & dumpstr);

protected:
    void resloveInFormats();
    bool validateFormat(uint32_t format, uint64_t modifier);
    void loadProperties();

protected:
    int mDrmFd;
    uint32_t mId;
    uint32_t mCrtcMask;

    uint32_t * mFormats;
    uint32_t mFormatCnt;
    struct drm_format_modifier * mModifiers;
    uint32_t mModifierCnt;

    /*plane propertys*/
    std::shared_ptr<DrmProperty> mType;

    std::shared_ptr<DrmProperty> mFbId;
    std::shared_ptr<DrmProperty> mSrcX;
    std::shared_ptr<DrmProperty> mSrcY;
    std::shared_ptr<DrmProperty> mSrcW;
    std::shared_ptr<DrmProperty> mSrcH;

    std::shared_ptr<DrmProperty> mCrtcId;
    std::shared_ptr<DrmProperty> mCrtcX;
    std::shared_ptr<DrmProperty> mCrtcY;
    std::shared_ptr<DrmProperty> mCrtcW;
    std::shared_ptr<DrmProperty> mCrtcH;

    std::shared_ptr<DrmProperty> mZpos;
    std::shared_ptr<DrmProperty> mInFence;
    std::shared_ptr<DrmProperty> mBlendMode;
    std::shared_ptr<DrmProperty> mAlpha;

    std::shared_ptr<DrmProperty> mInFormats;

    bool mBlank;

    std::shared_ptr<DrmBo> mLastDrmBo;
    std::shared_ptr<DrmBo> mDrmBo;
    std::shared_ptr<DrmFramebuffer> mFb;
    std::shared_ptr<HwDisplayCrtc> mCrtc;

    std::queue<std::shared_ptr<DrmBo>> mBoCache;

    int mDbgFlag;
};

 #endif/*DRM_PLANE_H*/
