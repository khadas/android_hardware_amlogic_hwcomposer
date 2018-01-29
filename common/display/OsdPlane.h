/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef OSD_PLANE_H
#define OSD_PLANE_H

#include <HwDisplayPlane.h>

class OsdPlane : public HwDisplayPlane {
public:
    OsdPlane(int32_t drvFd, uint32_t id);
    ~OsdPlane();

    uint32_t getPlaneType() {return mPlaneType;}

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t blank();

protected:
    int32_t getProperties();
};


 #endif/*OSD_PLANE_H*/
