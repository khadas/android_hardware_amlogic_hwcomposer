/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <unistd.h>

#include <HwDisplayPlane.h>

HwDisplayPlane::HwDisplayPlane(int32_t drvFd, uint32_t id) {
    mDrvFd = drvFd;
    mId = id;
    mDebugIdle = false;
    mDebugPattern = false;
}

HwDisplayPlane::~HwDisplayPlane() {
    close(mDrvFd);
}

void HwDisplayPlane::setDebugFlag(int dbgFlag) {
    if (dbgFlag & PLANE_DBG_IDLE)
        mDebugIdle = true;
    else
        mDebugIdle = false;

    if (dbgFlag & PLANE_DBG_PATTERN)
        mDebugPattern = true;
    else
        mDebugPattern = false;
}

