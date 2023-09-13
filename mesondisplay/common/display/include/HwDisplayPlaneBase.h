/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HW_DISPLAY_PLANE_BASE_H
#define HW_DISPLAY_PLANE_BASE_H

#include <stdlib.h>
#include <DrmFramebuffer.h>

class HwDisplayPlane {
public:
    HwDisplayPlane() { }
    virtual ~HwDisplayPlane() { }

    virtual uint32_t getId() = 0;
    virtual const char * getName() = 0;
    virtual uint32_t getType() = 0;
    virtual uint32_t getCapabilities() = 0;

    /*return the crtc masks.*/
    virtual uint32_t getPossibleCrtcs() = 0;
    virtual int32_t setCrtcId(uint32_t crtcid) = 0;

    /*Plane with fixed zorder will return a zorder >=0, or will return < 0.*/
    virtual int32_t getFixedZorder() = 0;

    virtual bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) = 0;
    virtual bool isAvailable() {return true;}

    virtual int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb,
        uint32_t zorder, int blankOp) = 0;

    /* release caches */
    virtual void clearPlaneResources() = 0;

    virtual void dump(std::string & dumpstr) = 0;
};

#endif /*HW_DISPLAY_PLANE_BASE_H*/

