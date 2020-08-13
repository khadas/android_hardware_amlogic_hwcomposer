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

class DrmPlane : public HwDisplayPlane {
public:
    DrmPlane();
    ~DrmPlane();

    const char * getName();
    uint32_t getPlaneType();
    uint32_t getCapabilities();

    int32_t getFixedZorder();

    uint32_t getPossibleCrtcs();
        bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb,
        uint32_t zorder, int blankOp);

    void setDebugFlag(int dbgFlag);

    void dump(String8 & dumpstr);

    uint32_t getPlaneId();
};

 #endif/*DRM_PLANE_H*/