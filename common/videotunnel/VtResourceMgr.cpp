/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <VtResourceMgr.h>

ANDROID_SINGLETON_STATIC_INSTANCE(VtResourceMgr)

VtResourceMgr::VtResourceMgr() {
}

VtResourceMgr::~VtResourceMgr() {
}

int32_t VtResourceMgr::connectInstance(int tunnelId __unused,
        std::shared_ptr<VtConsumer> & consumer __unused) {
    return 0;
}

int32_t VtResourceMgr::disconnectInstance(int tunnelId __unused,
        std::shared_ptr<VtConsumer> & consumer __unused) {
    return 0;
}

int32_t VtResourceMgr::acquireBuffer() {
    return 0;
}

int32_t VtResourceMgr::recieveCmd() {
    return 0;
}
