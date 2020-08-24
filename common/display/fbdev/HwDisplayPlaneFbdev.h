/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef HW_DISPLAY_PLANE_FBDEV_H
#define HW_DISPLAY_PLANE_FBDEV_H

#include <stdlib.h>
#include <DrmFramebuffer.h>
#include <HwDisplayCrtc.h>
#include <HwDisplayPlane.h>

class HwDisplayPlaneFbdev : public HwDisplayPlane {
public:
    HwDisplayPlaneFbdev(int32_t drvFd, uint32_t id);
    virtual ~HwDisplayPlaneFbdev();

    virtual const char * getName() = 0;
    virtual uint32_t getType() = 0;
    virtual uint32_t getCapabilities() = 0;

    /*Plane with fixed zorder will return a zorder >=0, or will return < 0.*/
    virtual int32_t getFixedZorder() = 0;

    virtual uint32_t getPossibleCrtcs() = 0;
    virtual bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) = 0;

    virtual int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb,
        uint32_t zorder, int blankOp) = 0;

    /*For debug, plane return a invalid type.*/
    enum {
        PLANE_DBG_IDLE = 1 << 0,
        PLANE_DBG_PATTERN = 1 << 1,
    };
    virtual void setDebugFlag(int dbgFlag);

    virtual void dump(String8 & dumpstr) = 0;

    int32_t getDrvFd() {return mDrvFd;}
    uint32_t getId() {return mId;}

protected:
    int32_t mDrvFd;
    uint32_t mId;
    int32_t mCapability;
    bool mDebugIdle;
    bool mDebugPattern;
};

 #endif/*HW_DISPLAY_PLANE_FBDEV_H*/
