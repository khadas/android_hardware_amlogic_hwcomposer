/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VideoPlane.h"
#include <MesonLog.h>

VideoPlane::VideoPlane(int32_t drvFd, uint32_t id) :
    HwDisplayPlane(drvFd, id) {
    mPlaneType = VIDEO_PLANE;
}

VideoPlane::~VideoPlane() {

}

int VideoPlane::setPlane(std::shared_ptr<DrmFramebuffer> & fb) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t VideoPlane::blank() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}


