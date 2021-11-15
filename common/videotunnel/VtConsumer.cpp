/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <VtConsumer.h>

VtConsumer::VtConsumer() {
}

VtConsumer::~VtConsumer() {
}

int32_t VtConsumer::SetReleaseListener(std::shared_ptr<VtReleaseListener> &listener) {
    mReleaseListener = listener;

    return 0;
}
