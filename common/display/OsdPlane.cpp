/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "OsdPlane.h"
#include <MesonLog.h>

OsdPlane::OsdPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane (drvFd, id) {
    getProperties();
}

OsdPlane::~OsdPlane() {

}

int32_t OsdPlane::getProperties() {
    /**/

    mPlaneType = OSD_PLANE;
    return 0;
}

int OsdPlane::setPlane(std::shared_ptr<DrmFramebuffer> & fb) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t OsdPlane::blank() {
    MESON_LOG_EMPTY_FUN();

    return 0;
}


