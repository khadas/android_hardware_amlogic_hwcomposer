/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <unistd.h>

#include "HwDisplayPlaneFbdev.h"

HwDisplayPlaneFbdev::HwDisplayPlaneFbdev(int32_t drvFd, uint32_t id)
    : HwDisplayPlane() {
    mDrvFd = drvFd;
    mId = id;
    mDebugIdle = false;
    mDebugPattern = false;
}

HwDisplayPlaneFbdev::~HwDisplayPlaneFbdev() {
    close(mDrvFd);
}

void HwDisplayPlaneFbdev::setDebugFlag(int dbgFlag) {
    if (dbgFlag & PLANE_DBG_IDLE)
        mDebugIdle = true;
    else
        mDebugIdle = false;

    if (dbgFlag & PLANE_DBG_PATTERN)
        mDebugPattern = true;
    else
        mDebugPattern = false;
}

