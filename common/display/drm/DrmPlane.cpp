/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>

#include "DrmPlane.h"

DrmPlane::DrmPlane()
    : HwDisplayPlane() {

}

DrmPlane::~DrmPlane() {
}

const char * DrmPlane::getName() {
    MESON_LOG_EMPTY_FUN();
    return NULL;
}

uint32_t DrmPlane::getPlaneId() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

uint32_t DrmPlane::getPlaneType() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

uint32_t DrmPlane::getCapabilities() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmPlane::getFixedZorder() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

uint32_t DrmPlane::getPossibleCrtcs() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

bool DrmPlane::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    UNUSED(fb);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmPlane::setPlane(std::shared_ptr<DrmFramebuffer> fb,
    uint32_t zorder, int blankOp) {
    UNUSED(fb);
    UNUSED(zorder);
    UNUSED(blankOp);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void DrmPlane::setDebugFlag(int dbgFlag) {
    UNUSED(dbgFlag);
    MESON_LOG_EMPTY_FUN();
}

void DrmPlane::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
    MESON_LOG_EMPTY_FUN();
}


