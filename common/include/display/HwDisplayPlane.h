/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef HW_DISPLAY_PLANE_H
#define HW_DISPLAY_PLANE_H

#include <stdlib.h>

#include <DrmFramebuffer.h>


enum {
    OSD_HW_CURSOR        = (1 << 0),
    OSD_VIDEO_CONFLICT   = (1 << 1),
    OSD_FREESCALE        = (1 << 2),
    OSD_UBOOT_LOGO       = (1 << 3),
    OSD_LAYER_ENABLE     = (1 << 31),
};

class HwDisplayPlane {
public:
    HwDisplayPlane(int32_t drvFd, uint32_t id);
    virtual ~HwDisplayPlane();

    virtual const char * getName() = 0;
    /*PlaneType is a comb of drm_plane_type_mask*/
    virtual uint32_t getPlaneType() = 0;

    virtual int32_t setPlane(std::shared_ptr<DrmFramebuffer> & fb) = 0;
    virtual int32_t getCapabilities() = 0;

    virtual int32_t blank(bool blank) = 0;

    // virtual int32_t pageFlip(int32_t &outFence) = 0;

    virtual void dump(String8 & dumpstr) = 0;

    int32_t getDrvFd() {return mDrvFd;}

    virtual int32_t updateOsdPosition(const char * axis) {return 0;}

protected:
    int32_t mDrvFd;
    uint32_t mId;
    uint32_t mPlaneType;
    int32_t mCapability;
};

 #endif/*HW_DISPLAY_PLANE_H*/
