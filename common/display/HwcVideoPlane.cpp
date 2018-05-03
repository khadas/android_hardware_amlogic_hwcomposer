/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include "HwcVideoPlane.h"


HwcVideoPlane::HwcVideoPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id) {
    snprintf(mName, 64, "HwcVideo-%d", id);
}

HwcVideoPlane::~HwcVideoPlane() {
}

const char * HwcVideoPlane::getName() {
    return mName;
}

uint32_t HwcVideoPlane::getPlaneType() {
    return HWC_VIDEO_PLANE;
}

int32_t HwcVideoPlane::getCapabilities() {
    return PLANE_SUPPORT_ZORDER;
}

int32_t HwcVideoPlane::setPlane(std::shared_ptr<DrmFramebuffer> & fb) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwcVideoPlane::blank(int blankOp) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void HwcVideoPlane::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
    MESON_LOG_EMPTY_FUN();
}

