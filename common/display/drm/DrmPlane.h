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
#include "DrmProperty.h"

class DrmPlane : public HwDisplayPlane {
public:
    DrmPlane(drmModePlanePtr p);
    ~DrmPlane();

    uint32_t getId();
    uint32_t getType();
    const char * getName();

    uint32_t getCapabilities();
    int32_t getFixedZorder();

    uint32_t getPossibleCrtcs();
    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb,
        uint32_t zorder, int blankOp);

    void setDebugFlag(int dbgFlag);

    void dump(String8 & dumpstr);

protected:
    bool validateFormat(uint32_t format);
    int32_t loadProperties();

protected:
    uint32_t mId;
    uint32_t mType;
    uint32_t mCrtcMask;
    uint32_t * mFormats;
    uint32_t mFormatCnt;

    /*plane propertys*/
    std::shared_ptr<DrmProperty> mFbId;
    std::shared_ptr<DrmProperty> mInFence;
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
    std::shared_ptr<DrmProperty> mBlendMode;
    std::shared_ptr<DrmProperty> mAlpha;



};

 #endif/*DRM_PLANE_H*/
