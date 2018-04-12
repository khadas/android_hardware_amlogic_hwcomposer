/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DummyPlane.h"

DummyPlane::DummyPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id) {
}

uint32_t DummyPlane::getPlaneType() {
    return OSD_PLANE;
}

int32_t DummyPlane::setPlane(std::shared_ptr<DrmFramebuffer> & fb) {
    UNUSED(fb);
    return 0;
}

int32_t DummyPlane::getCapabilities() {
    return 0;
}

int32_t DummyPlane::blank(bool blank) {
    UNUSED(blank);
    return 0;
}

void DummyPlane::dump(String8 & dumpstr) {
    dumpstr.appendFormat(" DummyPlane type (%d) \n",
        getPlaneType());
};
